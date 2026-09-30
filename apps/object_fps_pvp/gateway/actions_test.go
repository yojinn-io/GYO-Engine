package gateway

import (
	"math"
	"net/netip"
	"sync"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv5"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev5"
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
	p := &reservation{lifeGeneration: 1, playerID: 7, phase: active, session: peer, movementEpoch: 1, commands: map[uint64]*runtime.MovementCommand{}, actions: newActionWindow()}
	l := &runtimeLink{inputs: map[uint64]*runtime.PlayerInput{}, epochs: map[uint64]uint64{}, wake: make(chan struct{}, 1)}
	s := &Server{available: true, ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, TickRate: 60, SnapshotIntervalTicks: 1, CombatRules: testRules()}, link: l, players: map[uint64]*reservation{7: p}, sessions: map[uint64]*reservation{peer.ID: p}, controlOut: make(chan outbound, 64), snapshotOut: make(chan []byte, 1)}
	return s, p, endpoint, now
}
func deliverActions(t *testing.T, s *Server, p *reservation, endpoint netip.AddrPort, seq uint32, b *client.ActionBatch, now time.Time) {
	t.Helper()
	payload, _ := proto.Marshal(b)
	if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Actions, SessionID: p.session.ID, Sequence: seq}, payload, endpoint, now); err != nil {
		t.Fatal(err)
	}
}
func TestGatewayActionAtomicConflictACKTruthAndSessionIdentity(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	// Client has no trusted player field. Even a protobuf unknown field cannot
	// replace the identity selected from its authenticated session.
	batch := &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 2, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}}
	batch.ProtoReflect().SetUnknown([]byte{0x18, 99})
	deliverActions(t, s, p, netip.MustParseAddrPort("127.0.0.1:29002"), 2, batch, now)
	if len(p.actions.requests) != 0 {
		t.Fatal("spoofed endpoint")
	}
	deliverActions(t, s, p, endpoint, 2, batch, now)
	if len(p.actions.requests) != 1 || s.link.actionWindows[7] == nil || s.link.actionWindows[99] != nil {
		t.Fatal("identity mapping")
	}
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(2)}}); err != nil {
		t.Fatal(err)
	}
	deliverActions(t, s, p, endpoint, 3, &client.ActionBatch{AcknowledgedThrough: 2}, now)
	if p.actions.acknowledged != 0 {
		t.Fatal("ACK crossed missing decision1")
	}
	deliverActions(t, s, p, endpoint, 4, &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}, {LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 2, ObservedAuthorityTick: 1, Yaw: proto.Float32(1)}}}, now)
	if len(p.actions.requests) != 1 || p.actions.requests[1] != nil {
		t.Fatal("conflict partially admitted")
	}
	deliverActions(t, s, p, endpoint, 5, &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}, {LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}}, now)
	if len(p.actions.requests) != 2 {
		t.Fatal("identical duplicate rejected")
	}
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}}); err != nil {
		t.Fatal(err)
	}
	// A valid ACK paired with malformed/conflicting content must remain atomic.
	deliverActions(t, s, p, endpoint, 6, &client.ActionBatch{AcknowledgedThrough: 2, Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 2, ObservedAuthorityTick: 1, Yaw: proto.Float32(1)}}}, now)
	if p.actions.acknowledged != 0 {
		t.Fatal("bad batch committed ACK")
	}
	deliverActions(t, s, p, endpoint, 7, &client.ActionBatch{AcknowledgedThrough: 2}, now)
	if p.actions.acknowledged != 2 || s.link.actionWindows[7].acknowledged != 2 || len(p.actions.decisions) != 2 {
		t.Fatal("ACK lost or retired before authority")
	}
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, RetiredThrough: 2}); err != nil {
		t.Fatal(err)
	}
	deliverActions(t, s, p, endpoint, 8, batch, now)
	if p.actions.retired != 2 || len(p.actions.requests) != 0 || len(s.link.actionWindows[7].requests) != 0 {
		t.Fatal("retired request revived")
	}
	// Movement resets do not clear decisions, requests, ACKs or the retirement floor.
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 2, Players: []*runtime.PlayerState{{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, PlayerId: 7, MovementEpoch: 2}}, Combat: []*runtime.CombatState{{LifeGeneration: 1, MagazineAmmo: 12, PlayerId: 7, Hp: 100}}}}
	s.runtimeMessage(e)
	if p.actions.retired != 2 || p.movementEpoch != 2 {
		t.Fatal("movement reset cleared action lifecycle")
	}
}
func TestActionWindowsBoundedCircularAndDeadlineRecovery(t *testing.T) {
	for _, pause := range []time.Duration{250 * time.Millisecond, time.Second} {
		t.Run(pause.String(), func(t *testing.T) {
			s, p, _, now := actionFixture(t)
			for start := uint64(1); start <= 32; start += 8 {
				b := &runtime.ActionBatch{PlayerId: 7}
				for id := start; id < start+8; id++ {
					b.Shots = append(b.Shots, shot(id))
				}
				if p.actions.validate(b) != nil {
					t.Fatal("valid window")
				}
				p.actions.merge(b)
				if s.link.actions(b) != nil {
					t.Fatal("runtime window")
				}
			}
			if s.link.actions(&runtime.ActionBatch{PlayerId: 7, Shots: []*runtime.ShotRequest{shot(33)}}) == nil {
				t.Fatal("33rd pending action accepted")
			}
			// Withhold consumers while concurrent ingress repeats retained immutable data.
			var wg sync.WaitGroup
			wg.Add(1)
			go func() {
				defer wg.Done()
				until := time.Now().Add(pause)
				for time.Now().Before(until) {
					_ = s.link.actions(&runtime.ActionBatch{PlayerId: 7, Shots: []*runtime.ShotRequest{shot(1)}})
					time.Sleep(time.Millisecond)
				}
			}()
			time.Sleep(pause)
			wg.Wait()
			release := now.Add(pause)
			seen := map[uint64]bool{}
			for round := 0; round < 4; round++ {
				s.link.mu.Lock()
				frames := s.link.actionBatch(release.Add(time.Duration(round) * actionSendInterval))
				s.link.mu.Unlock()
				if len(frames) != 1 || len(frames[0].GetActions().Shots) != 8 {
					t.Fatalf("runtime result chunks %v", frames)
				}
				for _, q := range frames[0].GetActions().Shots {
					seen[q.ActionId] = true
				}
			}
			if len(seen) != 32 {
				t.Fatal("circular request coverage")
			}
			s.link.mu.Lock()
			burst := s.link.actionBatch(release.Add(100 * time.Millisecond))
			s.link.mu.Unlock()
			if len(burst) != 0 {
				t.Fatal("catch-up burst")
			}
			for start := uint64(1); start <= 32; start += 8 {
				r := &runtime.ActionResults{PlayerId: 7}
				for id := start; id < start+8; id++ {
					r.Decisions = append(r.Decisions, decision(id))
				}
				if err := s.receiveActionResults(r); err != nil {
					t.Fatal(err)
				}
			}
			// Fill the existing lossy control queue and replace snapshots. Neither can
			// drop a retained result; result delivery selects directly from its ledger.
			for i := 0; i < 64; i++ {
				s.controlOut <- outbound{}
			}
			seen = map[uint64]bool{}
			for round := 0; round < 4; round++ {
				packets := s.actionPackets(release.Add(time.Duration(round) * actionSendInterval))
				if len(packets) != 1 {
					t.Fatal("missing result batch")
				}
				h, b, err := framing.DecodeDatagram(packets[0].packet)
				var results client.ActionResults
				if err != nil || h.Type != adapter.ActionResults || proto.Unmarshal(b, &results) != nil || len(results.Decisions) != 8 {
					t.Fatal("bad result datagram")
				}
				for _, d := range results.Decisions {
					seen[d.ActionId] = true
				}
			}
			if len(seen) != 32 || len(p.actions.decisions) != 32 {
				t.Fatal("result coverage/retention")
			}
			if packets := s.actionPackets(release.Add(100 * time.Millisecond)); len(packets) != 0 {
				t.Fatal("result burst")
			}
			// Leave is the cleanup boundary, regardless of movement epoch.
			e := envelope()
			e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 7}}
			if err := s.link.control(e); err != nil {
				t.Fatal(err)
			}
			if len(s.link.actionWindows) != 0 {
				t.Fatal("leave retained actions")
			}
		})
	}
}
func TestActionOverflowAndRetiredDuplicateConflict(t *testing.T) {
	w := newActionWindow()
	w.retired = math.MaxUint64 - 1
	w.acknowledged = w.retired
	b := &runtime.ActionBatch{PlayerId: 1, Shots: []*runtime.ShotRequest{shot(math.MaxUint64)}}
	if w.validate(b) != nil {
		t.Fatal("last action id rejected")
	}
	w.merge(b)
	if w.validate(&runtime.ActionBatch{PlayerId: 1, Shots: []*runtime.ShotRequest{{LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, Yaw: proto.Float32(1)}, {LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, Yaw: proto.Float32(2)}}}) == nil {
		t.Fatal("conflicting duplicate retired contents")
	}
	if w.validate(&runtime.ActionBatch{PlayerId: 1, AcknowledgedThrough: math.MaxUint64}) == nil {
		t.Fatal("overflow ACK crossed pending")
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
	if err := s.admit(p, endpoint, seq+1, now); err != session.ErrRateLimit {
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
	// a later result packet, independent of any snapshot publication.
	first := receivePacket(t, p, adapter.ActionResults)
	second := receivePacket(t, p, adapter.ActionResults)
	if string(first) != string(second) {
		t.Fatal("decision changed after result loss")
	}
	var result client.ActionResults
	if proto.Unmarshal(second, &result) != nil || len(result.Decisions) != 1 || result.Decisions[0].ActionId != 1 {
		t.Fatal("result wire")
	}
	// Lost ACK is indistinguishable from never sending it. The retained result
	// must still arrive after another deadline.
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

func TestCompletedBlockedWriteReanchorsActionDeadline(t *testing.T) {
	s, p, _, now := actionFixture(t)
	in := &runtime.ActionBatch{PlayerId: 7, Shots: []*runtime.ShotRequest{shot(1)}}
	p.actions.merge(in)
	if s.link.actions(in) != nil {
		t.Fatal("admission")
	}
	s.link.mu.Lock()
	first := s.link.actionBatch(now)
	s.link.mu.Unlock()
	if len(first) != 1 {
		t.Fatal("first action")
	}
	release := now.Add(time.Second)
	s.link.actionWritten(7, release)
	s.link.mu.Lock()
	early := s.link.actionBatch(release.Add(actionSendInterval - 1))
	due := s.link.actionBatch(release.Add(actionSendInterval))
	s.link.mu.Unlock()
	if len(early) != 0 || len(due) != 1 {
		t.Fatal("TCP completion catch-up deadline")
	}
	if s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}}) != nil {
		t.Fatal("result")
	}
	packets := s.actionPackets(now)
	if len(packets) != 1 {
		t.Fatal("first result")
	}
	s.actionWritten(packets[0].actionPlayer, release)
	if len(s.actionPackets(release.Add(actionSendInterval-1))) != 0 || len(s.actionPackets(release.Add(actionSendInterval))) != 1 {
		t.Fatal("UDP completion catch-up deadline")
	}
}
