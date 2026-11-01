package gateway

import (
	"strings"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
)

func welcomeOf(t *testing.T, payload []byte) *client.Welcome {
	t.Helper()
	var welcome client.Welcome
	if err := proto.Unmarshal(payload, &welcome); err != nil {
		t.Fatal(err)
	}
	return &welcome
}

// pv7 §3: a Welcome echoes the session's latest authenticated Hello (origin =
// its transmit, receive = when the Gateway read it) and stamps its own
// transmit; a Hello echoing that Welcome is measured.
func TestWelcomeEchoesTheLatestAuthenticatedHello(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "echo"), peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token, Clock: &client.ClockSample{TransmitUs: 1_000}})
	accept(t, f, c)
	first := welcomeOf(t, receivePacket(t, p, adapter.Welcome))
	if first.Clock == nil || first.Clock.OriginUs == nil || *first.Clock.OriginUs != 1_000 || first.Clock.ReceiveUs == nil ||
		first.Clock.TransmitUs < *first.Clock.ReceiveUs {
		t.Fatalf("first Welcome echo %v", first.Clock)
	}
	// The Client turns around at once (receive == transmit), so the delay is
	// the round trip, never negative.
	origin, receive := first.Clock.TransmitUs, uint64(5_100)
	sendPacket(t, p, c, 2, adapter.Hello, &client.Hello{SessionToken: c.Token, Clock: &client.ClockSample{OriginUs: &origin, ReceiveUs: &receive, TransmitUs: 5_100}})
	second := welcomeOf(t, receivePacket(t, p, adapter.Welcome))
	if second.Clock == nil || *second.Clock.OriginUs != 5_100 || second.Clock.TransmitUs <= first.Clock.TransmitUs {
		t.Fatalf("second Welcome echo %v", second.Clock)
	}
	s.mu.Lock()
	sync := s.players[c.PlayerID].echo.sync
	samples, invalid := sync.samples, sync.invalid
	s.mu.Unlock()
	if samples != 1 || invalid != 0 {
		t.Fatalf("samples %d, invalid %d", samples, invalid)
	}
}

// An invalid or missing echo changes nothing else: the Hello still joins,
// keeps its session alive and is answered with a Welcome; the invalid sample
// is counted and ignored (no veto, pv7 §2).
func TestInvalidHelloEchoIsIgnored(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "bad-echo"), peer(t)
	unmatched, early := uint64(77), uint64(9)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token, Clock: &client.ClockSample{OriginUs: &unmatched, ReceiveUs: &early, TransmitUs: 10}})
	accept(t, f, c)
	if welcome := welcomeOf(t, receivePacket(t, p, adapter.Welcome)); welcome.PlayerId != c.PlayerID || welcome.CombatRules == nil {
		t.Fatalf("Welcome %v", welcome)
	}
	sendPacket(t, p, c, 2, adapter.Hello, &client.Hello{SessionToken: c.Token}) // no echo at all
	receivePacket(t, p, adapter.Welcome)
	s.mu.Lock()
	sync := s.players[c.PlayerID].echo.sync
	samples, invalid := sync.samples, sync.invalid
	s.mu.Unlock()
	if samples != 0 || invalid != 1 {
		t.Fatalf("samples %d, invalid %d", samples, invalid)
	}
}

// A session's last window is logged when it ends.
func TestSessionEchoWindowIsLoggedAtLeave(t *testing.T) {
	logs := captureIngressLog(t)
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "leave-echo"), peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token, Clock: &client.ClockSample{TransmitUs: 1_000}})
	accept(t, f, c)
	receivePacket(t, p, adapter.Welcome)
	if status, body := post(t, s, "/rooms/1/leave", map[string]any{"session_id": c.SessionID, "session_token": c.Token}); status != 200 {
		t.Fatalf("leave %d %s", status, body)
	}
	logs.mu.Lock()
	text := logs.buf.String()
	logs.mu.Unlock()
	if !strings.Contains(text, "client clock sync player=1 window_ms=") || !strings.Contains(text, " final=1") {
		t.Fatalf("no final client clock window in\n%s", text)
	}
}

// The echo of a Hello that fails authentication is never read (pv7 §3).
func TestUnauthenticatedHelloIsNotEchoed(t *testing.T) {
	s, _ := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "stranger"), peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: "not-the-token", Clock: &client.ClockSample{TransmitUs: 999}})
	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) {
		s.mu.Lock()
		started := s.players[c.PlayerID].echo.sync != nil
		s.mu.Unlock()
		if started {
			t.Fatal("an unauthenticated Hello was measured")
		}
		time.Sleep(10 * time.Millisecond)
	}
}
