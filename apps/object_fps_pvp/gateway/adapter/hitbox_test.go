package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/proto"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// pv7 §7, pv9 §2: a player hit is BODY, HEAD or LEG, every other decision
// UNSPECIFIED; an unknown part invalidates the results batch. HEAD and LEG pass
// although the Match produces only BODY until the hitbox batch.
func TestResultsCheckTheHitPart(t *testing.T) {
	hit := func(part runtime.HitPart) *runtime.ShotDecision {
		return &runtime.ShotDecision{LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT, ActionId: 1, ResolvedTick: 5,
			Accepted: true, HitKind: runtime.ShotHitKind_HIT_PLAYER, TargetId: 2, TargetLifeGeneration: 1, Damage: 25000, HitPart: part}
	}
	miss := func(part runtime.HitPart) *runtime.ShotDecision {
		return &runtime.ShotDecision{LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT, ActionId: 1, ResolvedTick: 5,
			Accepted: true, HitKind: runtime.ShotHitKind_HIT_WORLD, HitPart: part}
	}
	for _, good := range []*runtime.ShotDecision{hit(runtime.HitPart_HIT_PART_BODY), hit(runtime.HitPart_HIT_PART_HEAD),
		hit(runtime.HitPart_HIT_PART_LEG), miss(runtime.HitPart_HIT_PART_UNSPECIFIED)} {
		out, err := ResultsForClient(&runtime.ActionResults{PlayerId: 1, Decisions: []*runtime.ShotDecision{good}}, testRules())
		if err != nil || int32(out.Decisions[0].HitPart) != int32(good.HitPart) {
			t.Fatalf("%v: %v %v", good, out, err)
		}
	}
	for _, bad := range []*runtime.ShotDecision{hit(runtime.HitPart_HIT_PART_UNSPECIFIED), hit(4), hit(99),
		miss(runtime.HitPart_HIT_PART_BODY), miss(runtime.HitPart_HIT_PART_HEAD), miss(runtime.HitPart_HIT_PART_LEG), miss(4), miss(99)} {
		if _, err := ResultsForClient(&runtime.ActionResults{PlayerId: 1, Decisions: []*runtime.ShotDecision{bad}}, testRules()); err == nil {
			t.Fatalf("accepted %v", bad)
		}
	}
	if a, b := hit(runtime.HitPart_HIT_PART_BODY), hit(runtime.HitPart_HIT_PART_HEAD); EqualDecision(a, b) || !EqualDecision(a, proto.Clone(a).(*runtime.ShotDecision)) {
		t.Fatal("EqualDecision does not compare the hit part")
	}
	if EqualDecision(hit(runtime.HitPart_HIT_PART_BODY), hit(runtime.HitPart_HIT_PART_LEG)) {
		t.Fatal("EqualDecision does not compare the hit part")
	}
}

// pv7 §7: shot_damage <= maximum_shot_damage <= maximum_hp, and a decision's
// damage is bounded by maximum_shot_damage, not shot_damage.
func TestMaximumShotDamageBoundsTheRulesAndTheDamage(t *testing.T) {
	for name, edit := range map[string]func(*runtime.CombatRules){
		"missing":           func(r *runtime.CombatRules) { r.MaximumShotDamage = 0 },
		"below shot damage": func(r *runtime.CombatRules) { r.MaximumShotDamage = r.ShotDamage - 1 },
		"above maximum hp":  func(r *runtime.CombatRules) { r.MaximumShotDamage = r.MaximumHp + 1 },
	} {
		r := testRules()
		edit(r)
		if ValidRules(r) {
			t.Fatalf("%s accepted", name)
		}
	}
	for _, edit := range []func(*runtime.CombatRules){func(r *runtime.CombatRules) { r.MaximumShotDamage = r.ShotDamage },
		func(r *runtime.CombatRules) { r.MaximumShotDamage = r.MaximumHp }} {
		r := testRules()
		edit(r)
		if !ValidRules(r) || RulesForClient(r).MaximumShotDamage != r.MaximumShotDamage {
			t.Fatalf("rules %v refused or not forwarded", r)
		}
	}
	rules := testRules()
	rules.MaximumShotDamage = 50000
	decision := func(damage uint32) *runtime.ActionResults {
		return &runtime.ActionResults{PlayerId: 1, Decisions: []*runtime.ShotDecision{{LifeGeneration: 1, Kind: runtime.ActionKind_ACTION_SHOT,
			ActionId: 1, ResolvedTick: 5, Accepted: true, HitKind: runtime.ShotHitKind_HIT_PLAYER, TargetId: 2, TargetLifeGeneration: 1,
			Damage: damage, HitPart: runtime.HitPart_HIT_PART_HEAD}}}
	}
	if _, err := ResultsForClient(decision(50000), rules); err != nil {
		t.Fatalf("damage at maximum_shot_damage above shot_damage refused: %v", err)
	}
	if _, err := ResultsForClient(decision(50001), rules); err == nil {
		t.Fatal("damage above maximum_shot_damage accepted")
	}
}

// pv7 §6, §7: E, S and view_tick_q16 are request content: copied as they came,
// never validated, and part of comparing a repeated id within a batch.
func TestShotRequestCarriesEAndSAndTheViewTick(t *testing.T) {
	shot := &client.ShotRequest{ActionId: 1, LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Yaw: proto.Float32(0),
		Pitch: proto.Float32(0), MovementEpoch: math.MaxUint32, CommandSequence: 0, ViewTickQ16: math.MaxUint64}
	reload := &client.ShotRequest{ActionId: 2, LifeGeneration: 1, Kind: client.ActionKind_ACTION_RELOAD, ViewTickQ16: 99}
	payload, _ := proto.Marshal(&client.ActionBatch{Shots: []*client.ShotRequest{shot, reload}})
	out, err := DecodeActions(payload, 3)
	if err != nil || len(out.Shots) != 2 {
		t.Fatalf("decode: %v %v", out, err)
	}
	if s := out.Shots[0]; s.MovementEpoch != math.MaxUint32 || s.CommandSequence != 0 || s.ViewTickQ16 != math.MaxUint64 {
		t.Fatalf("shot content changed: %v", s)
	}
	if out.Shots[1].ViewTickQ16 != 99 {
		t.Fatal("a reload's view tick was checked or dropped; the Match records it")
	}
	for name, edit := range map[string]func(*runtime.ShotRequest){
		"E": func(s *runtime.ShotRequest) { s.MovementEpoch++ }, "S": func(s *runtime.ShotRequest) { s.CommandSequence++ },
		"view tick": func(s *runtime.ShotRequest) { s.ViewTickQ16-- },
	} {
		other := proto.Clone(out.Shots[0]).(*runtime.ShotRequest)
		edit(other)
		if EqualShot(out.Shots[0], other) {
			t.Fatalf("EqualShot ignores %s", name)
		}
		// The same id with other content in one batch is malformed, as for any field.
		conflict := proto.Clone(shot).(*client.ShotRequest)
		conflict.MovementEpoch, conflict.CommandSequence, conflict.ViewTickQ16 = other.MovementEpoch, other.CommandSequence, other.ViewTickQ16
		payload, _ := proto.Marshal(&client.ActionBatch{Shots: []*client.ShotRequest{shot, conflict}})
		if _, err := DecodeActions(payload, 3); err == nil {
			t.Fatalf("a repeated id with another %s accepted", name)
		}
	}
	if !EqualShot(out.Shots[0], proto.Clone(out.Shots[0]).(*runtime.ShotRequest)) {
		t.Fatal("EqualShot refuses an identical request")
	}
}
