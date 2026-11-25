// Package stack starts every Object_FPS_PVP service in one process (gateway
// declaration D2): each on a listener of its own, talking to the others over
// the internal protocol as when they run apart. For local play and the tests.
package stack

import (
	"context"
	"errors"
	"net"
	"net/http"
	"strings"
	"time"

	"gyo.local/object_fps_pvp/services/gateway"
	"gyo.local/object_fps_pvp/services/identity"
	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/matches"
	"gyo.local/object_fps_pvp/services/rooms"
	"gyo.local/object_fps_pvp/services/saves"
	"gyo.local/object_fps_pvp/services/token"
)

type Config struct {
	// Gateway is the external address (host:port; port 0: any free one); the
	// internal services listen on 127.0.0.1, ports chosen by the OS.
	Gateway  string
	Data     string // empty: memory only
	Provider matches.Provider
	Capacity int
	Maps     []string
	Now      func() time.Time
}

type Stack struct {
	GatewayURL, IdentityURL, RoomsURL, MatchesURL, SavesURL string
	// The services, for tests that look inside.
	Matches *matches.Service
	servers []*http.Server
	closed  chan struct{}
}

// Start listens on every address before it returns, so the URLs answer at
// once.
func Start(config Config) (*Stack, error) {
	key, err := token.NewKey()
	if err != nil {
		return nil, err
	}
	s := &Stack{closed: make(chan struct{})}
	serve := func(address string, handler http.Handler) (string, error) {
		listener, err := net.Listen("tcp", address)
		if err != nil {
			return "", err
		}
		server := &http.Server{Handler: handler, ReadHeaderTimeout: 5 * time.Second}
		s.servers = append(s.servers, server)
		go func() { _ = server.Serve(listener) }()
		return "http://" + listener.Addr().String(), nil
	}
	fail := func(err error) (*Stack, error) {
		s.Close()
		return nil, err
	}
	identityService, err := identity.New(identity.Config{Data: config.Data, SessionKey: key, Now: config.Now})
	if err != nil {
		return fail(err)
	}
	if s.IdentityURL, err = serve("127.0.0.1:0", identityService.Handler()); err != nil {
		return fail(err)
	}
	if s.SavesURL, err = serve("127.0.0.1:0", saves.New(saves.Config{Data: config.Data}).Handler()); err != nil {
		return fail(err)
	}
	if s.Matches, err = matches.New(matches.Config{Provider: config.Provider, Saves: strings.TrimPrefix(s.SavesURL, "http://"),
		Now: config.Now}); err != nil {
		return fail(err)
	}
	if s.MatchesURL, err = serve("127.0.0.1:0", s.Matches.Handler()); err != nil {
		return fail(err)
	}
	roomService, err := rooms.New(rooms.Config{Matches: api.NewClient(s.MatchesURL), Capacity: config.Capacity,
		Maps: config.Maps, Now: config.Now})
	if err != nil {
		return fail(err)
	}
	if s.RoomsURL, err = serve("127.0.0.1:0", roomService.Handler()); err != nil {
		return fail(err)
	}
	front, err := gateway.New(gateway.Config{Identity: s.IdentityURL, Rooms: s.RoomsURL, Saves: s.SavesURL,
		SessionKey: key, Now: config.Now})
	if err != nil {
		return fail(err)
	}
	address := config.Gateway
	if address == "" {
		address = "127.0.0.1:0"
	}
	if s.GatewayURL, err = serve(address, front.Handler()); err != nil {
		return fail(err)
	}
	return s, nil
}

// Close stops every listener; a Match already started goes on.
func (s *Stack) Close() {
	select {
	case <-s.closed:
		return
	default:
		close(s.closed)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	var errs []error
	for _, server := range s.servers {
		errs = append(errs, server.Shutdown(ctx))
	}
	_ = errors.Join(errs...)
}
