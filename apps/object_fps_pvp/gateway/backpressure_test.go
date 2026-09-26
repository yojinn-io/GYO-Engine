package gateway

import (
	"net/netip"
	"sync/atomic"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv3"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev3"
)

// Interleave publication between the two actual per-peer selection calls. This
// exercises the case where the first UDP write was slow, without a production
// blocking switch or pretending an ordinary loopback UDP socket is saturated.
func TestSnapshotSelectionRefreshesUnwrittenPeerAndRevalidatesSession(t *testing.T) {
	s := &Server{available: true, players: make(map[uint64]*reservation),
		controlOut: make(chan outbound, 64), snapshotOut: make(chan []byte, 1)}
	for id := uint64(1); id <= 2; id++ {
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
	decode := func(packet outbound) (framing.Header, uint64) {
		t.Helper()
		header, payload, err := framing.DecodeDatagram(packet.packet)
		var snapshot client.WorldSnapshot
		if err != nil || header.Type != adapter.Snapshot || proto.Unmarshal(payload, &snapshot) != nil {
			t.Fatal("invalid selected snapshot datagram")
		}
		return header, snapshot.Tick
	}
	payload, err := proto.Marshal(&client.WorldSnapshot{Tick: 1})
	if err != nil {
		t.Fatal(err)
	}
	payload, first := s.snapshotForPeer(1, payload)
	firstHeader, firstTick := decode(first)
	if firstTick != 1 || firstHeader.Sequence != 1 || firstHeader.SessionID != s.players[1].session.ID {
		t.Fatal("first peer did not receive the initial publication")
	}
	secondPeer := s.players[2]
	s.sendControl(secondPeer, adapter.Welcome, &client.Welcome{PlayerId: 2})
	s.sendControl(secondPeer, adapter.Welcome, &client.Welcome{PlayerId: 2})
	for tick := uint64(2); tick <= 3; tick++ {
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick}}
		s.runtimeMessage(e)
	}
	payload, second := s.snapshotForPeer(2, payload)
	secondHeader, secondTick := decode(second)
	if secondTick != 3 || secondHeader.Sequence != 3 || secondHeader.SessionID != secondPeer.session.ID ||
		len(s.snapshotOut) != 0 || s.snapshotReplacements.Load() != 2 {
		t.Fatal("unwritten second peer retained an obsolete publication or transport sequence")
	}
	for sequence := uint32(1); sequence <= 2; sequence++ {
		header, _, err := framing.DecodeDatagram((<-s.controlOut).packet)
		if err != nil || header.Type != adapter.Welcome || header.Sequence != sequence {
			t.Fatal("per-peer snapshot selection changed control queue order")
		}
	}
	delete(s.players, 2)
	_, removed := s.snapshotForPeer(2, payload)
	if removed.peer.IsValid() || len(removed.packet) != 0 || secondPeer.outSequence != 3 {
		t.Fatal("a peer removed after the batch was selected still received a snapshot")
	}
	s.available = false
	_, unavailable := s.snapshotForPeer(1, payload)
	if unavailable.peer.IsValid() || len(unavailable.packet) != 0 {
		t.Fatal("runtime failure revived a selected snapshot")
	}
}

