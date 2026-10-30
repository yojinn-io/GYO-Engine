package gateway

import (
	"context"
	"net"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev8"
)

// setForTest overrides a package variable until the test and every cleanup
// registered after this call (servers, links and their goroutines) are done.
func setForTest[T any](t *testing.T, variable *T, value T) {
	t.Helper()
	previous := *variable
	*variable = value
	t.Cleanup(func() { *variable = previous })
}

// scriptedRuntime is a Match that sends a valid Ready and then whatever the
// test writes; it reads and keeps every envelope the Gateway sends.
type scriptedRuntime struct {
	conn net.Conn
	mu   sync.Mutex
	got  []*runtime.RuntimeEnvelope
}

func (r *scriptedRuntime) send(t *testing.T, e *runtime.RuntimeEnvelope) {
	t.Helper()
	b, err := proto.Marshal(e)
	if err != nil {
		t.Fatal(err)
	}
	_ = framing.WriteFrame(r.conn, b) // the link may already be closed
}

func (r *scriptedRuntime) clocks() []*runtime.ClockSample {
	r.mu.Lock()
	defer r.mu.Unlock()
	var out []*runtime.ClockSample
	for _, e := range r.got {
		if c := e.GetClock(); c != nil {
			out = append(out, c)
		}
	}
	return out
}

// linkToScriptedRuntime connects a runtime link to a scripted Match; the link
// is not running yet.
func linkToScriptedRuntime(t *testing.T) (*runtimeLink, *scriptedRuntime) {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { listener.Close() })
	accepted := make(chan *scriptedRuntime, 1)
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			return
		}
		r := &scriptedRuntime{conn: conn}
		ready := envelope()
		ready.Message = &runtime.RuntimeEnvelope_Ready{Ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, ArenaId: "test_arena", ArenaVersion: 1,
			ArenaDigest: testArenaDigest, TickRate: 60, SnapshotIntervalTicks: 1, MaxPlayers: adapter.MaxPlayers, CombatRules: testRules()}}
		b, _ := proto.Marshal(ready)
		_ = framing.WriteFrame(conn, b)
		accepted <- r
		for {
			frame, err := framing.ReadFrame(conn)
			if err != nil {
				return
			}
			var e runtime.RuntimeEnvelope
			if proto.Unmarshal(frame, &e) == nil {
				r.mu.Lock()
				r.got = append(r.got, &e)
				r.mu.Unlock()
			}
		}
	}()
	link, _, err := connectRuntime(context.Background(), listener.Addr().String())
	if err != nil {
		t.Fatal(err)
	}
	r := <-accepted
	t.Cleanup(func() { link.close(); link.waitWriter(); r.conn.Close() })
	return link, r
}

func clockReply(transmit uint64) *runtime.RuntimeEnvelope {
	return &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion, Message: &runtime.RuntimeEnvelope_Clock{Clock: &runtime.ClockSample{TransmitUs: transmit}}}
}

func actionResults() *runtime.RuntimeEnvelope {
	return &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion, Message: &runtime.RuntimeEnvelope_ActionResults{ActionResults: &runtime.ActionResults{PlayerId: 1}}}
}

