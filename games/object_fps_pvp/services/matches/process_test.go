package matches

import (
	"context"
	"errors"
	"net"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"
)

// TestTheRealMatchStartsWithItsKey starts the Match executable the way the
// service does (CTest object_fps_pvp.match_process, label network, sets
// GYO_PVP_MATCH and GYO_PVP_CONTENT; skipped otherwise).
func TestTheRealMatchStartsWithItsKey(t *testing.T) {
	executable, content := os.Getenv("GYO_PVP_MATCH"), os.Getenv("GYO_PVP_CONTENT")
	if executable == "" {
		t.Skip("GYO_PVP_MATCH is not set")
	}
	local := &Local{Command: executable, Arguments: []string{"--content", content}, Advertise: "127.0.0.1",
		FirstPort: 47811, LastPort: 47811}
	running, err := local.Start(context.Background(), Spec{MatchID: 5, Key: make([]byte, 32), Capacity: 2, Map: "pvp_training_v1"})
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		running.Stop()
		<-running.Ended
	}()
	deadline := time.Now().Add(20 * time.Second)
	for !listening(t, "127.0.0.1:47811") {
		select {
		case <-running.Ended:
			t.Fatal("the Match ended instead of listening")
		default:
		}
		if time.Now().After(deadline) {
			t.Fatal("the Match did not listen on its port")
		}
		time.Sleep(50 * time.Millisecond)
	}
}

// listening sends a datagram the Match drops (not a handshake): a closed port
// answers with ICMP port unreachable, which a connected UDP socket reads as a
// refusal; an open one stays silent until the deadline.
func listening(t *testing.T, address string) bool {
	t.Helper()
	connection, err := net.Dial("udp", address)
	if err != nil {
		t.Fatal(err)
	}
	defer connection.Close()
	if _, err := connection.Write([]byte("probe")); err != nil {
		return false
	}
	_ = connection.SetReadDeadline(time.Now().Add(200 * time.Millisecond))
	_, err = connection.Read(make([]byte, 64))
	var timeout net.Error
	return errors.As(err, &timeout) && timeout.Timeout()
}

func TestTheRealMatchRefusesABadKey(t *testing.T) {
	executable, content := os.Getenv("GYO_PVP_MATCH"), os.Getenv("GYO_PVP_CONTENT")
	if executable == "" {
		t.Skip("GYO_PVP_MATCH is not set")
	}
	command := exec.Command(executable, "--content", content, "--port", "47812", "--match-id", "5", "--admission-key-stdin")
	command.Stdin = strings.NewReader("not hex\n")
	output, err := command.CombinedOutput()
	exit, ok := err.(*exec.ExitError)
	if !ok || exit.ExitCode() != 2 || !strings.Contains(string(output), "not a hexadecimal key") {
		t.Fatalf("a bad key: %v\n%s", err, output)
	}
}
