package rooms

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/matches"
)

func TestAStartRetriesAnUnansweredAllocationWithTheSameKey(t *testing.T) {
	var mu sync.Mutex
	var keys []string
	allocator := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		mu.Lock()
		keys = append(keys, r.Header.Get(api.HeaderIdempotency))
		first := len(keys) == 1
		mu.Unlock()
		if first {
			// No answer: the connection drops, as a matches service that died
			// after starting the Match would.
			connection, _, _ := w.(http.Hijacker).Hijack()
			connection.Close()
			return
		}
		api.WriteJSON(w, http.StatusOK, matches.Allocation{MatchID: 3, Tickets: []matches.Ticket{{Server: "127.0.0.1:1", UserID: 1, Token: "00"}}})
	}))
	defer allocator.Close()
	service, err := New(Config{Matches: api.NewClient(allocator.URL)})
	if err != nil {
		t.Fatal(err)
	}
	handler := service.Handler()
	send := func(method, path, body string) map[string]any {
		request := httptest.NewRequest(method, path, strings.NewReader(body))
		request.Header.Set(api.HeaderUser, "1")
		recorder := httptest.NewRecorder()
		handler.ServeHTTP(recorder, request)
		var out map[string]any
		_ = json.Unmarshal(recorder.Body.Bytes(), &out)
		return out
	}
	room := send(http.MethodPost, "/internal/v1/rooms", `{"mode":"coop","map":"pvp_training_v1"}`)
	path := "/internal/v1/rooms/" + jsonID(room)
	send(http.MethodPost, path+"/ready", `{"ready":true}`)
	started := send(http.MethodPost, path+"/start", "")
	if started["state"] != Started {
		t.Fatalf("the start after one dropped allocation: %v", started)
	}
	if len(keys) != 2 || keys[0] == "" || keys[0] != keys[1] {
		t.Fatalf("the allocation's keys were %q, want the same key twice", keys)
	}
}

func jsonID(room map[string]any) string {
	encoded, _ := json.Marshal(room["id"])
	return string(encoded)
}
