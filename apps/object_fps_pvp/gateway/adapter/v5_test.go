package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/proto"
	client "gyo.local/object_fps_pvp/protocol/clientv5"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev5"
)

func TestV5ActionKindPresenceAndLifeAreImmutable(t *testing.T) {
	shot := &client.ShotRequest{ActionId: 1, ObservedAuthorityTick: 7, Kind: client.ActionKind_ACTION_SHOT, LifeGeneration: 3, Yaw: proto.Float32(0), Pitch: proto.Float32(0)}
	reload := &client.ShotRequest{ActionId: 2, ObservedAuthorityTick: 7, Kind: client.ActionKind_ACTION_RELOAD, LifeGeneration: 2}
	encode := func(requests ...*client.ShotRequest) []byte {
		b, err := proto.Marshal(&client.ActionBatch{Shots: requests})
		if err != nil {
			t.Fatal(err)
		}
		return b
	}
	// Lifetimes are individually attached to actions, not filtered by transport.
	out, err := DecodeActions(encode(shot, reload), 9)
	if err != nil || out.Shots[0].LifeGeneration != 3 || out.Shots[1].LifeGeneration != 2 ||
		out.Shots[1].Kind != runtime.ActionKind_ACTION_RELOAD || out.Shots[1].Yaw != nil {
		t.Fatalf("mixed life/kind mapping: %v %v", out, err)
	}
	for _, edit := range []func(*client.ShotRequest){
		func(s *client.ShotRequest) { s.Kind = client.ActionKind_ACTION_UNSPECIFIED },
		func(s *client.ShotRequest) { s.Kind = 99 },
		func(s *client.ShotRequest) { s.LifeGeneration = 0 },
		func(s *client.ShotRequest) { s.Yaw = nil },
		func(s *client.ShotRequest) { s.Pitch = nil },
		func(s *client.ShotRequest) { s.Kind = client.ActionKind_ACTION_RELOAD },
	} {
		bad := proto.Clone(shot).(*client.ShotRequest)
		edit(bad)
		if _, err := DecodeActions(encode(reload, bad), 9); err == nil {
			t.Fatalf("accepted malformed action: %v", bad)
		}
	}
	for _, edit := range []func(*client.ShotRequest){
		func(s *client.ShotRequest) { s.LifeGeneration++ },
		func(s *client.ShotRequest) { s.Kind = client.ActionKind_ACTION_RELOAD; s.Yaw = nil; s.Pitch = nil },
	} {
		conflict := proto.Clone(shot).(*client.ShotRequest)
		edit(conflict)
		if _, err := DecodeActions(encode(shot, conflict), 9); err == nil {
			t.Fatal("same ID allowed different kind/life")
		}
	}
}

func TestV5SnapshotLifecycleReloadAndJumpValidation(t *testing.T) {
	in := &runtime.WorldSnapshot{Tick: 20,
		Players: []*runtime.PlayerState{{PlayerId: 1, MovementEpoch: 3, LifeGeneration: 2, LifeState: runtime.LifeState_LIFE_ALIVE, LifeStateTick: 10, Y: .4, VerticalVelocity: -2}},
		Combat:  []*runtime.CombatState{{PlayerId: 1, LifeGeneration: 2, Hp: 75, MagazineAmmo: 4, ReloadActionId: 5, ReloadStartTick: 15, ReloadEndTick: 105, LastShotActionId: 4, LastShotTick: 12}},
	}
	out, err := SnapshotForClient(in, testRules())
	if err != nil || out.Players[0].LifeGeneration != 2 || out.Players[0].Grounded || out.Players[0].VerticalVelocity != -2 ||
		out.Combat[0].ReloadActionId != 5 || out.Combat[0].ReloadEndTick != 105 || out.Combat[0].LastShotActionId != 4 {
		t.Fatalf("v5 fields lost: %v %v", out, err)
	}
	for _, edit := range []func(*runtime.WorldSnapshot){
		func(s *runtime.WorldSnapshot) { s.Players[0].LifeGeneration = 0 },
		func(s *runtime.WorldSnapshot) { s.Players[0].LifeState = runtime.LifeState_LIFE_UNSPECIFIED },
		func(s *runtime.WorldSnapshot) { s.Players[0].LifeState = 99 },
		func(s *runtime.WorldSnapshot) { s.Players[0].LifeStateTick = 21 },
		func(s *runtime.WorldSnapshot) { s.Players[0].RespawnTick = 200 },
		func(s *runtime.WorldSnapshot) { s.Players[0].VerticalVelocity = float32(math.NaN()) },
		func(s *runtime.WorldSnapshot) { s.Combat[0].LifeGeneration = 1 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].MagazineAmmo = 13 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].Hp = 0 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].ReloadActionId = 0 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].ReloadStartTick = 21 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].ReloadEndTick = 20 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].ReloadEndTick++ },
		func(s *runtime.WorldSnapshot) { s.Combat[0].LastShotActionId = 0 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].LastShotTick = 0 },
		func(s *runtime.WorldSnapshot) { s.Combat[0].LastShotTick = 21 },
	} {
		bad := proto.Clone(in).(*runtime.WorldSnapshot)
		edit(bad)
		if _, err := SnapshotForClient(bad, testRules()); err == nil {
			t.Fatalf("invalid lifecycle snapshot: %v", bad)
		}
	}
	in.Players[0].LifeState = runtime.LifeState_LIFE_DEAD
	in.Players[0].RespawnTick = 200
	in.Combat[0].Hp = 0
	in.Combat[0].ReloadActionId = 0
	in.Combat[0].ReloadStartTick = 0
	in.Combat[0].ReloadEndTick = 0
	if out, err := SnapshotForClient(in, testRules()); err != nil || out.Players[0].LifeState != client.LifeState_LIFE_DEAD || out.Players[0].RespawnTick != 200 {
		t.Fatalf("dead state: %v %v", out, err)
	}
	in.Players[0].RespawnTick = in.Players[0].LifeStateTick
	if _, err := SnapshotForClient(in, testRules()); err == nil {
		t.Fatal("invalid respawn schedule")
	}

	input := &client.PlayerInput{MovementEpoch: 3, LifeGeneration: 2, Commands: []*client.MovementCommand{{Sequence: 1, JumpRequested: true}}}
	b, _ := proto.Marshal(input)
	mapped, err := DecodeInput(b, 1)
	if err != nil || mapped.LifeGeneration != 2 || !mapped.Commands[0].JumpRequested {
		t.Fatalf("jump/life mapping: %v %v", mapped, err)
	}
	changed := proto.Clone(mapped.Commands[0]).(*runtime.MovementCommand)
	changed.JumpRequested = false
	if EqualCommand(mapped.Commands[0], changed) {
		t.Fatal("jump bit is mutable")
	}
	input.LifeGeneration = 0
	b, _ = proto.Marshal(input)
	if _, err := DecodeInput(b, 1); err == nil {
		t.Fatal("missing input life accepted")
	}
}

