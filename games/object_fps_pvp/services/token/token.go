// Package token signs and verifies the two self-contained tokens of the
// Object_FPS_PVP services (gateway declaration D5): the session token the
// gateway checks and the admission token a Match checks. Both are verified
// without asking anyone: an HMAC-SHA256 over their fields, an expiry.
package token

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"errors"
	"strings"
	"time"
)

// KeySize is the size of the keys NewKey draws.
const KeySize = 32

// NewKey draws a random key.
func NewKey() ([]byte, error) {
	key := make([]byte, KeySize)
	if _, err := rand.Read(key); err != nil {
		return nil, err
	}
	return key, nil
}

// What a token's check found (each refusal says which field failed).
var (
	ErrLength  = errors.New("token length")
	ErrVersion = errors.New("token version")
	ErrMAC     = errors.New("token signature")
	ErrMatch   = errors.New("token match")
	ErrUser    = errors.New("token user")
	ErrExpired = errors.New("token expired")
)

// The admission token's layout (declaration D5), integers big-endian:
// version u8 | matchId u64 | userId u64 | expiresAt u64 (Unix seconds) |
// mac [32] = HMAC-SHA256(key, the 25 bytes before it). A new field is a new
// version, which a Match that does not know it refuses.
const (
	AdmissionVersion = 1
	admissionSigned  = 1 + 8 + 8 + 8
	AdmissionSize    = admissionSigned + sha256.Size
)

// Admission is what an admission token says: this user may join this Match
// until ExpiresAt.
type Admission struct {
	MatchID   uint64
	UserID    uint64
	ExpiresAt time.Time
}

// SignAdmission makes the admission token with the Match's key.
func SignAdmission(key []byte, a Admission) []byte {
	out := make([]byte, admissionSigned, AdmissionSize)
	out[0] = AdmissionVersion
	binary.BigEndian.PutUint64(out[1:], a.MatchID)
	binary.BigEndian.PutUint64(out[9:], a.UserID)
	binary.BigEndian.PutUint64(out[17:], uint64(a.ExpiresAt.Unix()))
	return append(out, mac(key, out)...)
}

// VerifyAdmission is the Match's check (C++ games/object_fps_pvp does the
// same; tests compare both on the same vectors): the layout, the signature,
// then the match, the user and the expiry, in that order.
func VerifyAdmission(key, token []byte, now time.Time, matchID, userID uint64) error {
	if len(token) != AdmissionSize {
		return ErrLength
	}
	if token[0] != AdmissionVersion {
		return ErrVersion
	}
	if !hmac.Equal(token[admissionSigned:], mac(key, token[:admissionSigned])) {
		return ErrMAC
	}
	if binary.BigEndian.Uint64(token[1:]) != matchID {
		return ErrMatch
	}
	if binary.BigEndian.Uint64(token[9:]) != userID {
		return ErrUser
	}
	if now.Unix() >= int64(binary.BigEndian.Uint64(token[17:])) {
		return ErrExpired
	}
	return nil
}

// The session token (display-name declaration 5.1): base64url(version u8 |
// userId u64 | expiresAt u64 | nameLen u8 | name) "." base64url(HMAC-SHA256(
// key, those bytes)). The name is inside the signature: the gateway hands it on
// as the user's own.
const (
	SessionVersion = 2
	sessionFixed   = 1 + 8 + 8 + 1
	// MaxSessionName is the longest name (bytes) a session token holds.
	MaxSessionName = 255
)

var encoding = base64.RawURLEncoding

// Session is what a session token says: who the user is and the name they
// signed in with (empty: none).
type Session struct {
	UserID uint64
	Name   string
}

// SignSession makes a session token with the identity key. A name longer than
// MaxSessionName is a program error.
func SignSession(key []byte, session Session, expiresAt time.Time) string {
	if len(session.Name) > MaxSessionName {
		panic("token: session name longer than MaxSessionName")
	}
	payload := make([]byte, sessionFixed, sessionFixed+len(session.Name))
	payload[0] = SessionVersion
	binary.BigEndian.PutUint64(payload[1:], session.UserID)
	binary.BigEndian.PutUint64(payload[9:], uint64(expiresAt.Unix()))
	payload[17] = byte(len(session.Name))
	payload = append(payload, session.Name...)
	return encoding.EncodeToString(payload) + "." + encoding.EncodeToString(mac(key, payload))
}

// VerifySession returns what a session token says, or why it is refused: the
// layout, the version, the signature, then the expiry.
func VerifySession(key []byte, session string, now time.Time) (Session, error) {
	encodedPayload, encodedMAC, found := strings.Cut(session, ".")
	if !found {
		return Session{}, ErrLength
	}
	payload, err := encoding.DecodeString(encodedPayload)
	if err != nil || len(payload) == 0 {
		return Session{}, ErrLength
	}
	signature, err := encoding.DecodeString(encodedMAC)
	if err != nil || len(signature) != sha256.Size {
		return Session{}, ErrLength
	}
	if payload[0] != SessionVersion {
		return Session{}, ErrVersion
	}
	if len(payload) < sessionFixed || int(payload[17]) != len(payload)-sessionFixed {
		return Session{}, ErrLength
	}
	if !hmac.Equal(signature, mac(key, payload)) {
		return Session{}, ErrMAC
	}
	if now.Unix() >= int64(binary.BigEndian.Uint64(payload[9:])) {
		return Session{}, ErrExpired
	}
	return Session{UserID: binary.BigEndian.Uint64(payload[1:]), Name: string(payload[sessionFixed:])}, nil
}

func mac(key, message []byte) []byte {
	h := hmac.New(sha256.New, key)
	h.Write(message)
	return h.Sum(nil)
}
