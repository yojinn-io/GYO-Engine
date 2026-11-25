package saves

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func call(t *testing.T, handler http.Handler, method, path, key, body string) (int, map[string]any) {
	t.Helper()
	request := httptest.NewRequest(method, path, strings.NewReader(body))
	request.Header.Set("X-GYO-User", "0")
	if key != "" {
		request.Header.Set("Idempotency-Key", key)
	}
	recorder := httptest.NewRecorder()
	handler.ServeHTTP(recorder, request)
	var out map[string]any
	_ = json.Unmarshal(recorder.Body.Bytes(), &out)
	return recorder.Code, out
}

func TestANeverWrittenSaveIsVersionZero(t *testing.T) {
	status, save := call(t, New(Config{}).Handler(), http.MethodGet, "/internal/v1/saves/5", "", "")
	if status != http.StatusOK || save["version"].(float64) != 0 || save["body"] != nil {
		t.Fatalf("a new user's save: %d %v", status, save)
	}
}

func TestAWriteOfAnOlderVersionIsRefused(t *testing.T) {
	handler := New(Config{}).Handler()
	if status, _ := call(t, handler, http.MethodPut, "/internal/v1/saves/5", "", `{"version":0,"body":{"kills":1}}`); status != http.StatusOK {
		t.Fatalf("the first write: %d", status)
	}
	status, out := call(t, handler, http.MethodPut, "/internal/v1/saves/5", "", `{"version":0,"body":{"kills":2}}`)
	if status != http.StatusConflict || out["error"] != "version_conflict" || out["version"].(float64) != 1 {
		t.Fatalf("a write over version 1 that read version 0: %d %v", status, out)
	}
	_, save := call(t, handler, http.MethodGet, "/internal/v1/saves/5", "", "")
	if save["body"].(map[string]any)["kills"].(float64) != 1 {
		t.Fatalf("the refused write changed the save: %v", save)
	}
}

func TestARepeatedWriteKeyWritesOnce(t *testing.T) {
	handler := New(Config{}).Handler()
	body := `{"version":0,"body":{"kills":1}}`
	_, first := call(t, handler, http.MethodPut, "/internal/v1/saves/5", "match-1-user-5-leave-1", body)
	status, again := call(t, handler, http.MethodPut, "/internal/v1/saves/5", "match-1-user-5-leave-1", body)
	if status != http.StatusOK || again["version"] != first["version"] {
		t.Fatalf("the repeated write: %d %v, want the first answer %v", status, again, first)
	}
	_, save := call(t, handler, http.MethodGet, "/internal/v1/saves/5", "", "")
	if save["version"].(float64) != 1 {
		t.Fatalf("a repeated key wrote twice: version %v", save["version"])
	}
}

func TestSavesOutliveTheService(t *testing.T) {
	data := t.TempDir()
	call(t, New(Config{Data: data}).Handler(), http.MethodPut, "/internal/v1/saves/9", "", `{"version":0,"body":{"matches":4}}`)
	_, save := call(t, New(Config{Data: data}).Handler(), http.MethodGet, "/internal/v1/saves/9", "", "")
	if save["version"].(float64) != 1 || save["body"].(map[string]any)["matches"].(float64) != 4 {
		t.Fatalf("after a restart the save is %v", save)
	}
}

func TestABadSaveIsRefused(t *testing.T) {
	handler := New(Config{}).Handler()
	for _, body := range []string{``, `{"version":0}`, `{"version":0,"body":{}, "x":1}`, `not json`} {
		if status, out := call(t, handler, http.MethodPut, "/internal/v1/saves/5", "", body); status != http.StatusBadRequest || out["error"] != "invalid_save" {
			t.Errorf("save body %q: %d %v", body, status, out)
		}
	}
	if status, _ := call(t, handler, http.MethodGet, "/internal/v1/saves/0", "", ""); status != http.StatusBadRequest {
		t.Errorf("user 0's save answered %d", status)
	}
}