// Withholding the consumer models a blocked UDP writer without putting a test
// switch into Server. This checks the actual publication lane, not a copy of it.
func TestBlockedSnapshotConsumerKeepsLatestAndRecovers(t *testing.T) {
	for _, pause := range []time.Duration{250 * time.Millisecond, time.Second} {
		t.Run(pause.String(), func(t *testing.T) {
			s := &Server{available: true, players: make(map[uint64]*reservation), snapshotOut: make(chan []byte, 1)}
			var produced atomic.Uint64
			stop, done := make(chan struct{}), make(chan struct{})
			go func() {
				defer close(done)
				ticker := time.NewTicker(time.Second / adapter.AuthorityTickRate)
				defer ticker.Stop()
				var tick uint64
				for {
					select {
					case <-stop:
						return
					case <-ticker.C:
						tick++
						e := envelope()
						e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick}}
						s.runtimeMessage(e)
						produced.Store(tick)
					}
				}
			}()
			defer func() { close(stop); <-done }()
			time.Sleep(pause)
			if produced.Load() < uint64(pause/(time.Second/adapter.AuthorityTickRate))-3 {
				t.Fatal("blocked consumer stalled snapshot publication")
			}
			if len(s.snapshotOut) != 1 || cap(s.snapshotOut) != 1 || s.snapshotReplacements.Load() == 0 {
				t.Fatal("snapshot lane was not bounded/latest-only")
			}
			released := time.Now()
			var previous uint64
			stableSince := time.Time{}
			for time.Since(released) < 1500*time.Millisecond {
				select {
				case payload := <-s.snapshotOut:
					var snapshot client.WorldSnapshot
					if err := proto.Unmarshal(payload, &snapshot); err != nil {
						t.Fatal(err)
					}
					if snapshot.Tick <= previous {
						t.Fatal("snapshot went backwards after release")
					}
					previous = snapshot.Tick
					if produced.Load() > snapshot.Tick+1 {
						stableSince = time.Time{}
						continue
					}
					if stableSince.IsZero() {
						stableSince = time.Now()
					}
					if time.Since(stableSince) >= 250*time.Millisecond {
						return
					}
				case <-time.After(100 * time.Millisecond):
					stableSince = time.Time{}
				}
			}
			t.Fatal("snapshot lane did not recover within1.5s and remain current for250ms")
		})
	}
}

// The IPC input/lifecycle producer remains bounded while its writer is withheld.
// This complements the real TCP pause/fragmentation acceptance in Python.
func TestBlockedRuntimeConsumerPreservesBoundedCommandWindows(t *testing.T) {
	for _, pause := range []time.Duration{250 * time.Millisecond, time.Second} {
		t.Run(pause.String(), func(t *testing.T) {
			link := &runtimeLink{inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1)}
			for player := uint64(1); player <= 2; player++ {
				e := envelope()
				e.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: player}}
				if err := link.control(e); err != nil {
					t.Fatal(err)
				}
			}
			deadline := time.Now().Add(pause)
			var generated uint64
			for time.Now().Before(deadline) {
				generated++
				for player := uint64(1); player <= 2; player++ {
					sequence := (generated-1)%adapter.MaxFutureCommands + 1
					in := &runtime.PlayerInput{PlayerId: player, MovementEpoch: 1, Commands: []*runtime.MovementCommand{{Sequence: sequence, MoveForward: 1}}}
					if err := link.input(in); err != nil {
						t.Fatal(err)
					}
				}
				if len(link.inputs) > 2 || len(link.controls) > 64 {
					t.Fatal("mailbox capacity exceeded")
				}
				for _, input := range link.inputs {
					if len(input.Commands) > adapter.MaxFutureCommands {
						t.Fatal("future window capacity exceeded")
					}
				}
				time.Sleep(time.Second / adapter.InputSendRate)
			}
			released := time.Now()
			batch := link.batch()
			if len(batch) < 4 || batch[0].GetJoin() == nil || batch[1].GetJoin() == nil {
				t.Fatal("lifecycle order lost under backpressure")
			}
			counts := make(map[uint64]int)
			for _, message := range batch[2:] {
				in := message.GetInput()
				if in == nil || len(in.Commands) > adapter.MaxPendingCommands {
					t.Fatal("invalid IPC input chunk")
				}
				for _, command := range in.Commands {
					counts[in.PlayerId]++
					if command.Sequence != uint64(counts[in.PlayerId]) {
						t.Fatal("command omitted or duplicated while blocked")
					}
				}
			}
			expected := int(min(generated, uint64(adapter.MaxFutureCommands)))
			if counts[1] != expected || counts[2] != expected {
				t.Fatalf("window mismatch: %v expected %d", counts, expected)
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
