package adapter

import (
	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
	"math"
	"testing"
)

func TestActionSchemaIdentityAndMaximumDatagrams(t *testing.T) {
	batch := &client.ActionBatch{AcknowledgedThrough: math.MaxUint32}
	results := &runtime.ActionResults{PlayerId: math.MaxUint32, RetiredThrough: math.MaxUint32}
	rules := &runtime.CombatRules{MagazineCapacity: math.MaxUint32, ReloadTicks: math.MaxUint32, RespawnTicks: math.MaxUint32, MaximumHp: math.MaxUint32, ShotDamage: math.MaxUint32, MaximumShotDamage: math.MaxUint32, CooldownTicks: math.MaxUint32, ShotRange: math.MaxFloat32, MaximumReferenceAgeMs: math.MaxUint32}
	for i := 0; i < MaxActionBatch; i++ {
		id := uint32(math.MaxUint32) - uint32(i)
		batch.Shots = append(batch.Shots, &client.ShotRequest{LifeGeneration: math.MaxUint32, Kind: client.ActionKind_ACTION_SHOT, ActionId: id, ObservedAuthorityTick: math.MaxUint32, Yaw: proto.Float32(1e6), Pitch: proto.Float32(-float32(math.Pi / 2)),
			MovementEpoch: math.MaxUint32, CommandSequence: math.MaxUint32, ViewTickQ16: math.MaxUint64})
		results.Decisions = append(results.Decisions, &runtime.ShotDecision{LifeGeneration: math.MaxUint32, Kind: runtime.ActionKind_ACTION_SHOT, TargetLifeGeneration: math.MaxUint32, ActionId: id, ResolvedTick: math.MaxUint32, Accepted: true, HitKind: runtime.ShotHitKind_HIT_PLAYER, TargetId: math.MaxUint32, Damage: math.MaxUint32, HitPart: runtime.HitPart_HIT_PART_HEAD})
	}
	encoded, _ := proto.Marshal(batch)
	mapped, err := DecodeActions(encoded, 77)
	if err != nil || mapped.PlayerId != 77 || len(mapped.Shots) != 8 {
		t.Fatalf("identity or mapping: %v %v", mapped, err)
	}
	converted, err := ResultsForClient(results, rules)
	if err != nil {
		t.Fatal(err)
	}
	welcome := &client.Welcome{JumpHeight: .6, Gravity: 18, PlayerId: math.MaxUint32, TickRate: 60, SnapshotRate: 60, ArenaId: "test_arena", ArenaVersion: math.MaxUint32, CombatRules: RulesForClient(rules)}
	snapshot := &client.WorldSnapshot{Tick: math.MaxUint32 - 1}
	for i := uint32(0); i < MaxPlayers; i++ {
		snapshot.Players = append(snapshot.Players, &client.PlayerState{LifeGeneration: math.MaxUint32, LifeState: client.LifeState_LIFE_ALIVE, LifeStateTick: math.MaxUint32 - 100, VerticalVelocity: math.MaxFloat32, Grounded: true, PlayerId: math.MaxUint32 - i, X: -math.MaxFloat32, Y: -math.MaxFloat32, Z: -math.MaxFloat32, Yaw: 1e6, Pitch: -float32(math.Pi / 2), LastResolvedCommand: math.MaxUint32, MovementEpoch: math.MaxUint32, ContiguousPendingCommands: 32,
			MovementSlackSequence: proto.Uint32(math.MaxUint32), MovementSlackUs: proto.Int32(-MaxMovementSlackUs),
			ConnectionQualityFailures: ConnectionQualityFailedWindows - 1})
		snapshot.Combat = append(snapshot.Combat, &client.CombatState{LifeGeneration: math.MaxUint32, MagazineAmmo: math.MaxUint32, PlayerId: math.MaxUint32 - i, Hp: math.MaxUint32, NextAllowedShotTick: math.MaxUint32, ReloadActionId: math.MaxUint32, ReloadStartTick: math.MaxUint32 - 90, ReloadEndTick: math.MaxUint32, LastShotActionId: math.MaxUint32, LastShotTick: math.MaxUint32 - 91, LastDamageTick: math.MaxUint32, DamageCount: math.MaxUint32, LastAttackerId: math.MaxUint32, LastHitPart: client.HitPart_HIT_PART_HEAD})
	}
	for _, message := range []proto.Message{batch, converted, welcome, snapshot} {
		b, err := proto.Marshal(message)
		if err != nil {
			t.Fatal(err)
		}
		packet, err := framing.EncodeDatagram(framing.Header{Version: ClientVersion, Type: 6, SessionID: math.MaxUint64, Sequence: math.MaxUint32}, b)
		if err != nil || len(packet) > 1200 {
			t.Fatalf("%T maximum datagram %d: %v", message, len(b)+24, err)
		}
		t.Logf("%T maximum datagram=%d bytes incl 24-byte header", message, len(packet))
	}
	// The action datagrams are pinned exactly (protocol-v8 section 6): a new or
	// widened field shows.
	for message, want := range map[proto.Message]int{batch: 470, converted: 398} {
		if size := proto.Size(message) + framing.HeaderSize; size != want {
			t.Fatalf("%T maximum datagram %d bytes, want %d", message, size, want)
		}
	}
	for _, shots := range [][]*client.ShotRequest{{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Yaw: proto.Float32(0), Pitch: proto.Float32(0), ActionId: 0}}, {{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, Yaw: proto.Float32(float32(math.NaN()))}}, {{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Yaw: proto.Float32(0), ActionId: 1, Pitch: proto.Float32(2)}}, {{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Yaw: proto.Float32(0), Pitch: proto.Float32(0), ActionId: 1}, {LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, Yaw: proto.Float32(1)}}, append(batch.Shots, &client.ShotRequest{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Yaw: proto.Float32(0), Pitch: proto.Float32(0), ActionId: 1})} {
		b, _ := proto.Marshal(&client.ActionBatch{Shots: shots})
		if _, err := DecodeActions(b, 1); err == nil {
			t.Fatalf("accepted bad shots %v", shots)
		}
	}
	if _, err := DecodeActions(encoded, 0); err == nil {
		t.Fatal("zero identity")
	}
	if _, err := DecodeActions(make([]byte, 1200), 1); err == nil {
		t.Fatal("oversize")
	}
	if _, err := DecodeActions([]byte{255}, 1); err == nil {
		t.Fatal("malformed")
	}
	if got, err := DecodeActions(nil, 1); err != nil || got.AcknowledgedThrough != 0 {
		t.Fatal("pure ACK")
	}
}
func TestCombatRulesAndResultsValidation(t *testing.T) {
	if ValidRules(nil) {
		t.Fatal("missing rules")
	}
	for _, edit := range []func(*runtime.CombatRules){func(r *runtime.CombatRules) { r.MaximumHp = 0 }, func(r *runtime.CombatRules) { r.ShotDamage = r.MaximumHp + 1 }, func(r *runtime.CombatRules) { r.ShotRange = float32(math.NaN()) }, func(r *runtime.CombatRules) { r.CooldownTicks = 0 }, func(r *runtime.CombatRules) { r.MaximumReferenceAgeMs = 0 }} {
		r := testRules()
		edit(r)
		if ValidRules(r) {
			t.Fatal("invalid rules accepted")
		}
	}
	d := &runtime.ShotDecision{LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT, ActionId: 1, ResolvedTick: 5, Accepted: true}
	for _, edit := range []func(*runtime.ShotDecision){func(d *runtime.ShotDecision) { d.ActionId = 0 }, func(d *runtime.ShotDecision) { d.Accepted = false }, func(d *runtime.ShotDecision) { d.TargetId = 1 }, func(d *runtime.ShotDecision) { d.Rejection = 99 }, func(d *runtime.ShotDecision) { d.HitKind = 99 }, func(d *runtime.ShotDecision) { d.HitKind = runtime.ShotHitKind_HIT_PLAYER }, func(d *runtime.ShotDecision) { d.Damage = 26 }} {
		bad := proto.Clone(d).(*runtime.ShotDecision)
		edit(bad)
		if _, err := ResultsForClient(&runtime.ActionResults{PlayerId: 1, Decisions: []*runtime.ShotDecision{bad}}, testRules()); err == nil {
			t.Fatalf("invalid decision %v", bad)
		}
	}
	snapshot := &runtime.WorldSnapshot{Players: []*runtime.PlayerState{{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, PlayerId: 1, MovementEpoch: 1}}}
	if _, err := snapshotForClient(snapshot, testRules()); err == nil {
		t.Fatal("combat missing")
	}
	snapshot.Combat = []*runtime.CombatState{{LifeGeneration: 1, MagazineAmmo: 12, PlayerId: 2, Hp: 100000}}
	if _, err := snapshotForClient(snapshot, testRules()); err == nil {
		t.Fatal("combat identity mismatch")
	}
	snapshot.Combat[0].PlayerId = 1
	snapshot.Combat[0].Hp = 100001
	if _, err := snapshotForClient(snapshot, testRules()); err == nil {
		t.Fatal("invalid hp")
	}
	snapshot.Combat[0].Hp = 75000
	snapshot.Combat[0].NextAllowedShotTick = 90
	out, err := snapshotForClient(snapshot, testRules())
	if err != nil || out.Combat[0].Hp != 75000 || out.Combat[0].NextAllowedShotTick != 90 {
		t.Fatal("combat state mapping")
	}
}

