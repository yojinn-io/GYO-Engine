package api

import (
	"bytes"
	"net/http"
	"sync"
)

// MaxKey is the longest idempotency key a service takes (master's request_id
// limit, apps/object_fps_pvp/gateway/server.go:303).
const MaxKey = 128

// Replays makes a service's state-changing routes idempotent (declaration D4):
// the first successful (2xx) answer to a (user, route, key) is kept and every
// later request with the same key gets it again, also while the first is still
// running. A refusal (4xx, 5xx) is not kept: nothing changed, and the same key
// may try again (master kept only successes too). A request without a key is
// handled every time. At most limit answers are kept, the oldest dropped first.
type Replays struct {
	mu      sync.Mutex
	limit   int
	done    map[string]recorded
	order   []string
	pending map[string]chan struct{}
}

type recorded struct {
	status int
	header http.Header
	body   []byte
}

func NewReplays(limit int) *Replays {
	return &Replays{limit: limit, done: map[string]recorded{}, pending: map[string]chan struct{}{}}
}

// Wrap answers a repeated key from the record and records the first answer.
func (s *Replays) Wrap(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		key := r.Header.Get(HeaderIdempotency)
		if key == "" {
			next.ServeHTTP(w, r)
			return
		}
		if len(key) > MaxKey {
			WriteError(w, http.StatusBadRequest, "invalid_idempotency_key")
			return
		}
		entry := r.Header.Get(HeaderUser) + "\x00" + r.Method + " " + r.URL.Path + "\x00" + key
		own := make(chan struct{})
		for {
			s.mu.Lock()
			if answer, ok := s.done[entry]; ok {
				s.mu.Unlock()
				for name, values := range answer.header {
					w.Header()[name] = values
				}
				w.WriteHeader(answer.status)
				_, _ = w.Write(answer.body)
				return
			}
			wait, running := s.pending[entry]
			if !running {
				s.pending[entry] = own
				s.mu.Unlock()
				break
			}
			s.mu.Unlock()
			<-wait
		}
		recorder := &recorder{ResponseWriter: w, status: http.StatusOK}
		defer func() {
			s.mu.Lock()
			defer s.mu.Unlock()
			if recorder.status/100 == 2 {
				s.keep(entry, recorded{status: recorder.status, header: w.Header().Clone(), body: recorder.body.Bytes()})
			}
			if s.pending[entry] == own {
				delete(s.pending, entry)
			}
			close(own)
		}()
		next.ServeHTTP(recorder, r)
	})
}

func (s *Replays) keep(entry string, answer recorded) {
	s.done[entry] = answer
	s.order = append(s.order, entry)
	for len(s.order) > s.limit {
		delete(s.done, s.order[0])
		s.order = s.order[1:]
	}
}

type recorder struct {
	http.ResponseWriter
	status int
	body   bytes.Buffer
}

func (r *recorder) WriteHeader(status int) {
	r.status = status
	r.ResponseWriter.WriteHeader(status)
}

func (r *recorder) Write(b []byte) (int, error) {
	r.body.Write(b)
	return r.ResponseWriter.Write(b)
}
