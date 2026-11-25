package matches

import (
	"context"
	"encoding/hex"
	"errors"
	"fmt"
	"log"
	"os"
	"os/exec"
	"strconv"
	"sync"
)

// AdmissionKeyVariable carries a Match's key to its process (not its command
// line, which other users of the machine can read).
const AdmissionKeyVariable = "GYO_PVP_ADMISSION_KEY"

// Local starts each Match as a process on this machine (D6), on a free port
// of [FirstPort, LastPort]; the port is free again when the process ends.
type Local struct {
	// Command is the Match executable; Arguments come before the ones Local
	// adds (--port, --match-id, --capacity, --arena, --exit-when-empty).
	Command   string
	Arguments []string
	// Advertise is the address players are told to connect to (an IPv4
	// literal; the Match listens on every interface).
	Advertise string
	FirstPort int
	LastPort  int
	// Environment is added to this process's environment for each Match.
	Environment []string

	mu    sync.Mutex
	inUse map[int]bool
}

var ErrNoPort = errors.New("no free Match port")

func (l *Local) Start(ctx context.Context, spec Spec) (Running, error) {
	port, err := l.takePort()
	if err != nil {
		return Running{}, err
	}
	arguments := append(append([]string{}, l.Arguments...),
		"--port", strconv.Itoa(port),
		"--match-id", strconv.FormatUint(spec.MatchID, 10),
		"--capacity", strconv.Itoa(spec.Capacity),
		"--exit-when-empty")
	if spec.Map != "" {
		arguments = append(arguments, "--arena", spec.Map)
	}
	// Not CommandContext: the Match outlives the allocation request.
	command := exec.Command(l.Command, arguments...)
	command.Env = append(append(os.Environ(), l.Environment...), AdmissionKeyVariable+"="+hex.EncodeToString(spec.Key))
	command.Stdout, command.Stderr = os.Stdout, os.Stderr
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
	return Running{Address: l.Advertise + ":" + strconv.Itoa(port), Ended: ended}, nil
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
