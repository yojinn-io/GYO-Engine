package gateway

import (
	"testing"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

func TestV5LifeTransitionCancelsOnlyMovementAndKeepsMixedLifeActions(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	pending := &client.ShotRequest{ActionId: 1, ObservedAuthorityTick: 1, Kind: client.ActionKind_ACTION_SHOT, LifeGeneration: 1, Yaw: proto.Float32(0), Pitch: proto.Float32(0)}
	deliverActions(t, s, p, endpoint, 2, &client.ActionBatch{Shots: []*client.ShotRequest{pending}}, now)
	// A new authoritative life requires a new movement epoch but keeps action IDs.
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 200,
		Players: []*runtime.PlayerState{{PlayerId: 7, MovementEpoch: 2, LifeGeneration: 2, LifeState: runtime.LifeState_LIFE_ALIVE, LifeStateTick: 200, Grounded: true}},
		Combat:  []*runtime.CombatState{{PlayerId: 7, LifeGeneration: 2, Hp: 100, MagazineAmmo: 12}},
	}}
	s.runtimeMessage(e)
	if got := actionMessages(s.link.batch()); len(got) != 1 || len(got[0].Shots) != 1 {
		t.Fatal("respawn did not isolate movement from the queued actions")
	}
	reload := &client.ShotRequest{ActionId: 2, ObservedAuthorityTick: 200, Kind: client.ActionKind_ACTION_RELOAD, LifeGeneration: 2}
	future := &client.ShotRequest{ActionId: 3, ObservedAuthorityTick: 200, Kind: client.ActionKind_ACTION_RELOAD, LifeGeneration: 3}
	deliverActions(t, s, p, endpoint, 3, &client.ActionBatch{Shots: []*client.ShotRequest{pending, reload, future}}, now)
	if got := actionMessages(s.link.batch()); len(got) != 1 || len(got[0].Shots) != 3 {
		t.Fatal("old/future life actions were discarded before authority")
	}
	// Movement of any life reaches the Match as it came, which judges it
	// (pv7 §2a); the Gateway keeps no life or epoch of its own.
	for _, life := range []uint32{1, 3} {
		input := &client.PlayerInput{MovementEpoch: 2, LifeGeneration: life, Commands: []*client.MovementCommand{{Sequence: 1, JumpRequested: true}}}
		payload, _ := proto.Marshal(input)
		if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Input, SessionID: p.session.ID, Sequence: uint32(3 + life)}, payload, endpoint, now); err != nil {
			t.Fatal(err)
		}
	}
	var lives []uint32
	for _, message := range s.link.batch() {
		if in := message.GetInput(); in != nil {
			lives = append(lives, in.LifeGeneration)
		}
	}
	if len(lives) != 2 || lives[0] != 1 || lives[1] != 3 {
		t.Fatalf("movement of another life was not forwarded as it came: %v", lives)
	}
	stale := &runtime.ShotDecision{ActionId: 1, ResolvedTick: 201, Kind: runtime.ActionKind_ACTION_SHOT, LifeGeneration: 1, Rejection: runtime.ShotRejection_REJECTION_STALE_LIFE}
	accepted := &runtime.ShotDecision{ActionId: 2, ResolvedTick: 201, Kind: runtime.ActionKind_ACTION_RELOAD, LifeGeneration: 2, Accepted: true}
	invalid := &runtime.ShotDecision{ActionId: 3, ResolvedTick: 201, Kind: runtime.ActionKind_ACTION_RELOAD, LifeGeneration: 3, Rejection: runtime.ShotRejection_REJECTION_INVALID_LIFE}
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{stale, accepted, invalid}}); err != nil {
		t.Fatal(err)
	}
	// The cross-life ACK reaches the Match, which keeps the ledger (pv7 §2a).
	deliverActions(t, s, p, endpoint, 7, &client.ActionBatch{AcknowledgedThrough: 3}, now)
	if got := actionMessages(s.link.batch()); len(got) != 1 || got[0].AcknowledgedThrough != 3 {
		t.Fatal("the cross-life ACK did not reach the Match")
	}
}

func actionMessages(batch []*runtime.RuntimeEnvelope) []*runtime.ActionBatch {
	var out []*runtime.ActionBatch
	for _, e := range batch {
		if in := e.GetActions(); in != nil {
			out = append(out, in)
		}
	}
	return out
}
