// Package adapter maps Object FPS contracts. It never simulates gameplay.
package adapter

import (
	"errors"
	"fmt"
	"math"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv5"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev5"
)

const ClientVersion uint16 = 5
const RuntimeVersion uint32 = 5
const MaxPlayers = 2
const MaxPendingCommands = 12
const MaxFutureCommands = 32
const MaxMovementSlackUs = 1_000_000

// Consecutive failed connection-quality windows before the Match evicts.
const ConnectionQualityFailedWindows = 3
const AuthorityTickRate = 60
const SnapshotIntervalTicks = 1
const InputSendRate = 60
const MaxActions = 32
const MaxActionBatch = 8
const ActionSendRate = 30
const (
	Hello uint16 = iota + 1
	Welcome
	Input
	Snapshot
	Failure
	Actions
	ActionResults
)

var ErrInput = errors.New("invalid player input schema or range")

func DecodeInput(payload []byte, playerID uint64) (*runtime.PlayerInput, error) {
	var in client.PlayerInput
	if playerID == 0 || len(payload)+framing.HeaderSize > framing.MaxDatagram {
		return nil, ErrInput
	}
	if err := proto.Unmarshal(payload, &in); err != nil || in.MovementEpoch == 0 || in.LifeGeneration == 0 || len(in.Commands) == 0 || len(in.Commands) > MaxPendingCommands {
		return nil, ErrInput
	}
	out := &runtime.PlayerInput{PlayerId: playerID, MovementEpoch: in.MovementEpoch, LifeGeneration: in.LifeGeneration,
		ObservedAuthorityTick: in.ObservedAuthorityTick}
	var previous uint64
	for _, command := range in.Commands {
		if command == nil || command.Sequence <= previous || !finite(command.MoveForward) || !finite(command.MoveRight) ||
			!finite(command.Yaw) || !finite(command.Pitch) || math.Abs(float64(command.MoveForward)) > 1 ||
			math.Abs(float64(command.MoveRight)) > 1 || math.Abs(float64(command.Yaw)) > 1e6 ||
			math.Abs(float64(command.Pitch)) > float64(float32(math.Pi/2)) {
			return nil, ErrInput
		}
		previous = command.Sequence
		out.Commands = append(out.Commands, &runtime.MovementCommand{Sequence: command.Sequence,
			MoveForward: command.MoveForward, MoveRight: command.MoveRight, Yaw: command.Yaw, Pitch: command.Pitch, JumpRequested: command.JumpRequested})
	}
	return out, nil
}

// EqualCommand compares the application contract, not protobuf bookkeeping.
func EqualCommand(a, b *runtime.MovementCommand) bool {
	return a != nil && b != nil && a.Sequence == b.Sequence && a.MoveForward == b.MoveForward &&
		a.MoveRight == b.MoveRight && a.Yaw == b.Yaw && a.Pitch == b.Pitch && a.JumpRequested == b.JumpRequested
}

