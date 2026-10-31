package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// The pv8 field types (protocol-v8 section 2a, pv8-seq-narrow-declaration.md):
// these tables are the declared contract, kept by hand, not read from the
// proto. A field moved between them, or a new uint64 field, changes them.
var (
	clientNarrowed = map[string][]string{
		"ActionBatch":     {"acknowledged_through"},
		"ActionResults":   {"retired_through"},
		"CombatRules":     {"cooldown_ticks", "reload_ticks", "respawn_ticks"},
		"CombatState":     {"player_id", "next_allowed_shot_tick", "life_generation", "reload_action_id", "reload_start_tick", "reload_end_tick", "last_shot_action_id", "last_shot_tick", "last_damage_tick", "last_attacker_id"},
		"MovementCommand": {"sequence"},
		"PlayerInput":     {"movement_epoch", "life_generation", "observed_authority_tick"},
		"PlayerState":     {"player_id", "last_resolved_command", "movement_epoch", "life_generation", "life_state_tick", "respawn_tick", "movement_slack_sequence"},
		"ShotDecision":    {"action_id", "resolved_tick", "target_id", "life_generation", "target_life_generation"},
		"ShotRequest":     {"action_id", "observed_authority_tick", "life_generation", "movement_epoch", "command_sequence"},
		"Welcome":         {"player_id"},
		"WorldSnapshot":   {"tick"},
	}
	runtimeNarrowed = map[string][]string{
		"ActionBatch":       {"player_id", "acknowledged_through"},
		"ActionResults":     {"player_id", "retired_through"},
		"CombatRules":       {"cooldown_ticks", "reload_ticks", "respawn_ticks"},
		"CombatState":       {"player_id", "next_allowed_shot_tick", "life_generation", "reload_action_id", "reload_start_tick", "reload_end_tick", "last_shot_action_id", "last_shot_tick", "last_damage_tick", "last_attacker_id"},
		"GatewayDropReport": {"player_id"},
		"JoinResult":        {"player_id"},
		"MovementCommand":   {"sequence"},
		"PlayerEvicted":     {"player_id"},
		"PlayerInput":       {"player_id", "movement_epoch", "life_generation", "observed_authority_tick"},
		"PlayerJoin":        {"player_id"},
		"PlayerLeave":       {"player_id"},
		"PlayerState":       {"player_id", "last_resolved_command", "movement_epoch", "life_generation", "life_state_tick", "respawn_tick", "movement_slack_sequence"},
		"RuntimeError":      {"player_id"},
		"ShotDecision":      {"action_id", "resolved_tick", "target_id", "life_generation", "target_life_generation"},
		"ShotRequest":       {"action_id", "observed_authority_tick", "life_generation", "movement_epoch", "command_sequence"},
		"WorldSnapshot":     {"tick"},
	}
	clientKept = map[string][]string{
		"ActionResults": {"refused_batches"},
		"ClockSample":   {"origin_us", "receive_us", "transmit_us"},
		"ShotRequest":   {"view_tick_q16"},
		"Welcome":       {"arena_digest"},
	}
	runtimeKept = map[string][]string{
		"ActionResults":     {"refused_batches"},
		"ClockSample":       {"origin_us", "receive_us", "transmit_us"},
		"DropCount":         {"total"},
		"DropSample":        {"movement_epoch", "life_generation", "first_command_sequence", "last_command_sequence", "first_action_id", "last_action_id", "snapshot_tick"},
		"GatewayDropReport": {"report_sequence", "samples_omitted", "merged_streams"},
		"Ready":             {"arena_digest"},
		"ShotRequest":       {"view_tick_q16"},
	}
	// protocol-v8 section 2a, "預留的號碼".
	clientReserved = map[string][]protoreflect.FieldNumber{
		"PlayerState": {22, 23, 24}, "WorldSnapshot": {6, 7}, "Welcome": {2, 16, 17, 18, 19}, "ActionBatch": {3},
		"ActionResults": {5}, "ShotRequest": {10, 11, 12}, "ShotDecision": {12, 13},
	}
	runtimeReserved = map[string][]protoreflect.FieldNumber{
		"PlayerState": {22, 23, 24}, "WorldSnapshot": {5, 6}, "Ready": {14, 15, 16, 17}, "ActionBatch": {6},
		"ActionResults": {6}, "ShotRequest": {10, 11, 12}, "ShotDecision": {12, 13}, "RuntimeEnvelope": {22, 23},
	}
	clientReservedValues  = map[string][]protoreflect.EnumNumber{"ShotRejection": {10, 11, 12, 13, 14, 15, 16, 17}, "ShotHitKind": {3}}
	runtimeReservedValues = map[string][]protoreflect.EnumNumber{"ShotRejection": {10, 11, 12, 13, 14, 15, 16, 17}, "ShotHitKind": {3}, "EvictionReason": {3}}
)

