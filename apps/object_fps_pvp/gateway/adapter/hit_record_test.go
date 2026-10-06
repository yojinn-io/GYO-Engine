package adapter

import (
	"testing"

	"google.golang.org/protobuf/proto"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

// pv6 contract §2: the presentation-only hit record of the current life.
func TestHitRecordIsValidatedAndKeptVerbatim(t *testing.T) {
	in := &runtime.WorldSnapshot{Tick: 20,
		Players: []*runtime.PlayerState{{PlayerId: 1, MovementEpoch: 1, LifeGeneration: 2, LifeState: runtime.LifeState_LIFE_ALIVE, LifeStateTick: 10}},
		Combat:  []*runtime.CombatState{{PlayerId: 1, LifeGeneration: 2, Hp: 50, MagazineAmmo: 4, LastDamageTick: 15, DamageCount: 2, LastAttackerId: 7}},
	}
	out, err := SnapshotForClient(in, testRules())
	if err != nil || out.Combat[0].LastDamageTick != 15 || out.Combat[0].DamageCount != 2 || out.Combat[0].LastAttackerId != 7 {
		t.Fatalf("hit record not kept: %v %v", out, err)
	}
	none := proto.Clone(in).(*runtime.WorldSnapshot)
	none.Combat[0].LastDamageTick, none.Combat[0].DamageCount, none.Combat[0].LastAttackerId = 0, 0, 0
	if _, err := SnapshotForClient(none, testRules()); err != nil {
		t.Fatalf("no hit rejected: %v", err)
	}
	for name, edit := range map[string]func(*runtime.WorldSnapshot){
		"tick only zero":     func(s *runtime.WorldSnapshot) { s.Combat[0].LastDamageTick = 0 },
		"count only zero":    func(s *runtime.WorldSnapshot) { s.Combat[0].DamageCount = 0 },
		"attacker only zero": func(s *runtime.WorldSnapshot) { s.Combat[0].LastAttackerId = 0 },
		"before life state":  func(s *runtime.WorldSnapshot) { s.Combat[0].LastDamageTick = 9 },
		"after snapshot":     func(s *runtime.WorldSnapshot) { s.Combat[0].LastDamageTick = 21 },
		"self attacker":      func(s *runtime.WorldSnapshot) { s.Combat[0].LastAttackerId = 1 },
		"count above max hp": func(s *runtime.WorldSnapshot) { s.Combat[0].DamageCount = testRules().MaximumHp + 1 },
		"dead without hit": func(s *runtime.WorldSnapshot) {
			kill(s)
			s.Combat[0].LastDamageTick, s.Combat[0].DamageCount, s.Combat[0].LastAttackerId = 0, 0, 0
		},
		"dead of an older hit": func(s *runtime.WorldSnapshot) { kill(s); s.Combat[0].LastDamageTick = 15 },
	} {
		bad := proto.Clone(in).(*runtime.WorldSnapshot)
		edit(bad)
		if _, err := SnapshotForClient(bad, testRules()); err == nil {
			t.Fatalf("%s accepted: %v", name, bad)
		}
	}
	dead := proto.Clone(in).(*runtime.WorldSnapshot)
	kill(dead)
	if _, err := SnapshotForClient(dead, testRules()); err != nil {
		t.Fatalf("lethal hit rejected: %v", err)
	}
	limit := proto.Clone(in).(*runtime.WorldSnapshot)
	limit.Combat[0].DamageCount = testRules().MaximumHp
	if _, err := SnapshotForClient(limit, testRules()); err != nil {
		t.Fatalf("count at max hp rejected: %v", err)
	}
}

// kill makes the player die of its last hit at tick 18.
func kill(s *runtime.WorldSnapshot) {
	s.Players[0].LifeState, s.Players[0].LifeStateTick, s.Players[0].RespawnTick = runtime.LifeState_LIFE_DEAD, 18, 198
	s.Combat[0].Hp, s.Combat[0].LastDamageTick = 0, 18
}
