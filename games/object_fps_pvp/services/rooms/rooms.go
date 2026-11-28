// Package rooms is the room service (gateway declaration D2): the lobby's room
// list, creating and joining a room, readiness, and starting it, which locks
// the room and gets a Match from the matches service. Rooms live in memory:
// they are short-lived (a restart loses them; a started Match goes on).
package rooms

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"net/http"
	"regexp"
	"slices"
	"strconv"
	"sync"
	"time"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/matches"
)

// The room states: open (listed, joinable), starting (asking for a Match:
// nothing changes it), started (locked, not listed; members fetch their
// ticket).
const (
	Open     = "open"
	Starting = "starting"
	Started  = "started"
)

// idPattern is a map's and a mode's id.
var idPattern = regexp.MustCompile(`^[a-z0-9_]{1,64}$`)

type Config struct {
	// Matches is the allocation service the start asks.
	Matches *api.Client
	// Capacity: the players a room (and its Match) holds.
	Capacity int
	// Maps the rooms may choose; empty: any id of [a-z0-9_]{1,64}.
	Maps []string
	// Modes the rooms may choose (the Match's installed modes, modes/modes.json);
	// empty: any id of [a-z0-9_]{1,64}. The service only carries the choice: the
	// Match plays it (--mode).
	Modes []string
	// StartedTTL: how long a started room stays at most. It goes as soon as a
	// member asks for it after its Match has ended (the matches service says
	// match_ended); this bounds the rooms nobody asks for again.
	StartedTTL time.Duration
	Now        func() time.Time
}

type Service struct {
	config  Config
	mu      sync.Mutex
	rooms   map[uint64]*room
	next    uint64
	replays *api.Replays
}

type member struct {
	user uint64
	// name: the one the member signed in with (the gateway's name header).
	name   string
	ready  bool
	ticket *matches.Ticket
}

type room struct {
	id      uint64
	mode    string
	mapID   string
	owner   uint64
	members []*member
	state   string
	// started: its Match.
	matchID   uint64
	startedAt time.Time
}

func New(config Config) (*Service, error) {
	if config.Matches == nil {
		return nil, errors.New("rooms needs the matches service")
	}
	if config.Capacity <= 0 {
		config.Capacity = 4
	}
	if config.StartedTTL <= 0 {
		config.StartedTTL = 6 * time.Hour
	}
	if config.Now == nil {
		config.Now = time.Now
	}
	// Room ids start at a random point: a Client polling a room of an earlier
	// run of this service does not meet another room under the same id.
	var random [4]byte
	if _, err := rand.Read(random[:]); err != nil {
		return nil, err
	}
	return &Service{config: config, rooms: map[uint64]*room{}, next: uint64(binary.BigEndian.Uint32(random[:])) << 16,
		replays: api.NewReplays(4096)}, nil
}

// Summary is a room in the list.
type Summary struct {
	ID       uint64 `json:"id"`
	Mode     string `json:"mode"`
	Map      string `json:"map"`
	Players  int    `json:"players"`
	Capacity int    `json:"capacity"`
	State    string `json:"state"`
}

type Member struct {
	UserID uint64 `json:"userId"`
	Name   string `json:"name"`
	Ready  bool   `json:"ready"`
}

// Room is one room as a user sees it: Connect is that user's own ticket, once
// the room has started.
type Room struct {
	Summary
	Owner   uint64          `json:"owner"`
	Members []Member        `json:"members"`
	Connect *matches.Ticket `json:"connect,omitempty"`
}

func (s *Service) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("GET /internal/v1/rooms", s.list)
	mux.HandleFunc("GET /internal/v1/rooms/{id}", s.get)
	changes := func(pattern string, handler http.HandlerFunc) { mux.Handle(pattern, s.replays.Wrap(handler)) }
	changes("POST /internal/v1/rooms", s.create)
	changes("POST /internal/v1/rooms/{id}/join", s.join)
	changes("POST /internal/v1/rooms/{id}/leave", s.leave)
	changes("POST /internal/v1/rooms/{id}/ready", s.ready)
	changes("POST /internal/v1/rooms/{id}/start", s.start)
	return mux
}

