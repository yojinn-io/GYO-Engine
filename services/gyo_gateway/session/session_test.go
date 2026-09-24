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