func SnapshotForClient(in *runtime.WorldSnapshot, rules *runtime.CombatRules) (*client.WorldSnapshot, error) {
	if in == nil || len(in.Players) > MaxPlayers || len(in.Combat) != len(in.Players) || (len(in.Combat) > 0 && !ValidRules(rules)) {
		return nil, errors.New("invalid runtime snapshot")
	}
	out := &client.WorldSnapshot{Tick: in.Tick}
	seen := make(map[uint64]*runtime.PlayerState, len(in.Players))
	for _, p := range in.Players {
		if p == nil || p.PlayerId == 0 || seen[p.PlayerId] != nil || !finite(p.X) || !finite(p.Y) ||
			!finite(p.Z) || !finite(p.Yaw) || !finite(p.Pitch) || math.Abs(float64(p.Yaw)) > 1e6 ||
			math.Abs(float64(p.Pitch)) > float64(float32(math.Pi/2)) || p.MovementEpoch == 0 ||
			p.ContiguousPendingCommands > MaxFutureCommands || !finite(p.VerticalVelocity) || p.LifeGeneration == 0 ||
			(p.LifeState != runtime.LifeState_LIFE_ALIVE && p.LifeState != runtime.LifeState_LIFE_DEAD) ||
			p.LifeStateTick > in.Tick || (p.LifeState == runtime.LifeState_LIFE_ALIVE && p.RespawnTick != 0) ||
			(p.LifeState == runtime.LifeState_LIFE_DEAD && p.RespawnTick <= p.LifeStateTick) ||
			(p.MovementSlackSequence == nil) != (p.MovementSlackUs == nil) ||
			(p.MovementSlackUs != nil && (*p.MovementSlackUs > MaxMovementSlackUs || *p.MovementSlackUs < -MaxMovementSlackUs)) ||
			(p.MovementSlackSequence != nil && (*p.MovementSlackSequence == 0 || *p.MovementSlackSequence > p.LastResolvedCommand)) ||
			p.ConnectionQualityFailures >= ConnectionQualityFailedWindows {
			return nil, errors.New("invalid runtime player state")
		}
		seen[p.PlayerId] = p
		lifeState := client.LifeState_LIFE_ALIVE
		if p.LifeState == runtime.LifeState_LIFE_DEAD {
			lifeState = client.LifeState_LIFE_DEAD
		}
		out.Players = append(out.Players, &client.PlayerState{PlayerId: p.PlayerId, X: p.X,
			Y: p.Y, Z: p.Z, Yaw: p.Yaw, Pitch: p.Pitch, LastResolvedCommand: p.LastResolvedCommand,
			MovementEpoch: p.MovementEpoch, ContiguousPendingCommands: p.ContiguousPendingCommands,
			VerticalVelocity: p.VerticalVelocity, Grounded: p.Grounded, LifeGeneration: p.LifeGeneration,
			LifeState: lifeState, LifeStateTick: p.LifeStateTick, RespawnTick: p.RespawnTick,
			MovementSlackSequence: p.MovementSlackSequence, MovementSlackUs: p.MovementSlackUs,
			ConnectionQualityFailures: p.ConnectionQualityFailures})
	}
	combatSeen := make(map[uint64]bool, len(in.Combat))
	for _, state := range in.Combat {
		if state == nil || seen[state.PlayerId] == nil || combatSeen[state.PlayerId] || state.Hp > rules.MaximumHp ||
			state.LifeGeneration != seen[state.PlayerId].LifeGeneration || state.MagazineAmmo > rules.MagazineCapacity ||
			(state.Hp == 0) != (seen[state.PlayerId].LifeState == runtime.LifeState_LIFE_DEAD) ||
			(state.ReloadActionId == 0 && (state.ReloadStartTick != 0 || state.ReloadEndTick != 0)) ||
			(state.ReloadActionId != 0 && (state.Hp == 0 || state.ReloadStartTick > in.Tick || state.ReloadEndTick <= in.Tick ||
				state.ReloadEndTick <= state.ReloadStartTick || state.ReloadEndTick-state.ReloadStartTick != rules.ReloadTicks)) ||
			(state.LastShotActionId == 0) != (state.LastShotTick == 0) || state.LastShotTick > in.Tick {
			return nil, errors.New("invalid runtime combat state")
		}
		combatSeen[state.PlayerId] = true
		out.Combat = append(out.Combat, &client.CombatState{PlayerId: state.PlayerId, Hp: state.Hp,
			NextAllowedShotTick: state.NextAllowedShotTick, LifeGeneration: state.LifeGeneration,
			MagazineAmmo: state.MagazineAmmo, ReloadActionId: state.ReloadActionId, ReloadStartTick: state.ReloadStartTick,
			ReloadEndTick: state.ReloadEndTick, LastShotActionId: state.LastShotActionId, LastShotTick: state.LastShotTick})
	}
	return out, nil
}

// EvictionNotice is the Client error for a Match eviction: a stable code and
// a readable message with the last failed window's measurements.
func EvictionNotice(e *runtime.PlayerEvicted) (string, string) {
	if e.Reason == runtime.EvictionReason_EVICTION_HIGH_LATENCY {
		return "evicted_high_latency", fmt.Sprintf("Removed from the match: latency too high (about %d ms)", e.ReferenceAgeMs)
	}
	return "evicted_unstable_input", fmt.Sprintf("Removed from the match: unstable input (%.1f%% of movement lost, %d resets)",
		float64(e.SubstitutedPermille)/10, e.MovementResets)
}

func finite(v float32) bool { return !math.IsNaN(float64(v)) && !math.IsInf(float64(v), 0) }

