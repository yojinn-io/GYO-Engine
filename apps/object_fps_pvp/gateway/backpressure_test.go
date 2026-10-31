package gateway

import (
	"fmt"
	"net/netip"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// Interleave publication between the two actual per-peer selection calls. This
// exercises the case where the first UDP write was slow, without a production
// blocking switch or pretending an ordinary loopback UDP socket is saturated.
func TestSnapshotSelectionRefreshesUnwrittenPeerAndRevalidatesSession(t *testing.T) {
	s := &Server{available: true, ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, CombatRules: testRules()}, players: make(map[uint32]*reservation),
		controlOut: make(chan queuedControl, 64), snapshotOut: make(chan snapshotFrame, 1)}
	for id := uint32(1); id <= 2; id++ {
		peer, err := session.New(time.Now())
		if err != nil {
			t.Fatal(err)
		}
		endpoint := netip.AddrPortFrom(netip.MustParseAddr("127.0.0.1"), uint16(29000+id))
		if err := peer.Hello(peer.Token, endpoint, 1, time.Now()); err != nil {
			t.Fatal(err)
		}
		s.players[id] = &reservation{playerID: id, session: peer, phase: active}
	}
	decode := func(packet outbound) (framing.Header, uint32) {
		t.Helper()
		header, payload, err := framing.DecodeDatagram(packet.packet)
		var snapshot client.WorldSnapshot
		if err != nil || header.Type != adapter.Snapshot || proto.Unmarshal(payload, &snapshot) != nil {
			t.Fatal("invalid selected snapshot datagram")
		}
		return header, snapshot.Tick
	}
	frame, first := s.snapshotForPeer([]uint32{1, 2}, snapshotFrame{snapshot: &client.WorldSnapshot{Tick: 1}, read: time.Now(), tick: 1})
	firstHeader, firstTick := decode(first)
	if firstTick != 1 || firstHeader.Sequence != 1 || firstHeader.SessionID != s.players[1].session.ID {
		t.Fatal("first peer did not receive the initial publication")
	}
	secondPeer := s.players[2]
	s.sendControl(secondPeer, adapter.Welcome, &client.Welcome{JumpHeight: .6, Gravity: 18, PlayerId: 2})
	s.sendControl(secondPeer, adapter.Welcome, &client.Welcome{JumpHeight: .6, Gravity: 18, PlayerId: 2})
	for tick := uint32(2); tick <= 3; tick++ {
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick}}
		fillAnimation(e.GetSnapshot())
		s.runtimeMessage(e)
	}
	frame, second := s.snapshotForPeer([]uint32{2}, frame)
	secondHeader, secondTick := decode(second)
	// pv7 §2a: the two queued Welcomes took no sequence yet; the snapshot
	// written first takes 1, they take 2 and 3 when written (before pv7 they
	// took 1 and 2 at enqueue and the snapshot 3, so the Client dropped them).
	if secondTick != 3 || secondHeader.Sequence != 1 || secondHeader.SessionID != secondPeer.session.ID ||
		len(s.snapshotOut) != 0 || s.snapshotReplacements.Load() != 2 {
		t.Fatal("unwritten second peer retained an obsolete publication or transport sequence")
	}
	// log-contract, the coalesced_snapshots identity: the transport total counts
	// both replacements; the Gateway-wide drop_coalesced_snapshots counts only
	// the one made when tick 3 was published over tick 2; the send loop's own
	// replacement (tick 1's peer took tick 3) is their difference.
	if global := s.ingress[gatewayDropCoalescedSnapshots]; global != 1 || s.snapshotReplacements.Load()-global != 1 {
		t.Fatalf("coalesced identity: transport %d, Gateway-wide %d", s.snapshotReplacements.Load(), global)
	}
	for sequence := uint32(2); sequence <= 3; sequence++ {
		control, _ := s.controlPacket(<-s.controlOut)
		header, _, err := framing.DecodeDatagram(control.packet)
		if err != nil || header.Type != adapter.Welcome || header.Sequence != sequence {
			t.Fatal("per-peer snapshot selection changed control queue order")
		}
	}
	delete(s.players, 2)
	_, removed := s.snapshotForPeer([]uint32{2}, frame)
	if removed.peer.IsValid() || len(removed.packet) != 0 || secondPeer.outSequence != 3 {
		t.Fatal("a peer removed after the batch was selected still received a snapshot")
	}
	s.available = false
	_, unavailable := s.snapshotForPeer([]uint32{1}, frame)
	if unavailable.peer.IsValid() || len(unavailable.packet) != 0 {
		t.Fatal("runtime failure revived a selected snapshot")
	}
}

