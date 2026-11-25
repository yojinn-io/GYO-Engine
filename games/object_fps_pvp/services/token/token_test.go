package token

import (
	"bytes"
	"encoding/hex"
	"errors"
	"testing"
	"time"
)

var (
	testKey = bytes.Repeat([]byte{0x5a}, KeySize)
	now     = time.Unix(1_800_000_000, 0)
)

func admission() Admission {
	return Admission{MatchID: 0x0102030405060708, UserID: 42, ExpiresAt: now.Add(time.Minute)}
}

func TestAdmissionLayout(t *testing.T) {
	signed := SignAdmission(testKey, admission())
	if len(signed) != AdmissionSize {
		t.Fatalf("admission token size %d, want %d", len(signed), AdmissionSize)
	}
	want := "01" + "0102030405060708" + "000000000000002a" + hex.EncodeToString([]byte{0, 0, 0, 0, 0x6b, 0x49, 0xd2, 0x3c}) + "00000000"
	if got := hex.EncodeToString(signed[:admissionSigned]); got != want {
		t.Fatalf("admission token fields %s, want %s", got, want)
	}
}

func TestAdmissionAccepted(t *testing.T) {
	signed := SignAdmission(testKey, admission())
	if err := VerifyAdmission(testKey, signed, now, admission().MatchID, 42); err != nil {
		t.Fatalf("a fresh admission token is refused: %v", err)
	}
}

func TestAdmissionRefusals(t *testing.T) {
	a := admission()
	signed := SignAdmission(testKey, a)
	otherKey := bytes.Repeat([]byte{0x11}, KeySize)
	flipped := func(at int) []byte {
		changed := bytes.Clone(signed)
		changed[at] ^= 1
		return changed
	}
	cases := []struct {
		name  string
		key   []byte
		token []byte
		now   time.Time
		match uint64
		user  uint64
		want  error
	}{
		{"short", testKey, signed[:AdmissionSize-1], now, a.MatchID, a.UserID, ErrLength},
		{"long", testKey, append(bytes.Clone(signed), 0), now, a.MatchID, a.UserID, ErrLength},
		{"version", testKey, flipped(0), now, a.MatchID, a.UserID, ErrVersion},
		{"another key", otherKey, signed, now, a.MatchID, a.UserID, ErrMAC},
		{"changed match field", testKey, flipped(8), now, a.MatchID, a.UserID, ErrMAC},
		{"changed user field", testKey, flipped(16), now, a.MatchID, a.UserID, ErrMAC},
		{"changed expiry field", testKey, flipped(24), now, a.MatchID, a.UserID, ErrMAC},
		{"changed reserved field", testKey, flipped(28), now, a.MatchID, a.UserID, ErrMAC},
		{"changed signature", testKey, flipped(AdmissionSize - 1), now, a.MatchID, a.UserID, ErrMAC},
		{"another match", testKey, signed, now, a.MatchID + 1, a.UserID, ErrMatch},
		{"another user", testKey, signed, now, a.MatchID, a.UserID + 1, ErrUser},
		{"at expiry", testKey, signed, a.ExpiresAt, a.MatchID, a.UserID, ErrExpired},
	}
	for _, c := range cases {
		if err := VerifyAdmission(c.key, c.token, c.now, c.match, c.user); !errors.Is(err, c.want) {
			t.Errorf("admission %s: got %v, want %v", c.name, err, c.want)
		}
	}
	if err := VerifyAdmission(testKey, signed, a.ExpiresAt.Add(-time.Second), a.MatchID, a.UserID); err != nil {
		t.Errorf("admission a second before expiry: got %v, want accepted", err)
	}
}

func TestSessionRoundTrip(t *testing.T) {
	session := SignSession(testKey, 7, now.Add(time.Hour))
	user, err := VerifySession(testKey, session, now)
	if err != nil || user != 7 {
		t.Fatalf("session: user %d, %v; want 7, accepted", user, err)
	}
}

func TestSessionRefusals(t *testing.T) {
	session := SignSession(testKey, 7, now.Add(time.Hour))
	payload, signature, _ := bytesCut(session)
	other := SignSession(testKey, 8, now.Add(time.Hour))
	otherPayload, _, _ := bytesCut(other)
	cases := []struct {
		name    string
		key     []byte
		session string
		now     time.Time
		want    error
	}{
		{"no dot", testKey, payload + signature, now, ErrLength},
		{"bad base64", testKey, payload + ".!!", now, ErrLength},
		{"another key", bytes.Repeat([]byte{1}, KeySize), session, now, ErrMAC},
		{"another user's payload", testKey, otherPayload + "." + signature, now, ErrMAC},
		{"expired", testKey, session, now.Add(time.Hour), ErrExpired},
		{"version", testKey, SignSessionVersion(testKey, 2, 7, now.Add(time.Hour)), now, ErrVersion},
	}
	for _, c := range cases {
		if _, err := VerifySession(c.key, c.session, c.now); !errors.Is(err, c.want) {
			t.Errorf("session %s: got %v, want %v", c.name, err, c.want)
		}
	}
}

func bytesCut(s string) (string, string, bool) {
	for i := range s {
		if s[i] == '.' {
			return s[:i], s[i+1:], true
		}
	}
	return s, "", false
}

// SignSessionVersion is SignSession with another version byte, signed, so
// the version check is reached.
func SignSessionVersion(key []byte, version byte, userID uint64, expiresAt time.Time) string {
	payload := make([]byte, sessionSigned)
	payload[0] = version
	payload[8] = byte(userID)
	payload[16] = byte(expiresAt.Unix())
	return encoding.EncodeToString(payload) + "." + encoding.EncodeToString(mac(key, payload))
}
