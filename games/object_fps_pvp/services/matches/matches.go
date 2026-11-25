// Package matches is the allocation service (gateway declaration D2, D6): it
// gets a Match for a room from a provider and signs each player's admission
// token with that Match's own key (D5).
package matches

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"net/http"
	"sync"
	"time"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/token"
)

// Spec is what a room asks of a Match.
type Spec struct {
	MatchID  uint64
	Key      []byte
	Capacity int
	Mode     string
	Map      string
}

// Running is a Match a provider started: where players connect, a channel
// closed when it ends, and how to end it early.
type Running struct {
	Address string
	Ended   <-chan struct{}
	Stop    func()
}

// Provider starts Matches (D6): the local one starts processes on this
// machine; a fleet would ask its own allocator.
type Provider interface {
	Start(ctx context.Context, spec Spec) (Running, error)
}

type Config struct {
	Provider Provider
	// TokenTTL: how long a player's admission token is good for (a reconnect
	// within it needs no new token; D-8: a reconnect is a new connection).
	TokenTTL time.Duration
	Now      func() time.Time
}

type Service struct {
	config  Config
	mu      sync.Mutex
	running map[uint64]Running
	replays *api.Replays
}

func New(config Config) (*Service, error) {
	if config.Provider == nil {
		return nil, errors.New("matches needs a provider")
	}
	if config.TokenTTL <= 0 {
		config.TokenTTL = 10 * time.Minute
	}
	if config.Now == nil {
		config.Now = time.Now
	}
	return &Service{config: config, running: map[uint64]Running{}, replays: api.NewReplays(1024)}, nil
}

// Request is a room's allocation request.
type Request struct {
	RoomID   uint64   `json:"roomId"`
	Mode     string   `json:"mode"`
	Map      string   `json:"map"`
	Capacity int      `json:"capacity"`
	Players  []uint64 `json:"players"`
}

// Ticket is one player's way in: the Match's address and the admission token
// (hex), the NetConnectTarget the Client puts in its game World.
type Ticket struct {
	Server string `json:"server"`
	UserID uint64 `json:"userId"`
	Token  string `json:"token"`
}

type Allocation struct {
	MatchID uint64   `json:"matchId"`
	Tickets []Ticket `json:"tickets"`
}

func (s *Service) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.Handle("POST /internal/v1/matches", s.replays.Wrap(http.HandlerFunc(s.allocate)))
	return mux
}

// Running reports how many Matches have started and not ended.
func (s *Service) Running() int {
	s.mu.Lock()
	defer s.mu.Unlock()
	return len(s.running)
}

func (s *Service) allocate(w http.ResponseWriter, r *http.Request) {
	var request Request
	if api.Decode(w, r, api.MaxBody, &request) != nil || len(request.Players) == 0 ||
		request.Capacity < len(request.Players) {
		api.WriteError(w, http.StatusBadRequest, "invalid_allocation")
		return
	}
	key, err := token.NewKey()
	if err != nil {
		api.WriteError(w, http.StatusInternalServerError, "key_failed")
		return
	}
	matchID, err := newMatchID()
	if err != nil {
		api.WriteError(w, http.StatusInternalServerError, "key_failed")
		return
	}
	running, err := s.config.Provider.Start(r.Context(), Spec{MatchID: matchID, Key: key,
		Capacity: request.Capacity, Mode: request.Mode, Map: request.Map})
	if err != nil {
		api.WriteError(w, http.StatusServiceUnavailable, "no_match_available")
		return
	}
	s.mu.Lock()
	s.running[matchID] = running
	s.mu.Unlock()
	go func() {
		<-running.Ended
		s.mu.Lock()
		delete(s.running, matchID)
		s.mu.Unlock()
	}()
	expires := s.config.Now().Add(s.config.TokenTTL)
	allocation := Allocation{MatchID: matchID}
	for _, user := range request.Players {
		signed := token.SignAdmission(key, token.Admission{MatchID: matchID, UserID: user, ExpiresAt: expires})
		allocation.Tickets = append(allocation.Tickets, Ticket{Server: running.Address, UserID: user, Token: hex.EncodeToString(signed)})
	}
	api.WriteJSON(w, http.StatusOK, allocation)
}

func newMatchID() (uint64, error) {
	var random [8]byte
	for {
		if _, err := rand.Read(random[:]); err != nil {
			return 0, err
		}
		if id := binary.BigEndian.Uint64(random[:]); id != 0 {
			return id, nil
		}
	}
}
