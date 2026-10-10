package gateway

import (
	"bytes"
	"errors"
	"log"
	"net/netip"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv6"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

// ingressLog captures what the standard logger writes. Register it before the
// server, so the server's cleanup runs first.
type ingressLog struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (c *ingressLog) Write(p []byte) (int, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.buf.Write(p)
}

func captureIngressLog(t *testing.T) *ingressLog {
	t.Helper()
	c := &ingressLog{}
	previous := log.Writer()
	log.SetOutput(c)
	t.Cleanup(func() { log.SetOutput(previous) })
	return c
}

// One parsed ingress line; player 0 is the Gateway line.
type parsedIngressLine struct {
	player  uint64
	window  uint64 // window_ms
	final   bool
	counts  playerIngressCounts
	gateway gatewayIngressCounts
}

// lines parses the ingress lines in the order they were logged.
func (c *ingressLog) lines(t *testing.T) []parsedIngressLine {
	t.Helper()
	c.mu.Lock()
	text := c.buf.String()
	c.mu.Unlock()
	var out []parsedIngressLine
	for _, line := range strings.Split(text, "\n") {
		if strings.Contains(line, "ingress statistics ") {
			for _, phrase := range frozenLinePhrases {
				if strings.Contains(line, phrase) {
					t.Errorf("ingress line carries the frozen phrase %q: %s", phrase, line)
				}
			}
			if frozenSessionCounters.MatchString(line) {
				t.Errorf("ingress line carries the frozen session counters: %s", line)
			}
		}
		if i := strings.Index(line, "player ingress statistics "); i >= 0 {
			head, fields := splitIngressLine(t, line[i:], "player ingress statistics", []string{"version", "player", "window_ms", "final"})
			parsed := parsedIngressLine{player: head["player"], window: head["window_ms"], final: head["final"] == 1}
			fillIngressCounts(t, fields, playerIngressKeys[:], parsed.counts[:])
			out = append(out, parsed)
		} else if i := strings.Index(line, "gateway ingress statistics "); i >= 0 {
			head, fields := splitIngressLine(t, line[i:], "gateway ingress statistics", []string{"version", "window_ms", "final"})
			parsed := parsedIngressLine{window: head["window_ms"], final: head["final"] == 1}
			fillIngressCounts(t, fields, gatewayIngressKeys[:], parsed.gateway[:])
			out = append(out, parsed)
		}
	}
	return out
}

func (c *ingressLog) finalWritten(t *testing.T) bool {
	for _, line := range c.lines(t) {
		if line.player == 0 && line.final {
			return true
		}
	}
	return false
}

// waitFinal waits for the Gateway's final line.
func (c *ingressLog) waitFinal(t *testing.T, name string) {
	t.Helper()
	for deadline := time.Now().Add(2 * time.Second); !c.finalWritten(t); {
		if time.Now().After(deadline) {
			t.Fatalf("%s: no final line after the runtime failure", name)
		}
		time.Sleep(5 * time.Millisecond)
	}
}

// checkIngressLines checks what an analyzer requires of a process's lines:
// each player's lines, and the Gateway's, end with exactly one final line;
// none follows the final window. It returns the sums, over which IG, I0, T3
// and the Gateway-wide conservation must hold exactly.
func checkIngressLines(t *testing.T, name string, lines []parsedIngressLine, rateLimited uint64) ingressTake {
	t.Helper()
	total := ingressTake{players: map[uint64]*playerIngressCounts{}}
	finals := map[uint64]int{}
	ended := map[uint64]bool{}
	for _, line := range lines {
		// The final window is the Gateway's final line and the players' final
		// lines after it.
		if ended[line.player] || (ended[0] && (line.player == 0 || !line.final)) {
			t.Errorf("%s: ingress line after the final window: player %d", name, line.player)
		}
		if line.final {
			finals[line.player]++
			ended[line.player] = true
		}
		if line.player == 0 {
			for i, n := range line.gateway {
				total.gateway[i] += n
			}
			continue
		}
		if total.players[line.player] == nil {
			total.players[line.player] = new(playerIngressCounts)
		}
		total.players[line.player].add(&line.counts)
	}
	if finals[0] != 1 {
		t.Errorf("%s: gateway has %d final lines, want 1", name, finals[0])
	}
	for player := range total.players {
		if finals[player] != 1 {
			t.Errorf("%s: player %d has %d final lines, want 1", name, player, finals[player])
		}
	}
	checkIngressConservation(t, name, total, rateLimited)
	return total
}

// The ingress lines of each statistics window count what happened in it; a
// removed player is written while it has counters left, and the final window
// ends every player's lines once. No ingress line follows it.
func TestIngressWindowsAreIncrementsAndEndWithOneFinalLine(t *testing.T) {
	logs := captureIngressLog(t)
	s, p, endpoint, now := actionFixture(t)
	s.link = dialedLink(t)
	s.ingressWindowStart = now
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
	window := func(i int) { s.logStatisticsWindow(now.Add(time.Duration(i)*10*time.Second), 10*time.Second) }

	// Window 1: two windows of player 7, merged and written.
	f.send(t, f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 2))))
	f.send(t, f.datagram(adapter.Input, 3, inputMessage(1, moves(1, 3))))
	sequences(s.link)
	window(1)
	// Window 2: player 8 says Hello and goes; its counters outlive it. Player
	// 7's new commands stay pending.
	second := &outcomeFixture{s: s, now: now}
	reservePlayer8(t, second)
	second.send(t, second.datagram(adapter.Hello, 1, &client.Hello{SessionToken: second.p.session.Token}))
	f.send(t, f.datagram(adapter.Input, 4, inputMessage(1, moves(3, 5))))
	s.mu.Lock()
	s.remove(second.p)
	s.mu.Unlock()
	window(2)
	// Window 3: nothing from player 8.
	f.send(t, f.raw(99, 5, nil))
	window(3)
	// The end clears player 7's pending commands into the final window.
	s.runtimeFailed(errors.New("test end"))
	window(4)

	lines := logs.lines(t)
	checkIngressLines(t, "windows", lines, s.rateRejectedPackets.Load())
	type perWindow struct{ received, handed, gateway uint64 }
	var windows []perWindow
	player8 := map[int]bool{}
	for _, line := range lines {
		switch line.player {
		case 0:
			if want := uint64(10000); !line.final && line.window != want {
				t.Errorf("window %d: window_ms %d, want %d", len(windows)+1, line.window, want)
			}
			windows = append(windows, perWindow{gateway: line.gateway[gatewayReceivedDatagrams]})
		case 7:
			windows[len(windows)-1].received = line.counts[playerReceivedDatagrams]
			windows[len(windows)-1].handed = line.counts[playerFwdHandedMainCommands]
		case 8:
			player8[len(windows)] = true
		}
	}
	if len(windows) != 4 {
		t.Fatalf("%d ingress windows, want 3 and the final one", len(windows))
	}
	for i, want := range []perWindow{{2, 5, 2}, {1, 3, 2}, {1, 0, 1}, {0, 0, 0}} {
		got := windows[i]
		if got.received != want.received {
			t.Errorf("window %d: player 7 received %d datagrams, want %d", i+1, got.received, want.received)
		}
		if got.handed != want.handed {
			t.Errorf("window %d: player 7 handed %d commands, want %d", i+1, got.handed, want.handed)
		}
		if got.gateway != want.gateway {
			t.Errorf("window %d: gateway received %d datagrams, want %d", i+1, got.gateway, want.gateway)
		}
	}
	if !player8[2] {
		t.Error("removed player 8 has no line in window 2")
	}
	if player8[3] {
		t.Error("removed player 8 without counters was written in window 3")
	}
	if !player8[4] {
		t.Error("player 8 has no final line")
	}
}