// user reads the caller; the system (0) has no room of its own.
func user(w http.ResponseWriter, r *http.Request) (uint64, bool) {
	caller, ok := api.UserOf(r)
	if !ok || caller == 0 {
		api.WriteError(w, http.StatusUnauthorized, "no_user")
		return 0, false
	}
	return caller, true
}

// name reads the name the caller signed in with (empty: none).
func name(w http.ResponseWriter, r *http.Request) (string, bool) {
	caller, ok := api.NameOf(r)
	if !ok {
		api.WriteError(w, http.StatusBadRequest, "invalid_name")
		return "", false
	}
	return caller, true
}

// found finds the room of the path under s.mu, or answers why not.
func (s *Service) found(w http.ResponseWriter, r *http.Request) (*room, bool) {
	id, err := strconv.ParseUint(r.PathValue("id"), 10, 64)
	if err != nil {
		api.WriteError(w, http.StatusBadRequest, "invalid_room")
		return nil, false
	}
	found, ok := s.rooms[id]
	if !ok {
		api.WriteError(w, http.StatusNotFound, "room_not_found")
		return nil, false
	}
	return found, true
}

func (s *Service) list(w http.ResponseWriter, _ *http.Request) {
	s.mu.Lock()
	s.sweep()
	rooms := []Summary{}
	for _, found := range s.rooms {
		if found.state == Open {
			rooms = append(rooms, s.summary(found))
		}
	}
	s.mu.Unlock()
	slices.SortFunc(rooms, func(a, b Summary) int { return compare(a.ID, b.ID) })
	api.WriteJSON(w, http.StatusOK, map[string]any{"rooms": rooms})
}

func (s *Service) get(w http.ResponseWriter, r *http.Request) {
	caller, ok := user(w, r)
	if !ok {
		return
	}
	s.mu.Lock()
	s.sweep()
	found, ok := s.found(w, r)
	if !ok {
		s.mu.Unlock()
		return
	}
	if found.state != Started || found.memberOf(caller) == nil {
		api.WriteJSON(w, http.StatusOK, s.view(found, caller))
		s.mu.Unlock()
		return
	}
	id, matchID := found.id, found.matchID
	s.mu.Unlock()

	// A member of a started room gets a fresh token while the Match runs.
	var ticket matches.Ticket
	err := s.config.Matches.Call(r.Context(), http.MethodPost, "/internal/v1/matches/"+strconv.FormatUint(matchID, 10)+"/tickets",
		api.As(api.System), "", map[string]uint64{"userId": caller}, &ticket)
	var refused *api.Refused
	s.mu.Lock()
	defer s.mu.Unlock()
	found, ok = s.rooms[id]
	if !ok {
		api.WriteError(w, http.StatusNotFound, "room_not_found")
		return
	}
	if errors.As(err, &refused) && refused.Code == "match_ended" {
		delete(s.rooms, id)
		api.WriteError(w, http.StatusGone, "match_ended")
		return
	}
	if m := found.memberOf(caller); err == nil && m != nil {
		m.ticket = &ticket
	}
	// Unreachable matches service: the ticket the member had (it may still be
	// good).
	api.WriteJSON(w, http.StatusOK, s.view(found, caller))
}

