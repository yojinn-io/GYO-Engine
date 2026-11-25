package matches

import (
	"bufio"
	"context"
	"io"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"
)

// The real Match executable as the service starts it (CTest
// object_fps_pvp.match_process, label network, sets GYO_PVP_MATCH and
// GYO_PVP_CONTENT; skipped otherwise).
func realMatch(t *testing.T) (string, string) {
	t.Helper()
	executable, content := os.Getenv("GYO_PVP_MATCH"), os.Getenv("GYO_PVP_CONTENT")
	if executable == "" {
		t.Skip("GYO_PVP_MATCH is not set")
	}
	return executable, content
}

// startWatched starts a Match whose output the test reads line by line.
func startWatched(t *testing.T, idleSeconds, port int) (Running, <-chan string) {
	t.Helper()
	executable, content := realMatch(t)
	reader, writer := io.Pipe()
	local := &Local{Command: executable, Arguments: []string{"--content", content}, Advertise: "127.0.0.1",
		FirstPort: port, LastPort: port, IdleSeconds: idleSeconds, Output: writer}
	running, err := local.Start(context.Background(), Spec{MatchID: 5, Key: make([]byte, 32), Capacity: 2, Map: "pvp_training_v1"})
	if err != nil {
		t.Fatal(err)
	}
	lines := make(chan string, 64)
	go func() {
		scanner := bufio.NewScanner(reader)
		for scanner.Scan() {
			lines <- scanner.Text()
		}
		close(lines)
	}()
	go func() {
		<-running.Ended
		writer.Close()
	}()
	return running, lines
}

// waitFor reads lines until one contains text, or fails at the deadline.
func waitFor(t *testing.T, lines <-chan string, text string, within time.Duration) {
	t.Helper()
	deadline := time.After(within)
	for {
		select {
		case line, open := <-lines:
			if !open {
				t.Fatalf("the Match ended before saying %q", text)
			}
			if strings.Contains(line, text) {
				return
			}
		case <-deadline:
			t.Fatalf("the Match did not say %q within %v", text, within)
		}
	}
}

// The Match says it plays once its UDP socket is open (the Product is built,
// the net Module's Configure opened it). The time it takes is logged: the
// connect target is handed out before it (declaration D6).
func TestTheRealMatchListensWithItsKey(t *testing.T) {
	started := time.Now()
	running, lines := startWatched(t, 60, 47811)
	defer func() {
		running.Stop()
		<-running.Ended
	}()
	waitFor(t, lines, "on UDP port 47811", 20*time.Second)
	t.Logf("the Match was listening %v after it was started", time.Since(started))
}

func TestTheRealMatchEndsWhenNobodyCame(t *testing.T) {
	running, lines := startWatched(t, 1, 47813)
	waitFor(t, lines, "no player for 1 s: the match ends", 20*time.Second)
	select {
	case <-running.Ended:
	case <-time.After(10 * time.Second):
		running.Stop()
		t.Fatal("the Match said it ends but its process went on")
	}
}

func TestTheRealMatchRefusesABadKey(t *testing.T) {
	executable, content := realMatch(t)
	command := exec.Command(executable, "--content", content, "--port", "47812", "--match-id", "5", "--admission-key-stdin")
	command.Stdin = strings.NewReader("not hex\n")
	output, err := command.CombinedOutput()
	exit, ok := err.(*exec.ExitError)
	if !ok || exit.ExitCode() != 2 || !strings.Contains(string(output), "not a hexadecimal key") {
		t.Fatalf("a bad key: %v\n%s", err, output)
	}
}