// replaceRuntimeWrite installs a hook for the writer's frame writes. Register
// it before the server, so the server's cleanup runs first.
func replaceRuntimeWrite(t *testing.T, hook func(*framing.TCPConnection, []byte, time.Duration) error) {
	previous := runtimeWrite
	runtimeWrite = hook
	t.Cleanup(func() { runtimeWrite = previous })
}

func isInputFrame(payload []byte) bool {
	var e runtime.RuntimeEnvelope
	return proto.Unmarshal(payload, &e) == nil && e.GetInput() != nil
}

// endPathFixture is a served Gateway with one active player whose datagrams
// the test hands to receiveDatagram directly.
type endPathFixture struct {
	s        *Server
	f        *fakeRuntime
	cancel   func()
	done     <-chan struct{}
	player   uint64
	session  uint64
	endpoint netip.AddrPort
	sequence uint32
	read     uint64 // input commands the test took from the fake runtime
}

func newEndPathFixture(t *testing.T) *endPathFixture {
	t.Helper()
	s, f, cancel, done := startTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c := reserve(t, s, "end")
	e := &endPathFixture{s: s, f: f, cancel: cancel, done: done, player: c.PlayerID, session: c.SessionID,
		endpoint: netip.MustParseAddrPort("127.0.0.1:29011")}
	e.send(t, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	for deadline := time.Now().Add(2 * time.Second); ; {
		s.mu.Lock()
		active := s.players[c.PlayerID] != nil && s.players[c.PlayerID].phase == active
		s.mu.Unlock()
		if active {
			return e
		}
		if time.Now().After(deadline) {
			t.Fatal("player did not become active")
		}
		time.Sleep(2 * time.Millisecond)
	}
}

func (e *endPathFixture) send(t *testing.T, kind uint16, message proto.Message) {
	t.Helper()
	e.sequence++
	payload, _ := proto.Marshal(message)
	packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: kind, SessionID: e.session, Sequence: e.sequence}, payload)
	if err != nil {
		t.Fatal(err)
	}
	if err := e.s.receiveDatagram(packet, e.endpoint, time.Now()); err != nil {
		t.Fatal(err)
	}
}

