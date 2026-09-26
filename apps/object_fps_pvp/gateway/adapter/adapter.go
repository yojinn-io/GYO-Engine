// Package adapter maps Object FPS contracts. It never simulates gameplay.
package adapter

import (
	"errors"
	"math"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv3"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev3"
)

const ClientVersion uint16 = 3
const RuntimeVersion uint32 = 3
const MaxPlayers = 2
const MaxPendingCommands = 12
const MaxFutureCommands = 32
const AuthorityTickRate = 60
const SnapshotIntervalTicks = 1
const InputSendRate = 60
const (
	Hello uint16 = iota + 1
	Welcome
	Input
	Snapshot
	Failure
)

var ErrInput = errors.New("invalid player input schema or range")

func DecodeInput(payload []byte, playerID uint64) (*runtime.PlayerInput, error) {
	var in client.PlayerInput
	if playerID == 0 || len(payload)+framing.HeaderSize > framing.MaxDatagram {
		return nil, ErrInput
	}
	if err := proto.Unmarshal(payload, &in); err != nil || in.MovementEpoch == 0 || len(in.Commands) == 0 || len(in.Commands) > MaxPendingCommands {
		return nil, ErrInput
	}
	out := &runtime.PlayerInput{PlayerId: playerID, MovementEpoch: in.MovementEpoch}
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
			MoveForward: command.MoveForward, MoveRight: command.MoveRight, Yaw: command.Yaw, Pitch: command.Pitch})
	}
	return out, nil
}

// EqualCommand compares the application contract, not protobuf bookkeeping.
func EqualCommand(a, b *runtime.MovementCommand) bool {
	return a != nil && b != nil && a.Sequence == b.Sequence && a.MoveForward == b.MoveForward &&
		a.MoveRight == b.MoveRight && a.Yaw == b.Yaw && a.Pitch == b.Pitch
}

func SnapshotForClient(in *runtime.WorldSnapshot) (*client.WorldSnapshot, error) {
	if in == nil || len(in.Players) > MaxPlayers {
		return nil, errors.New("invalid runtime snapshot")
	}
	out := &client.WorldSnapshot{Tick: in.Tick}
	seen := make(map[uint64]bool, len(in.Players))
	for _, p := range in.Players {
		if p == nil || p.PlayerId == 0 || seen[p.PlayerId] || !finite(p.X) || !finite(p.Y) ||
			!finite(p.Z) || !finite(p.Yaw) || !finite(p.Pitch) || math.Abs(float64(p.Yaw)) > 1e6 ||
			math.Abs(float64(p.Pitch)) > float64(float32(math.Pi/2)) || p.MovementEpoch == 0 ||
			p.ContiguousPendingCommands > MaxFutureCommands {
			return nil, errors.New("invalid runtime player state")
		}
		seen[p.PlayerId] = true
		out.Players = append(out.Players, &client.PlayerState{PlayerId: p.PlayerId, X: p.X,
			Y: p.Y, Z: p.Z, Yaw: p.Yaw, Pitch: p.Pitch, LastResolvedCommand: p.LastResolvedCommand,
			MovementEpoch: p.MovementEpoch, ContiguousPendingCommands: p.ContiguousPendingCommands})
	}
	return out, nil
}

func finite(v float32) bool { return !math.IsNaN(float64(v)) && !math.IsInf(float64(v), 0) }
