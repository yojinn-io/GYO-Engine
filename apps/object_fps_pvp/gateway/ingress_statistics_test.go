package gateway

import (
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"testing"
	"time"
)

// The golden windows shared with the Match tests and the acceptance tools.
var ingressFixtures = filepath.Join("..", "..", "..", "tests", "object_fps_pvp", "fixtures", "ingress_v1")

var (
	ingressKeyRule = regexp.MustCompile(`^(fwd_|drop_)?[a-z]+(_[a-z]+)*_(datagrams|packets|inputs|commands|batches|shots|snapshots)$`)
	ingressValue   = regexp.MustCompile(`^\d+$`)
	// build/acceptance/object_fps_pvp/action_probe.py:594 (and run_gameplay_soak.py:29) read the
	// first match; network_statistics.py:17-19 parse the lines that carry these phrases.
	frozenSessionCounters = regexp.MustCompile(`rate_accepted_packets=(\d+) rate_limited_packets=(\d+) max_session_window_packets=(\d+)`)
	frozenLinePhrases     = []string{"player send statistics", "[ObjectFPS/PvP Match] network statistics",
		"[ObjectFPS/PvP] worker statistics"}
)

type ingressField struct{ key, value string }

func readIngressGolden(t *testing.T, name string) []string {
	t.Helper()
	data, err := os.ReadFile(filepath.Join(ingressFixtures, name))
	if err != nil {
		t.Fatal(err)
	}
	lines := strings.Split(strings.TrimSuffix(string(data), "\n"), "\n")
	if len(lines) != 3 {
		t.Fatalf("%s holds %d windows, want zero, non-zero and final", name, len(lines))
	}
	return lines
}

// splitIngressLine checks the fixed head and returns its values and the counters.
func splitIngressLine(t *testing.T, line, prefix string, head []string) (map[string]uint64, []ingressField) {
	t.Helper()
	rest, ok := strings.CutPrefix(line, prefix+" ")
	if !ok {
		t.Fatalf("line does not start with %q: %s", prefix, line)
	}
	values := map[string]uint64{}
	var fields []ingressField
	for i, word := range strings.Split(rest, " ") {
		key, value, ok := strings.Cut(word, "=")
		if !ok || !ingressValue.MatchString(value) {
			t.Fatalf("ingress field %q is not key=<decimal integer>", word)
		}
		if i < len(head) {
			if key != head[i] {
				t.Fatalf("head field %d is %s, want %s", i, key, head[i])
			}
			n, _ := strconv.ParseUint(value, 10, 64)
			values[key] = n
			continue
		}
		fields = append(fields, ingressField{key, value})
	}
	if values["version"] != ingressStatisticsVersion || values["final"] > 1 {
		t.Fatalf("head %v", values)
	}
	return values, fields
}

func fillIngressCounts(t *testing.T, fields []ingressField, keys []ingressKey, counts []uint64) {
	t.Helper()
	if len(fields) != len(keys) {
		t.Fatalf("golden line has %d counters, want %d", len(fields), len(keys))
	}
	for i, field := range fields {
		if want := ingressKeyName(keys[i]); field.key != want {
			t.Fatalf("ingress key %d is %s, want %s", i, field.key, want)
		}
		counts[i], _ = strconv.ParseUint(field.value, 10, 64)
	}
}

func sumIngress[C ~uint8](counts []uint64, first, last C) (sum uint64) {
	for i := first; i <= last; i++ {
		sum += counts[i]
	}
	return sum
}

