package gateway

import (
	"net/netip"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv8"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev8"
)

func shot(id uint64) *runtime.ShotRequest {
	return &runtime.ShotRequest{LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: id, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}
}
func decision(id uint64) *runtime.ShotDecision {
	return &runtime.ShotDecision{LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT, ActionId: id, ResolvedTick: 10, Accepted: true}
}
func actionFixture(t *testing.T) (*Server, *reservation, netip.AddrPort, time.Time) {
	t.Helper()
	now := time.Now()
	peer, err := session.New(now)
	if err != nil {
		t.Fatal(err)
	}
	endpoint := netip.MustParseAddrPort("127.0.0.1:29001")
	if peer.Hello(peer.Token, endpoint, 1, now) != nil {
		t.Fatal("hello")
	}
	p := &reservation{playerID: 7, phase: active, session: peer}
	// A dialed link closes like the real one: a broken invariant (D180 (g2))
	// fails the runtime, and that must not panic in a fixture.
	l := dialedLink(t)
	l.drops = newDropLedger()
	s := &Server{available: true, ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, TickRate: 60, SnapshotIntervalTicks: 1, CombatRules: testRules()}, link: l, players: map[uint64]*reservation{7: p}, sessions: map[uint64]*reservation{peer.ID: p}, controlOut: make(chan queuedControl, 64), snapshotOut: make(chan snapshotFrame, 1), resultWake: make(chan struct{}, 1)}
	return s, p, endpoint, now
}
func deliverActions(t *testing.T, s *Server, p *reservation, endpoint netip.AddrPort, seq uint32, b *client.ActionBatch, now time.Time) {
	t.Helper()
	payload, _ := proto.Marshal(b)
	if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Actions, SessionID: p.session.ID, Sequence: seq}, payload, endpoint, now); err != nil {
		t.Fatal(err)
	}
}

// The Client has no trusted player field: the batch is forwarded under the
// identity of its authenticated session, never another endpoint's, and an
// unknown protobuf field cannot replace it. Its content is the Match's to
// judge (pv7 §2a).
func TestGatewayActionAtomicConflictACKTruthAndSessionIdentity(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	batch := &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 2, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}}
	batch.ProtoReflect().SetUnknown([]byte{0x18, 99})
	deliverActions(t, s, p, netip.MustParseAddrPort("127.0.0.1:29002"), 2, batch, now)
	if got := s.link.batch(); len(got) != 0 {
		t.Fatalf("spoofed endpoint reached the Match: %v", got)
	}
	deliverActions(t, s, p, endpoint, 2, batch, now)
	got := s.link.batch()
	if len(got) != 1 || got[0].GetActions().GetPlayerId() != 7 || len(got[0].GetActions().Shots) != 1 {
		t.Fatalf("identity mapping: %v", got)
	}
}

