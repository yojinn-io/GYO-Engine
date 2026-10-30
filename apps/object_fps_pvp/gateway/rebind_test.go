package gateway

import (
	"fmt"
	"net/netip"
	"strings"
	"testing"
	"time"

	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv8"
)

// identity-rebind (network-architecture §4 "連線中斷與恢復"): the product
// passes T_int and T_max to the session's rebinding rule, logs each rebinding
// and restarts a Client's clock window when a verified datagram ends a silence.

func logText(logs *ingressLog) string {
	logs.mu.Lock()
	defer logs.mu.Unlock()
	return logs.buf.String()
}

func TestASilentClientRebindsToItsNewEndpointAndTheOldOneIsLockedOut(t *testing.T) {
	logs := captureIngressLog(t)
	s, p, endpoint, now := actionFixture(t)
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
	moved := netip.MustParseAddrPort("127.0.0.1:29555")
	// Within T_int of the last verified datagram: refused, nothing logged.
	if err := s.receiveDatagram(f.datagram(adapter.Hello, 2, &client.Hello{SessionToken: p.session.Token}), moved, now.Add(400*time.Millisecond)); err != nil {
		t.Fatal(err)
	}
	if p.session.Endpoint() != endpoint || strings.Contains(logText(logs), "session rebound") {
		t.Fatal("a Hello rebound within T_int")
	}
	takeAllIngress(s)
	if err := s.receiveDatagram(f.datagram(adapter.Hello, 3, &client.Hello{SessionToken: p.session.Token}), moved, now.Add(600*time.Millisecond)); err != nil {
		t.Fatal(err)
	}
	if p.session.Endpoint() != moved {
		t.Fatal("a silent session did not rebind")
	}
	want := fmt.Sprintf("session rebound version=1 player=7 old_port=%d new_port=%d silent_ms=600", endpoint.Port(), moved.Port())
	if text := logText(logs); strings.Count(text, "session rebound") != 1 || !strings.Contains(text, want) {
		t.Fatalf("want one %q in\n%s", want, text)
	}
	if strings.Contains(logText(logs), p.session.Token) {
		t.Fatal("the token was logged")
	}
	// The old endpoint is unauthorized now.
	if err := s.receiveDatagram(f.datagram(adapter.Input, 4, inputMessage(1, moves(1, 1))), endpoint, now.Add(700*time.Millisecond)); err != nil {
		t.Fatal(err)
	}
	if got := takeAllIngress(s).players[7][playerDropUnauthorizedInputPackets]; got != 1 {
		t.Fatalf("old endpoint input: %d unauthorized drops", got)
	}
}

func TestTheClientClockWindowRestartsOnlyWhenAVerifiedDatagramEndsASilence(t *testing.T) {
	logs := captureIngressLog(t)
	s, p, endpoint, now := actionFixture(t)
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
	sequence := uint32(2)
	at := func(d time.Duration) time.Time { return now.Add(d) }
	hello := func(d time.Duration) {
		t.Helper()
		sequence++
		if err := s.receiveDatagram(f.datagram(adapter.Hello, sequence, &client.Hello{SessionToken: p.session.Token}), endpoint, at(d)); err != nil {
			t.Fatal(err)
		}
	}
	input := func(from netip.AddrPort, d time.Duration) {
		t.Helper()
		sequence++
		if err := s.receiveDatagram(f.datagram(adapter.Input, sequence, inputMessage(1, moves(uint32(sequence), uint32(sequence)))), from, at(d)); err != nil {
			t.Fatal(err)
		}
	}
	resumes := func() int { return strings.Count(logText(logs), "reason=resume") }
	hello(0) // starts the session's clock sampling
	// 60 Hz Input and a Hello each second: never silent.
	for d := 16 * time.Millisecond; d <= 1200*time.Millisecond; d += 16 * time.Millisecond {
		input(endpoint, d)
		if d%time.Second < 16*time.Millisecond {
			hello(d)
		}
	}
	if resumes() != 0 {
		t.Fatal("60 Hz Input restarted the clock window")
	}
	last := p.session.LastVerified()
	// Another endpoint naming the session verifies nothing and restarts nothing.
	spoofed := netip.MustParseAddrPort("127.0.0.1:29777")
	input(spoofed, 2*time.Second)
	if resumes() != 0 || !p.session.LastVerified().Equal(last) {
		t.Fatal("a datagram from another endpoint restarted the window or refreshed the session")
	}
	// The bound endpoint after 0.8 s of silence: one resume, then none.
	input(endpoint, 2000*time.Millisecond)
	if resumes() != 1 {
		t.Fatalf("a verified datagram after a silence: %d resumes in\n%s", resumes(), logText(logs))
	}
	input(endpoint, 2016*time.Millisecond)
	if resumes() != 1 {
		t.Fatal("the next datagram restarted the window again")
	}
	if !strings.Contains(logText(logs), "client clock sync player=7 window_ms=") {
		t.Fatal("no client clock line")
	}
}