// close ends the Gateway as main does and waits for Serve to return.
func (e *endPathFixture) close(t *testing.T, name string) {
	t.Helper()
	closed := make(chan struct{})
	go func() { e.s.Close(); close(closed) }()
	select {
	case <-closed:
	case <-time.After(3 * time.Second):
		t.Fatalf("%s: Close did not return", name)
	}
	select {
	case <-e.done:
	case <-time.After(3 * time.Second):
		t.Fatalf("%s: Serve did not return", name)
	}
}

func (e *endPathFixture) awaitInput(t *testing.T, name string) {
	t.Helper()
	e.read += uint64(len(e.f.nextInput(t, name+": input did not reach the runtime").GetCommands()))
}

// receivedCommands drains the fake runtime until it is quiet and counts the
// input commands it read.
func (e *endPathFixture) receivedCommands() uint64 {
	commands := e.read
	for {
		select {
		case message := <-e.f.got:
			commands += uint64(len(message.GetInput().GetCommands()))
		case <-time.After(100 * time.Millisecond):
			return commands
		}
	}
}

// Every end path writes the final window exactly once, after the link closed
// and its writer exited, so IG, I0, T3 and the Gateway-wide conservation hold
// exactly over each player's lines: a normal close or a cancelled context
// (each through Close), a runtime failure (the Match went away; a write failed
// on the writer goroutine itself) followed by Close, a failure racing Close,
// and Close while the writer is blocked in a write.
func TestEveryEndPathWritesTheFinalWindowOnceAfterTheWriter(t *testing.T) {
	type expect struct{ written, abandoned, cleared uint64 }
	cases := []struct {
		name string
		end  func(t *testing.T, e *endPathFixture, logs *ingressLog, entered <-chan struct{}, release chan<- struct{}, block, fail *atomic.Bool)
		want expect
	}{
		{name: "close", end: func(t *testing.T, e *endPathFixture, _ *ingressLog, _ <-chan struct{}, _ chan<- struct{}, _, _ *atomic.Bool) {
			e.send(t, adapter.Input, inputMessage(1, moves(1, 3)))
			e.awaitInput(t, "close")
			e.close(t, "close")
		}, want: expect{written: 3}},
		{name: "cancel", end: func(t *testing.T, e *endPathFixture, _ *ingressLog, _ <-chan struct{}, _ chan<- struct{}, _, _ *atomic.Bool) {
			e.send(t, adapter.Input, inputMessage(1, moves(1, 3)))
			e.awaitInput(t, "cancel")
			e.cancel()
			select {
			case <-e.done:
			case <-time.After(3 * time.Second):
				t.Fatal("cancel: Serve did not return")
			}
		}, want: expect{written: 3}},
		{name: "runtime_failure", end: func(t *testing.T, e *endPathFixture, logs *ingressLog, _ <-chan struct{}, _ chan<- struct{}, _, _ *atomic.Bool) {
			e.send(t, adapter.Input, inputMessage(1, moves(1, 3)))
			e.awaitInput(t, "runtime_failure")
			e.f.mu.Lock()
			_ = e.f.conn.Close()
			e.f.mu.Unlock()
			logs.waitFinal(t, "runtime_failure")
			e.close(t, "runtime_failure")
		}, want: expect{written: 3}},
		{name: "write_failure", end: func(t *testing.T, e *endPathFixture, logs *ingressLog, _ <-chan struct{}, _ chan<- struct{}, _, fail *atomic.Bool) {
			fail.Store(true)
			e.send(t, adapter.Input, inputMessage(1, moves(1, 3)))
			logs.waitFinal(t, "write_failure")
			e.close(t, "write_failure")
		}, want: expect{abandoned: 3}},
		{name: "failure_racing_close", end: func(t *testing.T, e *endPathFixture, _ *ingressLog, _ <-chan struct{}, _ chan<- struct{}, _, _ *atomic.Bool) {
			e.send(t, adapter.Input, inputMessage(1, moves(1, 3)))
			e.awaitInput(t, "failure_racing_close")
			go e.s.runtimeFailed(errors.New("racing failure"))
			e.close(t, "failure_racing_close")
		}, want: expect{written: 3}},
		{name: "blocked_writer", end: func(t *testing.T, e *endPathFixture, logs *ingressLog, entered <-chan struct{}, release chan<- struct{}, block, _ *atomic.Bool) {
			block.Store(true)
			e.send(t, adapter.Input, inputMessage(1, moves(1, 3)))
			select {
			case <-entered:
			case <-time.After(2 * time.Second):
				t.Fatal("blocked_writer: the writer did not take the input")
			}
			// Pending behind the blocked write: a main window and a rejected one.
			e.send(t, adapter.Input, inputMessage(1, moves(4, 5)))
			e.send(t, adapter.Input, inputMessage(2, moves(1, 2)))
			closed := make(chan struct{})
			go func() { e.s.Close(); close(closed) }()
			for deadline := time.Now().Add(2 * time.Second); ; time.Sleep(time.Millisecond) {
				e.s.link.mu.Lock()
				linkClosed := e.s.link.closed
				e.s.link.mu.Unlock()
				if linkClosed {
					break
				}
				if time.Now().After(deadline) {
					t.Fatal("blocked_writer: Close did not close the link")
				}
			}
			time.Sleep(50 * time.Millisecond)
			if logs.finalWritten(t) {
				t.Error("blocked_writer: final line written while the writer was blocked")
			}
			close(release) // the write resumes on the closed connection and fails
			select {
			case <-closed:
			case <-time.After(3 * time.Second):
				t.Fatal("blocked_writer: Close did not return")
			}
			e.close(t, "blocked_writer")
		}, want: expect{abandoned: 3, cleared: 4}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			logs := captureIngressLog(t)
			var block, fail atomic.Bool
			entered := make(chan struct{}, 1)
			release := make(chan struct{})
			write := (*framing.TCPConnection).Write
			replaceRuntimeWrite(t, func(conn *framing.TCPConnection, payload []byte, timeout time.Duration) error {
				if isInputFrame(payload) {
					if fail.Load() {
						return errors.New("injected write failure")
					}
					if block.CompareAndSwap(true, false) {
						entered <- struct{}{}
						<-release
					}
				}
				return write(conn, payload, timeout)
			})
			e := newEndPathFixture(t)
			c.end(t, e, logs, entered, release, &block, &fail)
			total := checkIngressLines(t, c.name, logs.lines(t), e.s.rateRejectedPackets.Load())
			counts := total.players[e.player]
			if counts == nil {
				t.Fatalf("%s: player %d has no lines", c.name, e.player)
			}
			got := expect{written: counts[playerFwdLinkWrittenMainCommands] + counts[playerFwdLinkWrittenRejectedLaneCommands],
				abandoned: counts[playerDropAbandonedOnCloseCommands], cleared: counts[playerDropClearedOnCloseCommands]}
			if got != c.want {
				t.Errorf("%s: written %d abandoned %d cleared %d, want %+v", c.name, got.written, got.abandoned, got.cleared, c.want)
			}
			// What the link counts as written is what the runtime read.
			if read := e.receivedCommands(); read != got.written {
				t.Errorf("%s: link wrote %d commands, the runtime read %d", c.name, got.written, read)
			}
		})
	}
}
