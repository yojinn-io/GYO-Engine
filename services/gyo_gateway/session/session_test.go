package session

import (
	"errors"
	"net/netip"
	"testing"
	"time"
)

func TestSessionAuthenticationOrderingAndLiveness(t *testing.T) {
	now := time.Unix(100, 0)
	s, err := New(now)
	if err != nil {
		t.Fatal(err)
	}
	a := netip.MustParseAddrPort("127.0.0.1:1000")
	b := netip.MustParseAddrPort("127.0.0.1:1001")
	if s.ID == 0 || len(s.Token) != 64 {
		t.Fatal("bad identity")
	}
	if err := s.Accept(a, 1, now); !errors.Is(err, ErrUnauthorized) {
		t.Fatal("unbound input accepted")
	}
	if err := s.Hello("wrong", a, 1, now); !errors.Is(err, ErrUnauthorized) {
		t.Fatal("bad token accepted")
	}
	if err := s.Hello(s.Token, a, 1, now); err != nil {
		t.Fatal(err)
	}
	if err := s.Hello(s.Token, b, 2, now); !errors.Is(err, ErrUnauthorized) {
		t.Fatal("endpoint hijack")
	}
	if err := s.Accept(b, 2, now); !errors.Is(err, ErrUnauthorized) {
		t.Fatal("spoof accepted")
	}
	if err := s.Accept(a, 2, now.Add(time.Second)); err != nil {
		t.Fatal(err)
	}
	if err := s.Accept(a, 2, now.Add(4*time.Second)); !errors.Is(err, ErrStale) {
		t.Fatal("duplicate accepted")
	}
	if err := s.Hello(s.Token, a, 1, now.Add(5*time.Second)); err != nil {
		t.Fatal("idempotent hello failed")
	}
	if !s.Expired(now.Add(6*time.Second), 5*time.Second) {
		t.Fatal("stale packet refreshed session")
	}
}
func TestSessionBoundsPacketRate(t *testing.T) {
	now := time.Unix(100, 0)
	s, _ := New(now)
	peer := netip.MustParseAddrPort("127.0.0.1:9")
	if err := s.Hello(s.Token, peer, 1, now); err != nil {
		t.Fatal(err)
	}
	for i := uint32(2); i <= 120; i++ {
		if err := s.Accept(peer, i, now); err != nil {
			t.Fatal(err)
		}
	}
	if !errors.Is(s.Accept(peer, 121, now), ErrRateLimit) {
		t.Fatal("no rate limit")
	}
	if err := s.Accept(peer, 122, now.Add(time.Second)); err != nil {
		t.Fatal(err)
	}
}

func TestRejectedPayloadsConsumeRateWithoutRefreshingLiveness(t *testing.T) {
	now := time.Unix(100, 0)
	s, _ := New(now)
	peer := netip.MustParseAddrPort("127.0.0.1:9")
	if err := s.Hello(s.Token, peer, 1, now); err != nil {
		t.Fatal(err)
	}
	// Simulate payload validation failures: admission happens but sequence is
	// never committed, so the packet neither advances order nor refreshes time.
	for i := 0; i < 120; i++ {
		if err := s.Admit(peer, 2, now.Add(time.Second)); err != nil {
			t.Fatal(err)
		}
	}
	if !errors.Is(s.Admit(peer, 2, now.Add(time.Second)), ErrRateLimit) {
		t.Fatal("rejected payloads bypassed rate limit")
	}
	if !s.Expired(now.Add(5*time.Second), 5*time.Second) {
		t.Fatal("rejected payload refreshed liveness")
	}
	if err := s.Accept(peer, 2, now.Add(6*time.Second)); err != nil {
		t.Fatal("rejected payload consumed sequence")
	}
}

// A peer sending at a steady rate under the limit is never refused because the
// reader stalled: the datagrams that waited in the socket during the stall are
// read at once when it resumes, but the peer did not send them faster (v7.1 K1).
func TestAReaderStallDoesNotTurnASteadyPeerIntoRefusals(t *testing.T) {
	start := time.Unix(100, 0)
	s, _ := New(start)
	peer := netip.MustParseAddrPort("127.0.0.1:9")
	if err := s.Hello(s.Token, peer, 1, start); err != nil {
		t.Fatal(err)
	}
	const every = 11 * time.Millisecond // about 91 datagrams a second, as a firing player sends
	stallFrom, stallTo := start.Add(time.Second), start.Add(2*time.Second)
	refused, sequence := 0, uint32(2)
	for sent := start.Add(every); sent.Before(start.Add(4 * time.Second)); sent = sent.Add(every) {
		read := sent
		if !sent.Before(stallFrom) && sent.Before(stallTo) {
			read = stallTo // waited in the socket while the reader was stalled
		}
		if errors.Is(s.Accept(peer, sequence, read), ErrRateLimit) {
			refused++
		}
		sequence++
	}
	if refused != 0 {
		t.Fatalf("%d datagrams of a peer sending at %v intervals refused after a 1 s reader stall", refused, every)
	}
}

// The limit itself holds over any interval T: a flood is admitted at most
// 120 * T + 120 times (the most a fixed one-second window also admits across a
// window boundary).
func TestSessionBoundsASustainedFlood(t *testing.T) {
	start := time.Unix(100, 0)
	s, _ := New(start)
	peer := netip.MustParseAddrPort("127.0.0.1:9")
	if err := s.Hello(s.Token, peer, 1, start); err != nil {
		t.Fatal(err)
	}
	admitted, sequence := 1, uint32(2)
	for sent := start; sent.Before(start.Add(3 * time.Second)); sent = sent.Add(time.Second / 300) {
		if s.Accept(peer, sequence, sent) == nil {
			admitted++
		}
		sequence++
	}
	if admitted > 120*3+120 || admitted < 120*3 {
		t.Fatalf("a 300 per second flood for 3 s admitted %d times, want between %d and %d", admitted, 120*3, 120*3+120)
	}
}

// Idle time earns no more than the burst: after a long silence a flood still
// passes at most 120 packets at once.
func TestIdleTimeEarnsNoMoreThanTheBurst(t *testing.T) {
	start := time.Unix(100, 0)
	s, _ := New(start)
	peer := netip.MustParseAddrPort("127.0.0.1:9")
	if err := s.Hello(s.Token, peer, 1, start); err != nil {
		t.Fatal(err)
	}
	later := start.Add(10 * time.Second)
	admitted := 0
	for sequence := uint32(2); sequence < 302; sequence++ {
		if s.Accept(peer, sequence, later) == nil {
			admitted++
		}
	}
	if admitted != 120 {
		t.Fatalf("after 10 s of silence an instant flood admitted %d packets, want 120", admitted)
	}
}
