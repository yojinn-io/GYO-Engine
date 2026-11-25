package token

import (
	"bytes"
	"encoding/hex"
	"fmt"
	"os"
	"strings"
	"testing"
	"time"
)

// The admission vectors both ends check (the Match's C++ check reads the same
// file, tests/object_fps_pvp/AdmissionTokenTests.cpp). GYO_UPDATE_VECTORS=1
// writes the file again.
const vectorsPath = "testdata/admission_vectors.txt"

var refusalNames = map[error]string{nil: "accepted", ErrLength: "length", ErrVersion: "version", ErrMAC: "signature",
	ErrMatch: "match", ErrUser: "user", ErrExpired: "expired"}

func vectors() string {
	key := bytes.Repeat([]byte{0xa7}, KeySize)
	longKey := bytes.Repeat([]byte{0x3c}, 100) // longer than a SHA-256 block: HMAC hashes it first
	expires := time.Unix(1_800_000_600, 0)
	a := Admission{MatchID: 0x8877665544332211, UserID: 9, ExpiresAt: expires}
	signed := SignAdmission(key, a)
	flip := func(at int) []byte {
		changed := bytes.Clone(signed)
		changed[at] ^= 0x80
		return changed
	}
	type vector struct {
		name  string
		key   []byte
		token []byte
		now   int64
		match uint64
		user  uint64
	}
	now := expires.Unix() - 1
	list := []vector{
		{"accepted", key, signed, now, a.MatchID, a.UserID},
		{"long_key", longKey, SignAdmission(longKey, a), now, a.MatchID, a.UserID},
		{"short", key, signed[:AdmissionSize-1], now, a.MatchID, a.UserID},
		{"version", key, flip(0), now, a.MatchID, a.UserID},
		{"another_key", bytes.Repeat([]byte{0xa6}, KeySize), signed, now, a.MatchID, a.UserID},
		{"changed_user_field", key, flip(16), now, a.MatchID, a.UserID},
		{"changed_reserved_field", key, flip(26), now, a.MatchID, a.UserID},
		{"changed_mac", key, flip(AdmissionSize - 1), now, a.MatchID, a.UserID},
		{"another_match", key, signed, now, a.MatchID ^ 1, a.UserID},
		{"another_user", key, signed, now, a.MatchID, a.UserID + 1},
		{"at_expiry", key, signed, expires.Unix(), a.MatchID, a.UserID},
	}
	var out strings.Builder
	out.WriteString("# name key token now match user expected (written by services/token/vectors_test.go)\n")
	for _, v := range list {
		result := VerifyAdmission(v.key, v.token, time.Unix(v.now, 0), v.match, v.user)
		fmt.Fprintf(&out, "%s %s %s %d %d %d %s\n", v.name, hex.EncodeToString(v.key), hex.EncodeToString(v.token),
			v.now, v.match, v.user, refusalNames[result])
	}
	return out.String()
}

func TestAdmissionVectorsFile(t *testing.T) {
	want := vectors()
	if os.Getenv("GYO_UPDATE_VECTORS") != "" {
		if err := os.WriteFile(vectorsPath, []byte(want), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	got, err := os.ReadFile(vectorsPath)
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != want {
		t.Fatalf("%s is not what the Go check gives (GYO_UPDATE_VECTORS=1 writes it):\n%s", vectorsPath, want)
	}
	for _, name := range []string{"accepted", "length", "version", "signature", "match", "user", "expired"} {
		if !strings.Contains(want, " "+name+"\n") {
			t.Errorf("no vector ends in %s", name)
		}
	}
}
