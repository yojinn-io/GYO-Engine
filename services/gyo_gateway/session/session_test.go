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

// Rebinding (network-architecture §4 "連線中斷與恢復", identity-rebind): a
// Hello from a new endpoint moves the session only when the token matches, its
// sequence is newer, the bound endpoint has been silent for the rule's Silence
// and the session has not expired; the old endpoint is then unauthorized.
var rebind = Rebinding{Silence: 500 * time.Millisecond, Timeout: 5 * time.Second}

func boundSession(t *testing.T, now time.Time) (*Session, netip.AddrPort, netip.AddrPort) {
	t.Helper()
	s, err := New(now)
	if err != nil {
		t.Fatal(err)
	}
	a := netip.MustParseAddrPort("127.0.0.1:1000")
	b := netip.MustParseAddrPort("127.0.0.1:2000")
	if err := s.Hello(s.Token, a, 1, now); err != nil {
		t.Fatal(err)
	}
	return s, a, b
}

func TestARebindingMovesASilentSessionAndLocksOutTheOldEndpoint(t *testing.T) {
	now := time.Unix(100, 0)
	s, a, b := boundSession(t, now)
	at := now.Add(600 * time.Millisecond)
	rebound, err := s.HelloRebinding(s.Token, b, 2, at, rebind)
	if err != nil {
		t.Fatal(err)
	}
	if rebound.Previous != a || rebound.Silent != 600*time.Millisecond {
		t.Fatalf("rebound %+v", rebound)
	}
	if s.Endpoint() != b || !s.LastVerified().Equal(at) {
		t.Fatal("the session did not move to the new endpoint")
	}
	if err := s.Admit(a, 3, at); !errors.Is(err, ErrUnauthorized) {
		t.Fatal("the old endpoint is still authorized")
	}
	if err := s.Accept(b, 3, at); err != nil {
		t.Fatal(err)
	}
	// A Hello from the endpoint now bound rebinds nothing.
	if rebound, err := s.HelloRebinding(s.Token, b, 4, at.Add(time.Second), rebind); err != nil || rebound != (Rebound{}) {
		t.Fatalf("a bound Hello: %+v %v", rebound, err)
	}
}

func TestARebindingNeedsEveryCondition(t *testing.T) {
	now := time.Unix(100, 0)
	cases := []struct {
		name     string
		token    string
		sequence uint32
		at       time.Duration
		rule     Rebinding
	}{
		{"silence too short", "", 2, 499 * time.Millisecond, rebind},
		{"sequence not newer", "", 1, 600 * time.Millisecond, rebind},
		{"wrong token", "wrong", 2, 600 * time.Millisecond, rebind},
		{"session expired", "", 2, 5 * time.Second, rebind},
		{"no rule (Hello)", "", 2, 4 * time.Second, Rebinding{}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			s, a, b := boundSession(t, now)
			token := s.Token
			if c.token != "" {
				token = c.token
			}
			if _, err := s.HelloRebinding(token, b, c.sequence, now.Add(c.at), c.rule); !errors.Is(err, ErrUnauthorized) {
				t.Fatalf("rebound: %v", err)
			}
			if s.Endpoint() != a || !s.LastVerified().Equal(now) {
				t.Fatal("a refused rebinding moved or refreshed the session")
			}
		})
	}
}

func TestTheBoundEndpointKeepsItsSessionByAnyVerifiedDatagram(t *testing.T) {
	now := time.Unix(100, 0)
	t.Run("a sequence jump from the old endpoint", func(t *testing.T) {
		s, a, b := boundSession(t, now)
		if err := s.Admit(a, 1000, now.Add(300*time.Millisecond)); err != nil {
			t.Fatal(err)
		}
		if _, err := s.HelloRebinding(s.Token, b, 1001, now.Add(600*time.Millisecond), rebind); !errors.Is(err, ErrUnauthorized) {
			t.Fatal("a new endpoint rebound within Silence of the old endpoint's last datagram")
		}
	})
	t.Run("only Input from the old endpoint", func(t *testing.T) {
		s, a, b := boundSession(t, now)
		sequence := uint32(1)
		for at := 16 * time.Millisecond; at <= 600*time.Millisecond; at += 16 * time.Millisecond {
			sequence++
			if err := s.Accept(a, sequence, now.Add(at)); err != nil {
				t.Fatal(err)
			}
		}
		if _, err := s.HelloRebinding(s.Token, b, sequence+1, now.Add(600*time.Millisecond), rebind); !errors.Is(err, ErrUnauthorized) {
			t.Fatal("a new endpoint rebound while the old one sent Input")
		}
	})
	t.Run("stale and rate-limited datagrams verify too", func(t *testing.T) {
		s, a, _ := boundSession(t, now)
		at := now.Add(200 * time.Millisecond)
		if err := s.Admit(a, 1, at); !errors.Is(err, ErrStale) {
			t.Fatal(err)
		}
		if !s.LastVerified().Equal(at) {
			t.Fatal("a stale datagram from the bound endpoint did not verify")
		}
		limited := at
		for err := error(nil); !errors.Is(err, ErrRateLimit); {
			limited = limited.Add(time.Microsecond)
			err = s.Admit(a, 1, limited)
		}
		if !s.LastVerified().Equal(limited) {
			t.Fatal("a rate-limited datagram from the bound endpoint did not verify")
		}
	})
	t.Run("another endpoint verifies nothing", func(t *testing.T) {
		s, _, b := boundSession(t, now)
		if err := s.Admit(b, 2, now.Add(time.Second)); !errors.Is(err, ErrUnauthorized) {
			t.Fatal(err)
		}
		if _, err := s.HelloRebinding("wrong", b, 2, now.Add(time.Second), rebind); !errors.Is(err, ErrUnauthorized) {
			t.Fatal(err)
		}
		if !s.LastVerified().Equal(now) {
			t.Fatal("another endpoint refreshed the last verified time")
		}
	})
}
