// Package session authenticates and bounds transport peers, not game players.
package session

import (
	"crypto/rand"
	"crypto/subtle"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"net/netip"
	"sync"
	"time"

	"gyo.local/gateway/framing"
)

var (
	ErrUnauthorized = errors.New("unauthorized peer")
	ErrStale        = errors.New("stale packet sequence")
	ErrRateLimit    = errors.New("packet rate limit exceeded")
)

type Session struct {
	ID          uint64
	Token       string
	mu          sync.Mutex
	endpoint    netip.AddrPort
	lastSeen    time.Time
	// When the bound endpoint last sent a datagram that passed this session's
	// verification: a Hello with the token, or an Admit endpoint match,
	// whatever its sequence, payload or rate. Only the bound endpoint
	// refreshes it, so a forger can only keep it fresh (block a rebinding),
	// never make the endpoint look silent.
	lastVerified time.Time
	sequence    uint32
	hasSequence bool
	// The rate limit's virtual schedule (GCRA): when the session's next packet
	// is due at the limit's pace; zero before the first packet.
	schedule time.Time
}

// The packet rate limit: a token bucket of ratePerSecond packets a second,
// rateBurst deep, kept as the generic cell rate algorithm (GCRA) on integer
// time. Over any interval T at most ratePerSecond*T + ratePerSecond packets
// pass (the most a fixed one-second window also passes across its boundary).
// Unlike a fixed window it measures the peer's pace, not the reader's: the
// datagrams that waited in the socket while the reader stalled spend the
// credit their sender earned meanwhile (v7.1 K1, D121).
const (
	ratePerSecond = 120
	rateInterval  = time.Second / ratePerSecond
	rateBurst     = (ratePerSecond - 1) * rateInterval // how far ahead of now the schedule may run
)

func New(now time.Time) (*Session, error) {
	var random [40]byte
	if _, err := rand.Read(random[:]); err != nil {
		return nil, err
	}
	id := binary.BigEndian.Uint64(random[:8])
	if id == 0 {
		id = 1
	}
	return &Session{ID: id, Token: hex.EncodeToString(random[8:]), lastSeen: now}, nil
}

func (s *Session) MatchesToken(token string) bool {
	return subtle.ConstantTimeCompare([]byte(token), []byte(s.Token)) == 1
}

// Rebinding is the caller's rule for moving a session to a new endpoint: the
// bound endpoint silent for at least Silence, and the session not expired
// under Timeout (as Expired). A zero Silence never rebinds.
type Rebinding struct {
	Silence, Timeout time.Duration
}

// Rebound describes a Hello that moved the session: the endpoint it left and
// how long that endpoint had been silent.
type Rebound struct {
	Previous netip.AddrPort
	Silent   time.Duration
}

// Hello is idempotent. A repeated authenticated sequence may receive a reply,
// but only a fresh sequence refreshes liveness. It never rebinds: a Hello from
// another endpoint is unauthorized.
func (s *Session) Hello(token string, peer netip.AddrPort, sequence uint32, now time.Time) error {
	_, err := s.HelloRebinding(token, peer, sequence, now, Rebinding{})
	return err
}

// HelloRebinding is Hello, and a Hello from another endpoint moves the session
// there when every condition holds: the token matches, the sequence is newer
// than the session's, the bound endpoint has sent no verified datagram for
// rule.Silence, and the session has not expired. The old endpoint is then
// unauthorized. The decision is taken before anything is refreshed.
func (s *Session) HelloRebinding(token string, peer netip.AddrPort, sequence uint32, now time.Time, rule Rebinding) (Rebound, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.MatchesToken(token) || !peer.IsValid() {
		return Rebound{}, ErrUnauthorized
	}
	var rebound Rebound
	moving := s.endpoint.IsValid() && s.endpoint != peer
	if moving {
		if rule.Silence <= 0 || !s.hasSequence || !framing.NewerSequence(sequence, s.sequence) ||
			now.Sub(s.lastVerified) < rule.Silence || now.Sub(s.lastSeen) >= rule.Timeout {
			return Rebound{}, ErrUnauthorized
		}
		rebound = Rebound{Previous: s.endpoint, Silent: now.Sub(s.lastVerified)}
	} else {
		s.lastVerified = now // the bound (or first) endpoint, verified before the rate limit
	}
	if err := s.rate(now); err != nil {
		return Rebound{}, err
	}
	if moving {
		s.lastVerified = now
	}
	s.endpoint = peer
	if !s.hasSequence || framing.NewerSequence(sequence, s.sequence) {
		s.sequence, s.hasSequence, s.lastSeen = sequence, true, now
	}
	return rebound, nil
}

// LastVerified is when the bound endpoint last sent a verified datagram
// (zero before the first Hello).
func (s *Session) LastVerified() time.Time {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.lastVerified
}

func (s *Session) Accept(peer netip.AddrPort, sequence uint32, now time.Time) error {
	if err := s.Admit(peer, sequence, now); err != nil {
		return err
	}
	return s.CommitSequence(sequence, now)
}

// Admit bounds authenticated traffic before a caller decodes its payload. It
// does not extend liveness: malformed or obsolete application payloads must not
// keep a session alive. A successfully validated payload calls CommitSequence.
func (s *Session) Admit(peer netip.AddrPort, sequence uint32, now time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.endpoint.IsValid() || s.endpoint != peer {
		return ErrUnauthorized
	}
	s.lastVerified = now // the endpoint matched: verified, before the rate limit
	if err := s.rate(now); err != nil {
		return err
	}
	if s.hasSequence && !framing.NewerSequence(sequence, s.sequence) {
		return ErrStale
	}
	return nil
}

func (s *Session) CommitSequence(sequence uint32, now time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.hasSequence && !framing.NewerSequence(sequence, s.sequence) {
		return ErrStale
	}
	s.sequence, s.hasSequence, s.lastSeen = sequence, true, now
	return nil
}

func (s *Session) Endpoint() netip.AddrPort {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.endpoint
}

func (s *Session) Expired(now time.Time, timeout time.Duration) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	return now.Sub(s.lastSeen) >= timeout
}

// rate admits one packet at now, or refuses it without spending credit.
func (s *Session) rate(now time.Time) error {
	due := s.schedule
	if due.Before(now) {
		due = now // credit does not build up beyond the burst
	}
	if due.Sub(now) > rateBurst {
		return ErrRateLimit
	}
	s.schedule = due.Add(rateInterval)
	return nil
}
