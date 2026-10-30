package gateway

import (
	"context"
	"errors"
	"fmt"
	"math/rand"
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

// actionWire stands in for the runtime connection of a link's writer: it
// counts the ActionBatches each successful write carried, per player, and
// fails every write while fail is set.
type actionWire struct {
	fail    bool
	written map[uint32]uint64 // player -> action batches written
}

func replaceActionWire(t *testing.T) *actionWire {
	t.Helper()
	wire := &actionWire{written: map[uint32]uint64{}}
	replaceRuntimeWrite(t, func(_ *framing.TCPConnection, payload []byte, _ time.Duration) error {
		if wire.fail {
			return errors.New("injected write failure")
		}
		var e runtime.RuntimeEnvelope
		if err := proto.Unmarshal(payload, &e); err != nil {
			return err
		}
		if in := e.GetActions(); in != nil {
			wire.written[in.PlayerId]++
		}
		return nil
	})
	return wire
}

func shotBatch(player, ack uint32, ids ...uint32) *runtime.ActionBatch {
	in := &runtime.ActionBatch{PlayerId: player, AcknowledgedThrough: ack}
	for _, id := range ids {
		in.Shots = append(in.Shots, shot(id))
	}
	return in
}

// actionPackets reads one player's action-packet counters from the link:
// written, overflowed, cleared on close, abandoned.
func actionPackets(l *runtimeLink, player uint32) (written, overflow, cleared, abandoned uint64) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if c := l.ingress[player]; c != nil {
		return c[playerFwdLinkWrittenActionsPackets], c[playerDropIpcQueueOverflowActionsPackets],
			c[playerDropClearedOnCloseActionsPackets], c[playerDropAbandonedOnCloseActionsPackets]
	}
	return 0, 0, 0, 0
}

// pv7 §2a: each Client datagram's action batch reaches the Match as one IPC
// message, exactly as decoded: a repeat, a conflicting request, an ACK-only
// batch behind the retirement, a request beyond any window. The Match judges
// them (D-1 is moot); the link neither validates, merges, holds nor resends.
func TestLinkForwardsEachActionBatchAsItCame(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	conflicting := &client.ShotRequest{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(1)}
	sent := []*client.ActionBatch{
		shotsMessage(1, 2),
		shotsMessage(1, 2), // a repeat
		{Shots: []*client.ShotRequest{conflicting}},
		{AcknowledgedThrough: 0}, // ACK-only, behind any retirement
		{AcknowledgedThrough: 9}, // acknowledges what was never decided
		shotsMessage(900),        // far beyond any window
	}
	for k, b := range sent {
		deliverActions(t, s, p, endpoint, uint32(k+2), b, now.Add(time.Duration(k)*time.Millisecond))
	}
	batch := s.link.batch()
	if len(batch) != len(sent) {
		t.Fatalf("%d IPC messages for %d datagrams", len(batch), len(sent))
	}
	for k, e := range batch {
		got, want := e.GetActions(), sent[k]
		if got == nil || got.PlayerId != 7 || got.AcknowledgedThrough != want.AcknowledgedThrough || len(got.Shots) != len(want.Shots) {
			t.Fatalf("message %d is not datagram %d: %v", k, k, got)
		}
		for i, shot := range got.Shots {
			if shot.ActionId != want.Shots[i].ActionId || shot.GetYaw() != want.Shots[i].GetYaw() {
				t.Fatalf("message %d shot %d changed: %v", k, i, shot)
			}
		}
	}
	counts := s.playerIngress[7]
	if counts[playerFwdHandedActionsPackets] != uint64(len(sent)) || counts[playerDropLinkRejectedActionsPackets] != 0 {
		t.Fatalf("datagram outcomes: %v", counts)
	}
}

