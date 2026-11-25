package matches

import (
	"context"
	"encoding/hex"
	"os"
	"strings"
	"testing"
	"time"
)

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
	want := "--port 7811 --match-id 77 --capacity 3 --exit-when-empty --arena pvp_corners"
	if !strings.Contains(arguments, want) {
		os.Stderr.WriteString("fake match: arguments " + arguments + "\n")
		return 3
	}
	if os.Getenv(AdmissionKeyVariable) != hex.EncodeToString([]byte{1, 2, 3}) {
		os.Stderr.WriteString("fake match: no key\n")
		return 4
	}
	time.Sleep(50 * time.Millisecond)
	return 0
}

func TestLocalStartsAMatchProcessAndFreesItsPort(t *testing.T) {
	local := &Local{Command: os.Args[0], Arguments: []string{"-test.run=^$"}, Advertise: "127.0.0.1",
		FirstPort: 7811, LastPort: 7811, Environment: []string{"GYO_FAKE_MATCH=1"}}
	spec := Spec{MatchID: 77, Key: []byte{1, 2, 3}, Capacity: 3, Map: "pvp_corners"}
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
	again, err := local.Start(context.Background(), spec)
	if err != nil {
		t.Fatalf("the port of an ended Match is not free: %v", err)
	}
	<-again.Ended
}