// pv7 §2: only Snapshot, JoinResult, Evicted and Error refresh liveness; clock
// replies and action results come from the Match's IPC thread, which answers
// while the simulation is stuck. On a fake clock: a frame at half the budget,
// then the clock past the budget, then probes that refresh nothing. A
// refreshing frame keeps the link alive; any other fails it at the next probe,
// without starting a read past the budget (its non-positive timeout would
// cancel the deadline).
func TestOnlyAuthoritativeFramesRefreshRuntimeLiveness(t *testing.T) {
	envelopeOf := func(m any) *runtime.RuntimeEnvelope {
		e := &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion}
		switch m := m.(type) {
		case *runtime.WorldSnapshot:
			e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: m}
		case *runtime.JoinResult:
			e.Message = &runtime.RuntimeEnvelope_JoinResult{JoinResult: m}
		case *runtime.PlayerEvicted:
			e.Message = &runtime.RuntimeEnvelope_Evicted{Evicted: m}
		case *runtime.RuntimeError:
			e.Message = &runtime.RuntimeEnvelope_Error{Error: m}
		}
		return e
	}
	cases := []struct {
		name      string
		frame     *runtime.RuntimeEnvelope
		probe     func(uint64) *runtime.RuntimeEnvelope
		refreshes bool
	}{
		{"snapshot", envelopeOf(&runtime.WorldSnapshot{Tick: 1}), clockReply, true},
		{"join result", envelopeOf(&runtime.JoinResult{PlayerId: 9}), clockReply, true},
		{"evicted", envelopeOf(&runtime.PlayerEvicted{PlayerId: 9}), clockReply, true},
		{"error", envelopeOf(&runtime.RuntimeError{PlayerId: 9, Code: "test"}), clockReply, true},
		{"clock reply", clockReply(1), func(uint64) *runtime.RuntimeEnvelope { return actionResults() }, false},
		{"action results", actionResults(), clockReply, false},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			link, r := linkToScriptedRuntime(t)
			var now atomic.Int64
			start := time.Now()
			link.now = func() time.Time { return start.Add(time.Duration(now.Load())) }
			failed := make(chan error, 1)
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()
			link.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(err error) { failed <- err })
			now.Store(int64(runtimeLiveness / 2))
			r.send(t, c.frame)
			time.Sleep(50 * time.Millisecond) // the reader took the frame
			now.Store(int64(runtimeLiveness * 6 / 5))
			stop := make(chan struct{})
			defer close(stop)
			go func() {
				for transmit := uint64(100); ; transmit++ {
					select {
					case <-stop:
						return
					case <-time.After(10 * time.Millisecond):
					}
					r.send(t, c.probe(transmit))
				}
			}()
			select {
			case err := <-failed:
				if c.refreshes {
					t.Fatalf("%s did not refresh liveness: %v", c.name, err)
				}
				if !strings.Contains(err.Error(), "runtime liveness") || disconnectReason(err) != "liveness_timeout" {
					t.Fatalf("failure %v (reason %s)", err, disconnectReason(err))
				}
			case <-time.After(500 * time.Millisecond):
				if !c.refreshes {
					t.Fatalf("%s kept the runtime link alive", c.name)
				}
			}
		})
	}
}

// A snapshot refreshes liveness; once they stop, only diagnostic frames arrive
// and the link fails one budget after the last snapshot.
func TestSnapshotsKeepTheRuntimeLinkAliveAndOnlyThey(t *testing.T) {
	setForTest(t, &runtimeLiveness, time.Second)
	link, r := linkToScriptedRuntime(t)
	failed := make(chan error, 1)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	link.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(err error) { failed <- err })
	snapshot := &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion, Message: &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 1}}}
	var lastSnapshot time.Time
	for transmit, until := uint64(1), time.Now().Add(2500*time.Millisecond); time.Now().Before(until); transmit++ {
		r.send(t, snapshot)
		lastSnapshot = time.Now()
		r.send(t, clockReply(transmit))
		time.Sleep(50 * time.Millisecond)
	}
	select {
	case err := <-failed:
		t.Fatalf("snapshots did not keep the link alive: %v", err)
	default:
	}
	stop := make(chan struct{})
	defer close(stop)
	go func() {
		for transmit := uint64(1_000); ; transmit++ {
			select {
			case <-stop:
				return
			case <-time.After(20 * time.Millisecond):
			}
			r.send(t, clockReply(transmit))
			r.send(t, actionResults())
		}
	}()
	select {
	case <-failed:
		if elapsed := time.Since(lastSnapshot); elapsed < 800*time.Millisecond {
			t.Fatalf("failed %v after the last snapshot, before the budget", elapsed)
		}
	case <-time.After(5 * time.Second):
		t.Fatal("diagnostic frames kept the runtime link alive")
	}
}

// The heartbeat uses its own slot and wake-up: it never wakes the data writer
// (l.wake), and a newer one replaces an unsent one, counted (D57).
func TestHeartbeatUsesOnlyTheDiagnosticSlot(t *testing.T) {
	link, _ := linkToScriptedRuntime(t) // not running: nothing drains the slot
	link.heartbeat(time.Now())
	link.heartbeat(time.Now())
	if len(link.wake) != 0 || len(link.diagnosticWake) != 1 {
		t.Fatalf("wake %d, diagnostic wake %d", len(link.wake), len(link.diagnosticWake))
	}
	link.clockMu.Lock()
	replaced := link.clock.replaced
	link.clockMu.Unlock()
	if replaced != 1 {
		t.Fatalf("replaced %d", replaced)
	}
}