// Withholding the consumer models a blocked UDP writer without putting a test
// switch into Server. This checks the actual publication lane, not a copy of it.
// The verdicts follow publication order, not wall-clock cadence: a host that
// wakes late changes how fast the steps run, never what they must show.
func TestBlockedSnapshotConsumerKeepsLatestAndRecovers(t *testing.T) {
	// 15 and 60 publications are the 250 ms and one-second stalls at 60 Hz.
	for _, blocked := range []uint32{15, 60} {
		t.Run(fmt.Sprintf("%d-publications", blocked), func(t *testing.T) {
			s := &Server{available: true, ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, CombatRules: testRules()}, players: make(map[uint32]*reservation), snapshotOut: make(chan snapshotFrame, 1)}
			publish := func(first, last uint32) {
				done := make(chan struct{})
				go func() {
					defer close(done)
					for tick := first; tick <= last; tick++ {
						e := envelope()
						e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick}}
						fillAnimation(e.GetSnapshot())
						s.runtimeMessage(e)
					}
				}()
				// A generous guard against a publication that blocks, not a cadence bound.
				select {
				case <-done:
				case <-time.After(5 * time.Second):
					t.Fatal("blocked consumer stalled snapshot publication")
				}
			}
			receive := func() uint32 {
				select {
				case frame := <-s.snapshotOut:
					return frame.snapshot.Tick
				case <-time.After(5 * time.Second):
					t.Fatal("no snapshot was pending for the consumer")
				}
				return 0
			}
			// Consumer withheld: every publication returns and only the newest stays.
			publish(1, blocked)
			if len(s.snapshotOut) != 1 || cap(s.snapshotOut) != 1 || s.snapshotReplacements.Load() != uint64(blocked-1) {
				t.Fatal("snapshot lane was not bounded/latest-only")
			}
			// Release: the first delivery is the newest, then each publication is current.
			if tick := receive(); tick != blocked {
				t.Fatalf("released consumer received stale tick %d instead of %d", tick, blocked)
			}
			for tick := blocked + 1; tick <= blocked+30; tick++ {
				publish(tick, tick)
				if received := receive(); received != tick {
					t.Fatalf("snapshot lane did not stay current: received %d after publishing %d", received, tick)
				}
			}
			// Concurrent producer and consumer: ticks only advance and the last one arrives.
			last := blocked + 30 + 120
			done := make(chan struct{})
			go func() {
				defer close(done)
				for tick := blocked + 31; tick <= last; tick++ {
					e := envelope()
					e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick}}
					fillAnimation(e.GetSnapshot())
					s.runtimeMessage(e)
				}
			}()
			previous := blocked + 30
			for previous != last {
				tick := receive()
				if tick <= previous {
					t.Fatal("snapshot went backwards after release")
				}
				previous = tick
			}
			<-done
		})
	}
}

// The IPC input/lifecycle producer remains bounded while its writer is withheld.
// This complements the real TCP pause/fragmentation acceptance in Python.
func TestBlockedRuntimeConsumerPreservesBoundedCommandWindows(t *testing.T) {
	for _, pause := range []time.Duration{250 * time.Millisecond, time.Second} {
		t.Run(pause.String(), func(t *testing.T) {
			link := &runtimeLink{wake: make(chan struct{}, 1)}
			for player := uint32(1); player <= 2; player++ {
				e := envelope()
				e.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: player}}
				if err := link.control(e); err != nil {
					t.Fatal(err)
				}
			}
			deadline := time.Now().Add(pause)
			var generated uint32
			for time.Now().Before(deadline) {
				generated++
				for player := uint32(1); player <= 2; player++ {
					in := &runtime.PlayerInput{LifeGeneration: 1, PlayerId: player, MovementEpoch: 1, Commands: []*runtime.MovementCommand{{Sequence: generated, MoveForward: 1}}}
					if err := link.input(in, dropEvent{}); err != nil {
						t.Fatal(err)
					}
				}
				link.mu.Lock()
				over := len(link.queue) > 2+2*maxQueuedInputs || link.queuedData[dataKey{player: 1}] > maxQueuedInputs || link.queuedData[dataKey{player: 2}] > maxQueuedInputs
				link.mu.Unlock()
				if over {
					t.Fatal("queue capacity exceeded")
				}
				time.Sleep(time.Second / adapter.InputSendRate)
			}
			released := time.Now()
			batch := link.batch()
			if len(batch) < 4 || batch[0].GetJoin() == nil || batch[1].GetJoin() == nil {
				t.Fatal("lifecycle order lost under backpressure")
			}
			// Each player's newest windows, in order, none omitted or doubled
			// among them; the older ones overflowed (pv7 §2a).
			kept := min(generated, maxQueuedInputs)
			next := map[uint32]uint32{1: generated - kept + 1, 2: generated - kept + 1}
			for _, message := range batch[2:] {
				in := message.GetInput()
				if in == nil || len(in.Commands) != 1 || in.Commands[0].Sequence != next[in.PlayerId] {
					t.Fatalf("window omitted, doubled or out of order while blocked: %v", in)
				}
				next[in.PlayerId]++
			}
			if next[1] != generated+1 || next[2] != generated+1 {
				t.Fatalf("windows mismatch: next %v after %d generated", next, generated)
			}
			for time.Since(released) < 250*time.Millisecond {
				if len(link.batch()) != 0 {
					t.Fatal("drained input revived without new publication")
				}
				time.Sleep(5 * time.Millisecond)
			}
			if time.Since(released) > 1500*time.Millisecond {
				t.Fatal("runtime lane recovery exceeded1.5s")
			}
		})
	}
}
