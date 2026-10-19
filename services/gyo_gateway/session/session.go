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

// Hello is idempotent. A repeated authenticated sequence may receive a reply,
// but only a fresh sequence refreshes liveness. Endpoint rebinding is not in v1.
func (s *Session) Hello(token string, peer netip.AddrPort, sequence uint32, now time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.MatchesToken(token) ||
		!peer.IsValid() || (s.endpoint.IsValid() && s.endpoint != peer) {
		return ErrUnauthorized
	}
	if err := s.rate(now); err != nil {
		return err
	}
	s.endpoint = peer
	if !s.hasSequence || framing.NewerSequence(sequence, s.sequence) {
		s.sequence, s.hasSequence, s.lastSeen = sequence, true, now
	}
	return nil
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
