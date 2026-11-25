// Package api holds what every Object_FPS_PVP service shares: the internal
// protocol's headers, JSON bodies and errors (gateway declaration D3, D4).
package api

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"strconv"
)

// The internal protocol's headers (declaration D4). HeaderUser is the user the
// gateway verified (0: the system, such as a Match); HeaderIdempotency is the
// caller's key for a request that changes state.
const (
	HeaderUser        = "X-GYO-User"
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
