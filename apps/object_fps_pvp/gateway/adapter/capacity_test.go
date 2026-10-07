package adapter

import (
	"testing"

	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

// A snapshot with more players than the room holds is a runtime fault (pv6 contract §1).
func TestSnapshotAboveCapacityIsInvalid(t *testing.T) {
	snapshot := &runtime.WorldSnapshot{Tick: 10}
	for id := uint64(1); id <= MaxPlayers+1; id++ {
		snapshot.Players = append(snapshot.Players, &runtime.PlayerState{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, PlayerId: id, MovementEpoch: 1})
		snapshot.Combat = append(snapshot.Combat, &runtime.CombatState{LifeGeneration: 1, MagazineAmmo: 12, PlayerId: id, Hp: 100})
	}
	if _, err := SnapshotForClient(snapshot, testRules()); err == nil {
		t.Fatal("snapshot above capacity accepted")
	}
	snapshot.Players, snapshot.Combat = snapshot.Players[:MaxPlayers], snapshot.Combat[:MaxPlayers]
	if _, err := SnapshotForClient(snapshot, testRules()); err != nil {
		t.Fatalf("snapshot at capacity rejected: %v", err)
	}
}
