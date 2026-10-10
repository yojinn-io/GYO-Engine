package gateway

import (
	"errors"
	"testing"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv6"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

func TestV5LifeTransitionCancelsOnlyMovementAndKeepsMixedLifeActions(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	move := &runtime.PlayerInput{PlayerId: 7, MovementEpoch: 1, LifeGeneration: 1, Commands: []*runtime.MovementCommand{{Sequence: 1, JumpRequested: true}}}
	if err := s.link.input(move); err != nil {
		t.Fatal(err)
	}
	p.commands[1] = proto.Clone(move.Commands[0]).(*runtime.MovementCommand)
	pending := &client.ShotRequest{ActionId: 1, ObservedAuthorityTick: 1, Kind: client.ActionKind_ACTION_SHOT, LifeGeneration: 1, Yaw: proto.Float32(0), Pitch: proto.Float32(0)}
	deliverActions(t, s, p, endpoint, 2, &client.ActionBatch{Shots: []*client.ShotRequest{pending}}, now)
	// A new authoritative life requires a new movement epoch but keeps action IDs.
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 200,
		Players: []*runtime.PlayerState{{PlayerId: 7, MovementEpoch: 2, LifeGeneration: 2, LifeState: runtime.LifeState_LIFE_ALIVE, LifeStateTick: 200, Grounded: true}},
		Combat:  []*runtime.CombatState{{PlayerId: 7, LifeGeneration: 2, Hp: 100, MagazineAmmo: 12}},
	}}
	s.runtimeMessage(e)
	if p.lifeGeneration != 2 || p.movementEpoch != 2 || len(p.commands) != 0 || len(s.link.inputs) != 0 ||
		len(p.actions.requests) != 1 || len(s.link.actionWindows[7].requests) != 1 {
		t.Fatal("respawn did not isolate movement from action retention")
	}
	reload := &client.ShotRequest{ActionId: 2, ObservedAuthorityTick: 200, Kind: client.ActionKind_ACTION_RELOAD, LifeGeneration: 2}
	future := &client.ShotRequest{ActionId: 3, ObservedAuthorityTick: 200, Kind: client.ActionKind_ACTION_RELOAD, LifeGeneration: 3}
	deliverActions(t, s, p, endpoint, 3, &client.ActionBatch{Shots: []*client.ShotRequest{pending, reload, future}}, now)
	if len(p.actions.requests) != 3 || len(s.link.actionWindows[7].requests) != 3 {
		t.Fatal("old/future life actions were discarded before authority")
	}
	for _, life := range []uint64{1, 3} {
		input := &client.PlayerInput{MovementEpoch: 2, LifeGeneration: life, Commands: []*client.MovementCommand{{Sequence: 1, JumpRequested: true}}}
		payload, _ := proto.Marshal(input)
		if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Input, SessionID: p.session.ID, Sequence: uint32(3 + life)}, payload, endpoint, now); err != nil {
			t.Fatal(err)
		}
		if len(p.commands) != 0 || len(s.link.inputs) != 0 {
			t.Fatal("wrong-life movement leaked")
		}
	}
	// An old life is refused for the rejected lane, never silently accepted.
	if err := s.link.input(move); !errors.Is(err, adapter.ErrInput) || len(s.link.inputs) != 0 {
		t.Fatal("link silently accepted a stale life")
	}
	move.MovementEpoch = 2
	move.LifeGeneration = 3
	if err := s.link.input(move); err == nil {
		t.Fatal("future input life accepted")
	}
	move.LifeGeneration = 2
	if err := s.link.input(move); err != nil {
		t.Fatal(err)
	}
	mutated := proto.Clone(move).(*runtime.PlayerInput)
	mutated.Commands[0].JumpRequested = false
	if err := s.link.input(mutated); err == nil {
		t.Fatal("jump intent changed inside immutable input")
	}
	s.link.acknowledge(7, 9, 1, 999)
	if s.link.lives[7] != 2 || s.link.epochs[7] != 2 || len(s.link.inputs[7].Commands) != 1 {
		t.Fatal("old-life authority ACK pruned new-life movement")
	}
	stale := &runtime.ShotDecision{ActionId: 1, ResolvedTick: 201, Kind: runtime.ActionKind_ACTION_SHOT, LifeGeneration: 1, Rejection: runtime.ShotRejection_REJECTION_STALE_LIFE}
	accepted := &runtime.ShotDecision{ActionId: 2, ResolvedTick: 201, Kind: runtime.ActionKind_ACTION_RELOAD, LifeGeneration: 2, Accepted: true}
	invalid := &runtime.ShotDecision{ActionId: 3, ResolvedTick: 201, Kind: runtime.ActionKind_ACTION_RELOAD, LifeGeneration: 3, Rejection: runtime.ShotRejection_REJECTION_INVALID_LIFE}
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{stale, accepted, invalid}}); err != nil {
		t.Fatal(err)
	}
	deliverActions(t, s, p, endpoint, 7, &client.ActionBatch{AcknowledgedThrough: 3}, now)
	if p.actions.acknowledged != 3 || s.link.actionWindows[7].acknowledged != 3 {
		t.Fatal("cross-life results could not close contiguous ACK")
	}
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, RetiredThrough: 3}); err != nil {
		t.Fatal(err)
	}
	if p.actions.retired != 3 || len(p.actions.requests) != 0 || len(s.link.actionWindows[7].requests) != 0 {
		t.Fatal("cross-life ledger failed to retire")
	}
}

func TestV5ActionConflictsAreAtomicAcrossLifeAndKind(t *testing.T) {
	for _, edit := range []func(*runtime.ShotRequest){
		func(s *runtime.ShotRequest) { s.LifeGeneration = 2 },
		func(s *runtime.ShotRequest) { s.Kind = runtime.ActionKind_ACTION_RELOAD; s.Yaw = nil; s.Pitch = nil },
	} {
		w := newActionWindow()
		original := shot(1)
		w.merge(&runtime.ActionBatch{PlayerId: 7, Shots: []*runtime.ShotRequest{original}})
		w.mergeResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}})
		conflict := proto.Clone(original).(*runtime.ShotRequest)
		edit(conflict)
		if err := w.validate(&runtime.ActionBatch{PlayerId: 7, AcknowledgedThrough: 1, Shots: []*runtime.ShotRequest{shot(2), conflict}}); err == nil {
			t.Fatal("changed kind/life admitted with ACK")
		}
		if w.acknowledged != 0 || len(w.requests) != 1 {
			t.Fatal("conflicting batch mutated ledger")
		}
	}
	w := newActionWindow()
	w.merge(&runtime.ActionBatch{PlayerId: 7, Shots: []*runtime.ShotRequest{shot(1)}})
	for _, edit := range []func(*runtime.ShotDecision){
		func(d *runtime.ShotDecision) { d.LifeGeneration++ },
		func(d *runtime.ShotDecision) { d.Kind = runtime.ActionKind_ACTION_RELOAD },
	} {
		bad := decision(1)
		edit(bad)
		if err := w.validateResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{bad}}); err == nil {
			t.Fatal("result identity disagreed with request")
		}
	}
}