// Diagnostic writes take no data pass: with no data and no action deadline
// the writer takes nothing, however many heartbeats it writes, and each
// heartbeat's transmit is stamped when written, strictly increasing.
func TestHeartbeatWritesTakeNoDataPass(t *testing.T) {
	setForTest(t, &writerIdleWait, time.Hour)
	link, r := linkToScriptedRuntime(t)
	defer link.close() // before the writer's wait is restored
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	link.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	for i := 0; i < 20; i++ {
		link.heartbeat(time.Now())
		time.Sleep(5 * time.Millisecond)
	}
	deadline := time.Now().Add(2 * time.Second)
	for len(r.clocks()) < 5 && time.Now().Before(deadline) {
		time.Sleep(5 * time.Millisecond)
	}
	clocks := r.clocks()
	if len(clocks) < 5 {
		t.Fatalf("%d heartbeats written", len(clocks))
	}
	for i := 1; i < len(clocks); i++ {
		if clocks[i].TransmitUs <= clocks[i-1].TransmitUs {
			t.Fatalf("transmit %d after %d", clocks[i].TransmitUs, clocks[i-1].TransmitUs)
		}
	}
	link.mu.Lock()
	takes := link.takes
	link.mu.Unlock()
	if takes != 0 {
		t.Fatalf("diagnostic writes took %d data passes", takes)
	}
}

// A diagnostic write leaves the writer's action timer as it was: with a short
// idle wait, heartbeats every few milliseconds do not postpone the timed data
// passes.
func TestHeartbeatWritesKeepTheWriterTimer(t *testing.T) {
	setForTest(t, &writerIdleWait, 50*time.Millisecond)
	link, _ := linkToScriptedRuntime(t)
	defer link.close()
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	link.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	for until := time.Now().Add(600 * time.Millisecond); time.Now().Before(until); {
		link.heartbeat(time.Now())
		time.Sleep(5 * time.Millisecond)
	}
	link.mu.Lock()
	takes := link.takes
	link.mu.Unlock()
	if takes < 5 {
		t.Fatalf("%d timed data passes in 600 ms of 50 ms waits: heartbeats postponed the timer", takes)
	}
}

// A data wake-up that arrives while a heartbeat is being written still gets
// its data pass: the diagnostic branch consumes only its own wake-up.
func TestHeartbeatWritesLeaveDataWakeups(t *testing.T) {
	setForTest(t, &writerIdleWait, time.Hour)
	previousWrite := runtimeWrite
	entered, release := make(chan struct{}), make(chan struct{})
	var blocking atomic.Bool
	setForTest(t, &runtimeWrite, func(c *framing.TCPConnection, payload []byte, timeout time.Duration) error {
		if blocking.CompareAndSwap(true, false) {
			entered <- struct{}{}
			<-release
		}
		return previousWrite(c, payload, timeout)
	})
	link, _ := linkToScriptedRuntime(t)
	defer link.close()
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	link.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	const rounds = 20
	for round := uint64(1); round <= rounds; round++ {
		blocking.Store(true)
		link.heartbeat(time.Now())
		select { // the writer is inside a heartbeat write
		case <-entered:
		case <-time.After(2 * time.Second):
			t.Fatalf("round %d: the heartbeat was not written", round)
		}
		link.notify()
		link.heartbeat(time.Now())
		release <- struct{}{}
		deadline := time.Now().Add(2 * time.Second)
		for {
			link.mu.Lock()
			takes := link.takes
			link.mu.Unlock()
			if takes == round {
				break
			}
			if takes > round || time.Now().After(deadline) {
				t.Fatalf("round %d: %d data passes", round, takes)
			}
			time.Sleep(time.Millisecond)
		}
	}
}

