// Package adapter maps Object FPS contracts. It never simulates gameplay.
package adapter

import (
	"errors"
	"math"

	"google.golang.org/protobuf/proto"
	client "gyo.local/object_fps_pvp/protocol/clientv1"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev1"
)

const ClientVersion uint16 = 1
const RuntimeVersion uint32 = 1
const MaxPlayers = 2
const AuthorityTickRate = 60
const SnapshotIntervalTicks = 3
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
	if err := proto.Unmarshal(payload, &in); err != nil {
		return nil, ErrInput
	}
	if in.InputSequence == 0 || !finite(in.MoveForward) || !finite(in.MoveRight) ||
		!finite(in.Yaw) || !finite(in.Pitch) || math.Abs(float64(in.MoveForward)) > 1 ||
		math.Abs(float64(in.MoveRight)) > 1 || math.Abs(float64(in.Yaw)) > 1e6 ||
		math.Abs(float64(in.Pitch)) > math.Pi/2 {
		return nil, ErrInput
	}
	return &runtime.PlayerInput{PlayerId: playerID, InputSequence: in.InputSequence,
		ClientTick: in.ClientTick, MoveForward: in.MoveForward, MoveRight: in.MoveRight,
		Yaw: in.Yaw, Pitch: in.Pitch}, nil
}

func SnapshotForClient(in *runtime.WorldSnapshot) (*client.WorldSnapshot, error) {
	if in == nil || len(in.Players) > MaxPlayers {
		return nil, errors.New("invalid runtime snapshot")
	}
	out := &client.WorldSnapshot{Tick: in.Tick}
	seen := make(map[uint64]bool, len(in.Players))
	for _, p := range in.Players {
		if p == nil || p.PlayerId == 0 || seen[p.PlayerId] || !finite(p.X) || !finite(p.Y) ||
			!finite(p.Z) || !finite(p.Yaw) || !finite(p.Pitch) {
			return nil, errors.New("invalid runtime player state")
		}
		seen[p.PlayerId] = true
		out.Players = append(out.Players, &client.PlayerState{PlayerId: p.PlayerId, X: p.X,
			Y: p.Y, Z: p.Z, Yaw: p.Yaw, Pitch: p.Pitch, LastInputSequence: p.LastInputSequence})
	}
	return out, nil
}

func finite(v float32) bool { return !math.IsNaN(float64(v)) && !math.IsInf(float64(v), 0) }
