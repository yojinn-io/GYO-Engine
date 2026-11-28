// Package api holds what every Object_FPS_PVP service shares: the internal
// protocol's headers, JSON bodies and errors (gateway declaration D3, D4).
package api

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"strconv"
)

// The internal protocol's headers (declaration D4). HeaderUser is the user the
// gateway verified (0: the system, such as a Match); HeaderName is the name
// that user signed in with, base64url without padding (a header holds ASCII
// only; display-name declaration 5.1), absent when they have none;
// HeaderIdempotency is the caller's key for a request that changes state.
const (
	HeaderUser        = "X-GYO-User"
	HeaderName        = "X-GYO-Name"
	HeaderIdempotency = "Idempotency-Key"
)

// MaxBody is the largest request body a service reads by default (master's
// gateway took 4096 bytes, apps/object_fps_pvp/gateway/server.go:229).
const MaxBody = 4096

// ErrBody is a request body that is not one JSON object of the expected shape.
var ErrBody = errors.New("invalid body")

// Decode reads one JSON value of v's shape from the request: at most limit
// bytes, no unknown fields, nothing after it. An empty body decodes as {}.
func Decode(w http.ResponseWriter, r *http.Request, limit int64, v any) error {
	body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, limit))
	if err != nil {
		return ErrBody
	}
	if len(bytes.TrimSpace(body)) == 0 {
		body = []byte("{}")
	}
	decoder := json.NewDecoder(bytes.NewReader(body))
	decoder.DisallowUnknownFields()
	if decoder.Decode(v) != nil {
		return ErrBody
	}
	if decoder.Decode(new(any)) != io.EOF {
		return ErrBody
	}
	return nil
}

// WriteJSON writes value with its length, so a client that reads by
// Content-Length never meets a chunked body.
func WriteJSON(w http.ResponseWriter, status int, value any) {
	body, err := json.Marshal(value)
	if err != nil {
		status, body = http.StatusInternalServerError, []byte(`{"error":"encode_failed"}`)
	}
	WriteRaw(w, status, body)
}

// WriteRaw writes a JSON body that is already encoded.
func WriteRaw(w http.ResponseWriter, status int, body []byte) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Content-Length", strconv.Itoa(len(body)))
	w.WriteHeader(status)
	_, _ = w.Write(body)
}

// Error is the body of every refusal: what happened, not what to do about it
// (AGENTS §1-9).
type Error struct {
	Error string `json:"error"`
}

func WriteError(w http.ResponseWriter, status int, code string) {
	WriteJSON(w, status, Error{Error: code})
}

// UserOf reads the internal protocol's user header; false when it is missing
// or not a number.
func UserOf(r *http.Request) (uint64, bool) {
	value := r.Header.Get(HeaderUser)
	if value == "" {
		return 0, false
	}
	user, err := strconv.ParseUint(value, 10, 64)
	return user, err == nil
}

// What CallerOf found wrong: no user header (or not a number), or a name
// header that is not base64url (only the gateway writes it: a fault of the
// internal protocol, not of the player).
var (
	ErrNoUser = errors.New("no user header")
	ErrName   = errors.New("name header not base64url")
)

// CallerOf reads who an internal request is for, the other end of Send: the
// user header and the name header (missing: no name).
func CallerOf(r *http.Request) (Caller, error) {
	user, ok := UserOf(r)
	if !ok {
		return Caller{}, ErrNoUser
	}
	name, err := base64.RawURLEncoding.DecodeString(r.Header.Get(HeaderName))
	if err != nil {
		return Caller{}, ErrName
	}
	return Caller{User: user, Name: string(name)}, nil
}