// pv7 §2a: the Gateway keeps one results frame per player, latest wins; the
// three it replaced are counted (34), the Match sends them again.
func TestResultSlotKeepsTheLatestFrame(t *testing.T) {
	s, _, _, now := actionFixture(t)
	for start := uint64(1); start <= 32; start += 8 {
		r := &runtime.ActionResults{PlayerId: 7}
		for id := start; id < start+8; id++ {
			r.Decisions = append(r.Decisions, decision(id))
		}
		if err := s.receiveActionResults(r); err != nil {
			t.Fatal(err)
		}
	}
	packets, _, _ := s.actionPackets(now)
	if len(packets) != 1 {
		t.Fatalf("%d results frames released", len(packets))
	}
	h, b, err := framing.DecodeDatagram(packets[0].packet)
	var results client.ActionResults
	if err != nil || h.Type != adapter.ActionResults || proto.Unmarshal(b, &results) != nil || len(results.Decisions) != 8 ||
		results.Decisions[0].ActionId != 25 {
		t.Fatalf("released frame %v", &results)
	}
	replaced := uint64(0)
	if counts := s.playerIngress[7]; counts != nil {
		replaced = counts[playerDropResultsOverRatePackets]
	}
	if replaced != 3 {
		t.Fatalf("replaced frames counted %d, want 3", replaced)
	}
	if packets, _, _ := s.actionPackets(now.Add(100 * time.Millisecond)); len(packets) != 0 {
		t.Fatal("result burst")
	}
}
func TestMovementActionsHelloShareUnchanged120PacketBudget(t *testing.T) {
	s, p, endpoint, start := actionFixture(t)
	seq := uint32(1)
	// Four actual fixed windows including initial Hello. Movement at60Hz, action
	// resends at30Hz and keepalive at1Hz coexist without raising the public limit.
	for second := 0; second < 4; second++ {
		base := start.Add(time.Duration(second) * time.Second)
		for tick := 0; tick < 60; tick++ {
			now := base.Add(time.Duration(tick) * time.Second / 60)
			seq++
			in := &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 1}}}
			payload, _ := proto.Marshal(in)
			if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Input, SessionID: p.session.ID, Sequence: seq}, payload, endpoint, now); err != nil {
				t.Fatal(err)
			}
			if tick%2 == 0 {
				seq++
				deliverActions(t, s, p, endpoint, seq, &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}}, now)
			}
			if tick == 0 {
				seq++
				payload, _ = proto.Marshal(&client.Hello{SessionToken: p.session.Token})
				if s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Hello, SessionID: p.session.ID, Sequence: seq}, payload, endpoint, now) != nil {
					t.Fatal("hello")
				}
			}
		}
		// Every preceding valid packet must have committed its sequence. A fresh
		// Admit of that final sequence returns stale, never rate-limited.
		if err := p.session.Admit(endpoint, seq, base.Add(999*time.Millisecond)); err != session.ErrStale {
			t.Fatalf("60+30+1 failed window %d: %v", second, err)
		}
	}
	// Verify the unchanged public cap explicitly includes Hello and malformed/
	// unknown authenticated traffic. A new fixed window has exactly120 capacity.
	now := start.Add(5 * time.Second)
	seq++
	hello, _ := proto.Marshal(&client.Hello{SessionToken: p.session.Token})
	_ = s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Hello, SessionID: p.session.ID, Sequence: seq}, hello, endpoint, now)
	for i := 0; i < 119; i++ {
		seq++
		_ = s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: 99, SessionID: p.session.ID, Sequence: seq}, nil, endpoint, now)
	}
	if err := s.admit(p, ingressOther, endpoint, seq+1, now); err != session.ErrRateLimit {
		t.Fatalf("all authenticated traffic not counted: %v", err)
	}
	if s.maxSessionWindowPackets.Load() != 120 || s.rateRejectedPackets.Load() != 1 {
		t.Fatalf("rate diagnostics: max=%d rejected=%d", s.maxSessionWindowPackets.Load(), s.rateRejectedPackets.Load())
	}
}

