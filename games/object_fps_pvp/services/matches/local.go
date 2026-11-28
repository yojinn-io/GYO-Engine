package matches

import (
	"context"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"sync"
)

// Local starts each Match as a process on this machine (D6), on a free port
// of [FirstPort, LastPort]; the port is free again when the process ends.
type Local struct {
	// Command is the Match executable; Arguments come before the ones Local
	// adds (--port, --match-id, --capacity, --arena, --saves,
	// --admission-key-stdin, --idle-seconds, --expect-users,
	// --player-names-stdin). The key goes to the Match's standard input, in
	// neither its command line nor its environment; so do the players' names,
	// on its second line (display-name declaration 5.1).
	Command   string
	Arguments []string
	// Advertise is the address players are told to connect to (an IPv4
	// literal; the Match listens on every interface).
	Advertise string
	FirstPort int
	LastPort  int
	// Environment is added to this process's environment for each Match.
	Environment []string
	// IdleSeconds: a Match ends once nobody has been in it this long (from
	// its start, or since the last player left; a player who drops may come
	// back within it). Zero: 60.
	IdleSeconds int
	// Output receives the Match's standard output and error (nil: this
	// process's standard output).
	Output io.Writer

	mu    sync.Mutex
	inUse map[int]bool
}

var ErrNoPort = errors.New("no free Match port")

func (l *Local) Start(ctx context.Context, spec Spec) (Running, error) {
	idle := l.IdleSeconds
	if idle <= 0 {
		idle = 60
	}
	port, err := l.takePort()
	if err != nil {
		return Running{}, err
	}
	arguments := append(append([]string{}, l.Arguments...),
		"--port", strconv.Itoa(port),
		"--match-id", strconv.FormatUint(spec.MatchID, 10),
		"--capacity", strconv.Itoa(spec.Capacity),
		"--admission-key-stdin", "--idle-seconds", strconv.Itoa(idle))
	if spec.Map != "" {
		arguments = append(arguments, "--arena", spec.Map)
	}
	if spec.Mode != "" {
		arguments = append(arguments, "--mode", spec.Mode)
	}
	if spec.Saves != "" {
		arguments = append(arguments, "--saves", spec.Saves)
	}
	if len(spec.Players) > 0 {
		users := make([]string, len(spec.Players))
		for j, player := range spec.Players {
			users[j] = strconv.FormatUint(player.UserID, 10)
		}
		arguments = append(arguments, "--expect-users", strings.Join(users, ","))
	}
	input := hex.EncodeToString(spec.Key) + "\n"
	if roster, named := names(spec.Players); named {
		arguments = append(arguments, "--player-names-stdin")
		input += roster + "\n"
	}
	// Not CommandContext: the Match outlives the allocation request.
	command := exec.Command(l.Command, arguments...)
	command.Env = append(os.Environ(), l.Environment...)
	command.Stdin = strings.NewReader(input)
	var output io.Writer = os.Stdout
	if l.Output != nil {
		output = l.Output
	}
	command.Stdout, command.Stderr = output, output
	if err := command.Start(); err != nil {
		l.givePort(port)
		return Running{}, fmt.Errorf("start Match: %w", err)
	}
	ended := make(chan struct{})
	go func() {
		err := command.Wait()
		log.Printf("match ended match=%d port=%d result=%v", spec.MatchID, port, err)
		l.givePort(port)
		close(ended)
	}()
	log.Printf("match started match=%d port=%d pid=%d", spec.MatchID, port, command.Process.Pid)
	process := command.Process
	return Running{Address: l.Advertise + ":" + strconv.Itoa(port), Ended: ended, Stop: func() { _ = process.Kill() }}, nil
}

// names is the players' roster as one line of JSON,
// {"players":[{"userId":7,"name":"..."}]}; false when no player has a name.
func names(players []Player) (string, bool) {
	named := false
	for _, player := range players {
		named = named || player.Name != ""
	}
	if !named {
		return "", false
	}
	roster, err := json.Marshal(struct {
		Players []Player `json:"players"`
	}{players})
	if err != nil {
		return "", false
	}
	return string(roster), true
}

func (l *Local) takePort() (int, error) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.inUse == nil {
		l.inUse = map[int]bool{}
	}
	for port := l.FirstPort; port <= l.LastPort; port++ {
		if !l.inUse[port] {
			l.inUse[port] = true
			return port, nil
		}
	}
	return 0, ErrNoPort
}

func (l *Local) givePort(port int) {
	l.mu.Lock()
	defer l.mu.Unlock()
	delete(l.inUse, port)
}
