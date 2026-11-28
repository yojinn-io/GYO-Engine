// Package gateway is the entry to everything but the Match (gateway
// declaration D2, D3): the external protocol's adapter. It keeps no state: it
// checks the session token (self-contained, no lookup), puts the user it names,
// that user's name and the client's idempotency key on an internal request (D4),
// sends it to the service behind and hands the answer back as it is.
package gateway

import (
	"errors"
	"io"
	"log"
	"net/http"
	"strconv"
	"strings"
	"time"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/token"
)

// ProtocolVersion is the external protocol's version, the first segment of
// every path (/v1/...). A Client of another version gets 404 for its paths.
const ProtocolVersion = 1

type Config struct {
	// The services behind it (base URLs of the internal protocol).
	Identity, Rooms, Saves string
	SessionKey             []byte
	Now                    func() time.Time
	HTTP                   *http.Client
}

type Gateway struct {
	config Config
}

func New(config Config) (*Gateway, error) {
	if len(config.SessionKey) == 0 {
		return nil, errors.New("gateway needs the session key")
	}
	if config.Now == nil {
		config.Now = time.Now
	}
	if config.HTTP == nil {
		config.HTTP = &http.Client{Timeout: 15 * time.Second}
	}
	return &Gateway{config: config}, nil
}

func (g *Gateway) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("POST /v1/session", func(w http.ResponseWriter, r *http.Request) {
		g.forward(w, r, g.config.Identity, "/internal/v1/sessions", api.Nobody)
	})
	mux.HandleFunc("GET /v1/rooms", g.signedIn(func(w http.ResponseWriter, r *http.Request, caller api.Caller) {
		g.forward(w, r, g.config.Rooms, "/internal/v1/rooms", caller)
	}))
	mux.HandleFunc("POST /v1/rooms", g.signedIn(func(w http.ResponseWriter, r *http.Request, caller api.Caller) {
		g.forward(w, r, g.config.Rooms, "/internal/v1/rooms", caller)
	}))
	mux.HandleFunc("GET /v1/rooms/{id}", g.signedIn(func(w http.ResponseWriter, r *http.Request, caller api.Caller) {
		if id, ok := roomID(w, r); ok {
			g.forward(w, r, g.config.Rooms, "/internal/v1/rooms/"+id, caller)
		}
	}))
	for _, action := range []string{"join", "leave", "ready", "start"} {
		mux.HandleFunc("POST /v1/rooms/{id}/"+action, g.signedIn(func(w http.ResponseWriter, r *http.Request, caller api.Caller) {
			if id, ok := roomID(w, r); ok {
				g.forward(w, r, g.config.Rooms, "/internal/v1/rooms/"+id+"/"+action, caller)
			}
		}))
	}
	mux.HandleFunc("GET /v1/saves/me", g.signedIn(func(w http.ResponseWriter, r *http.Request, caller api.Caller) {
		// The save service has no use for the name: only the rooms hear it (display-name declaration 6).
		g.forward(w, r, g.config.Saves, "/internal/v1/saves/"+strconv.FormatUint(caller.User, 10), api.As(caller.User))
	}))
	return unknownAsJSON(mux)
}

// Versions is what a Client of another protocol version is told.
type Versions struct {
	Error    string `json:"error"`
	Versions []int  `json:"versions"`
}

// unknownAsJSON answers what no route takes in the external protocol's form:
// a path of another version says which versions this gateway speaks
// (unsupported_version, as master's gateway stated its own version), any other
// unknown_route.
func unknownAsJSON(mux *http.ServeMux) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if _, pattern := mux.Handler(r); pattern != "" {
			mux.ServeHTTP(w, r)
			return
		}
		if !strings.HasPrefix(r.URL.Path, "/v"+strconv.Itoa(ProtocolVersion)+"/") {
			api.WriteJSON(w, http.StatusNotFound, Versions{Error: "unsupported_version", Versions: []int{ProtocolVersion}})
			return
		}
		api.WriteError(w, http.StatusNotFound, "unknown_route")
	})
}

// signedIn checks the bearer session token before the route, which gets the
// user and the name the token holds.
func (g *Gateway) signedIn(route func(http.ResponseWriter, *http.Request, api.Caller)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		session, found := strings.CutPrefix(r.Header.Get("Authorization"), "Bearer ")
		if !found {
			api.WriteError(w, http.StatusUnauthorized, "no_session")
			return
		}
		verified, err := token.VerifySession(g.config.SessionKey, session, g.config.Now())
		if err != nil {
			code := "invalid_session"
			if errors.Is(err, token.ErrExpired) {
				code = "session_expired"
			}
			api.WriteError(w, http.StatusUnauthorized, code)
			return
		}
		route(w, r, api.Caller{User: verified.UserID, Name: verified.Name})
	}
}

// roomID is the path's room id: digits only, so it cannot change the
// internal path.
func roomID(w http.ResponseWriter, r *http.Request) (string, bool) {
	id := r.PathValue("id")
	if _, err := strconv.ParseUint(id, 10, 64); err != nil {
		api.WriteError(w, http.StatusBadRequest, "invalid_room")
		return "", false
	}
	return id, true
}

// forward sends the request to a service for the caller, with the client's
// body and idempotency key, and copies the answer back.
func (g *Gateway) forward(w http.ResponseWriter, r *http.Request, base, path string, caller api.Caller) {
	body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, api.MaxBody))
	if err != nil {
		api.WriteError(w, http.StatusRequestEntityTooLarge, "body_too_large")
		return
	}
	if len(body) == 0 {
		body = nil
	}
	status, answer, err := api.Send(r.Context(), g.config.HTTP, r.Method, base+path, caller, r.Header.Get(api.HeaderIdempotency), body)
	if err != nil {
		log.Printf("gateway forward failed path=%s error=%v", path, err)
		api.WriteError(w, http.StatusBadGateway, "service_unavailable")
		return
	}
	api.WriteRaw(w, status, answer)
}
