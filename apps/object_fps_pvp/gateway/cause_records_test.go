package gateway

import (
	"context"
	"go/ast"
	"go/parser"
	"go/token"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv7"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

// capturedLink runs a dialed link whose runtime writes are decoded and kept;
// hold (if set) runs inside each write, before it is kept.
func capturedLink(t *testing.T, hold func(*runtime.RuntimeEnvelope)) (*runtimeLink, func(int) []*runtime.RuntimeEnvelope) {
	t.Helper()
	var mu sync.Mutex
	var written []*runtime.RuntimeEnvelope
	setForTest(t, &runtimeWrite, func(_ *framing.TCPConnection, payload []byte, _ time.Duration) error {
		e := &runtime.RuntimeEnvelope{}
		if err := proto.Unmarshal(payload, e); err != nil {
			return err
		}
		if hold != nil {
			hold(e)
		}
		mu.Lock()
		written = append(written, e)
		mu.Unlock()
		return nil
	})
	l := dialedLink(t)
	l.drops, l.now, l.diagnosticWake = newDropLedger(), time.Now, make(chan struct{}, 1)
	l.clock = newClockSync(gatewayMicros(time.Now()))
	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	l.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	t.Cleanup(l.close)
	wait := func(n int) []*runtime.RuntimeEnvelope {
		t.Helper()
		deadline := time.Now().Add(3 * time.Second)
		for {
			mu.Lock()
			var data []*runtime.RuntimeEnvelope
			for _, e := range written {
				if e.GetInput() != nil || e.GetActions() != nil {
					data = append(data, e)
				}
			}
			mu.Unlock()
			if len(data) >= n {
				return data
			}
			if time.Now().After(deadline) {
				t.Fatalf("%d of %d data messages written", len(data), n)
			}
			time.Sleep(time.Millisecond)
		}
	}
	return l, wait
}

// pv7 §2, §5: each IPC input and action message carries its own datagram's
// dwell (receipt to the start of its write) and GYOP sequence, one to one.
func TestDataMessagesCarryTheirOwnDatagramsDwellAndSequence(t *testing.T) {
	l, wait := capturedLink(t, nil)
	now := time.Now()
	if err := l.input(linkWindow(1, 1, 1, 1), dropEvent{at: now.Add(-30 * time.Millisecond), transport: 7}); err != nil {
		t.Fatal(err)
	}
	if err := l.input(linkWindow(1, 1, 2, 2), dropEvent{at: now.Add(-300 * time.Millisecond), transport: 8}); err != nil {
		t.Fatal(err)
	}
	if err := l.actions(&runtime.ActionBatch{PlayerId: 1, AcknowledgedThrough: 2}, dropEvent{at: now.Add(-60 * time.Millisecond), transport: 9}); err != nil {
		t.Fatal(err)
	}
	if err := l.input(linkWindow(1, 1, 3, 3), dropEvent{}); err != nil { // no datagram: no diagnostics
		t.Fatal(err)
	}
	data := wait(4)
	elapsed := time.Since(now)
	check := func(name string, dwell *uint32, sequence *uint32, wantSequence uint32, least time.Duration) {
		t.Helper()
		if dwell == nil || sequence == nil || *sequence != wantSequence {
			t.Fatalf("%s: dwell %v sequence %v (want sequence %d)", name, dwell, sequence, wantSequence)
		}
		got := time.Duration(*dwell) * time.Microsecond
		if got < least || got > least+elapsed {
			t.Fatalf("%s: dwell %v not within [%v, %v]", name, got, least, least+elapsed)
		}
	}
	check("first input", data[0].GetInput().GatewayDwellUs, data[0].GetInput().ClientTransportSequence, 7, 30*time.Millisecond)
	check("second input", data[1].GetInput().GatewayDwellUs, data[1].GetInput().ClientTransportSequence, 8, 300*time.Millisecond)
	check("actions", data[2].GetActions().GatewayDwellUs, data[2].GetActions().ClientTransportSequence, 9, 60*time.Millisecond)
	if in := data[3].GetInput(); in.GatewayDwellUs != nil || in.ClientTransportSequence != nil {
		t.Fatalf("a window without a datagram carries diagnostics: %v", in)
	}
}

// The dwell is clamped at 10 s (pv7 §2).
func TestDwellIsClampedAtTenSeconds(t *testing.T) {
	l, wait := capturedLink(t, nil)
	if err := l.input(linkWindow(1, 1, 1, 1), dropEvent{at: time.Now().Add(-25 * time.Second), transport: 3}); err != nil {
		t.Fatal(err)
	}
	if dwell := wait(1)[0].GetInput().GetGatewayDwellUs(); dwell != 10_000_000 {
		t.Fatalf("dwell %d", dwell)
	}
	if dwellMicros(time.Now(), time.Now().Add(-time.Second)) != 0 {
		t.Fatal("a negative dwell is not 0")
	}
}

// The dwell ends when the message's own write starts, not when the writer
// took its pass: two messages taken in one pass, the first write blocking
// 500 ms, and the second's dwell shows it.
func TestDwellEndsWhenTheMessagesOwnWriteStarts(t *testing.T) {
	entered, release := make(chan struct{}, 1), make(chan struct{})
	l, wait := capturedLink(t, func(e *runtime.RuntimeEnvelope) {
		switch e.GetInput().GetCommands()[0].GetSequence() {
		case 100: // holds the writer until both next windows are queued
			entered <- struct{}{}
			<-release
		case 1: // the first write of the next pass blocks
			time.Sleep(500 * time.Millisecond)
		}
	})
	if err := l.input(linkWindow(1, 1, 100, 100), dropEvent{at: time.Now(), transport: 1}); err != nil {
		t.Fatal(err)
	}
	<-entered
	for k := uint64(1); k <= 2; k++ {
		if err := l.input(linkWindow(1, 1, k, k), dropEvent{at: time.Now(), transport: uint32(k + 1)}); err != nil {
			t.Fatal(err)
		}
	}
	close(release)
	data := wait(3)
	if first, later := data[1].GetInput().GetGatewayDwellUs(), data[2].GetInput().GetGatewayDwellUs(); later < first+500_000 {
		t.Fatalf("the later dwell %d us does not include the first write's 500 ms (first %d us)", later, first)
	}
}

// pv7 §5 (D-4): every player's write fills its own gateway_hold_us, from the
// Gateway reading the IPC frame to that write; a frame replaced mid-loop is
// encoded from the newer one's reading.
func TestEachPlayerGetsItsOwnGatewayHold(t *testing.T) {
	s := &Server{available: true, players: make(map[uint64]*reservation), snapshotOut: make(chan snapshotFrame, 1),
		link: &runtimeLink{drops: newDropLedger()}}
	for id := uint64(1); id <= 3; id++ {
		s.players[id] = &reservation{playerID: id, session: newActiveSession(t, id), phase: active}
	}
	hold := func(packet outbound) time.Duration {
		t.Helper()
		_, payload, err := framing.DecodeDatagram(packet.packet)
		var snapshot client.WorldSnapshot
		if err != nil || proto.Unmarshal(payload, &snapshot) != nil || snapshot.GatewayHoldUs == nil {
			t.Fatalf("snapshot without a hold: %v %v", &snapshot, err)
		}
		return time.Duration(*snapshot.GatewayHoldUs) * time.Microsecond
	}
	start := time.Now()
	frame := snapshotFrame{snapshot: &client.WorldSnapshot{Tick: 5, MatchHoldUs: proto.Uint32(77)}, read: start.Add(-50 * time.Millisecond), tick: 5}
	players := []uint64{1, 2, 3}
	frame, first := s.snapshotForPeer(players, frame)
	time.Sleep(20 * time.Millisecond)
	frame, second := s.snapshotForPeer(players[1:], frame)
	h1, h2 := hold(first), hold(second)
	if h1 < 50*time.Millisecond || h2 < h1+20*time.Millisecond || h2 > 50*time.Millisecond+time.Since(start) {
		t.Fatalf("holds %v then %v", h1, h2)
	}
	s.snapshotOut <- snapshotFrame{snapshot: &client.WorldSnapshot{Tick: 6}, read: time.Now(), tick: 6}
	frame, third := s.snapshotForPeer(players[2:], frame)
	if frame.tick != 6 || hold(third) > 20*time.Millisecond {
		t.Fatalf("the replacing frame was served with tick %d and hold %v", frame.tick, hold(third))
	}
}

// The reader checks the datagram bound with the widest hold a write can add.
func TestSnapshotBoundIncludesTheWidestHold(t *testing.T) {
	out := &client.WorldSnapshot{Tick: 1}
	widest := uint32(dropMaximumDwell / time.Microsecond)
	if widest != 10_000_000 || proto.Size(&client.WorldSnapshot{Tick: 1, GatewayHoldUs: &widest}) <= proto.Size(out) {
		t.Fatal("the widest hold is not the 10 s clamp")
	}
	for _, d := range []time.Duration{0, time.Second, dropMaximumDwell, 3 * dropMaximumDwell} {
		h := dwellMicros(time.Now().Add(-d), time.Now())
		if proto.Size(&client.WorldSnapshot{Tick: 1, GatewayHoldUs: &h}) > proto.Size(&client.WorldSnapshot{Tick: 1, GatewayHoldUs: &widest}) {
			t.Fatalf("a %v hold encodes wider than the bound", d)
		}
	}
}

// D32 (pv7 §2): dwell, hold, the Client's sequence and the refusals stay out of
// every decision. The session package names none of their symbols; neither do
// admission (action_delivery.go), the routing of a datagram and the timeouts
// (server.go).
func TestCauseRecordsStayOutOfDecisions(t *testing.T) {
	symbols := []string{"GatewayDwellUs", "GatewayHoldUs", "MatchHoldUs", "ClientTransportSequence", "RefusedBatches",
		"LastRefusal", "dwellMicros", "stampDwell"}
	files, err := filepath.Glob("../../../services/gyo_gateway/session/*.go")
	if err != nil || len(files) == 0 {
		t.Fatalf("session package not found: %v", err)
	}
	for _, file := range files {
		text, err := os.ReadFile(file)
		if err != nil {
			t.Fatal(err)
		}
		for _, symbol := range symbols {
			if strings.Contains(string(text), symbol) {
				t.Fatalf("%s names %q", file, symbol)
			}
		}
	}
	decisions := map[string]bool{"admit": true, "trackAdmission": true, "expireAt": true, "receivePacket": true,
		"forwardInput": true}
	found := 0
	var decls []ast.Decl
	for _, file := range []string{"server.go", "action_delivery.go"} {
		parsed, err := parser.ParseFile(token.NewFileSet(), file, nil, 0)
		if err != nil {
			t.Fatal(err)
		}
		decls = append(decls, parsed.Decls...)
	}
	for _, decl := range decls {
		fn, ok := decl.(*ast.FuncDecl)
		if !ok || !decisions[fn.Name.Name] {
			continue
		}
		found++
		ast.Inspect(fn.Body, func(node ast.Node) bool {
			if id, ok := node.(*ast.Ident); ok {
				for _, symbol := range symbols {
					if id.Name == symbol {
						t.Errorf("%s names %q", fn.Name.Name, symbol)
					}
				}
			}
			return true
		})
	}
	if found != len(decisions) {
		t.Fatalf("found %d of the %d decision functions", found, len(decisions))
	}
}
