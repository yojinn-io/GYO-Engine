package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv3"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev3"
)

func TestInputIdentityMappingAndSchemaValidation(t *testing.T) {
	in := &client.PlayerInput{MovementEpoch: 1, Commands: []*client.MovementCommand{
		{Sequence: 10, MoveForward: 1, MoveRight: -1, Yaw: 1.2, Pitch: -0.3}, {Sequence: 11},
	}}
	bytes, err := proto.Marshal(in)
	if err != nil {
		t.Fatal(err)
	}
	got, err := DecodeInput(bytes, 7)
	if err != nil || got.PlayerId != 7 || len(got.Commands) != 2 || got.Commands[0].Sequence != 10 || got.Commands[0].MoveForward != 1 || got.Commands[0].Yaw != in.Commands[0].Yaw || got.Commands[1].Sequence != 11 {
		t.Fatalf("mapping %+v %v", got, err)
	}
	for _, commands := range [][]*client.MovementCommand{
		nil, {{Sequence: 0}}, {{Sequence: 1, MoveForward: 2}}, {{Sequence: 1, Yaw: float32(math.NaN())}},
		{{Sequence: 1, Pitch: 2}}, {{Sequence: 1, MoveRight: float32(math.Inf(1))}},
		{{Sequence: 1}, {Sequence: 1}}, {{Sequence: 2}, {Sequence: 1}},
	} {
		bad := &client.PlayerInput{MovementEpoch: 1, Commands: commands}
		b, _ := proto.Marshal(bad)
		if _, err := DecodeInput(b, 7); err == nil {
			t.Fatalf("bad input %+v accepted", bad)
		}
	}
	for _, bad := range [][]byte{{0xff}, {8, 1, 16, 2}, make([]byte, framing.MaxDatagram)} {
		if _, err := DecodeInput(bad, 7); err == nil {
			t.Fatal("malformed, v1, or oversized protobuf accepted")
		}
	}
	if _, err := DecodeInput(bytes, 0); err == nil {
		t.Fatal("zero player identity accepted")
	}
	boundary, _ := proto.Marshal(&client.PlayerInput{MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 1, Pitch: float32(math.Pi / 2)}}})
	if _, err := DecodeInput(boundary, 7); err != nil {
		t.Fatal("float32 pitch boundary differs from Match:", err)
	}
}

func TestMaximumCommandWindowFitsDatagram(t *testing.T) {
	in := &client.PlayerInput{MovementEpoch: math.MaxUint64}
	for i := 0; i < MaxPendingCommands; i++ {
		in.Commands = append(in.Commands, &client.MovementCommand{Sequence: math.MaxUint64 - MaxPendingCommands + 1 + uint64(i),
			MoveForward: -1, MoveRight: -1, Yaw: 1e6, Pitch: -float32(math.Pi / 2)})
	}
	bytes, err := proto.Marshal(in)
	if err != nil {
		t.Fatal(err)
	}
	if len(bytes)+framing.HeaderSize > framing.MaxDatagram {
		t.Fatal("full window exceeds UDP bound")
	}
	if _, err := DecodeInput(bytes, 1); err != nil {
		t.Fatal(err)
	}
	in.Commands = append(in.Commands, &client.MovementCommand{Sequence: math.MaxUint64})
	bytes, _ = proto.Marshal(in)
	if _, err := DecodeInput(bytes, 1); err == nil {
		t.Fatal("13-command window accepted")
	}
}

func TestSnapshotMappingDoesNotChangeWorldState(t *testing.T) {
	in := &runtime.WorldSnapshot{Tick: 3, Players: []*runtime.PlayerState{{MovementEpoch: 1, PlayerId: 7, X: 123, Y: 2, Z: -10, Yaw: 1, LastResolvedCommand: 88}}}
	out, err := SnapshotForClient(in)
	if err != nil {
		t.Fatal(err)
	}
	b, err := proto.Marshal(out)
	if err != nil {
		t.Fatal(err)
	}
	var decoded client.WorldSnapshot
	if err := proto.Unmarshal(b, &decoded); err != nil {
		t.Fatal(err)
	}
	if decoded.Tick != 3 || decoded.Players[0].X != 123 || decoded.Players[0].LastResolvedCommand != 88 {
		t.Fatal("state changed")
	}
}

func TestEpochAndContiguousQueueContract(t *testing.T) {
	input := &client.PlayerInput{MovementEpoch: 9, Commands: []*client.MovementCommand{{Sequence: 1}}}
	payload, _ := proto.Marshal(input)
	mapped, err := DecodeInput(payload, 1)
	if err != nil || mapped.MovementEpoch != 9 {
		t.Fatalf("epoch lost: %v %v", mapped, err)
	}
	input.MovementEpoch = 0
	payload, _ = proto.Marshal(input)
	if _, err := DecodeInput(payload, 1); err == nil {
		t.Fatal("epoch zero accepted")
	}
	state := &runtime.PlayerState{PlayerId: 1, MovementEpoch: 9, LastResolvedCommand: 2, ContiguousPendingCommands: MaxFutureCommands}
	snapshot := &runtime.WorldSnapshot{Tick: 8, Players: []*runtime.PlayerState{state}}
	out, err := SnapshotForClient(snapshot)
	if err != nil || out.Players[0].MovementEpoch != 9 || out.Players[0].ContiguousPendingCommands != MaxFutureCommands {
		t.Fatalf("authority metadata lost: %v %v", out, err)
	}
	state.ContiguousPendingCommands++
	if _, err := SnapshotForClient(snapshot); err == nil {
		t.Fatal("unbounded authority queue accepted")
	}
	state.ContiguousPendingCommands = 0
	state.MovementEpoch = 0
	if _, err := SnapshotForClient(snapshot); err == nil {
		t.Fatal("epoch-zero authority accepted")
	}
}
