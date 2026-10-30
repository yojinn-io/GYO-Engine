package gateway

import (
	"bytes"
	"math"
	"net/http"
	"testing"

	"gyo.local/object_fps_pvp/gateway/adapter"
)

// protocol-v8 §2a: player ids end at the uint32 maximum although the
// Gateway counts them in uint64; the last id is still handed out.
func TestPlayerIDsEndAtTheWireCounterMaximum(t *testing.T) {
	s, _ := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != http.StatusOK {
		t.Fatal(status)
	}
	s.mu.Lock()
	s.nextPlayer = math.MaxUint32 - 1
	s.mu.Unlock()
	if last := reserve(t, s, "last"); last.PlayerID != math.MaxUint32 {
		t.Fatalf("last player id %d", last.PlayerID)
	}
	status, body := post(t, s, "/rooms/1/join", map[string]any{"request_id": "beyond", "protocol_version": adapter.ClientVersion})
	if status != http.StatusServiceUnavailable || !bytes.Contains(body, []byte("player_ids_exhausted")) {
		t.Fatalf("beyond the maximum %d %s", status, body)
	}
}
