package gateway

import (
	"bytes"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/token"
)

// The headers each internal request carried.
func recording(t *testing.T) (*httptest.Server, *[]http.Header) {
	t.Helper()
	var seen []http.Header
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		seen = append(seen, r.Header.Clone())
		api.WriteJSON(w, http.StatusOK, map[string]bool{"ok": true})
	}))
	t.Cleanup(server.Close)
	return server, &seen
}

func TestTheGatewayCarriesWhoAndTheKey(t *testing.T) {
	key := bytes.Repeat([]byte{9}, token.KeySize)
	service, seen := recording(t)
	front, _ := New(Config{Identity: service.URL, Rooms: service.URL, Saves: service.URL, SessionKey: key})
	handler := front.Handler()

	login := httptest.NewRequest(http.MethodPost, "/v1/session", strings.NewReader(`{"deviceId":"d"}`))
	login.Header.Set(api.HeaderUser, "0") // a Client's own header is never forwarded
	handler.ServeHTTP(httptest.NewRecorder(), login)

	create := httptest.NewRequest(http.MethodPost, "/v1/rooms", strings.NewReader(`{}`))
	create.Header.Set("Authorization", "Bearer "+token.SignSession(key, 7, time.Now().Add(time.Hour)))
	create.Header.Set(api.HeaderIdempotency, "k-1")
	create.Header.Set(api.HeaderUser, "99")
	handler.ServeHTTP(httptest.NewRecorder(), create)

	if len(*seen) != 2 {
		t.Fatalf("internal requests %d, want 2", len(*seen))
	}
	if users := (*seen)[0].Values(api.HeaderUser); len(users) != 0 {
		t.Fatalf("the sign-in carried a user %v: an anonymous request has none", users)
	}
	if got := (*seen)[1].Get(api.HeaderUser); got != "7" {
		t.Fatalf("the room request carried user %q, want the session's 7", got)
	}
	if got := (*seen)[1].Get(api.HeaderIdempotency); got != "k-1" {
		t.Fatalf("the room request carried key %q, want the Client's k-1", got)
	}
}
