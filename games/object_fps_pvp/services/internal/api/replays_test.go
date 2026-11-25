package api

import (
	"net/http"
	"net/http/httptest"
	"strconv"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

func TestConcurrentRequestsWithOneKeyRunOnce(t *testing.T) {
	var runs atomic.Int32
	handler := NewReplays(16).Wrap(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
		n := runs.Add(1)
		time.Sleep(20 * time.Millisecond)
		WriteJSON(w, http.StatusOK, map[string]int32{"run": n})
	}))
	var wg sync.WaitGroup
	bodies := make([]string, 8)
	for i := range bodies {
		wg.Add(1)
		go func() {
			defer wg.Done()
			request := httptest.NewRequest(http.MethodPost, "/x", nil)
			request.Header.Set(HeaderUser, "1")
			request.Header.Set(HeaderIdempotency, "k")
			recorder := httptest.NewRecorder()
			handler.ServeHTTP(recorder, request)
			bodies[i] = recorder.Body.String()
		}()
	}
	wg.Wait()
	if runs.Load() != 1 {
		t.Fatalf("one key ran the handler %d times", runs.Load())
	}
	for _, body := range bodies {
		if body != bodies[0] {
			t.Fatalf("answers differ: %q and %q", body, bodies[0])
		}
	}
}

func TestServerErrorsAreNotKept(t *testing.T) {
	var runs atomic.Int32
	handler := NewReplays(16).Wrap(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
		runs.Add(1)
		WriteError(w, http.StatusServiceUnavailable, "busy")
	}))
	for range 2 {
		request := httptest.NewRequest(http.MethodPost, "/x", nil)
		request.Header.Set(HeaderIdempotency, "k")
		handler.ServeHTTP(httptest.NewRecorder(), request)
	}
	if runs.Load() != 2 {
		t.Fatalf("a 503 was replayed instead of retried: %d runs", runs.Load())
	}
}

func TestTheOldestAnswerIsDroppedPastTheLimit(t *testing.T) {
	var runs atomic.Int32
	handler := NewReplays(2).Wrap(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
		WriteJSON(w, http.StatusOK, runs.Add(1))
	}))
	send := func(key string) string {
		request := httptest.NewRequest(http.MethodPost, "/x", nil)
		request.Header.Set(HeaderIdempotency, key)
		recorder := httptest.NewRecorder()
		handler.ServeHTTP(recorder, request)
		return recorder.Body.String()
	}
	for i := range 3 {
		send(strconv.Itoa(i))
	}
	if got := send("2"); got != "3" {
		t.Fatalf("a kept key answered %s, want 3", got)
	}
	if got := send("0"); got != "4" {
		t.Fatalf("a dropped key answered %s, want a new run (4)", got)
	}
}