func TestIngressStatisticsLinesEqualTheGoldenWindows(t *testing.T) {
	players := readIngressGolden(t, "gateway-player.txt")
	gateways := readIngressGolden(t, "gateway-global.txt")
	for i := range players {
		head, fields := splitIngressLine(t, players[i], "player ingress statistics", []string{"version", "player", "window_ms", "final"})
		var p playerIngressCounts
		fillIngressCounts(t, fields, playerIngressKeys[:], p[:])
		if got := playerIngressStatisticsLine(head["player"], time.Duration(head["window_ms"])*time.Millisecond,
			head["final"] == 1, &p); got != players[i] {
			t.Fatalf("player window %d\n got %s\nwant %s", i, got, players[i])
		}

		head, fields = splitIngressLine(t, gateways[i], "gateway ingress statistics", []string{"version", "window_ms", "final"})
		var g gatewayIngressCounts
		fillIngressCounts(t, fields, gatewayIngressKeys[:], g[:])
		if got := gatewayIngressStatisticsLine(time.Duration(head["window_ms"])*time.Millisecond, head["final"] == 1,
			&g); got != gateways[i] {
			t.Fatalf("gateway window %d\n got %s\nwant %s", i, got, gateways[i])
		}

		// The golden values satisfy the conservations under these counters.
		if outcomes := sumIngress(p[:], playerFwdMainInputPackets, playerDropAdmissionUnknownPackets); outcomes != p[playerReceivedDatagrams] {
			t.Fatalf("window %d: datagram outcomes %d != received %d", i, outcomes, p[playerReceivedDatagrams])
		}
		handed := p[playerFwdHandedMainCommands] + p[playerFwdHandedRejectedLaneCommands]
		if settled := sumIngress(p[:], playerFwdLinkWrittenMainCommands, playerDropAbandonedOnCloseCommands); settled != handed {
			t.Fatalf("window %d: link settled %d of %d handed commands", i, settled, handed)
		}
		if rate := sumIngress(p[:], playerDropRateLimitedHelloPackets, playerDropRateLimitedOtherPackets); rate != g[gatewayDropRateLimitedPackets] {
			t.Fatalf("window %d: per-player rate drops %d != gateway %d", i, rate, g[gatewayDropRateLimitedPackets])
		}
		if read := sumIngress(g[:], gatewayDropUndecodableDatagrams, gatewayDropUnknownSessionDatagrams) +
			p[playerReceivedDatagrams]; read != g[gatewayReceivedDatagrams] {
			t.Fatalf("window %d: gateway datagrams %d != received %d", i, read, g[gatewayReceivedDatagrams])
		}
	}
	if !strings.Contains(players[2], " final=1 ") || !strings.Contains(gateways[2], " final=1 ") {
		t.Fatal("the last golden window is not final")
	}
}

func TestIngressKeysAreUniqueAndFollowTheNamingRule(t *testing.T) {
	for name, keys := range map[string][]ingressKey{"player": playerIngressKeys[:], "gateway": gatewayIngressKeys[:]} {
		seen := map[string]bool{"version": true, "player": true, "window_ms": true, "final": true}
		for i, key := range keys {
			n := ingressKeyName(key)
			if !ingressKeyRule.MatchString(n) {
				t.Fatalf("%s ingress key %d %q breaks <reason>[_<kind>]_<unit>", name, i, n)
			}
			if seen[n] {
				t.Fatalf("%s ingress key %d %q repeats", name, i, n)
			}
			seen[n] = true
		}
	}
	// Every reason, kind and unit has a spelling: a missing one would leave an
	// empty word in a key.
	for _, names := range [][]string{ingressReasonNames[:], ingressKindNames[1:], ingressUnitNames[:]} {
		for i, n := range names {
			if n == "" || n == "_" {
				t.Fatalf("ingress word %d has no name", i)
			}
		}
	}
}

func TestIngressLinesStayOutOfTheFrozenParsers(t *testing.T) {
	var p playerIngressCounts
	var g gatewayIngressCounts
	for i := range p {
		p[i] = uint64(1000 + i)
	}
	for i := range g {
		g[i] = uint64(2000 + i)
	}
	lines := []string{
		playerIngressStatisticsLine(7, 10*time.Second, false, &playerIngressCounts{}),
		playerIngressStatisticsLine(7, 10*time.Second, true, &p),
		gatewayIngressStatisticsLine(10*time.Second, false, &gatewayIngressCounts{}),
		gatewayIngressStatisticsLine(10*time.Second, true, &g),
	}
	lines = append(lines, readIngressGolden(t, "gateway-player.txt")...)
	lines = append(lines, readIngressGolden(t, "gateway-global.txt")...)
	for _, line := range lines {
		for _, phrase := range frozenLinePhrases {
			if strings.Contains(line, phrase) {
				t.Fatalf("ingress line would be parsed as %s: %s", phrase, line)
			}
		}
		if frozenSessionCounters.MatchString(line) {
			t.Fatalf("ingress line matches the frozen Session counter pattern: %s", line)
		}
		for _, word := range strings.Split(line, " ") {
			if _, value, ok := strings.Cut(word, "="); ok && !ingressValue.MatchString(value) {
				t.Fatalf("ingress value %q is not a decimal integer", word)
			}
		}
	}
}
