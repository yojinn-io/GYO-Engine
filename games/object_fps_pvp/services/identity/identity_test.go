package identity

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"gyo.local/object_fps_pvp/services/token"
)

var key = bytes.Repeat([]byte{3}, token.KeySize)

func login(t *testing.T, s *Service, device string) (int, Session) {
	t.Helper()
	recorder := httptest.NewRecorder()
	s.Handler().ServeHTTP(recorder, httptest.NewRequest(http.MethodPost, "/internal/v1/sessions",
		strings.NewReader(`{"deviceId":"`+device+`"}`)))
	var session Session
	_ = json.Unmarshal(recorder.Body.Bytes(), &session)
	return recorder.Code, session
}

func TestADeviceKeepsItsUserAcrossARestart(t *testing.T) {
	data := t.TempDir()
	first, _ := New(Config{Data: data, SessionKey: key})
	_, a := login(t, first, "device-a")
	_, b := login(t, first, "device-b")
	restarted, err := New(Config{Data: data, SessionKey: key})
	if err != nil {
		t.Fatal(err)
	}
	_, again := login(t, restarted, "device-a")
	_, c := login(t, restarted, "device-c")
	if a.UserID == b.UserID || again.UserID != a.UserID || c.UserID == a.UserID || c.UserID == b.UserID {
		t.Fatalf("users a=%d b=%d a-after-restart=%d c=%d", a.UserID, b.UserID, again.UserID, c.UserID)
	}
}

func TestTheSessionTokenNamesTheUser(t *testing.T) {
	now := time.Unix(1_900_000_000, 0)
	s, _ := New(Config{SessionKey: key, SessionTTL: time.Hour, Now: func() time.Time { return now }})
	_, session := login(t, s, "device")
	user, err := token.VerifySession(key, session.SessionToken, now)
	if err != nil || user != session.UserID || session.ExpiresAt != now.Add(time.Hour).Unix() {
		t.Fatalf("session %+v: user %d, %v", session, user, err)
	}
}

func TestABadDeviceIsRefused(t *testing.T) {
	s, _ := New(Config{SessionKey: key})
	for _, device := range []string{"", "has space", strings.Repeat("x", 129)} {
		if status, _ := login(t, s, device); status != http.StatusBadRequest {
			t.Errorf("device %q answered %d", device, status)
		}
	}
}