// The ClockSample envelope's bytes, shared with the Match's IpcHostTests
// ("PvP IPC clock samples have the agreed bytes"): both bindings agree.
func TestClockSampleBytesMatchTheMatchBinding(t *testing.T) {
	origin, receive := uint64(1), uint64(2)
	for _, c := range []struct {
		sample *runtime.ClockSample
		bytes  []byte
	}{
		{&runtime.ClockSample{OriginUs: &origin, ReceiveUs: &receive, TransmitUs: 3}, []byte{0x08, 0x08, 0xa2, 0x01, 0x06, 0x08, 0x01, 0x10, 0x02, 0x18, 0x03}},
		{&runtime.ClockSample{TransmitUs: 3}, []byte{0x08, 0x08, 0xa2, 0x01, 0x02, 0x18, 0x03}},
	} {
		got, err := proto.Marshal(&runtime.RuntimeEnvelope{ProtocolVersion: 8, Message: &runtime.RuntimeEnvelope_Clock{Clock: c.sample}})
		if err != nil || string(got) != string(c.bytes) {
			t.Fatalf("% x, want % x (%v)", got, c.bytes, err)
		}
		var back runtime.RuntimeEnvelope
		if proto.Unmarshal(c.bytes, &back) != nil || !proto.Equal(back.GetClock(), c.sample) {
			t.Fatalf("% x reads back as %v", c.bytes, back.GetClock())
		}
	}
}

// A round trip through a Match that answers like IpcHost: the first heartbeat
// carries no origin, later ones echo the latest reply, and the replies are
// measured.
func TestHeartbeatRoundTripIsMeasured(t *testing.T) {
	link, r := linkToScriptedRuntime(t)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	link.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	matchClock := uint64(5_000_000_000) // another domain, far from the Gateway's
	for i := 0; i < 4; i++ {
		link.heartbeat(time.Now())
		deadline := time.Now().Add(2 * time.Second)
		for len(r.clocks()) <= i && time.Now().Before(deadline) {
			time.Sleep(2 * time.Millisecond)
		}
		clocks := r.clocks()
		if len(clocks) <= i {
			t.Fatalf("heartbeat %d not written", i)
		}
		heartbeat := clocks[i]
		if (i == 0) != (heartbeat.OriginUs == nil) || (heartbeat.OriginUs == nil) != (heartbeat.ReceiveUs == nil) {
			t.Fatalf("heartbeat %d: %v", i, heartbeat)
		}
		if i > 0 && *heartbeat.OriginUs != matchClock {
			t.Fatalf("heartbeat %d echoes %d, want %d", i, *heartbeat.OriginUs, matchClock)
		}
		matchClock += 1_000
		origin, receive := heartbeat.TransmitUs, matchClock
		r.send(t, &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion, Message: &runtime.RuntimeEnvelope_Clock{
			Clock: &runtime.ClockSample{OriginUs: &origin, ReceiveUs: &receive, TransmitUs: matchClock}}})
		deadline = time.Now().Add(2 * time.Second)
		for time.Now().Before(deadline) {
			link.clockMu.Lock()
			_, _, echoed := link.clock.echo()
			echo, _, _ := link.clock.echo()
			link.clockMu.Unlock()
			if echoed && echo == matchClock {
				break
			}
			time.Sleep(2 * time.Millisecond)
		}
	}
	link.clockMu.Lock()
	samples, invalid := link.clock.samples, link.clock.invalid
	link.clockMu.Unlock()
	if samples != 4 || invalid != 0 {
		t.Fatalf("samples %d, invalid %d", samples, invalid)
	}
}

// D32 (pv7 §2): the clock estimate stays out of every decision. The session
// package and server.go (timeouts, admission) name none of its symbols.
func TestClockStaysOutOfDecisions(t *testing.T) {
	files, err := filepath.Glob("../../../services/gyo_gateway/session/*.go")
	if err != nil || len(files) == 0 {
		t.Fatalf("session package not found: %v", err)
	}
	files = append(files, "server.go")
	for _, file := range files {
		text, err := os.ReadFile(file)
		if err != nil {
			t.Fatal(err)
		}
		for _, symbol := range []string{"ClockSample", "clockSync", "clockStamp", "clockWindow", "GetClock", "observeClock", "heartbeat(", ".clock"} {
			if strings.Contains(string(text), symbol) {
				t.Fatalf("%s names %q", file, symbol)
			}
		}
	}
}