// Rules originate in Match. The adapter validates their contract, not policy defaults.
func ValidRules(r *runtime.CombatRules) bool {
	return r != nil && r.MaximumHp > 0 && r.ShotDamage > 0 && r.ShotDamage <= r.MaximumHp && r.CooldownTicks > 0 && finite(r.ShotRange) && r.ShotRange > 0 && r.MaximumReferenceAgeMs > 0 && r.MagazineCapacity > 0 && r.ReloadTicks > 0 && r.RespawnTicks > 0
}
func RulesForClient(r *runtime.CombatRules) *client.CombatRules {
	return &client.CombatRules{MaximumHp: r.MaximumHp, ShotDamage: r.ShotDamage, CooldownTicks: r.CooldownTicks, ShotRange: r.ShotRange, MaximumReferenceAgeMs: r.MaximumReferenceAgeMs, MagazineCapacity: r.MagazineCapacity, ReloadTicks: r.ReloadTicks, RespawnTicks: r.RespawnTicks}
}
func ValidMovementRules(jumpHeight, gravity float32) bool {
	return finite(jumpHeight) && jumpHeight > 0 && finite(gravity) && gravity > 0 &&
		math.Sqrt(2*float64(jumpHeight)*float64(gravity)) <= math.MaxFloat32
}

var ErrActions = errors.New("invalid action batch schema, window, acknowledgement or immutable content")

func ValidShot(s *runtime.ShotRequest) bool {
	if s == nil || s.ActionId == 0 || s.LifeGeneration == 0 {
		return false
	}
	switch s.Kind {
	case runtime.ActionKind_ACTION_SHOT:
		return s.Yaw != nil && s.Pitch != nil && finite(s.GetYaw()) && finite(s.GetPitch()) && math.Abs(float64(s.GetYaw())) <= 1e6 && math.Abs(float64(s.GetPitch())) <= float64(float32(math.Pi/2))
	case runtime.ActionKind_ACTION_RELOAD:
		return s.Yaw == nil && s.Pitch == nil
	default:
		return false
	}
}
func EqualShot(a, b *runtime.ShotRequest) bool {
	return a != nil && b != nil && a.ActionId == b.ActionId && a.ObservedAuthorityTick == b.ObservedAuthorityTick &&
		a.Kind == b.Kind && a.LifeGeneration == b.LifeGeneration && (a.Yaw == nil) == (b.Yaw == nil) &&
		(a.Pitch == nil) == (b.Pitch == nil) && a.GetYaw() == b.GetYaw() && a.GetPitch() == b.GetPitch()
}
func DecodeActions(payload []byte, playerID uint64) (*runtime.ActionBatch, error) {
	var in client.ActionBatch
	if playerID == 0 || len(payload)+framing.HeaderSize > framing.MaxDatagram || proto.Unmarshal(payload, &in) != nil || len(in.Shots) > MaxActionBatch {
		return nil, ErrActions
	}
	out := &runtime.ActionBatch{PlayerId: playerID, AcknowledgedThrough: in.AcknowledgedThrough}
	seen := map[uint64]*runtime.ShotRequest{}
	for _, shot := range in.Shots {
		if shot == nil {
			return nil, ErrActions
		}
		var kind runtime.ActionKind
		switch shot.Kind {
		case client.ActionKind_ACTION_SHOT:
			kind = runtime.ActionKind_ACTION_SHOT
		case client.ActionKind_ACTION_RELOAD:
			kind = runtime.ActionKind_ACTION_RELOAD
		default:
			return nil, ErrActions
		}
		s := &runtime.ShotRequest{ActionId: shot.ActionId, ObservedAuthorityTick: shot.ObservedAuthorityTick,
			Yaw: shot.Yaw, Pitch: shot.Pitch, Kind: kind, LifeGeneration: shot.LifeGeneration}
		if !ValidShot(s) {
			return nil, ErrActions
		}
		if prior := seen[s.ActionId]; prior != nil {
			if !EqualShot(prior, s) {
				return nil, ErrActions
			}
			continue
		}
		seen[s.ActionId] = s
		out.Shots = append(out.Shots, s)
	}
	return out, nil
}
func ResultsForClient(in *runtime.ActionResults, rules *runtime.CombatRules) (*client.ActionResults, error) {
	if in == nil || in.PlayerId == 0 || len(in.Decisions) > MaxActionBatch || !ValidRules(rules) {
		return nil, ErrActions
	}
	out := &client.ActionResults{RetiredThrough: in.RetiredThrough}
	seen := map[uint64]bool{}
	for _, d := range in.Decisions {
		if d == nil || d.ActionId == 0 || d.ResolvedTick == 0 || d.LifeGeneration == 0 || seen[d.ActionId] || d.Rejection < runtime.ShotRejection_REJECTION_NONE || d.Rejection > runtime.ShotRejection_REJECTION_MAGAZINE_FULL || d.HitKind < runtime.ShotHitKind_HIT_MISS || d.HitKind > runtime.ShotHitKind_HIT_PLAYER || d.Damage > rules.ShotDamage || (d.Kind != runtime.ActionKind_ACTION_SHOT && d.Kind != runtime.ActionKind_ACTION_RELOAD) {
			return nil, ErrActions
		}
		if d.Accepted != (d.Rejection == runtime.ShotRejection_REJECTION_NONE) || (!d.Accepted && (d.HitKind != runtime.ShotHitKind_HIT_MISS || d.Damage != 0 || d.TargetId != 0 || d.TargetLifeGeneration != 0)) || (d.HitKind == runtime.ShotHitKind_HIT_PLAYER && (d.TargetId == 0 || d.TargetLifeGeneration == 0)) || (d.HitKind != runtime.ShotHitKind_HIT_PLAYER && (d.TargetId != 0 || d.Damage != 0 || d.TargetLifeGeneration != 0)) || (d.Kind == runtime.ActionKind_ACTION_RELOAD && (d.HitKind != runtime.ShotHitKind_HIT_MISS || d.Rejection == runtime.ShotRejection_REJECTION_COOLDOWN || d.Rejection == runtime.ShotRejection_REJECTION_EMPTY_MAGAZINE)) || (d.Kind == runtime.ActionKind_ACTION_SHOT && d.Rejection == runtime.ShotRejection_REJECTION_MAGAZINE_FULL) {
			return nil, ErrActions
		}
		seen[d.ActionId] = true
		kind := client.ActionKind_ACTION_SHOT
		if d.Kind == runtime.ActionKind_ACTION_RELOAD {
			kind = client.ActionKind_ACTION_RELOAD
		}
		out.Decisions = append(out.Decisions, &client.ShotDecision{ActionId: d.ActionId, ResolvedTick: d.ResolvedTick, Accepted: d.Accepted, Rejection: client.ShotRejection(d.Rejection), HitKind: client.ShotHitKind(d.HitKind), TargetId: d.TargetId, Damage: d.Damage, Kind: kind, LifeGeneration: d.LifeGeneration, TargetLifeGeneration: d.TargetLifeGeneration})
	}
	return out, nil
}

