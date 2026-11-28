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
	return loginWith(t, s, map[string]string{"deviceId": device})
}

func loginWith(t *testing.T, s *Service, request map[string]string) (int, Session) {
	t.Helper()
	body, _ := json.Marshal(request)
	recorder := httptest.NewRecorder()
	s.Handler().ServeHTTP(recorder, httptest.NewRequest(http.MethodPost, "/internal/v1/sessions", bytes.NewReader(body)))
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
	verified, err := token.VerifySession(key, session.SessionToken, now)
	if err != nil || verified != (token.Session{UserID: session.UserID}) || session.Name != "" ||
		session.ExpiresAt != now.Add(time.Hour).Unix() {
		t.Fatalf("session %+v: token says %+v, %v", session, verified, err)
	}
}

func TestTheSessionTokenHoldsTheNormalizedName(t *testing.T) {
	now := time.Unix(1_900_000_000, 0)
	s, _ := New(Config{SessionKey: key, Now: func() time.Time { return now }})
	_, session := loginWith(t, s, map[string]string{"deviceId": "device", "name": "  Kuro\u202e  の\tMac\n "})
	verified, err := token.VerifySession(key, session.SessionToken, now)
	if err != nil || session.Name != "Kuro の Mac" || verified != (token.Session{UserID: session.UserID, Name: session.Name}) {
		t.Fatalf("session %+v: token says %+v, %v; want the name Kuro の Mac in both", session, verified, err)
	}
	// The name is not kept: the same device signs in with another one, and that is the one it gets.
	_, again := loginWith(t, s, map[string]string{"deviceId": "device", "name": "Other"})
	if again.UserID != session.UserID || again.Name != "Other" {
		t.Fatalf("the same device signed in again as %+v, want user %d named Other", again, session.UserID)
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
