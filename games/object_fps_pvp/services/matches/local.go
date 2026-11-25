package matches

import (
	"context"
	"encoding/hex"
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
	// --admission-key-stdin, --idle-seconds). The key goes to the Match's standard input, in neither
	// its command line nor its environment.
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
	if spec.Saves != "" {
		arguments = append(arguments, "--saves", spec.Saves)
	}
	// Not CommandContext: the Match outlives the allocation request.
	if len(spec.Players) > 0 {
		users := make([]string, len(spec.Players))
		for j, user := range spec.Players {
			users[j] = strconv.FormatUint(user, 10)
		}
		arguments = append(arguments, "--expect-users", strings.Join(users, ","))
	}
	command := exec.Command(l.Command, arguments...)
	command.Env = append(os.Environ(), l.Environment...)
	command.Stdin = strings.NewReader(hex.EncodeToString(spec.Key) + "\n")
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
