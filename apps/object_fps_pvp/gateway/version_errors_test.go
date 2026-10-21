package gateway

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"strconv"
	"strings"
	"testing"
	"time"

	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

// pv7 contract §1: a refused join names the Gateway's own version; every
// protocol_version key is its sender's. The version is read before the rest of
// the body, so keys or a request_id this version rejects do not hide another
// version.
func TestJoinRefusalNamesTheGatewayVersion(t *testing.T) {
	logs := captureIngressLog(t)
	s, _ := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != http.StatusOK {
		t.Fatal(status)
	}
	older, newer := adapter.ClientVersion-1, adapter.ClientVersion+1
	for _, request := range []map[string]any{
		{"request_id": "old", "protocol_version": older},
		{"request_id": "new", "protocol_version": newer, "client_reason": 1},
		{"request_id": "", "protocol_version": newer},
		{"request_id": "missing"},
	} {
		status, body := post(t, s, "/rooms/1/join", request)
		if status != http.StatusConflict {
			t.Fatalf("%v: status %d %s", request, status, body)
		}
		var reply map[string]any
		if err := json.Unmarshal(body, &reply); err != nil {
			t.Fatal(err)
		}
		if len(reply) != 2 || reply["error"] != "protocol_version" || reply["protocol_version"] != float64(adapter.ClientVersion) {
			t.Fatalf("%v: refusal body %s", request, body)
		}
	}
	logs.mu.Lock()
	text := logs.buf.String()
	logs.mu.Unlock()
	for _, received := range []uint16{older, newer, 0} {
		if want := fmt.Sprintf("code=protocol_version expected=%d received=%d\n", adapter.ClientVersion, received); !strings.Contains(text, want) {
			t.Fatalf("no refusal log %q in\n%s", want, text)
		}
	}
}

// What is not a version is a malformed join (400), as before; with this
// version the rest of the body is then checked as before.
func TestMalformedJoinsAreNotVersionRefusals(t *testing.T) {
	s, _ := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != http.StatusOK {
		t.Fatal(status)
	}
	current := adapter.ClientVersion
	for _, c := range []struct{ body, code string }{
		{`{"request_id":"x","protocol_version":"7"}`, "invalid_join"},
		{`{"request_id":"x","protocol_version":-1}`, "invalid_join"},
		{`{"request_id":"x","protocol_version":65536}`, "invalid_join"},
		{`{"request_id":"x","protocol_version":7.5}`, "invalid_join"},
		{`[]`, "invalid_join"},
		{fmt.Sprintf(`{"request_id":"x","protocol_version":%d} {}`, current), "invalid_join"},
		{fmt.Sprintf(`{"request_id":"x","protocol_version":%d,"client_reason":1}`, current), "invalid_join"},
		{fmt.Sprintf(`{"request_id":"","protocol_version":%d}`, current), "invalid_request_id"},
	} {
		response, err := http.Post("http://"+s.HTTPAddress()+"/rooms/1/join", "application/json", strings.NewReader(c.body))
		if err != nil {
			t.Fatal(err)
		}
		body, _ := io.ReadAll(response.Body)
		response.Body.Close()
		var reply map[string]any
		if response.StatusCode != http.StatusBadRequest || json.Unmarshal(body, &reply) != nil || reply["error"] != c.code {
			t.Fatalf("%s: %d %s, want 400 %s", c.body, response.StatusCode, body, c.code)
		}
	}
}

// otherVersionPayload is an envelope of the given version whose Ready this
// schema cannot parse: its arena_id is not UTF-8. Only field 1 can be read.
func otherVersionPayload(t *testing.T, version uint32) []byte {
	t.Helper()
	readyField := (&runtime.RuntimeEnvelope{}).ProtoReflect().Descriptor().Fields().ByName("ready").Number()
	arenaField := (&runtime.Ready{}).ProtoReflect().Descriptor().Fields().ByName("arena_id").Number()
	ready := protowire.AppendBytes(protowire.AppendTag(nil, arenaField, protowire.BytesType), []byte{0xff, 0xfe})
	payload := protowire.AppendVarint(protowire.AppendTag(nil, 1, protowire.VarintType), uint64(version))
	payload = protowire.AppendBytes(protowire.AppendTag(payload, readyField, protowire.BytesType), ready)
	if proto.Unmarshal(payload, new(runtime.RuntimeEnvelope)) == nil {
		t.Fatal("the other-version payload parses; it no longer tests the version read")
	}
	return payload
}