func TestReadyRequiresRulesAndBoundsWorstCaseWelcome(t *testing.T) {
	// pv8 §3: the movement rules are part of the worst case.
	ready := &runtime.Ready{JumpHeight: .6, Gravity: 18, CrouchBodyHeight: 1.2, CrouchEyeHeight: 1, CrouchSpeedScale: .5, SprintSpeedScale: 1,
		TickRate: 60, SnapshotIntervalTicks: 1, ArenaVersion: math.MaxUint32, ArenaDigest: 1, AnimationDigest: 1, CombatRules: testRules()}
	for ReadyFitsWelcome(ready) {
		ready.ArenaId += "x"
	}
	if ReadyFitsWelcome(ready) {
		t.Fatal("unbounded descriptor")
	}
	ready.ArenaId = ready.ArenaId[:len(ready.ArenaId)-1]
	if !ReadyFitsWelcome(ready) {
		t.Fatal("boundary descriptor rejected")
	}
	// The largest time echo (pv7 §3) is part of the worst case.
	largest := uint64(math.MaxUint64)
	welcome := &client.Welcome{JumpHeight: .6, Gravity: 18, PlayerId: math.MaxUint32, TickRate: 60, SnapshotRate: 60, ArenaId: ready.ArenaId, ArenaVersion: ready.ArenaVersion, ArenaDigest: ready.ArenaDigest, AnimationDigest: ready.AnimationDigest, CombatRules: RulesForClient(ready.CombatRules),
		Clock:            &client.ClockSample{OriginUs: &largest, ReceiveUs: &largest, TransmitUs: largest},
		CrouchBodyHeight: ready.CrouchBodyHeight, CrouchEyeHeight: ready.CrouchEyeHeight, CrouchSpeedScale: ready.CrouchSpeedScale, SprintSpeedScale: ready.SprintSpeedScale}
	if proto.Size(welcome)+24 != 1200 {
		t.Fatalf("maximum admitted Welcome=%d", proto.Size(welcome)+24)
	}
	// pv9 §6: animation_digest (10 B) shortens the largest arena id from 1052 to 1042 bytes.
	if len(ready.ArenaId) != 1042 {
		t.Fatalf("boundary arena id length=%d", len(ready.ArenaId))
	}
	ready.CombatRules = nil
	if ReadyFitsWelcome(ready) {
		t.Fatal("missing Match rules accepted")
	}
}