// Every action batch handed to the link ends in exactly one exit: written,
// dropped from a full FIFO (33), cleared on close or abandoned by a failed
// write. A Leave or an eviction drops none: queued batches still reach the
// Match in arrival order.
func TestLinkConservesActionBatchesAcrossEveryExit(t *testing.T) {
	players := []uint32{1, 2, 3}
	var exercised [4]int // seeds in which each exit was non-zero
	for seed := int64(1); seed <= 300; seed++ {
		t.Run(fmt.Sprint(seed), func(t *testing.T) {
			r := rand.New(rand.NewSource(seed))
			wire := replaceActionWire(t)
			l := dialedLink(t)
			handed := map[uint32]uint64{}
			var held []linkWrite
			holding := false
			for step := 0; step < 200; step++ {
				p := players[r.Intn(len(players))]
				switch op := r.Intn(12); {
				case op < 8: // the Client's batch; now and then a burst past the FIFO
					n := 1
					if r.Intn(20) == 0 {
						n = maxQueuedInputs + r.Intn(8)
					}
					for range n {
						if err := l.actions(shotBatch(p, uint32(r.Intn(3)), uint32(1+r.Intn(40))), dropEvent{}); err != nil {
							t.Fatal(err)
						}
						handed[p]++
					}
				case op < 10: // a write pass, sometimes held while more arrive
					if holding {
						if err := l.write(held); err != nil {
							t.Fatal(err)
						}
						holding = false
						continue
					}
					pass := l.take()
					if r.Intn(3) == 0 {
						held, holding = pass, true
					} else if err := l.write(pass); err != nil {
						t.Fatal(err)
					}
				case op == 10: // a Leave: queued batches stay
					leave := envelope()
					leave.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: p}}
					if err := l.control(leave); err != nil {
						t.Fatal(err)
					}
				default: // an eviction (forget, no Leave)
					l.forget(p)
				}
			}
			end := r.Intn(3)
			wire.fail = end == 0
			if holding {
				_ = l.write(held)
			}
			if end == 1 {
				wire.fail = true
				_ = l.write(l.take())
			}
			l.close()
			for _, p := range players {
				written, overflow, cleared, abandoned := actionPackets(l, p)
				for i, n := range []uint64{written, overflow, cleared, abandoned} {
					if n > 0 {
						exercised[i]++
					}
				}
				if handed[p] != written+overflow+cleared+abandoned {
					t.Fatalf("player %d: handed %d != written %d + overflow %d + cleared %d + abandoned %d",
						p, handed[p], written, overflow, cleared, abandoned)
				}
				if written != wire.written[p] {
					t.Fatalf("player %d: counted %d written, the wire carried %d", p, written, wire.written[p])
				}
			}
		})
	}
	for i, n := range exercised {
		if n < 10 {
			t.Errorf("exit %d was non-zero for only %d players: the interleavings miss it", i, n)
		}
	}
	t.Logf("players per non-zero exit (written, overflow, cleared, abandoned): %v", exercised)
}

// The action-packet exits reach the 10 s and final lines: a left player's
// queued batch is written or cleared like any other (no Leave discard).
func TestActionDropsReachTheWindowAndFinalLines(t *testing.T) {
	logs := captureIngressLog(t)
	s, p, endpoint, now := actionFixture(t)
	s.link = dialedLink(t)
	s.ingressWindowStart = now
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
	f.send(t, f.datagram(adapter.Actions, 2, shotsMessage(1, 2)))
	second, err := session.New(now)
	if err != nil {
		t.Fatal(err)
	}
	stayEndpoint := netip.MustParseAddrPort("127.0.0.1:29009")
	if second.Hello(second.Token, stayEndpoint, 1, now) != nil {
		t.Fatal("hello")
	}
	stay := &reservation{playerID: 9, phase: active, session: second}
	s.mu.Lock()
	s.players[9], s.sessions[second.ID] = stay, stay
	s.mu.Unlock()
	g := &outcomeFixture{s: s, p: stay, endpoint: stayEndpoint, now: now}
	g.send(t, g.datagram(adapter.Actions, 2, shotsMessage(1, 2, 3)))

	// Player 7 leaves as leaveRoom does: its queued batch stays queued.
	s.mu.Lock()
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 7}}
	if err := s.link.control(e); err != nil {
		t.Fatal(err)
	}
	s.remove(p)
	s.mu.Unlock()
	s.logStatisticsWindow(now.Add(10*time.Second), 10*time.Second)
	s.runtimeFailed(errors.New("test end"))

	lines := logs.lines(t)
	checkIngressLines(t, "action drops", lines, s.rateRejectedPackets.Load())
	type seen struct{ handed, cleared uint64 }
	got := map[uint32]seen{}
	for _, line := range lines {
		if line.player != 0 {
			g := got[line.player]
			got[line.player] = seen{g.handed + line.counts[playerFwdHandedActionsPackets],
				g.cleared + line.counts[playerDropClearedOnCloseActionsPackets]}
		}
	}
	if want := (map[uint32]seen{7: {1, 1}, 9: {1, 1}}); fmt.Sprint(got) != fmt.Sprint(want) {
		t.Errorf("players' (handed, cleared on close) action packets %v, want %v", got, want)
	}
}

