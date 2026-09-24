package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/proto"
	client "gyo.local/object_fps_pvp/protocol/clientv1"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev1"
)

func TestInputIdentityMappingAndSchemaValidation(t *testing.T) {
	in := &client.PlayerInput{InputSequence: 10, ClientTick: 55, MoveForward: 1, MoveRight: -1, Yaw: 1.2, Pitch: -0.3}
	bytes, err := proto.Marshal(in)
	if err != nil {
		t.Fatal(err)
	}
	got, err := DecodeInput(bytes, 7)
	if err != nil || got.PlayerId != 7 || got.InputSequence != 10 || got.ClientTick != 55 || got.MoveForward != 1 || got.Yaw != in.Yaw {
		t.Fatalf("mapping %+v %v", got, err)
	}
	for _, bad := range []*client.PlayerInput{{InputSequence: 1, MoveForward: 2}, {InputSequence: 1, Yaw: float32(math.NaN())}, {InputSequence: 1, Pitch: 2}, {}} {
		b, _ := proto.Marshal(bad)
		if _, err := DecodeInput(b, 7); err == nil {
			t.Fatalf("bad input %+v accepted", bad)
		}
	}
	if _, err := DecodeInput([]byte{0xff}, 7); err == nil {
		t.Fatal("malformed protobuf accepted")
	}
}
func TestSnapshotMappingDoesNotChangeWorldState(t *testing.T) {
	in := &runtime.WorldSnapshot{Tick: 3, Players: []*runtime.PlayerState{{PlayerId: 7, X: 123, Y: 2, Z: -10, Yaw: 1, LastInputSequence: 88}}}
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
	if decoded.Tick != 3 || decoded.Players[0].X != 123 || decoded.Players[0].LastInputSequence != 88 {
		t.Fatal("state changed")
	}
}
