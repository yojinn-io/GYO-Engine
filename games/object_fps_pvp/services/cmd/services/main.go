// Command services runs the Object_FPS_PVP services (gateway declaration D2):
//
//	services all      --gateway ADDR --data DIR --match PATH [...]   every service in this process
//	services gateway  --listen ADDR --identity URL --rooms URL --saves URL
//	services identity --listen ADDR --data DIR
//	services rooms    --listen ADDR --matches URL
//	services matches  --listen ADDR --match PATH [...]
//	services saves    --listen ADDR --data DIR
//
// The session key (shared by identity and gateway) comes from GYO_PVP_SESSION_KEY
// (hex); "all" draws its own.
package main

import (
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"time"

	"gyo.local/object_fps_pvp/services/gateway"
	"gyo.local/object_fps_pvp/services/identity"
	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/matches"
	"gyo.local/object_fps_pvp/services/rooms"
	"gyo.local/object_fps_pvp/services/saves"
	"gyo.local/object_fps_pvp/services/stack"
)

const sessionKeyVariable = "GYO_PVP_SESSION_KEY"

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: services all|gateway|identity|rooms|matches|saves [flags]")
		os.Exit(2)
	}
	if err := run(os.Args[1], os.Args[2:]); err != nil {
		log.Fatal(err)
	}
}

// matchFlags are the local Match provider's settings (D6).
type matchFlags struct {
	command, content, advertise string
	firstPort, lastPort         int
}

func (m *matchFlags) register(set *flag.FlagSet) {
	set.StringVar(&m.command, "match", "", "the Match executable (gyo_object_fps_pvp-match)")
	set.StringVar(&m.content, "content", "", "the content directory the Match reads (--content)")
	set.StringVar(&m.advertise, "advertise", "127.0.0.1", "the IPv4 address players connect to")
	set.IntVar(&m.firstPort, "first-port", 7800, "the first Match port")
	set.IntVar(&m.lastPort, "last-port", 7899, "the last Match port")
}

func (m *matchFlags) provider() (matches.Provider, error) {
	if m.command == "" {
		return nil, errors.New("--match is required")
	}
	local := &matches.Local{Command: m.command, Advertise: m.advertise, FirstPort: m.firstPort, LastPort: m.lastPort}
	if m.content != "" {
		local.Arguments = []string{"--content", m.content}
	}
	return local, nil
}

func sessionKey() ([]byte, error) {
	if value := os.Getenv(sessionKeyVariable); value != "" {
		return hex.DecodeString(value)
	}
	return nil, errors.New(sessionKeyVariable + " is required")
}

func run(name string, arguments []string) error {
	set := flag.NewFlagSet(name, flag.ExitOnError)
	listen := set.String("listen", "127.0.0.1:0", "the address to listen on")
	data := set.String("data", "", "the data directory (identity.json, saves/)")
	capacity := set.Int("capacity", 4, "the players a room and its Match hold")
	maps := set.String("maps", "", "the maps rooms may choose, comma-separated (empty: any)")
	var match matchFlags
	switch name {
	case "all":
		gatewayAddress := set.String("gateway", "127.0.0.1:7700", "the gateway's address")
		match.register(set)
		_ = set.Parse(arguments)
		provider, err := match.provider()
		if err != nil {
			return err
		}
		running, err := stack.Start(stack.Config{Gateway: *gatewayAddress, Data: *data, Provider: provider,
			Capacity: *capacity, Maps: split(*maps)})
		if err != nil {
			return err
		}
		defer running.Close()
		log.Printf("services gateway=%s", running.GatewayURL)
		wait()
		return nil
	case "gateway":
		identityURL := set.String("identity", "", "the identity service's URL")
		roomsURL := set.String("rooms", "", "the room service's URL")
		savesURL := set.String("saves", "", "the save service's URL")
		_ = set.Parse(arguments)
		key, err := sessionKey()
		if err != nil {
			return err
		}
		front, err := gateway.New(gateway.Config{Identity: *identityURL, Rooms: *roomsURL, Saves: *savesURL, SessionKey: key})
		if err != nil {
			return err
		}
		return serve(*listen, front.Handler())
	case "identity":
		_ = set.Parse(arguments)
		key, err := sessionKey()
		if err != nil {
			return err
		}
		service, err := identity.New(identity.Config{Data: *data, SessionKey: key})
		if err != nil {
			return err
		}
		return serve(*listen, service.Handler())
	case "rooms":
		matchesURL := set.String("matches", "", "the matches service's URL")
		_ = set.Parse(arguments)
		service, err := rooms.New(rooms.Config{Matches: api.NewClient(*matchesURL), Capacity: *capacity, Maps: split(*maps)})
		if err != nil {
			return err
		}
		return serve(*listen, service.Handler())
	case "matches":
		match.register(set)
		_ = set.Parse(arguments)
		provider, err := match.provider()
		if err != nil {
			return err
		}
		service, err := matches.New(matches.Config{Provider: provider})
		if err != nil {
			return err
		}
		return serve(*listen, service.Handler())
	case "saves":
		_ = set.Parse(arguments)
		return serve(*listen, saves.New(saves.Config{Data: *data}).Handler())
	}
	return fmt.Errorf("unknown service %q", name)
}

func serve(address string, handler http.Handler) error {
	server := &http.Server{Addr: address, Handler: handler, ReadHeaderTimeout: 5 * time.Second}
	log.Printf("listening %s", address)
	return server.ListenAndServe()
}

func split(list string) []string {
	if list == "" {
		return nil
	}
	return strings.Split(list, ",")
}

func wait() {
	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt)
	<-signals
}