// A Ready of another version fails readiness with both versions named, before
// its content is judged or even parsed, and the Gateway tells the Match its own
// version before it closes.
func TestReadyOfAnotherVersionIsNamedAndAnswered(t *testing.T) {
	marshal := func(e *runtime.RuntimeEnvelope) []byte {
		b, err := proto.Marshal(e)
		if err != nil {
			t.Fatal(err)
		}
		return b
	}
	ready := func(version, maxPlayers uint32) []byte {
		return marshal(&runtime.RuntimeEnvelope{ProtocolVersion: version, Message: &runtime.RuntimeEnvelope_Ready{Ready: &runtime.Ready{JumpHeight: .6, Gravity: 18,
			ArenaId: "test_arena", ArenaVersion: 1, ArenaDigest: testArenaDigest, TickRate: 60, SnapshotIntervalTicks: 1, MaxPlayers: maxPlayers, CombatRules: testRules()}}})
	}
	cases := []struct {
		name    string
		version uint32
		payload []byte
	}{
		{"older", adapter.RuntimeVersion - 1, ready(adapter.RuntimeVersion-1, adapter.MaxPlayers)},
		{"newer", adapter.RuntimeVersion + 1, ready(adapter.RuntimeVersion+1, adapter.MaxPlayers)},
		{"older, content this version rejects", adapter.RuntimeVersion - 1, ready(adapter.RuntimeVersion-1, 0)},
		{"newer, content this version cannot parse", adapter.RuntimeVersion + 1, otherVersionPayload(t, adapter.RuntimeVersion+1)},
	}
	for _, c := range cases {
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			t.Fatal(err)
		}
		defer listener.Close()
		notice := make(chan *runtime.RuntimeEnvelope, 1)
		go func() {
			conn, err := listener.Accept()
			if err != nil {
				return
			}
			defer conn.Close()
			if framing.WriteFrame(conn, c.payload) != nil {
				return
			}
			_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
			frame, err := framing.ReadFrame(conn)
			if err != nil {
				return
			}
			var answer runtime.RuntimeEnvelope
			if proto.Unmarshal(frame, &answer) == nil {
				notice <- &answer
			}
		}()
		link, _, err := connectRuntime(context.Background(), listener.Addr().String())
		if err == nil {
			link.close()
			t.Fatalf("%s: runtime v%d admitted", c.name, c.version)
		}
		want := fmt.Sprintf("runtime readiness contract mismatch: protocol version expected %d, received %d", adapter.RuntimeVersion, c.version)
		if err.Error() != want {
			t.Fatalf("%s: error %q, want %q", c.name, err, want)
		}
		select {
		case answer := <-notice:
			if answer.ProtocolVersion != adapter.RuntimeVersion || answer.Message != nil || len(answer.ProtoReflect().GetUnknown()) != 0 {
				t.Fatalf("%s: notice %v", c.name, answer)
			}
		case <-time.After(3 * time.Second):
			t.Fatalf("%s: runtime v%d: no version notice", c.name, c.version)
		}
	}
}

// After Ready, another version (also one this schema cannot parse), a message
// this version does not know and an envelope with no message fail the link
// with distinct reasons.
func TestRuntimeEnvelopeFailuresAreDistinct(t *testing.T) {
	marshal := func(e *runtime.RuntimeEnvelope) []byte {
		b, err := proto.Marshal(e)
		if err != nil {
			t.Fatal(err)
		}
		return b
	}
	unknown := protowire.AppendVarint(protowire.AppendTag(marshal(envelope()), 99, protowire.VarintType), 1)
	newer := fmt.Sprintf("runtime protocol version mismatch: expected %d, received %d", adapter.RuntimeVersion, adapter.RuntimeVersion+1)
	cases := []struct {
		name    string
		payload []byte
		reason  string
		want    string
	}{
		{"version", marshal(&runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion + 1}), "version_mismatch", newer},
		{"version, content this version cannot parse", otherVersionPayload(t, adapter.RuntimeVersion+1), "version_mismatch", newer},
		{"unknown", unknown, "protocol", "unknown runtime message fields=[99]"},
		{"empty", marshal(envelope()), "protocol", "runtime protocol mismatch: empty message"},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			logs := captureIngressLog(t)
			_, f := newTestServer(t)
			f.mu.Lock()
			err := framing.WriteFrame(f.conn, c.payload)
			f.mu.Unlock()
			if err != nil {
				t.Fatal(err)
			}
			// log-contract: the reason code first, the original error quoted last.
			want := "runtime disconnected reason=" + c.reason + " error=" + strconv.Quote(c.want)
			deadline := time.Now().Add(3 * time.Second)
			for {
				logs.mu.Lock()
				text := logs.buf.String()
				logs.mu.Unlock()
				if strings.Contains(text, want) {
					return
				}
				if time.Now().After(deadline) {
					t.Fatalf("no %q in\n%s", want, text)
				}
				time.Sleep(10 * time.Millisecond)
			}
		})
	}
}
