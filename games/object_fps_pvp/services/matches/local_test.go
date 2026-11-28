package matches

import (
	"bufio"
	"bytes"
	"context"
	"encoding/hex"
	"io"
	"os"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"
)

// output collects what the fake Match writes (it says what it found wrong).
type output struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (o *output) Write(p []byte) (int, error) {
	o.mu.Lock()
	defer o.mu.Unlock()
	return o.buf.Write(p)
}

func (o *output) String() string {
	o.mu.Lock()
	defer o.mu.Unlock()
	return o.buf.String()
}

// The test binary stands in for the Match: run with GYO_FAKE_MATCH set, it
// checks what Local gave it and exits.
func TestMain(m *testing.M) {
	if os.Getenv("GYO_FAKE_MATCH") != "" {
		os.Exit(fakeMatch())
	}
	os.Exit(m.Run())
}

func fakeMatch() int {
	arguments := strings.Join(os.Args[1:], " ")
	want := "--port 7811 --match-id 77 --capacity 3 --admission-key-stdin --idle-seconds 60 --arena pvp_corners_v1 --mode core_pvp --expect-users 7,9"
	if !strings.Contains(arguments, want) {
		os.Stderr.WriteString("fake match: arguments " + arguments + "\n")
		return 3
	}
	input := bufio.NewReader(os.Stdin)
	line, _ := input.ReadString('\n')
	if line != hex.EncodeToString([]byte{1, 2, 3})+"\n" {
		os.Stderr.WriteString("fake match: no key on the standard input\n")
		return 4
	}
	rest, _ := io.ReadAll(input)
	if !strings.Contains(arguments, "--player-names-stdin") {
		os.Stderr.WriteString("fake match: no --player-names-stdin in " + arguments + "\n")
		return 5
	}
	wantRest := `{"players":[{"userId":7,"name":""},{"userId":9,"name":""}]}` + "\n"
	if os.Getenv("GYO_FAKE_MATCH") == "named" {
		vector, _ := os.ReadFile(rosterVector)
		wantRest = string(vector)
	}
	if string(rest) != wantRest {
		os.Stderr.WriteString("fake match: after the key " + strconv.Quote(string(rest)) + "\n")
		return 6
	}
	if strings.Contains(arguments, "甲") {
		os.Stderr.WriteString("fake match: a name on the command line\n")
		return 7
	}
	time.Sleep(50 * time.Millisecond)
	return 0
}

func TestLocalStartsAMatchProcessAndFreesItsPort(t *testing.T) {
	said := &output{}
	local := &Local{Command: os.Args[0], Arguments: []string{"-test.run=^$"}, Advertise: "127.0.0.1",
		FirstPort: 7811, LastPort: 7811, Environment: []string{"GYO_FAKE_MATCH=1"}, Output: said}
	spec := Spec{MatchID: 77, Key: []byte{1, 2, 3}, Capacity: 3, Mode: "core_pvp", Map: "pvp_corners_v1",
		Players: []Player{{UserID: 7}, {UserID: 9}}}
	running, err := local.Start(context.Background(), spec)
	if err != nil {
		t.Fatal(err)
	}
	if running.Address != "127.0.0.1:7811" {
		t.Fatalf("the Match's address is %s", running.Address)
	}
	if _, err := local.Start(context.Background(), spec); err != ErrNoPort {
		t.Fatalf("a second Match on the one port: %v, want ErrNoPort", err)
	}
	select {
	case <-running.Ended:
	case <-time.After(10 * time.Second):
		t.Fatal("the Match process did not end")
	}
	// The fake Match says what it found wrong (its arguments, its key) before it exits.
	if strings.Contains(said.String(), "fake match:") {
		t.Fatalf("the Match was not started as asked: %s", said.String())
	}
	again, err := local.Start(context.Background(), spec)
	if err != nil {
		t.Fatalf("the port of an ended Match is not free: %v", err)
	}
	<-again.Ended
}

// The roster line both ends check (match/PlayerRoster.hpp reads the same file in
// the C++ tests): the line Roster gives for rosterPlayers.
const rosterVector = "testdata/roster_line.txt"

var rosterPlayers = []Player{{UserID: 7, Name: "甲 A, \"q\""}, {UserID: 9}}

func TestTheRosterLineIsTheSharedVector(t *testing.T) {
	vector, err := os.ReadFile(rosterVector)
	if err != nil {
		t.Fatal(err)
	}
	line, err := Roster(rosterPlayers)
	if err != nil || line+"\n" != string(vector) {
		t.Fatalf("Roster gives %q (%v), the shared vector is %q", line, err, vector)
	}
}

func TestLocalHandsTheNamesOnTheStandardInput(t *testing.T) {
	said := &output{}
	local := &Local{Command: os.Args[0], Arguments: []string{"-test.run=^$"}, Advertise: "127.0.0.1",
		FirstPort: 7811, LastPort: 7811, Environment: []string{"GYO_FAKE_MATCH=named"}, Output: said}
	spec := Spec{MatchID: 77, Key: []byte{1, 2, 3}, Capacity: 3, Mode: "core_pvp", Map: "pvp_corners_v1",
		Players: rosterPlayers}
	running, err := local.Start(context.Background(), spec)
	if err != nil {
		t.Fatal(err)
	}
	select {
	case <-running.Ended:
	case <-time.After(10 * time.Second):
		t.Fatal("the Match process did not end")
	}
	if strings.Contains(said.String(), "fake match:") {
		t.Fatalf("the Match was not started as asked: %s", said.String())
	}
}