func TestV5ResultEnumsLifeAndAuthoritativeRules(t *testing.T) {
	d := &runtime.ShotDecision{ActionId: 1, ResolvedTick: 20, Kind: runtime.ActionKind_ACTION_RELOAD, LifeGeneration: 3, Rejection: runtime.ShotRejection_REJECTION_STALE_LIFE}
	for _, rejection := range []runtime.ShotRejection{runtime.ShotRejection_REJECTION_STALE_LIFE, runtime.ShotRejection_REJECTION_INVALID_LIFE, runtime.ShotRejection_REJECTION_DEAD, runtime.ShotRejection_REJECTION_RELOADING, runtime.ShotRejection_REJECTION_MAGAZINE_FULL} {
		d.Rejection = rejection
		out, err := ResultsForClient(&runtime.ActionResults{PlayerId: 1, Decisions: []*runtime.ShotDecision{d}}, testRules())
		if err != nil || out.Decisions[0].Kind != client.ActionKind_ACTION_RELOAD || out.Decisions[0].LifeGeneration != 3 || int32(out.Decisions[0].Rejection) != int32(rejection) {
			t.Fatalf("terminal lifecycle result: %v %v", out, err)
		}
	}
	for _, edit := range []func(*runtime.ShotDecision){
		func(d *runtime.ShotDecision) { d.Kind = runtime.ActionKind_ACTION_UNSPECIFIED },
		func(d *runtime.ShotDecision) { d.LifeGeneration = 0 },
		func(d *runtime.ShotDecision) { d.TargetLifeGeneration = 2 },
		func(d *runtime.ShotDecision) { d.Rejection = runtime.ShotRejection_REJECTION_COOLDOWN },
	} {
		bad := proto.Clone(d).(*runtime.ShotDecision)
		edit(bad)
		if _, err := ResultsForClient(&runtime.ActionResults{PlayerId: 1, Decisions: []*runtime.ShotDecision{bad}}, testRules()); err == nil {
			t.Fatalf("invalid v5 result: %v", bad)
		}
	}
	ready := &runtime.Ready{TickRate: 60, SnapshotIntervalTicks: 1, JumpHeight: .6, Gravity: 18, CombatRules: testRules()}
	for _, edit := range []func(*runtime.Ready){
		func(r *runtime.Ready) { r.JumpHeight = 0 },
		func(r *runtime.Ready) { r.Gravity = 0 },
		func(r *runtime.Ready) { r.Gravity = float32(math.Inf(1)) },
		func(r *runtime.Ready) { r.CombatRules.MagazineCapacity = 0 },
		func(r *runtime.Ready) { r.CombatRules.ReloadTicks = 0 },
		func(r *runtime.Ready) { r.CombatRules.RespawnTicks = 0 },
	} {
		bad := proto.Clone(ready).(*runtime.Ready)
		edit(bad)
		if ReadyFitsWelcome(bad) {
			t.Fatalf("missing/invalid v5 rules: %v", bad)
		}
	}
	if !ReadyFitsWelcome(ready) {
		t.Fatal("valid authority rules rejected")
	}
}

func TestV5EpochStartWaitKeepsPresenceAndBound(t *testing.T) {
	in := &runtime.WorldSnapshot{Tick: 8,
		Players: []*runtime.PlayerState{{PlayerId: 1, MovementEpoch: 2, LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, LastResolvedCommand: 1}},
		Combat:  []*runtime.CombatState{{PlayerId: 1, LifeGeneration: 1, Hp: 100, MagazineAmmo: 12}},
	}
	out, err := SnapshotForClient(in, testRules())
	if err != nil || out.Players[0].EpochStartWaitUs != nil {
		t.Fatalf("unmeasured start wait must stay absent: %v %v", out, err)
	}
	for _, wait := range []uint32{0, 14781, MaxEpochStartWaitUs} {
		in.Players[0].EpochStartWaitUs = proto.Uint32(wait)
		out, err = SnapshotForClient(in, testRules())
		if err != nil || out.Players[0].EpochStartWaitUs == nil || *out.Players[0].EpochStartWaitUs != wait {
			t.Fatalf("start wait %d lost: %v %v", wait, out, err)
		}
	}
	in.Players[0].EpochStartWaitUs = proto.Uint32(MaxEpochStartWaitUs + 1)
	if _, err := SnapshotForClient(in, testRules()); err == nil {
		t.Fatal("unbounded start wait accepted")
	}
}