func EqualDecision(a, b *runtime.ShotDecision) bool {
	return a != nil && b != nil && a.ActionId == b.ActionId && a.ResolvedTick == b.ResolvedTick && a.Accepted == b.Accepted && a.Rejection == b.Rejection && a.HitKind == b.HitKind && a.TargetId == b.TargetId && a.Damage == b.Damage && a.Kind == b.Kind && a.LifeGeneration == b.LifeGeneration && a.TargetLifeGeneration == b.TargetLifeGeneration
}

// Bound the dynamic arena descriptor using the largest legal identity fields,
// so every Welcome built from an admitted Ready fits the complete UDP envelope.
func ReadyFitsWelcome(r *runtime.Ready) bool {
	if r == nil || r.SnapshotIntervalTicks == 0 || !ValidRules(r.CombatRules) || !ValidMovementRules(r.JumpHeight, r.Gravity) {
		return false
	}
	welcome := &client.Welcome{PlayerId: math.MaxUint64, MatchId: math.MaxUint64, TickRate: r.TickRate, SnapshotRate: r.TickRate / r.SnapshotIntervalTicks, ArenaId: r.ArenaId, ArenaVersion: r.ArenaVersion, CombatRules: RulesForClient(r.CombatRules), JumpHeight: r.JumpHeight, Gravity: r.Gravity}
	return proto.Size(welcome)+framing.HeaderSize <= framing.MaxDatagram
}
