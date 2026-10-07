package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv6"
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

// A full room's snapshot with every field at its largest encoding (pv6 contract §7,
// 2026-10-07 revision: 35+255N bytes) still fits one datagram; the worker probe pins
// the same message at the same size.
func TestFullRoomSnapshotDatagramSize(t *testing.T) {
	snapshot := &client.WorldSnapshot{Tick: math.MaxUint64}
	for i := uint64(0); i < MaxPlayers; i++ {
		snapshot.Players = append(snapshot.Players, &client.PlayerState{PlayerId: math.MaxUint64 - i, X: math.MaxFloat32, Y: math.MaxFloat32, Z: math.MaxFloat32, Yaw: 3, Pitch: 1.5,
			LastResolvedCommand: math.MaxUint64, MovementEpoch: math.MaxUint64, ContiguousPendingCommands: MaxFutureCommands, VerticalVelocity: 4, Grounded: true,
			LifeGeneration: math.MaxUint64, LifeState: client.LifeState_LIFE_ALIVE, LifeStateTick: math.MaxUint64, RespawnTick: math.MaxUint64,
			MovementSlackSequence: proto.Uint64(math.MaxUint64), MovementSlackUs: proto.Int32(-MaxMovementSlackUs), ConnectionQualityFailures: ConnectionQualityFailedWindows - 1})
		snapshot.Combat = append(snapshot.Combat, &client.CombatState{PlayerId: math.MaxUint64 - i, Hp: math.MaxUint32, NextAllowedShotTick: math.MaxUint64, LifeGeneration: math.MaxUint64, MagazineAmmo: math.MaxUint32,
			ReloadActionId: math.MaxUint64, ReloadStartTick: math.MaxUint64, ReloadEndTick: math.MaxUint64, LastShotActionId: math.MaxUint64, LastShotTick: math.MaxUint64,
			LastDamageTick: math.MaxUint64, DamageCount: math.MaxUint32, LastAttackerId: math.MaxUint64})
	}
	if size := proto.Size(snapshot) + framing.HeaderSize; size != 35+255*MaxPlayers || size > framing.MaxDatagram {
		t.Fatalf("full room snapshot datagram %d bytes", size)
	}
}
