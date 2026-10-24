package gateway

import (
	"encoding/json"
	"os"
	"testing"
	"time"
)

// T_int and T_max are one value at both ends (network-architecture §4): the
// Client's (Movement.hpp, "PvP connection timing matches the shared fixture")
// and these read the same file.
func TestConnectionTimingMatchesTheSharedFixture(t *testing.T) {
	data, err := os.ReadFile("../../../tests/object_fps_pvp/fixtures/connection_timing.json")
	if err != nil {
		t.Fatal(err)
	}
	var timing struct {
		InterruptedAfterMs int64 `json:"interrupted_after_ms"`
		TimeoutAfterMs     int64 `json:"timeout_after_ms"`
	}
	if err := json.Unmarshal(data, &timing); err != nil {
		t.Fatal(err)
	}
	if got := time.Duration(timing.InterruptedAfterMs) * time.Millisecond; got != connectionInterruptedAfter {
		t.Errorf("T_int: fixture %v, Gateway %v", got, connectionInterruptedAfter)
	}
	if got := time.Duration(timing.TimeoutAfterMs) * time.Millisecond; got != sessionTimeout {
		t.Errorf("T_max: fixture %v, Gateway %v", got, sessionTimeout)
	}
}
