package gateway

import (
	"bufio"
	"errors"
	"net/netip"
	"os"
	"strconv"
	"strings"
	"testing"
	"time"

	"gyo.local/gateway/session"
)

// v7.1 K1 (D118): the arrivals of one firing player's session at the Gateway's
// reader in a gateway-1000ms run (testdata/k1-arrivals.txt). The player sent
// at its usual rate; only the Gateway's own 1 s stall bunched the datagrams.
// None of them should be refused on the session's rate limit.
func TestTheSessionRateLimitRefusesNothingOfAGatewayStallReplay(t *testing.T) {
	file, err := os.Open("testdata/k1-arrivals.txt")
	if err != nil {
		t.Fatal(err)
	}
	defer file.Close()
	var arrivals []time.Duration
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		us, err := strconv.ParseInt(line, 10, 64)
		if err != nil {
			t.Fatal(err)
		}
		arrivals = append(arrivals, time.Duration(us)*time.Microsecond)
	}
	if len(arrivals) < 1000 {
		t.Fatalf("%d arrivals read", len(arrivals))
	}
	start := time.Unix(100, 0)
	s, err := session.New(start)
	if err != nil {
		t.Fatal(err)
	}
	peer := netip.MustParseAddrPort("127.0.0.1:9")
	refused := 0
	for k, at := range arrivals {
		sequence, now := uint32(k+1), start.Add(at)
		if k == 0 {
			err = s.Hello(s.Token, peer, sequence, now)
		} else {
			err = s.Accept(peer, sequence, now)
		}
		if errors.Is(err, session.ErrRateLimit) {
			refused++
		} else if err != nil {
			t.Fatal(err)
		}
	}
	if refused != 0 {
		t.Fatalf("%d of %d datagrams refused on the rate limit after the Gateway's stall", refused, len(arrivals))
	}
}
