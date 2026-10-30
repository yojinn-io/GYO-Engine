package adapter

import (
	"math"
	"testing"

	"google.golang.org/protobuf/proto"
	client "gyo.local/object_fps_pvp/protocol/clientv8"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev8"
)

// pv7 §5 (D-4): the Match's hold reaches the Client as it came, its presence
// included, and no value of it invalidates a snapshot (D-5).
func TestSnapshotCarriesTheMatchHoldAsItCame(t *testing.T) {
	for _, hold := range []*uint32{nil, proto.Uint32(0), proto.Uint32(1234), proto.Uint32(math.MaxUint32)} {
		out, err := SnapshotForClient(&runtime.WorldSnapshot{Tick: 7, MatchHoldUs: hold}, nil)
		if err != nil {
			t.Fatalf("hold %v: %v", hold, err)
		}
		if (out.MatchHoldUs == nil) != (hold == nil) || out.GetMatchHoldUs() != (&runtime.WorldSnapshot{MatchHoldUs: hold}).GetMatchHoldUs() {
			t.Fatalf("hold %v became %v", hold, out.MatchHoldUs)
		}
		if out.GatewayHoldUs != nil {
			t.Fatal("the adapter filled the Gateway's hold; each write fills it")
		}
	}
}

// pv7 §5 (D-6): the refusal fields reach the Client as the Match sent them, an
// unknown reason included (diagnostics have no veto, D-5).
func TestResultsCarryTheRefusalsAsTheyCame(t *testing.T) {
	for _, refusal := range []runtime.ActionRefusal{runtime.ActionRefusal_ACTION_REFUSAL_UNSPECIFIED,
		runtime.ActionRefusal_ACTION_REFUSAL_RESETTING, runtime.ActionRefusal_ACTION_REFUSAL_FULL, runtime.ActionRefusal(99)} {
		out, err := ResultsForClient(&runtime.ActionResults{PlayerId: 1, RetiredThrough: 3, RefusedBatches: 41,
			LastRefusal: refusal}, testRules())
		if err != nil {
			t.Fatal(err)
		}
		if out.RefusedBatches != 41 || int32(out.LastRefusal) != int32(refusal) || out.RetiredThrough != 3 {
			t.Fatalf("refusal %v became %v", refusal, out)
		}
	}
	if client.ActionRefusal_ACTION_REFUSAL_FULL.Number() != runtime.ActionRefusal_ACTION_REFUSAL_FULL.Number() ||
		len(client.ActionRefusal_name) != len(runtime.ActionRefusal_name) {
		t.Fatal("the client and runtime ActionRefusal differ")
	}
	for number, name := range runtime.ActionRefusal_name {
		if client.ActionRefusal_name[number] != name {
			t.Fatalf("ActionRefusal %d: runtime %s, client %s", number, name, client.ActionRefusal_name[number])
		}
	}
}
