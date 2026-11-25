// Package identity is the credentials service (gateway declaration D2): a
// device id becomes a user id, kept in a file, and a session token the
// gateway checks without asking it again.
package identity

import (
	"encoding/json"
	"errors"
	"net/http"
	"os"
	"path/filepath"
	"sync"
	"time"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/internal/store"
	"gyo.local/object_fps_pvp/services/token"
)

// Config is the service's setup. Data is the directory of identity.json
// (empty: kept in memory only, the tests).
type Config struct {
	Data       string
	SessionKey []byte
	SessionTTL time.Duration
	Now        func() time.Time
}

type Service struct {
	config  Config
	mu      sync.Mutex
	devices map[string]uint64
	next    uint64
}

// The file: every device and the last user id given.
type stored struct {
	Devices  map[string]uint64 `json:"devices"`
	LastUser uint64            `json:"lastUser"`
}

func New(config Config) (*Service, error) {
	if len(config.SessionKey) == 0 {
		return nil, errors.New("identity needs a session key")
	}
	if config.SessionTTL <= 0 {
		config.SessionTTL = 24 * time.Hour
	}
	if config.Now == nil {
		config.Now = time.Now
	}
	s := &Service{config: config, devices: map[string]uint64{}}
	if config.Data != "" {
		body, err := os.ReadFile(s.path())
		if err == nil {
			var file stored
			if err := json.Unmarshal(body, &file); err != nil {
				return nil, err
			}
			if file.Devices != nil {
				s.devices = file.Devices
			}
			s.next = file.LastUser
		} else if !errors.Is(err, os.ErrNotExist) {
			return nil, err
		}
	}
	return s, nil
}

func (s *Service) path() string { return filepath.Join(s.config.Data, "identity.json") }

// Session is the answer to a device login.
type Session struct {
	UserID       uint64 `json:"userId"`
	SessionToken string `json:"sessionToken"`
	ExpiresAt    int64  `json:"expiresAt"`
}

func (s *Service) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("POST /internal/v1/sessions", s.login)
	return mux
}

func validDevice(device string) bool {
	if len(device) == 0 || len(device) > 128 {
		return false
	}
	for _, c := range []byte(device) {
		if c < 0x21 || c > 0x7e {
			return false
		}
	}
	return true
}

func (s *Service) login(w http.ResponseWriter, r *http.Request) {
	var request struct {
		DeviceID string `json:"deviceId"`
	}
	if api.Decode(w, r, api.MaxBody, &request) != nil || !validDevice(request.DeviceID) {
		api.WriteError(w, http.StatusBadRequest, "invalid_device")
		return
	}
	user, err := s.userOf(request.DeviceID)
	if err != nil {
		api.WriteError(w, http.StatusInternalServerError, "identity_store_failed")
		return
	}
	expires := s.config.Now().Add(s.config.SessionTTL)
	api.WriteJSON(w, http.StatusOK, Session{UserID: user,
		SessionToken: token.SignSession(s.config.SessionKey, user, expires), ExpiresAt: expires.Unix()})
}

// userOf returns the device's user, giving a new one (and storing it) the
// first time.
func (s *Service) userOf(device string) (uint64, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if user, ok := s.devices[device]; ok {
		return user, nil
	}
	user := s.next + 1
	s.devices[device] = user
	if err := s.store(user); err != nil {
		delete(s.devices, device)
		return 0, err
	}
	s.next = user
	return user, nil
}

func (s *Service) store(last uint64) error {
	if s.config.Data == "" {
		return nil
	}
	body, err := json.Marshal(stored{Devices: s.devices, LastUser: last})
	if err != nil {
		return err
	}
	return store.WriteFileAtomic(s.path(), body)
}
