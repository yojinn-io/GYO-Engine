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
	window      time.Time
	packets     uint32
}

func New(now time.Time) (*Session, error) {
	var random [40]byte
	if _, err := rand.Read(random[:]); err != nil {
		return nil, err
	}
	id := binary.BigEndian.Uint64(random[:8])
	if id == 0 {
		id = 1
	}
	return &Session{ID: id, Token: hex.EncodeToString(random[8:]), lastSeen: now, window: now}, nil
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

func (s *Session) rate(now time.Time) error {
	if now.Sub(s.window) >= time.Second {
		s.window, s.packets = now, 0
	}
	s.packets++
	if s.packets > 120 {
		return ErrRateLimit
	}
	return nil
}
