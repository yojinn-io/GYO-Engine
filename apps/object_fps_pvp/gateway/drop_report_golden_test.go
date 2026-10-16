package gateway

import (
	"encoding/json"
	"os"
	"testing"

	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
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
	// Logged only, never reported (pv7 §4).
	for _, counter := range []playerIngressCounter{playerDropClearedOnCloseCommands, playerDropAbandonedOnCloseCommands,
		playerDropClearedOnCloseActionsShots, playerDropClearedOnCloseActionsAcks, playerDropAbandonedOnCloseActionsShots, playerDropAbandonedOnCloseActionsAcks} {
		if playerDropReason[counter] != 0 {
			t.Fatalf("%s is reported", ingressKeyName(playerIngressKeys[counter]))
		}
	}
	for _, counter := range []gatewayIngressCounter{gatewayDropRoomUnavailableDatagrams, gatewayDropRateLimitedPackets} {
		if gatewayDropReason[counter] != 0 {
			t.Fatalf("%s is reported", ingressKeyName(gatewayIngressKeys[counter]))
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