func TestActionsRealUDPAndTCPResendUntilContiguousClientACK(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "actions"), peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	var welcome client.Welcome
	if proto.Unmarshal(receivePacket(t, p, adapter.Welcome), &welcome) != nil || welcome.CombatRules == nil || welcome.CombatRules.ShotDamage != 25 {
		t.Fatal("Match rules not in Welcome")
	}
	sendPacket(t, p, c, 2, adapter.Actions, &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}})
	in := f.next(t).GetActions()
	if in == nil || in.PlayerId != c.PlayerID || len(in.Shots) != 1 || in.Shots[0].ActionId != 1 {
		t.Fatalf("wire action mapping %v", in)
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_ActionResults{ActionResults: &runtime.ActionResults{PlayerId: c.PlayerID, Decisions: []*runtime.ShotDecision{decision(1)}}}
	f.send(e)
	// Drop the first client result, then require a byte-equivalent decision from
	// a later result packet, independent of any snapshot publication. Since
	// pv7 §2a the Match resends until the ACK; the Gateway forwards each frame.
	first := receivePacket(t, p, adapter.ActionResults)
	f.send(e)
	second := receivePacket(t, p, adapter.ActionResults)
	if string(first) != string(second) {
		t.Fatal("decision changed after result loss")
	}
	var result client.ActionResults
	if proto.Unmarshal(second, &result) != nil || len(result.Decisions) != 1 || result.Decisions[0].ActionId != 1 {
		t.Fatal("result wire")
	}
	// Lost ACK is indistinguishable from never sending it. The Match's next
	// resend still reaches the Client.
	f.send(e)
	receivePacket(t, p, adapter.ActionResults)
	sendPacket(t, p, c, 3, adapter.Actions, &client.ActionBatch{AcknowledgedThrough: 1})
	for {
		in = f.next(t).GetActions()
		if in != nil && in.AcknowledgedThrough == 1 {
			break
		}
	}
	if len(in.Shots) != 0 {
		t.Fatal("decided request still forwarded with ACK")
	}
	e = envelope()
	e.Message = &runtime.RuntimeEnvelope_ActionResults{ActionResults: &runtime.ActionResults{PlayerId: c.PlayerID, RetiredThrough: 1}}
	f.send(e)
	for {
		if proto.Unmarshal(receivePacket(t, p, adapter.ActionResults), &result) != nil {
			t.Fatal("retirement wire")
		}
		if result.RetiredThrough == 1 {
			break
		}
	}
	sendPacket(t, p, c, 4, adapter.Actions, &client.ActionBatch{AcknowledgedThrough: 1, Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 2, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}})
	for {
		in = f.next(t).GetActions()
		if in != nil && len(in.Shots) > 0 && in.Shots[0].ActionId == 2 {
			break
		}
	}
	if in.PlayerId != c.PlayerID {
		t.Fatal("recovery changed identity")
	}
}
func TestCompletedBlockedWriteReanchorsTheResultDeadline(t *testing.T) {
	s, _, _, now := actionFixture(t)
	release := now.Add(time.Second)
	if s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}}) != nil {
		t.Fatal("result")
	}
	packets, _, _ := s.actionPackets(now)
	if len(packets) != 1 {
		t.Fatal("first result")
	}
	s.actionWritten(packets[0].actionPlayer, release, 0)
	// Results keep a strict interval from the completed write: the next frame
	// (the Match resends until the ACK) waits for it.
	if s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}}) != nil {
		t.Fatal("resent result")
	}
	earlyResults, _, _ := s.actionPackets(release.Add(actionSendInterval - 1))
	dueResults, _, _ := s.actionPackets(release.Add(actionSendInterval))
	if len(earlyResults) != 0 || len(dueResults) != 1 {
		t.Fatal("UDP completion catch-up deadline")
	}
}

// An eviction sends no Leave: the Gateway forgets the player on the link, its
// queued batches still reach the Match (which counts them for an unknown
// player), and replacements are not limited by any per-player window.
func TestEvictionKeepsQueuedActionsAndReplacementsDeliver(t *testing.T) {
	s, p, _, _ := actionFixture(t)
	l := s.link
	if err := l.actions(&runtime.ActionBatch{PlayerId: p.playerID, Shots: []*runtime.ShotRequest{shot(1)}}, dropEvent{}); err != nil {
		t.Fatal(err)
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Evicted{Evicted: &runtime.PlayerEvicted{PlayerId: p.playerID,
		Reason: runtime.EvictionReason_EVICTION_HIGH_LATENCY, ReferenceAgeMs: 190}}
	l.actionWritten(p.playerID, time.Now()) // the link's statistics of the player
	s.runtimeMessage(e)
	l.mu.Lock()
	_, kept := l.actionStats[p.playerID]
	l.mu.Unlock()
	if kept {
		t.Fatal("the evicted player's link statistics kept")
	}
	for id := uint64(100); id < 100+2*adapter.MaxPlayers; id++ {
		if err := l.actions(&runtime.ActionBatch{PlayerId: id, Shots: []*runtime.ShotRequest{shot(1)}}, dropEvent{}); err != nil {
			t.Fatalf("replacement %d could not deliver actions: %v", id, err)
		}
	}
	batch := l.batch()
	if len(batch) != 1+2*adapter.MaxPlayers || batch[0].GetActions().GetPlayerId() != p.playerID {
		t.Fatalf("the evicted player's queued batch did not go first: %d messages", len(batch))
	}
}