func (s *Service) create(w http.ResponseWriter, r *http.Request) {
	caller, ok := user(w, r)
	if !ok {
		return
	}
	callerName, ok := name(w, r)
	if !ok {
		return
	}
	var request struct {
		Mode string `json:"mode"`
		Map  string `json:"map"`
	}
	if api.Decode(w, r, api.MaxBody, &request) != nil {
		api.WriteError(w, http.StatusBadRequest, "invalid_room")
		return
	}
	if !idPattern.MatchString(request.Mode) || (len(s.config.Modes) > 0 && !slices.Contains(s.config.Modes, request.Mode)) {
		api.WriteError(w, http.StatusBadRequest, "unknown_mode")
		return
	}
	if !idPattern.MatchString(request.Map) || (len(s.config.Maps) > 0 && !slices.Contains(s.config.Maps, request.Map)) {
		api.WriteError(w, http.StatusBadRequest, "unknown_map")
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sweep()
	if !s.leaveOpenRooms(w, caller) {
		return
	}
	s.next++
	created := &room{id: s.next, mode: request.Mode, mapID: request.Map, owner: caller,
		members: []*member{{user: caller, name: callerName}}, state: Open}
	s.rooms[created.id] = created
	api.WriteJSON(w, http.StatusOK, s.view(created, caller))
}

func (s *Service) join(w http.ResponseWriter, r *http.Request) {
	caller, ok := user(w, r)
	if !ok {
		return
	}
	callerName, ok := name(w, r)
	if !ok {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sweep()
	found, ok := s.found(w, r)
	if !ok {
		return
	}
	if found.memberOf(caller) != nil {
		api.WriteJSON(w, http.StatusOK, s.view(found, caller))
		return
	}
	if !s.changeable(w, found) {
		return
	}
	if len(found.members) >= s.config.Capacity {
		api.WriteError(w, http.StatusConflict, "room_full")
		return
	}
	if !s.leaveOpenRooms(w, caller) {
		return
	}
	found.members = append(found.members, &member{user: caller, name: callerName})
	api.WriteJSON(w, http.StatusOK, s.view(found, caller))
}

func (s *Service) leave(w http.ResponseWriter, r *http.Request) {
	caller, ok := user(w, r)
	if !ok {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sweep()
	found, ok := s.found(w, r)
	if !ok {
		return
	}
	if found.state == Starting {
		api.WriteError(w, http.StatusConflict, "room_starting")
		return
	}
	s.remove(found, caller)
	api.WriteJSON(w, http.StatusOK, map[string]bool{"left": true})
}

func (s *Service) ready(w http.ResponseWriter, r *http.Request) {
	caller, ok := user(w, r)
	if !ok {
		return
	}
	var request struct {
		Ready bool `json:"ready"`
	}
	if api.Decode(w, r, api.MaxBody, &request) != nil {
		api.WriteError(w, http.StatusBadRequest, "invalid_ready")
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sweep()
	found, ok := s.found(w, r)
	if !ok {
		return
	}
	self := found.memberOf(caller)
	if self == nil {
		api.WriteError(w, http.StatusForbidden, "not_member")
		return
	}
	if !s.changeable(w, found) {
		return
	}
	self.ready = request.Ready
	api.WriteJSON(w, http.StatusOK, s.view(found, caller))
}

// start locks the room and asks for a Match: the owner, every member ready.
// While it waits for the answer nothing else changes the room (starting);
// a refused allocation opens it again.
func (s *Service) start(w http.ResponseWriter, r *http.Request) {
	caller, ok := user(w, r)
	if !ok {
		return
	}
	s.mu.Lock()
	s.sweep()
	found, ok := s.found(w, r)
	if !ok {
		s.mu.Unlock()
		return
	}
	if found.state == Started {
		// The start already happened (a retry without the same key).
		api.WriteJSON(w, http.StatusOK, s.view(found, caller))
		s.mu.Unlock()
		return
	}
	if !s.changeable(w, found) {
		s.mu.Unlock()
		return
	}
	if found.owner != caller {
		api.WriteError(w, http.StatusForbidden, "not_owner")
		s.mu.Unlock()
		return
	}
	request := matches.Request{RoomID: found.id, Mode: found.mode, Map: found.mapID, Capacity: s.config.Capacity}
	for _, m := range found.members {
		if !m.ready {
			api.WriteError(w, http.StatusConflict, "not_all_ready")
			s.mu.Unlock()
			return
		}
		request.Players = append(request.Players, matches.Player{UserID: m.user, Name: m.name})
	}
	key, err := attemptKey()
	if err != nil {
		api.WriteError(w, http.StatusInternalServerError, "key_failed")
		s.mu.Unlock()
		return
	}
	found.state = Starting
	s.mu.Unlock()

	allocation, err := s.allocate(context.WithoutCancel(r.Context()), key, request)

	s.mu.Lock()
	defer s.mu.Unlock()
	if err != nil || len(allocation.Tickets) != len(found.members) {
		found.state = Open
		code := "allocation_failed"
		var refused *api.Refused
		if errors.As(err, &refused) && refused.Code != "" {
			code = refused.Code
		}
		api.WriteError(w, http.StatusServiceUnavailable, code)
		return
	}
	found.matchID = allocation.MatchID
	for i := range allocation.Tickets {
		ticket := allocation.Tickets[i]
		if m := found.memberOf(ticket.UserID); m != nil {
			m.ticket = &ticket
		}
	}
	found.state = Started
	found.startedAt = s.config.Now()
	api.WriteJSON(w, http.StatusOK, s.view(found, caller))
}

// startAttempts: how many times a start sends its allocation when the matches
// service cannot be reached (the same key each time, so at most one Match).
const startAttempts = 3

// allocate asks the matches service for a Match; a transport failure (no
// answer) is sent again with the same key, a refusal is the answer.
func (s *Service) allocate(ctx context.Context, key string, request matches.Request) (matches.Allocation, error) {
	var allocation matches.Allocation
	var err error
	for attempt := 0; attempt < startAttempts; attempt++ {
		err = s.config.Matches.Call(ctx, http.MethodPost, "/internal/v1/matches", api.As(api.System), key, request, &allocation)
		var refused *api.Refused
		if err == nil || errors.As(err, &refused) {
			return allocation, err
		}
	}
	return allocation, err
}

// changeable answers why a room cannot change now (starting, started).
func (s *Service) changeable(w http.ResponseWriter, found *room) bool {
	switch found.state {
	case Starting:
		api.WriteError(w, http.StatusConflict, "room_starting")
		return false
	case Started:
		api.WriteError(w, http.StatusConflict, "room_started")
		return false
	}
	return true
}

// leaveOpenRooms takes the caller out of the open room they are in: a user is
// in one room at a time, and joining another moves them. A room starting
// holds them (it answers room_starting).
func (s *Service) leaveOpenRooms(w http.ResponseWriter, caller uint64) bool {
	for _, other := range s.rooms {
		if other.memberOf(caller) == nil {
			continue
		}
		if other.state == Starting {
			api.WriteError(w, http.StatusConflict, "room_starting")
			return false
		}
		if other.state == Open {
			s.remove(other, caller)
		}
	}
	return true
}

// remove takes a member out; the next member owns a room its owner left, and
// an empty room is gone.
func (s *Service) remove(found *room, caller uint64) {
	found.members = slices.DeleteFunc(found.members, func(m *member) bool { return m.user == caller })
	if len(found.members) == 0 {
		delete(s.rooms, found.id)
		return
	}
	if found.owner == caller {
		found.owner = found.members[0].user
	}
}

// sweep drops the started rooms whose members had their time to fetch their
// tickets.
func (s *Service) sweep() {
	now := s.config.Now()
	for id, found := range s.rooms {
		if found.state == Started && now.Sub(found.startedAt) >= s.config.StartedTTL {
			delete(s.rooms, id)
		}
	}
}

func (r *room) memberOf(user uint64) *member {
	for _, m := range r.members {
		if m.user == user {
			return m
		}
	}
	return nil
}

func (s *Service) summary(r *room) Summary {
	return Summary{ID: r.id, Mode: r.mode, Map: r.mapID, Players: len(r.members), Capacity: s.config.Capacity, State: r.state}
}

func (s *Service) view(r *room, caller uint64) Room {
	view := Room{Summary: s.summary(r), Owner: r.owner, Members: []Member{}}
	for _, m := range r.members {
		view.Members = append(view.Members, Member{UserID: m.user, Name: m.name, Ready: m.ready})
		if m.user == caller && m.ticket != nil {
			ticket := *m.ticket
			view.Connect = &ticket
		}
	}
	return view
}

func attemptKey() (string, error) {
	var random [16]byte
	if _, err := rand.Read(random[:]); err != nil {
		return "", err
	}
	return "start-" + hex.EncodeToString(random[:]), nil
}

func compare(a, b uint64) int {
	switch {
	case a < b:
		return -1
	case a > b:
		return 1
	}
	return 0
}
