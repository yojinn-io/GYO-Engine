package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// A snapshot with more players than the room holds is a runtime fault (pv6 contract §1).
func TestSnapshotAboveCapacityIsInvalid(t *testing.T) {
	snapshot := &runtime.WorldSnapshot{Tick: 10}
	for id := uint32(1); id <= MaxPlayers+1; id++ {
		snapshot.Players = append(snapshot.Players, &runtime.PlayerState{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, PlayerId: id, MovementEpoch: 1})
		snapshot.Combat = append(snapshot.Combat, &runtime.CombatState{LifeGeneration: 1, MagazineAmmo: 12, PlayerId: id, Hp: 100000})
	}
	if _, err := SnapshotForClient(snapshot, testRules()); err == nil {
		t.Fatal("snapshot above capacity accepted")
	}
	snapshot.Players, snapshot.Combat = snapshot.Players[:MaxPlayers], snapshot.Combat[:MaxPlayers]
	if _, err := SnapshotForClient(snapshot, testRules()); err != nil {
		t.Fatalf("snapshot at capacity rejected: %v", err)
	}
}

// maximumSnapshot is n players' snapshot with every field at its largest
// encoding. The contract maximum keeps the values the Gateway admits
// (ContiguousPendingCommands, the slack sample, the holds at their 10 s clamp,
// a valid LifeState and HitPart); the type maximum puts every uint32 at its
// maximum, every enum at MaxInt32 and movement_slack_us at MinInt32
// (protocol-v8 section 6).
func maximumSnapshot(n uint32, typeMaximum bool) *client.WorldSnapshot {
	hold := uint32(10_000_000)
	if typeMaximum {
		hold = math.MaxUint32
	}
	snapshot := &client.WorldSnapshot{Tick: math.MaxUint32, MatchHoldUs: proto.Uint32(hold), GatewayHoldUs: proto.Uint32(hold)}
	for i := uint32(0); i < n; i++ {
		player := &client.PlayerState{PlayerId: math.MaxUint32 - i, X: math.MaxFloat32, Y: math.MaxFloat32, Z: math.MaxFloat32, Yaw: 3, Pitch: 1.5,
			LastResolvedCommand: math.MaxUint32, MovementEpoch: math.MaxUint32, ContiguousPendingCommands: MaxFutureCommands, VerticalVelocity: 4, Grounded: true,
			LifeGeneration: math.MaxUint32, LifeState: client.LifeState_LIFE_ALIVE, LifeStateTick: math.MaxUint32, RespawnTick: math.MaxUint32,
			MovementSlackSequence: proto.Uint32(math.MaxUint32), MovementSlackUs: proto.Int32(-MaxMovementSlackUs), ConnectionQualityFailures: ConnectionQualityFailedWindows - 1,
			Crouched: true, Sprinting: true}
		combat := &client.CombatState{PlayerId: math.MaxUint32 - i, Hp: math.MaxUint32, NextAllowedShotTick: math.MaxUint32, LifeGeneration: math.MaxUint32, MagazineAmmo: math.MaxUint32,
			ReloadActionId: math.MaxUint32, ReloadStartTick: math.MaxUint32, ReloadEndTick: math.MaxUint32, LastShotActionId: math.MaxUint32, LastShotTick: math.MaxUint32,
			LastDamageTick: math.MaxUint32, DamageCount: math.MaxUint32, LastAttackerId: math.MaxUint32,
			LastHitPart: client.HitPart_HIT_PART_HEAD}
		if typeMaximum {
			player.ContiguousPendingCommands, player.ConnectionQualityFailures = math.MaxUint32, math.MaxUint32
			player.LifeState, combat.LastHitPart = client.LifeState(math.MaxInt32), client.HitPart(math.MaxInt32)
			player.MovementSlackUs = proto.Int32(math.MinInt32)
		}
		snapshot.Players, snapshot.Combat = append(snapshot.Players, player), append(snapshot.Combat, combat)
	}
	return snapshot
}

// A full room's snapshot at its largest encoding still fits one datagram, and
// the sizes are pinned exactly so a new or widened field shows (pv6 contract §7
// 35+255N, pv7 §8 45+257N; pv8 §2a narrowed the counters to uint32, protocol-v8
// section 6: the contract maximum 40+177N, the type maximum 42+195N with the stance). The
// worker probe pins the same message at the same size.
func TestFullRoomSnapshotDatagramSize(t *testing.T) {
	for n := uint32(1); n <= MaxPlayers; n++ {
		if size := proto.Size(maximumSnapshot(n, false)) + framing.HeaderSize; size != 40+177*int(n) || size > framing.MaxDatagram {
			t.Fatalf("%d-player contract maximum snapshot datagram %d bytes", n, size)
		}
		if size := proto.Size(maximumSnapshot(n, true)) + framing.HeaderSize; size != 42+195*int(n) || size > framing.MaxDatagram {
			t.Fatalf("%d-player type maximum snapshot datagram %d bytes", n, size)
		}
	}
}