func checkKinds(t *testing.T, file protoreflect.FileDescriptor, narrowed, kept map[string][]string) {
	t.Helper()
	declared := map[string]protoreflect.Kind{}
	for message, fields := range narrowed {
		for _, field := range fields {
			declared[message+"."+field] = protoreflect.Uint32Kind
		}
	}
	for message, fields := range kept {
		for _, field := range fields {
			declared[message+"."+field] = protoreflect.Uint64Kind
			if field == "arena_digest" {
				declared[message+"."+field] = protoreflect.Fixed64Kind
			}
		}
	}
	messages := file.Messages()
	for i := 0; i < messages.Len(); i++ {
		fields := messages.Get(i).Fields()
		for j := 0; j < fields.Len(); j++ {
			field := fields.Get(j)
			name := string(messages.Get(i).Name()) + "." + string(field.Name())
			want, listed := declared[name]
			wide := field.Kind() == protoreflect.Uint64Kind || field.Kind() == protoreflect.Fixed64Kind
			switch {
			case listed && want == protoreflect.Uint32Kind && field.Kind() != protoreflect.Uint32Kind:
				t.Errorf("%s: %s is %v, want uint32 (narrowed)", file.Path(), name, field.Kind())
			case listed && want != protoreflect.Uint32Kind && field.Kind() != want:
				t.Errorf("%s: %s is %v, want it kept %v", file.Path(), name, field.Kind(), want)
			case !listed && wide:
				t.Errorf("%s: %s is %v and in neither table", file.Path(), name, field.Kind())
			}
			delete(declared, name)
		}
	}
	for name := range declared {
		t.Errorf("%s: declared field %s is missing", file.Path(), name)
	}
}

func checkReserved(t *testing.T, file protoreflect.FileDescriptor, fields map[string][]protoreflect.FieldNumber, values map[string][]protoreflect.EnumNumber) {
	t.Helper()
	for message, numbers := range fields {
		d := file.Messages().ByName(protoreflect.Name(message))
		for _, n := range numbers {
			if d == nil || !d.ReservedRanges().Has(n) || d.Fields().ByNumber(n) != nil {
				t.Errorf("%s: %s field %d is not reserved", file.Path(), message, n)
			}
		}
	}
	for enum, numbers := range values {
		d := file.Enums().ByName(protoreflect.Name(enum))
		for _, n := range numbers {
			if d == nil || !d.ReservedRanges().Has(n) || d.Values().ByNumber(n) != nil {
				t.Errorf("%s: %s value %d is not reserved", file.Path(), enum, n)
			}
		}
	}
}

func TestPv8FieldTypesAndReservedNumbers(t *testing.T) {
	clientFile, runtimeFile := (&client.Welcome{}).ProtoReflect().Descriptor().ParentFile(), (&runtime.Ready{}).ProtoReflect().Descriptor().ParentFile()
	checkKinds(t, clientFile, clientNarrowed, clientKept)
	checkKinds(t, runtimeFile, runtimeNarrowed, runtimeKept)
	checkReserved(t, clientFile, clientReserved, clientReservedValues)
	checkReserved(t, runtimeFile, runtimeReserved, runtimeReservedValues)
	if welcome := clientFile.Messages().ByName("Welcome"); !welcome.ReservedNames().Has("match_id") || welcome.Fields().ByName("match_id") != nil {
		t.Error("Welcome.match_id is not reserved")
	}
	// The same message in both contracts keeps each common number's kind.
	messages := clientFile.Messages()
	for i := 0; i < messages.Len(); i++ {
		c := messages.Get(i)
		r := runtimeFile.Messages().ByName(c.Name())
		if r == nil {
			continue
		}
		for j := 0; j < c.Fields().Len(); j++ {
			cf := c.Fields().Get(j)
			if rf := r.Fields().ByNumber(cf.Number()); rf != nil && rf.Name() == cf.Name() && rf.Kind() != cf.Kind() {
				t.Errorf("%s.%s: client %v, runtime %v", c.Name(), cf.Name(), cf.Kind(), rf.Kind())
			}
		}
	}
}

// A varint past 32 bits in a narrowed field decodes as its low 32 bits, the
// value a Client could send directly, and DecodeInput treats it as that value
// (protocol-v8 section 2a, the adversarial check's #1).
func TestNarrowedFieldBeyond32BitsDecodesAsItsLowBits(t *testing.T) {
	encode := func(epoch uint64) []byte {
		in := &client.PlayerInput{LifeGeneration: 1, Commands: []*client.MovementCommand{{Sequence: 1}}}
		b, err := proto.Marshal(in)
		if err != nil {
			t.Fatal(err)
		}
		// PlayerInput.movement_epoch is field 2.
		return protowire.AppendVarint(protowire.AppendTag(b, 2, protowire.VarintType), epoch)
	}
	wide, err := DecodeInput(encode(math.MaxUint32+2), 7)
	if err != nil {
		t.Fatal(err)
	}
	narrow, err := DecodeInput(encode(1), 7)
	if err != nil || !proto.Equal(wide, narrow) || wide.MovementEpoch != 1 {
		t.Fatalf("2^32+1 decoded as %v, 1 as %v (%v)", wide, narrow, err)
	}
}