// pv7 §2a, §4 (33): beyond maxQueuedActions queued action batches of the
// player, its oldest is dropped and reported with its datagram's identity:
// the bounds of its action ids (0 for an ACK-only batch) and their count.
func TestActionQueueOverflowReportsTheDroppedBatches(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	sent := []*client.ActionBatch{{AcknowledgedThrough: 5}, shotsMessage(3, 1, 2)}
	for k := range maxQueuedActions {
		sent = append(sent, shotsMessage(uint32(10+k)))
	}
	for k, b := range sent {
		deliverActions(t, s, p, endpoint, uint32(k+2), b, now.Add(time.Duration(k)*10*time.Millisecond))
	}
	_, overflow, _, _ := actionPackets(s.link, 7)
	if overflow != 2 {
		t.Fatalf("overflowed batches counted %d, want 2", overflow)
	}
	if batch := s.link.batch(); len(batch) != maxQueuedActions || len(batch[0].GetActions().GetShots()) == 0 ||
		batch[0].GetActions().GetShots()[0].ActionId != 10 {
		t.Fatalf("%d batches kept, want the newest %d", len(batch), maxQueuedActions)
	}
	report := s.link.drops.report(7, now.Add(time.Minute))
	// transport sequence, first id, last id, items; an ACK-only batch has no id
	// and the ledger counts the batch itself as its one item
	want := [][4]uint64{{2, 0, 0, 1}, {3, 1, 3, 3}}
	if report == nil || len(report.Samples) != len(want) {
		t.Fatalf("overflow report %v", report)
	}
	for k, sample := range report.Samples {
		got := [4]uint64{uint64(sample.TransportSequence), sample.FirstActionId, sample.LastActionId, uint64(sample.ItemCount)}
		if sample.Reason != runtime.DropReason_DROP_IPC_QUEUE_OVERFLOW_ACTIONS_PACKETS || got != want[k] {
			t.Fatalf("sample %d %v does not carry the dropped batch %v", k, sample, want[k])
		}
	}
}

// An action batch wakes the writer at once: it does not wait for the idle
// timer or the next input.
func TestRuntimeLinkWritesAnActionAtOnce(t *testing.T) {
	setForTest(t, &writerIdleWait, 10*time.Second)
	written := make(chan struct{}, 4)
	setForTest(t, &runtimeWrite, func(_ *framing.TCPConnection, payload []byte, _ time.Duration) error {
		var e runtime.RuntimeEnvelope
		if proto.Unmarshal(payload, &e) == nil && e.GetActions() != nil {
			written <- struct{}{}
		}
		return nil
	})
	l := dialedLink(t)
	l.drops, l.now, l.diagnosticWake = newDropLedger(), time.Now, make(chan struct{}, 1)
	l.clock = newClockSync(gatewayMicros(time.Now()))
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	l.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	defer l.close()
	time.Sleep(20 * time.Millisecond) // the writer is idle, waiting on its timer
	if err := l.actions(shotBatch(1, 0, 1), dropEvent{}); err != nil {
		t.Fatal(err)
	}
	select {
	case <-written:
	case <-time.After(time.Second):
		t.Fatal("an action waited for the writer's idle timer")
	}
}

