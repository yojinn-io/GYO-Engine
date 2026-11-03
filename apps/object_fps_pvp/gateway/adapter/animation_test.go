package adapter

import (
	"testing"

	"google.golang.org/protobuf/proto"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// pv9 §4.5 (v8-product-anim-authority A3, E5): the Gateway's single-message
// checks of the animation authority, the same table the Match guards
// (ValidPlayerAnimations) and the Client applies; and the copy (E6).
func animatedSnapshot() *runtime.WorldSnapshot {
	return &runtime.WorldSnapshot{Tick: 20,
		Players: []*runtime.PlayerState{
			{PlayerId: 1, LifeGeneration: 2, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1},
			{PlayerId: 2, LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: false, MovementEpoch: 1}},
		Combat: []*runtime.CombatState{
			{PlayerId: 1, LifeGeneration: 2, MagazineAmmo: 12, Hp: 100000},
			{PlayerId: 2, LifeGeneration: 1, MagazineAmmo: 12, Hp: 100000}},
		Animation: []*runtime.PlayerAnimation{
			{PlayerId: 1, LifeGeneration: 2, GaitPhaseQ16: 0xFFFF1234, MoveWeightPermille: 1000, JogWeightPermille: 250,
				JumpPhase: runtime.JumpPhase_JUMP_PHASE_LAND, JumpPhaseTick: 20},
			{PlayerId: 2, LifeGeneration: 1, JumpPhase: runtime.JumpPhase_JUMP_PHASE_START, JumpPhaseTick: 18}}}
}

func TestSnapshotAnimationIsCheckedAndCopied(t *testing.T) {
	out, err := SnapshotForClient(animatedSnapshot(), testRules())
	if err != nil || len(out.Animation) != 2 {
		t.Fatalf("valid animation refused: %v %v", out, err)
	}
	want := &client.PlayerAnimation{PlayerId: 1, LifeGeneration: 2, GaitPhaseQ16: 0xFFFF1234, MoveWeightPermille: 1000,
		JogWeightPermille: 250, JumpPhase: client.JumpPhase_JUMP_PHASE_LAND, JumpPhaseTick: 20}
	if !proto.Equal(out.Animation[0], want) || out.Animation[1].JumpPhase != client.JumpPhase_JUMP_PHASE_START {
		t.Fatalf("animation not copied as it came: %v", out.Animation)
	}
	for name, edit := range map[string]func(*runtime.WorldSnapshot){
		"missing value":     func(s *runtime.WorldSnapshot) { s.Animation = s.Animation[:1] },
		"no animation":      func(s *runtime.WorldSnapshot) { s.Animation = []*runtime.PlayerAnimation{} },
		"unknown player":    func(s *runtime.WorldSnapshot) { s.Animation[1].PlayerId = 7 },
		"player twice":      func(s *runtime.WorldSnapshot) { s.Animation[1] = proto.Clone(s.Animation[0]).(*runtime.PlayerAnimation) },
		"other life":        func(s *runtime.WorldSnapshot) { s.Animation[0].LifeGeneration = 1 },
		"move weight 1001":  func(s *runtime.WorldSnapshot) { s.Animation[0].MoveWeightPermille = 1001 },
		"jog weight 1001":   func(s *runtime.WorldSnapshot) { s.Animation[0].JogWeightPermille = 1001 },
		"sprint weight":     func(s *runtime.WorldSnapshot) { s.Animation[0].SprintWeightPermille = 1 },
		"phase zero":        func(s *runtime.WorldSnapshot) { s.Animation[0].JumpPhase = runtime.JumpPhase_JUMP_PHASE_UNSPECIFIED },
		"phase five":        func(s *runtime.WorldSnapshot) { s.Animation[0].JumpPhase = 5 },
		"phase 258":         func(s *runtime.WorldSnapshot) { s.Animation[0].JumpPhase = 258 },
		"zero after tick":   func(s *runtime.WorldSnapshot) { s.Animation[0].JumpPhaseTick = 21 },
		"start grounded":    func(s *runtime.WorldSnapshot) { s.Animation[0].JumpPhase = runtime.JumpPhase_JUMP_PHASE_START },
		"airborne grounded": func(s *runtime.WorldSnapshot) { s.Animation[0].JumpPhase = runtime.JumpPhase_JUMP_PHASE_AIRBORNE },
		"land in the air":   func(s *runtime.WorldSnapshot) { s.Animation[1].JumpPhase = runtime.JumpPhase_JUMP_PHASE_LAND },
		"grounded in air":   func(s *runtime.WorldSnapshot) { s.Animation[1].JumpPhase = runtime.JumpPhase_JUMP_PHASE_GROUNDED },
		"nil value":         func(s *runtime.WorldSnapshot) { s.Animation[1] = nil },
	} {
		s := animatedSnapshot()
		edit(s)
		if _, err := SnapshotForClient(s, testRules()); err == nil {
			t.Fatalf("%s accepted", name)
		}
	}
	// Accepted: another order, and a dead player is not cross-checked.
	swapped := animatedSnapshot()
	swapped.Animation[0], swapped.Animation[1] = swapped.Animation[1], swapped.Animation[0]
	if _, err := SnapshotForClient(swapped, testRules()); err != nil {
		t.Fatalf("animation in another order refused: %v", err)
	}
	dead := animatedSnapshot()
	dead.Players[0].LifeState, dead.Players[0].LifeStateTick, dead.Players[0].RespawnTick = runtime.LifeState_LIFE_DEAD, 10, 200
	dead.Combat[0].Hp, dead.Animation[0].JumpPhase, dead.Animation[0].JumpPhaseTick = 0, runtime.JumpPhase_JUMP_PHASE_AIRBORNE, 9
	dead.Combat[0].LastDamageTick, dead.Combat[0].DamageCount, dead.Combat[0].LastAttackerId, dead.Combat[0].LastHitPart = 10, 4, 2, runtime.HitPart_HIT_PART_BODY
	if _, err := SnapshotForClient(dead, testRules()); err != nil {
		t.Fatalf("a dead player's frozen animation refused: %v", err)
	}
}
