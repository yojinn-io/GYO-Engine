// Package saves is the save service (gateway declaration D2, D7): one save per
// user, a version number for optimistic writes, the body in the product's own
// format (the service does not read it). Only a Match writes (the authority);
// a player reads their own through the gateway.
package saves

import (
	"encoding/json"
	"errors"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"sync"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/internal/store"
)

// MaxSave is the largest save body a write takes.
const MaxSave = 64 << 10

// Config: Data is the directory saves/ goes in (empty: memory only, the tests).
type Config struct {
	Data string
}

type Service struct {
	config  Config
	mu      sync.Mutex
	memory  map[uint64]Save
	replays *api.Replays
}

// Save is one user's save: Version counts its writes (0: never written, Body
// null).
type Save struct {
	UserID  uint64          `json:"userId"`
	Version uint64          `json:"version"`
	Body    json.RawMessage `json:"body"`
}

// Write replaces a save written Version times; the body is the new save.
type Write struct {
	Version uint64          `json:"version"`
	Body    json.RawMessage `json:"body"`
}

// Conflict is a refused write's answer: the save was written meanwhile.
type Conflict struct {
	Error   string `json:"error"`
	Version uint64 `json:"version"`
}

func New(config Config) *Service {
	return &Service{config: config, memory: map[uint64]Save{}, replays: api.NewReplays(4096)}
}

func (s *Service) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("GET /internal/v1/saves/{user}", s.read)
	mux.Handle("PUT /internal/v1/saves/{user}", s.replays.Wrap(http.HandlerFunc(s.write)))
	return mux
}

func userPath(r *http.Request) (uint64, bool) {
	user, err := strconv.ParseUint(r.PathValue("user"), 10, 64)
	return user, err == nil && user != 0
}

func (s *Service) read(w http.ResponseWriter, r *http.Request) {
	user, ok := userPath(r)
	if !ok {
		api.WriteError(w, http.StatusBadRequest, "invalid_user")
		return
	}
	s.mu.Lock()
	save, err := s.load(user)
	s.mu.Unlock()
	if err != nil {
		api.WriteError(w, http.StatusInternalServerError, "save_store_failed")
		return
	}
	api.WriteJSON(w, http.StatusOK, save)
}

func (s *Service) write(w http.ResponseWriter, r *http.Request) {
	user, ok := userPath(r)
	if !ok {
		api.WriteError(w, http.StatusBadRequest, "invalid_user")
		return
	}
	var request Write
	if api.Decode(w, r, MaxSave, &request) != nil || len(request.Body) == 0 || !json.Valid(request.Body) {
		api.WriteError(w, http.StatusBadRequest, "invalid_save")
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	current, err := s.load(user)
	if err != nil {
		api.WriteError(w, http.StatusInternalServerError, "save_store_failed")
		return
	}
	if current.Version != request.Version {
		api.WriteJSON(w, http.StatusConflict, Conflict{Error: "version_conflict", Version: current.Version})
		return
	}
	next := Save{UserID: user, Version: current.Version + 1, Body: request.Body}
	if err := s.store(next); err != nil {
		api.WriteError(w, http.StatusInternalServerError, "save_store_failed")
		return
	}
	api.WriteJSON(w, http.StatusOK, next)
}

func (s *Service) file(user uint64) string {
	return filepath.Join(s.config.Data, "saves", strconv.FormatUint(user, 10)+".json")
}

// load reads a user's save under s.mu: from memory when there is no Data.
func (s *Service) load(user uint64) (Save, error) {
	if s.config.Data == "" {
		if save, ok := s.memory[user]; ok {
			return save, nil
		}
		return Save{UserID: user, Body: json.RawMessage("null")}, nil
	}
	body, err := os.ReadFile(s.file(user))
	if errors.Is(err, os.ErrNotExist) {
		return Save{UserID: user, Body: json.RawMessage("null")}, nil
	}
	if err != nil {
		return Save{}, err
	}
	var save Save
	if err := json.Unmarshal(body, &save); err != nil {
		return Save{}, err
	}
	return save, nil
}

func (s *Service) store(save Save) error {
	if s.config.Data == "" {
		s.memory[save.UserID] = save
		return nil
	}
	body, err := json.Marshal(save)
	if err != nil {
		return err
	}
	return store.WriteFileAtomic(s.file(save.UserID), body)
}
