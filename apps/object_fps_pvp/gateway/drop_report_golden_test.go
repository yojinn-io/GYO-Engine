package gateway

import (
	"encoding/json"
	"os"
	"testing"

	runtime "gyo.local/object_fps_pvp/protocol/runtimev8"
)

// dropReasonGolden is one entry of tests/object_fps_pvp/fixtures/drop_reasons.json,
// the golden Go, C++ and Python read: the DropReason value, its proto name, the
// ingress key it counts and its scope.
type dropReasonGolden struct {
	Value int32  `json:"value"`
	Name  string `json:"name"`
	Key   string `json:"key"`
	Scope string `json:"scope"`
	Exit  bool   `json:"exit"` // an ingress counter counts it in this commit
}

func dropReasonsTable() []dropReasonGolden {
	var out []dropReasonGolden
	for _, entry := range dropReasons {
		scope := "player"
		if entry.scope == dropGatewayScope {
			scope = "gateway"
		}
		exit := false
		for counter := range playerDropReason {
			exit = exit || playerDropReason[counter] == entry.reason
		}
		for counter := range gatewayDropReason {
			exit = exit || gatewayDropReason[counter] == entry.reason
		}
		out = append(out, dropReasonGolden{int32(entry.reason), entry.reason.String(), ingressKeyName(entry.key), scope, exit})
	}
	return out
}

// pv7 §4: one DropReason per drop_ key, named after it, in the proto and the
// golden alike; scopes 1-34 player and 40-45 Gateway.
func TestDropReasonsMatchTheGoldenAndTheKeys(t *testing.T) {
	table := dropReasonsTable()
	if os.Getenv("GYO_WRITE_DROP_GOLDEN") != "" {
		b, _ := json.MarshalIndent(table, "", "  ")
		if err := os.WriteFile("../../../tests/object_fps_pvp/fixtures/drop_reasons.json", append(b, '\n'), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	b, err := os.ReadFile("../../../tests/object_fps_pvp/fixtures/drop_reasons.json")
	if err != nil {
		t.Fatal(err)
	}
	var golden []dropReasonGolden
	if err := json.Unmarshal(b, &golden); err != nil {
		t.Fatal(err)
	}
	if len(golden) != len(table) {
		t.Fatalf("golden has %d reasons, the table %d", len(golden), len(table))
	}
	for i := range table {
		if golden[i] != table[i] {
			t.Fatalf("reason %d: golden %+v, table %+v", i, golden[i], table[i])
		}
		g := table[i]
		if want := "DROP_" + upper(g.Key[len("drop_"):]); g.Name != want {
			t.Fatalf("%s is not its key %s in upper case", g.Name, g.Key)
		}
		if player := g.Value >= 1 && g.Value <= 34; player != (g.Scope == "player") || !player && (g.Value < 40 || g.Value > 45) {
			t.Fatalf("%s: value %d out of its %s range", g.Name, g.Value, g.Scope)
		}
	}
	// Every proto value but UNSPECIFIED is in the table, once.
	if got, want := len(table), len(runtime.DropReason_name)-1; got != want {
		t.Fatalf("%d reasons in the table, %d in the proto", got, want)
	}
	// Each counter maps to one reason at most, and only drop_ counters do.
	seen := map[runtime.DropReason]bool{}
	for counter, reason := range playerDropReason {
		if reason == 0 {
			continue
		}
		if seen[reason] || playerIngressKeys[counter].outcome != ingressDropped {
			t.Fatalf("player counter %s maps to %s twice or is not a drop", ingressKeyName(playerIngressKeys[counter]), reason)
		}
		seen[reason] = true
	}
	// The drop counters without a DropReason, by why they stay off the wire
	// (log-contract §5.1 lists them; dead-letter-position, D145).
	const (
		logOnly   = "logged only: the link has failed when they happen, or the key is a sum (pv7 §4)"
		removed   = "a path pv7 removed: the key stays in log v1 (R1), always 0 (removed_paths)"
		attribute = "a second unit of a reported drop: only its packets are reported"
	)
	playerUnreported := map[playerIngressCounter]string{
		playerDropClearedOnCloseCommands: logOnly, playerDropAbandonedOnCloseCommands: logOnly,
		playerDropClearedOnCloseActionsShots: logOnly, playerDropClearedOnCloseActionsAcks: logOnly,
		playerDropAbandonedOnCloseActionsShots: logOnly, playerDropAbandonedOnCloseActionsAcks: logOnly,
		playerDropClearedOnCloseActionsPackets: logOnly, playerDropAbandonedOnCloseActionsPackets: logOnly,
		playerDropResolvedTrimCommands: removed, playerDropRejectedLaneOverflowCommands: removed,
		playerDropDiscardedOnLeaveCommands: removed, playerDropInvalidActionsPackets: removed,
		playerDropInvalidActionsShots: removed, playerDropLinkRejectedActionsPackets: removed,
		playerDropLinkRejectedActionsShots: removed, playerDropDiscardedOnLeaveActionsShots: removed,
		playerDropDiscardedOnLeaveActionsAcks:   removed,
		playerDropIpcQueueOverflowInputCommands: attribute,
	}
	gatewayUnreported := map[gatewayIngressCounter]string{
		gatewayDropRoomUnavailableDatagrams: logOnly, gatewayDropRateLimitedPackets: logOnly,
		gatewayDropOldTickSnapshots: removed, gatewayDropRegressedSnapshots: removed,
	}
	// Both directions: a declared counter is not reported, and every drop
	// counter is reported or declared, so a new Gateway exit cannot stay off
	// the wire unclassified.
	for counter := playerIngressCounter(0); counter < playerIngressCounterCount; counter++ {
		key, why := playerIngressKeys[counter], playerUnreported[counter]
		if why != "" && (key.outcome != ingressDropped || playerDropReason[counter] != 0) {
			t.Errorf("player counter %s is declared unreported (%s) but is not an unreported drop", ingressKeyName(key), why)
		}
		if key.outcome == ingressDropped && playerDropReason[counter] == 0 && why == "" {
			t.Errorf("player drop counter %s has no DropReason and is not declared unreported", ingressKeyName(key))
		}
	}
	for counter := gatewayIngressCounter(0); counter < gatewayIngressCounterCount; counter++ {
		key, why := gatewayIngressKeys[counter], gatewayUnreported[counter]
		if why != "" && (key.outcome != ingressDropped || gatewayDropReason[counter] != 0) {
			t.Errorf("Gateway counter %s is declared unreported (%s) but is not an unreported drop", ingressKeyName(key), why)
		}
		if key.outcome == ingressDropped && gatewayDropReason[counter] == 0 && why == "" {
			t.Errorf("Gateway drop counter %s has no DropReason and is not declared unreported", ingressKeyName(key))
		}
	}
}

func upper(s string) string {
	b := []byte(s)
	for i, c := range b {
		if c >= 'a' && c <= 'z' {
			b[i] = c - 'a' + 'A'
		}
	}
	return string(b)
}