// pv7 §2a, §4: input windows and action batches are bounded apart, each
// dropping the oldest of its own kind (32, 33): a player keeps 64 input
// windows however many action batches wait, and the reverse.
func TestQueueBoundsInputsAndActionsApart(t *testing.T) {
	l := dialedLink(t)
	l.drops = newDropLedger()
	for k := uint32(1); k <= 10; k++ {
		if err := l.actions(shotBatch(1, 0, k), dropEvent{}); err != nil {
			t.Fatal(err)
		}
	}
	for k := uint32(1); k <= maxQueuedInputs+3; k++ {
		if err := l.input(linkWindow(1, 1, k, k), dropEvent{}); err != nil {
			t.Fatal(err)
		}
	}
	for k := uint32(11); k <= maxQueuedActions+2; k++ {
		if err := l.actions(shotBatch(1, 0, k), dropEvent{}); err != nil {
			t.Fatal(err)
		}
	}
	totals := linkTotals{}.take(l).of(1)
	if totals[playerDropIpcQueueOverflowInputPackets] != 3 || totals[playerDropIpcQueueOverflowActionsPackets] != 2 {
		t.Fatalf("overflowed input windows %d (want 3), action batches %d (want 2)",
			totals[playerDropIpcQueueOverflowInputPackets], totals[playerDropIpcQueueOverflowActionsPackets])
	}
	var inputs, actions []uint32
	for _, e := range l.batch() {
		if in := e.GetInput(); in != nil {
			inputs = append(inputs, in.Commands[0].Sequence)
		} else if a := e.GetActions(); a != nil {
			actions = append(actions, a.Shots[0].ActionId)
		}
	}
	if len(inputs) != maxQueuedInputs || inputs[0] != 4 || len(actions) != maxQueuedActions || actions[0] != 3 {
		t.Fatalf("kept %d input windows from %v and %d action batches from %v; want the newest of each kind",
			len(inputs), inputs[:min(1, len(inputs))], len(actions), actions[:min(1, len(actions))])
	}
}

// One write pass keeps each player's input windows and action batches in the
// order they arrived (pv7 §2a), interleaved with other players' and controls.
func TestPlayerDataKeepsArrivalOrderAcrossKinds(t *testing.T) {
	l := dialedLink(t)
	var want []string
	add := func(player uint32, kind string, n uint32) {
		var err error
		if kind == "input" {
			err = l.input(linkWindow(player, 1, n, n), dropEvent{})
		} else {
			err = l.actions(shotBatch(player, 0, n), dropEvent{})
		}
		if err != nil {
			t.Fatal(err)
		}
		want = append(want, fmt.Sprintf("%d/%s/%d", player, kind, n))
	}
	add(1, "input", 1)
	add(1, "actions", 1)
	add(2, "actions", 1)
	add(1, "input", 2)
	add(2, "input", 1)
	add(1, "actions", 2)
	var got []string
	for _, e := range l.batch() {
		if in := e.GetInput(); in != nil {
			got = append(got, fmt.Sprintf("%d/input/%d", in.PlayerId, in.Commands[0].Sequence))
		} else if a := e.GetActions(); a != nil {
			got = append(got, fmt.Sprintf("%d/actions/%d", a.PlayerId, a.Shots[0].ActionId))
		}
	}
	if fmt.Sprint(got) != fmt.Sprint(want) {
		t.Fatalf("written %v, want the arrival order %v", got, want)
	}
}

// A batch queued before the player left is still written, but records no
// send statistics: forget's removal stays final.
func TestBatchWrittenAfterLeaveKeepsNoStatistics(t *testing.T) {
	wire := replaceActionWire(t)
	l := dialedLink(t)
	if err := l.actions(shotBatch(1, 0, 1), dropEvent{}); err != nil {
		t.Fatal(err)
	}
	l.forget(1)
	if err := l.write(l.take()); err != nil {
		t.Fatal(err)
	}
	l.mu.Lock()
	_, kept := l.actionStats[1]
	l.mu.Unlock()
	if wire.written[1] != 1 || kept {
		t.Fatalf("written %d (want 1); statistics recreated after forget: %v", wire.written[1], kept)
	}
}
