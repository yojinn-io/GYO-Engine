package gateway

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/netip"
	"slices"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv8"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev8"
)

func TestCreateRoomWaitsForFragmentedJSON(t *testing.T) {
	s, _ := newTestServer(t)
	conn, err := net.DialTimeout("tcp", s.HTTPAddress(), time.Second)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	// cpp-httplib can send headers and body in separate TCP writes. A close
	// response before consuming the body can reset the Windows connection.
	_, err = io.WriteString(conn, "POST /rooms HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n")
	if err != nil {
		t.Fatal(err)
	}
	_ = conn.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
	var early [1]byte
	if _, err = conn.Read(early[:]); err == nil {
		t.Fatal("room creation replied before its request body arrived")
	} else if timeout, ok := err.(net.Error); !ok || !timeout.Timeout() {
		t.Fatalf("connection closed before request body: %v", err)
	}
	_ = conn.SetDeadline(time.Now().Add(2 * time.Second))
	if _, err = io.WriteString(conn, "{}"); err != nil {
		t.Fatal(err)
	}
	response, err := http.ReadResponse(bufio.NewReader(conn), nil)
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	body, err := io.ReadAll(response.Body)
	if err != nil || response.StatusCode != http.StatusOK || !json.Valid(body) {
		t.Fatalf("incomplete create response: status=%d read=%v", response.StatusCode, err)
	}
}

type fakeRuntime struct {
	listener net.Listener
	conn     net.Conn
	mu       sync.Mutex
	got      chan *runtime.RuntimeEnvelope
	// Heartbeats (diagnostics) go apart, so requests read by next and quiet
	// are only the ones the tests drive; a full channel drops the newest.
	clocks chan *runtime.ClockSample
	drops  chan *runtime.GatewayDropReport // the same for drop reports
}

func newTestServer(t *testing.T) (*Server, *fakeRuntime) {
	t.Helper()
	s, f, _, _ := startTestServer(t)
	return s, f
}

// startTestServer also returns Serve's cancel and a channel closed when Serve
// returned.
func startTestServer(t *testing.T) (*Server, *fakeRuntime, context.CancelFunc, <-chan struct{}) {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	f := &fakeRuntime{listener: listener, got: make(chan *runtime.RuntimeEnvelope, 32), clocks: make(chan *runtime.ClockSample, 64), drops: make(chan *runtime.GatewayDropReport, 256)}
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			return
		}
		f.mu.Lock()
		f.conn = conn
		f.mu.Unlock()
		ready := envelope()
		ready.Message = &runtime.RuntimeEnvelope_Ready{Ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, ArenaId: "test_arena", ArenaVersion: 1, ArenaDigest: testArenaDigest, TickRate: 60, SnapshotIntervalTicks: 1, MaxPlayers: adapter.MaxPlayers, CombatRules: testRules()}}
		f.send(ready)
		for {
			b, err := framing.ReadFrame(conn)
			if err != nil {
				return
			}
			var e runtime.RuntimeEnvelope
			if proto.Unmarshal(b, &e) != nil {
				return
			}
			if clock := e.GetClock(); clock != nil {
				select {
				case f.clocks <- clock:
				default:
				}
				continue
			}
			if report := e.GetDropReport(); report != nil {
				select {
				case f.drops <- report:
				default:
				}
				continue
			}
			f.got <- &e
		}
	}()
	ctx, cancel := context.WithCancel(context.Background())
	s, err := New(ctx, Config{HTTPAddress: "127.0.0.1:0", UDPAddress: "127.0.0.1:0", RuntimeAddress: listener.Addr().String(), AdvertiseIP: "127.0.0.1"})
	if err != nil {
		cancel()
		_ = listener.Close()
		t.Fatal(err)
	}
	done := make(chan struct{})
	go func() { _ = s.Serve(ctx); close(done) }()
	t.Cleanup(func() {
		cancel()
		closed := make(chan struct{})
		go func() { s.Close(); close(closed) }()
		select {
		case <-closed:
		case <-time.After(3 * time.Second):
			t.Error("server did not close")
		}
		_ = listener.Close()
		f.mu.Lock()
		if f.conn != nil {
			_ = f.conn.Close()
		}
		f.mu.Unlock()
		select {
		case <-done:
		case <-time.After(2 * time.Second):
			t.Error("server did not stop")
		}
	})
	return s, f, cancel, done
}
func (f *fakeRuntime) send(e *runtime.RuntimeEnvelope) {
	if snapshot := e.GetSnapshot(); snapshot != nil && snapshot.Combat == nil {
		for _, p := range snapshot.Players {
			snapshot.Combat = append(snapshot.Combat, &runtime.CombatState{LifeGeneration: 1, MagazineAmmo: 12, PlayerId: p.PlayerId, Hp: 100})
		}
	}
	f.mu.Lock()
	defer f.mu.Unlock()
	b, _ := proto.Marshal(e)
	if f.conn != nil {
		_ = framing.WriteFrame(f.conn, b)
	}
}
func (f *fakeRuntime) next(t *testing.T) *runtime.RuntimeEnvelope {
	t.Helper()
	select {
	case e := <-f.got:
		return e
	case <-time.After(2 * time.Second):
		t.Fatal("runtime request missing")
		return nil
	}
}

// nextInput fails with missing, so each forwarding test names its own loss.
func (f *fakeRuntime) nextInput(t *testing.T, missing string) *runtime.PlayerInput {
	t.Helper()
	select {
	case e := <-f.got:
		return e.GetInput()
	case <-time.After(2 * time.Second):
		t.Fatal(missing)
		return nil
	}
}
func (f *fakeRuntime) quiet(t *testing.T) {
	t.Helper()
	select {
	case e := <-f.got:
		t.Fatalf("unexpected runtime request %v", e)
	case <-time.After(40 * time.Millisecond):
	}
}

type credentials struct {
	PlayerID  uint32 `json:"player_id"`
	SessionID uint64 `json:"session_id"`
	Token     string `json:"session_token"`
	UDPIP     string `json:"udp_ip"`
	UDPPort   uint16 `json:"udp_port"`
	Version   uint16 `json:"protocol_version"`
	Arena     string `json:"arena_id"`
	Digest    uint64 `json:"arena_digest"`
}

// testArenaDigest is an opaque nonzero digest; the Gateway only forwards it.
const testArenaDigest uint64 = 0xfedcba9876543210

func post(t *testing.T, s *Server, path string, body any) (int, []byte) {
	t.Helper()
	b, _ := json.Marshal(body)
	response, err := http.Post("http://"+s.HTTPAddress()+path, "application/json", bytes.NewReader(b))
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	content, err := io.ReadAll(response.Body)
	if err != nil {
		t.Fatal(err)
	}
	return response.StatusCode, content
}
func reserve(t *testing.T, s *Server, id string) credentials {
	t.Helper()
	status, body := post(t, s, "/rooms/1/join", map[string]any{"request_id": id, "protocol_version": adapter.ClientVersion})
	if status != http.StatusOK {
		t.Fatalf("join %d: %s", status, body)
	}
	// pv8 §2a: match_id had no reader and left the join reply.
	if bytes.Contains(body, []byte("match_id")) {
		t.Fatalf("join reply still carries match_id: %s", body)
	}
	var c credentials
	if err := json.Unmarshal(body, &c); err != nil {
		t.Fatal(err)
	}
	if c.SessionID == 0 || c.PlayerID == 0 || c.Token == "" || c.UDPPort == 0 || c.Arena != "test_arena" || c.Digest != testArenaDigest {
		t.Fatalf("bad credentials %+v", c)
	}
	return c
}
func peer(t *testing.T) *net.UDPConn {
	t.Helper()
	conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = conn.Close() })
	return conn
}
func sendPacket(t *testing.T, p *net.UDPConn, c credentials, seq uint32, kind uint16, message proto.Message) {
	t.Helper()
	b, _ := proto.Marshal(message)
	packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: kind, SessionID: c.SessionID, Sequence: seq}, b)
	if err != nil {
		t.Fatal(err)
	}
	_, err = p.WriteToUDPAddrPort(packet, netip.AddrPortFrom(netip.MustParseAddr(c.UDPIP), c.UDPPort))
	if err != nil {
		t.Fatal(err)
	}
}
func receivePacket(t *testing.T, p *net.UDPConn, kind uint16) []byte {
	t.Helper()
	_ = p.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, framing.MaxDatagram+1)
	for {
		n, _, err := p.ReadFromUDP(buf)
		if err != nil {
			t.Fatal(err)
		}
		h, payload, err := framing.DecodeDatagram(buf[:n])
		if err != nil {
			t.Fatal(err)
		}
		if h.Type == kind {
			return payload
		}
	}
}
func accept(t *testing.T, f *fakeRuntime, c credentials) {
	t.Helper()
	request := f.next(t)
	if request.GetJoin() == nil || request.GetJoin().PlayerId != c.PlayerID {
		t.Fatalf("wrong join: %v", request)
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_JoinResult{JoinResult: &runtime.JoinResult{PlayerId: c.PlayerID, Accepted: true}}
	f.send(e)
}

func TestHTTPReservationHandshakeInputAndSnapshot(t *testing.T) {
	s, f := newTestServer(t)
	response, err := http.Get("http://" + s.HTTPAddress() + "/rooms")
	if err != nil {
		t.Fatal(err)
	}
	var rooms struct {
		Rooms []json.RawMessage `json:"rooms"`
	}
	err = json.NewDecoder(response.Body).Decode(&rooms)
	_ = response.Body.Close()
	if err != nil || len(rooms.Rooms) != 0 {
		t.Fatal("room existed before explicit creation")
	}
	status, _ := post(t, s, "/rooms/1/join", map[string]any{"request_id": "before_create", "protocol_version": adapter.ClientVersion})
	if status != 404 {
		t.Fatalf("join before create: %d", status)
	}
	for i := 0; i < 2; i++ {
		status, _ = post(t, s, "/rooms", map[string]any{})
		if status != 200 {
			t.Fatal(status)
		}
	}
	c := reserve(t, s, "first")
	repeated := reserve(t, s, "first")
	if repeated.SessionID != c.SessionID || repeated.PlayerID != c.PlayerID {
		t.Fatal("reservation not idempotent")
	}
	f.quiet(t)
	p := peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: "wrong"})
	f.quiet(t)
	sendPacket(t, p, c, 2, adapter.Hello, &client.Hello{SessionToken: c.Token})
	request := f.next(t)
	if request.GetJoin() == nil || request.GetJoin().PlayerId != c.PlayerID {
		t.Fatalf("bad join %v", request)
	}
	sendPacket(t, p, c, 3, adapter.Hello, &client.Hello{SessionToken: c.Token})
	f.quiet(t)
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_JoinResult{JoinResult: &runtime.JoinResult{PlayerId: c.PlayerID, Accepted: true}}
	f.send(e)
	var welcome client.Welcome
	if err := proto.Unmarshal(receivePacket(t, p, adapter.Welcome), &welcome); err != nil || welcome.PlayerId != c.PlayerID || welcome.TickRate != 60 || welcome.ArenaDigest != testArenaDigest {
		t.Fatalf("welcome %v %v", &welcome, err)
	}
	sendPacket(t, p, c, 4, adapter.Hello, &client.Hello{SessionToken: c.Token})
	receivePacket(t, p, adapter.Welcome)
	f.quiet(t)
	sendPacket(t, p, c, 5, adapter.Input, &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 1, MoveForward: 1, Yaw: .5}, {Sequence: 2, MoveForward: 1}, {Sequence: 3}}})
	in := f.next(t).GetInput()
	if in == nil || in.PlayerId != c.PlayerID || len(in.Commands) != 3 || in.Commands[0].Sequence != 1 || in.Commands[2].Sequence != 3 {
		t.Fatalf("bad input %v", in)
	}
	// Redundant batches remain valid after the mailbox drains, and reach the
	// Match as they came (pv7 §2a); it keeps the first arrival.
	sendPacket(t, p, c, 6, adapter.Input, &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 2, MoveForward: 1}, {Sequence: 3}}})
	if in := f.nextInput(t, "redundant window did not reach the runtime"); in == nil || len(in.Commands) != 2 {
		t.Fatalf("redundant window: %v", in)
	}
	// A conflict reaches the Match unmerged, which rejects the entire batch,
	// including its new command.
	sendPacket(t, p, c, 7, adapter.Input, &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 2, MoveForward: -1}, {Sequence: 4}}})
	if in := f.nextInput(t, "conflicting window did not reach the runtime"); in == nil || len(in.Commands) != 2 || in.Commands[0].MoveForward != -1 {
		t.Fatalf("conflicting window did not reach the runtime: %v", in)
	}
	e = envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 3, Players: []*runtime.PlayerState{{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1, PlayerId: c.PlayerID, X: 5, Z: 7, LastResolvedCommand: 3}}}}
	f.send(e)
	var snapshot client.WorldSnapshot
	if err := proto.Unmarshal(receivePacket(t, p, adapter.Snapshot), &snapshot); err != nil || snapshot.Tick != 3 || len(snapshot.Players) != 1 || snapshot.Players[0].X != 5 || snapshot.Players[0].LastResolvedCommand != 3 {
		t.Fatalf("snapshot %v %v", &snapshot, err)
	}
	status, _ = post(t, s, "/rooms/1/leave", map[string]any{"session_id": c.SessionID, "session_token": c.Token})
	if status != 200 {
		t.Fatal(status)
	}
	if leave := f.next(t).GetLeave(); leave == nil || leave.PlayerId != c.PlayerID ||
		leave.Reason != runtime.LeaveReason_LEAVE_REASON_CLIENT_REQUEST {
		t.Fatalf("leave missing or without the client request reason: %v", leave)
	}
	sendPacket(t, p, c, 8, adapter.Input, &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 4, MoveForward: 1}}})
	f.quiet(t)
}

func TestMatchEvictionRemovesPlayerAndTellsClient(t *testing.T) {
	s, f := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != 200 {
		t.Fatal(status)
	}
	c := reserve(t, s, "evicted")
	p := peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	receivePacket(t, p, adapter.Welcome)
	sendPacket(t, p, c, 2, adapter.Input, &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, ObservedAuthorityTick: 9,
		Commands: []*client.MovementCommand{{Sequence: 1}, {Sequence: 2}, {Sequence: 3}}})
	if in := f.next(t).GetInput(); in == nil || in.ObservedAuthorityTick != 9 {
		t.Fatalf("observed tick not forwarded: %v", in)
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Evicted{Evicted: &runtime.PlayerEvicted{PlayerId: c.PlayerID,
		Reason: runtime.EvictionReason_EVICTION_HIGH_LATENCY, ReferenceAgeMs: 190}}
	f.send(e)
	var failure client.Error
	if err := proto.Unmarshal(receivePacket(t, p, adapter.Failure), &failure); err != nil ||
		failure.Code != "evicted_high_latency" || !strings.Contains(failure.Message, "190 ms") {
		t.Fatalf("eviction notice %v %v", &failure, err)
	}
	// The Match already removed the player: no input or Leave follows.
	sendPacket(t, p, c, 3, adapter.Input, &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 4}}})
	if status, _ := post(t, s, "/rooms/1/leave", map[string]any{"session_id": c.SessionID, "session_token": c.Token}); status != 200 {
		t.Fatal(status)
	}
	f.quiet(t)
}

func TestCapacityExpiryAndRuntimeFailure(t *testing.T) {
	s, f := newTestServer(t)
	status, _ := post(t, s, "/rooms", map[string]any{})
	if status != 200 {
		t.Fatal(status)
	}
	a := reserve(t, s, "a")
	for i := 1; i < adapter.MaxPlayers; i++ {
		_ = reserve(t, s, fmt.Sprintf("filler%d", i))
	}
	status, body := post(t, s, "/rooms/1/join", map[string]any{"request_id": "c", "protocol_version": adapter.ClientVersion})
	if status != 409 || !bytes.Contains(body, []byte("room_full")) {
		t.Fatalf("capacity status %d %s", status, body)
	}
	s.expireAt(time.Now().Add(6 * time.Second))
	f.quiet(t)
	c := reserve(t, s, "c")
	if c.PlayerID <= a.PlayerID {
		t.Fatal("player id reused")
	}
	p := peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	receivePacket(t, p, adapter.Welcome)
	s.expireAt(time.Now().Add(6 * time.Second))
	if leave := f.next(t).GetLeave(); leave == nil || leave.PlayerId != c.PlayerID ||
		leave.Reason != runtime.LeaveReason_LEAVE_REASON_SESSION_TIMEOUT {
		t.Fatalf("expiry leave missing or without the session timeout reason: %v", leave)
	}
	f.mu.Lock()
	_ = f.conn.Close()
	f.mu.Unlock()
	deadline := time.Now().Add(2 * time.Second)
	for {
		status, _ = post(t, s, "/rooms/1/join", map[string]any{"request_id": "after_failure", "protocol_version": adapter.ClientVersion})
		if status == 503 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("runtime failure status %d", status)
		}
		time.Sleep(5 * time.Millisecond)
	}
}

func TestSnapshotsFollowRuntimePublicationsForBothPeers(t *testing.T) {
	s, f := newTestServer(t)
	status, _ := post(t, s, "/rooms", map[string]any{})
	if status != 200 {
		t.Fatal(status)
	}
	a, b := reserve(t, s, "a"), reserve(t, s, "b")
	pa, pb := peer(t), peer(t)
	for _, entry := range []struct {
		c credentials
		p *net.UDPConn
	}{{a, pa}, {b, pb}} {
		sendPacket(t, entry.p, entry.c, 1, adapter.Hello, &client.Hello{SessionToken: entry.c.Token})
		accept(t, f, entry.c)
		receivePacket(t, entry.p, adapter.Welcome)
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 3, Players: []*runtime.PlayerState{
		{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1, PlayerId: a.PlayerID, X: 1, Z: 2}, {LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1, PlayerId: b.PlayerID, X: 3, Z: 4},
	}}}
	f.send(e)
	first, second := receivePacket(t, pa, adapter.Snapshot), receivePacket(t, pb, adapter.Snapshot)
	if !sameWorld(t, first, second) {
		t.Fatal("peers observed different world payloads")
	}
	var snapshot client.WorldSnapshot
	if err := proto.Unmarshal(first, &snapshot); err != nil || len(snapshot.Players) != 2 || snapshot.Tick != 3 {
		t.Fatalf("full snapshot: %v %v", &snapshot, err)
	}
	// No producer clock in Gateway: elapsed time causes no datagram. A
	// repeated publication is forwarded as it came (D1 removed, pv7 §2a); the
	// Client applies only a newer tick.
	f.send(e)
	for _, p := range []*net.UDPConn{pa, pb} {
		if repeat := receivePacket(t, p, adapter.Snapshot); !sameWorld(t, repeat, first) {
			t.Fatal("the repeated publication changed")
		}
	}
	_ = pa.SetReadDeadline(time.Now().Add(120 * time.Millisecond))
	if _, _, err := pa.ReadFromUDP(make([]byte, framing.MaxDatagram)); err == nil {
		t.Fatal("gateway sent a snapshot without an authoritative publication")
	} else if timeout, ok := err.(net.Error); !ok || !timeout.Timeout() {
		t.Fatal(err)
	}
	e.GetSnapshot().Tick = 6
	f.send(e)
	for _, p := range []*net.UDPConn{pa, pb} {
		var next client.WorldSnapshot
		if err := proto.Unmarshal(receivePacket(t, p, adapter.Snapshot), &next); err != nil || next.Tick != 6 {
			t.Fatalf("next publication: %v %v", &next, err)
		}
	}
}

func TestPendingJoinTimeoutCannotResurrectSession(t *testing.T) {
	s, f := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != 200 {
		t.Fatal(status)
	}
	c := reserve(t, s, "pending")
	p := peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	if join := f.next(t).GetJoin(); join == nil || join.PlayerId != c.PlayerID {
		t.Fatal("missing join")
	}
	s.expireAt(time.Now().Add(4 * time.Second))
	if leave := f.next(t).GetLeave(); leave == nil || leave.PlayerId != c.PlayerID ||
		leave.Reason != runtime.LeaveReason_LEAVE_REASON_JOIN_TIMEOUT {
		t.Fatalf("pending timeout must send ordered leave with the join timeout reason: %v", leave)
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_JoinResult{JoinResult: &runtime.JoinResult{PlayerId: c.PlayerID, Accepted: true}}
	f.send(e)
	sendPacket(t, p, c, 2, adapter.Hello, &client.Hello{SessionToken: c.Token})
	f.quiet(t)
	replacement := reserve(t, s, "pending")
	if replacement.PlayerID == c.PlayerID || replacement.SessionID == c.SessionID {
		t.Fatal("expired identity reused")
	}
}

func TestV1ClientAndAllOldRuntimeVersionsAreRejected(t *testing.T) {
	s, f := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != 200 {
		t.Fatal(status)
	}
	status, body := post(t, s, "/rooms/1/join", map[string]any{"request_id": "old", "protocol_version": 1})
	if status != http.StatusConflict || !bytes.Contains(body, []byte("protocol_version")) {
		t.Fatalf("v1 join: %d %s", status, body)
	}
	c, p := reserve(t, s, "new"), peer(t)
	payload, _ := proto.Marshal(&client.Hello{SessionToken: c.Token})
	old, _ := framing.EncodeDatagram(framing.Header{Version: 1, Type: adapter.Hello, SessionID: c.SessionID, Sequence: 1}, payload)
	if _, err := p.WriteToUDPAddrPort(old, netip.AddrPortFrom(netip.MustParseAddr(c.UDPIP), c.UDPPort)); err != nil {
		t.Fatal(err)
	}
	f.quiet(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	receivePacket(t, p, adapter.Welcome)

	// Every other version, and the current one without an arena digest.
	type readyCase struct {
		version uint32
		digest  uint64
	}
	cases := []readyCase{{adapter.RuntimeVersion, 0}}
	for _, version := range rejectedVersions() {
		cases = append(cases, readyCase{version, testArenaDigest})
	}
	for _, c := range cases {
		version, digest := c.version, c.digest
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			t.Fatal(err)
		}
		defer listener.Close()
		done := make(chan struct{})
		go func() {
			defer close(done)
			conn, err := listener.Accept()
			if err != nil {
				return
			}
			defer conn.Close()
			ready := &runtime.RuntimeEnvelope{ProtocolVersion: version, Message: &runtime.RuntimeEnvelope_Ready{Ready: &runtime.Ready{JumpHeight: .6, Gravity: 18,
				ArenaId: "test_arena", ArenaVersion: 1, ArenaDigest: digest, TickRate: 60, SnapshotIntervalTicks: 1, MaxPlayers: adapter.MaxPlayers, CombatRules: testRules()}}}
			b, _ := proto.Marshal(ready)
			_ = framing.WriteFrame(conn, b)
		}()
		if link, _, err := connectRuntime(context.Background(), listener.Addr().String()); err == nil {
			link.close()
			t.Fatalf("runtime v%d with digest %x admitted", version, digest)
		}
		<-done
	}
}

// The Gateway forwards the Match's arena digest verbatim (pv6 contract §2):
// room list, HTTP join reply (checked by reserve) and Welcome.
func TestRoomsAndJoinForwardTheArenaDigest(t *testing.T) {
	s, _ := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != http.StatusOK {
		t.Fatal(status)
	}
	response, err := http.Get("http://" + s.HTTPAddress() + "/rooms")
	if err != nil {
		t.Fatal(err)
	}
	var rooms struct {
		Rooms []map[string]any `json:"rooms"`
	}
	decoder := json.NewDecoder(response.Body)
	decoder.UseNumber() // The digest uses the full uint64 range.
	err = decoder.Decode(&rooms)
	_ = response.Body.Close()
	if err != nil || len(rooms.Rooms) != 1 || rooms.Rooms[0]["arena_digest"] != json.Number(strconv.FormatUint(testArenaDigest, 10)) {
		t.Fatalf("room list %v %v", rooms, err)
	}
	// pv8 §2a: match_id had no reader and left the room list.
	if _, found := rooms.Rooms[0]["match_id"]; found {
		t.Fatalf("room list still carries match_id: %v", rooms)
	}
	reserve(t, s, "digest")
}

// pv7 §2a end to end: every input datagram reaches the Match as it came, a
// repeat, a mutation, a reordered packet and a resolved copy alike; the Match
// keeps the first arrival and refuses what conflicts (D49).
func TestGatewayForwardsEveryInputDatagramToTheRuntime(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "movement"), peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	receivePacket(t, p, adapter.Welcome)
	first := &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 1, MoveForward: 1}, {Sequence: 2}}}
	mutated := &client.PlayerInput{LifeGeneration: 1, MovementEpoch: 1, Commands: []*client.MovementCommand{{Sequence: 2, MoveForward: 1}, {Sequence: 3}}}
	for _, c2 := range []struct {
		name     string
		sequence uint32
		in       *client.PlayerInput
	}{{"first", 2, first}, {"repeat", 3, first}, {"mutation", 4, mutated}, {"reordered", 2, first}} {
		sendPacket(t, p, c, c2.sequence, adapter.Input, c2.in)
		in := f.nextInput(t, c2.name+" did not reach the runtime")
		if in == nil || len(in.Commands) != len(c2.in.Commands) || in.Commands[0].Sequence != c2.in.Commands[0].Sequence ||
			in.Commands[0].MoveForward != c2.in.Commands[0].MoveForward {
			t.Fatalf("%s reached the runtime changed: %v", c2.name, in)
		}
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 9, Players: []*runtime.PlayerState{{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1, PlayerId: c.PlayerID, LastResolvedCommand: 2}}}}
	f.send(e)
	receivePacket(t, p, adapter.Snapshot)
	// Resolved by the Match: the copy still goes, the Match measures it.
	sendPacket(t, p, c, 5, adapter.Input, first)
	if in := f.nextInput(t, "resolved copy did not reach the runtime"); in == nil || len(in.Commands) != 2 {
		t.Fatalf("resolved copy: %v", in)
	}
}

func TestActiveHelloKeepsSessionAliveWithoutRefreshingGameplay(t *testing.T) {
	s, f := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != http.StatusOK {
		t.Fatal(status)
	}
	c, p := reserve(t, s, "worker-keepalive"), peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	receivePacket(t, p, adapter.Welcome)
	start := time.Now()
	payload, _ := proto.Marshal(&client.Hello{SessionToken: c.Token})
	if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Hello,
		SessionID: c.SessionID, Sequence: 2}, payload, p.LocalAddr().(*net.UDPAddr).AddrPort(), start.Add(4*time.Second)); err != nil {
		t.Fatal(err)
	}
	receivePacket(t, p, adapter.Welcome)
	s.expireAt(start.Add(6 * time.Second))
	s.mu.Lock()
	alive := s.players[c.PlayerID] != nil && s.players[c.PlayerID].phase == active
	s.mu.Unlock()
	if !alive {
		t.Fatal("active worker keepalive did not extend session liveness")
	}
	f.quiet(t) // Hello must not forward a new join, input, or leave to Match.
	s.expireAt(start.Add(10 * time.Second))
	if leave := f.next(t).GetLeave(); leave == nil || leave.PlayerId != c.PlayerID {
		t.Fatal("silent session did not expire normally")
	}
}

func TestOtherClientVersionsAreRejected(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "current"), peer(t)
	payload, _ := proto.Marshal(&client.Hello{SessionToken: c.Token})
	for _, v := range rejectedVersions() {
		version := uint16(v)
		status, _ := post(t, s, "/rooms/1/join", map[string]any{"request_id": "old", "protocol_version": version})
		if status != http.StatusConflict {
			t.Fatalf("HTTP v%d accepted", version)
		}
		old, _ := framing.EncodeDatagram(framing.Header{Version: version, Type: adapter.Hello, SessionID: c.SessionID, Sequence: 1}, payload)
		if _, err := p.WriteToUDPAddrPort(old, netip.AddrPortFrom(netip.MustParseAddr(c.UDPIP), c.UDPPort)); err != nil {
			t.Fatal(err)
		}
		f.quiet(t)
	}
}

// rejectedVersions lists every older protocol version and the next one.
func rejectedVersions() []uint32 {
	versions := []uint32{}
	for version := uint32(1); version <= adapter.RuntimeVersion+1; version++ {
		if version != adapter.RuntimeVersion {
			versions = append(versions, version)
		}
	}
	return versions
}

func testRules() *runtime.CombatRules {
	return &runtime.CombatRules{MagazineCapacity: 12, ReloadTicks: 90, RespawnTicks: 180, MaximumHp: 100, ShotDamage: 25, MaximumShotDamage: 25, CooldownTicks: 10, ShotRange: 100, MaximumReferenceAgeMs: 250}
}

// Readiness requires the Match's capacity to equal the Gateway's (pv6 contract
// §1): Gateway and Match of different capacities fail at startup, in both directions.
func TestReadinessRequiresTheSameCapacity(t *testing.T) {
	for _, capacity := range []uint32{adapter.MaxPlayers - 1, adapter.MaxPlayers + 1} {
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			t.Fatal(err)
		}
		defer listener.Close()
		done := make(chan struct{})
		go func() {
			defer close(done)
			conn, err := listener.Accept()
			if err != nil {
				return
			}
			defer conn.Close()
			ready := envelope()
			ready.Message = &runtime.RuntimeEnvelope_Ready{Ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, ArenaId: "test_arena", ArenaVersion: 1,
				ArenaDigest: testArenaDigest, TickRate: 60, SnapshotIntervalTicks: 1, MaxPlayers: capacity, CombatRules: testRules()}}
			b, _ := proto.Marshal(ready)
			_ = framing.WriteFrame(conn, b)
		}()
		if link, _, err := connectRuntime(context.Background(), listener.Addr().String()); err == nil {
			link.close()
			t.Fatalf("runtime with capacity %d admitted", capacity)
		}
		<-done
	}
}

// Every peer of a full room receives the same snapshot (pv6 contract §1: four players).
func TestSnapshotsReachEveryPeerOfAFullRoom(t *testing.T) {
	s, f := newTestServer(t)
	if status, _ := post(t, s, "/rooms", map[string]any{}); status != 200 {
		t.Fatal(status)
	}
	type joined struct {
		c credentials
		p *net.UDPConn
	}
	var peers []joined
	var players []*runtime.PlayerState
	for i := 0; i < adapter.MaxPlayers; i++ {
		c := reserve(t, s, fmt.Sprintf("peer%d", i))
		p := peer(t)
		sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
		accept(t, f, c)
		receivePacket(t, p, adapter.Welcome)
		peers = append(peers, joined{c, p})
		players = append(players, &runtime.PlayerState{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1, PlayerId: c.PlayerID, X: float32(i)})
	}
	if status, body := post(t, s, "/rooms/1/join", map[string]any{"request_id": "fifth", "protocol_version": adapter.ClientVersion}); status != 409 || !bytes.Contains(body, []byte("room_full")) {
		t.Fatalf("fifth join %d %s", status, body)
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 3, Players: players}}
	f.send(e)
	first := receivePacket(t, peers[0].p, adapter.Snapshot)
	for _, other := range peers[1:] {
		if !sameWorld(t, first, receivePacket(t, other.p, adapter.Snapshot)) {
			t.Fatal("peers observed different world payloads")
		}
	}
	var snapshot client.WorldSnapshot
	if err := proto.Unmarshal(first, &snapshot); err != nil || len(snapshot.Players) != adapter.MaxPlayers {
		t.Fatalf("full room snapshot: %v %v", &snapshot, err)
	}
}

// G10: results of a player that is not active do nothing, and a removed
// player's identity never comes back. (a) A joining player's actions do not
// reach the Match and Match results for it leave no ledger and no packet. (b)
// Results for a player who left are dropped without failing the room. (c) The
// same request id reserves a new player id.
func TestResultsOfAPlayerNotActiveAreInertAndItsIdentityNeverReturns(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	shotBatch := &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}}
	results := func(player uint32) {
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_ActionResults{ActionResults: &runtime.ActionResults{PlayerId: player, Decisions: []*runtime.ShotDecision{decision(1)}}}
		f.send(e)
	}
	// Packet kinds reaching a peer until one of the given kind, or a Failure,
	// arrives.
	until := func(p *net.UDPConn, kind uint16) []uint16 {
		t.Helper()
		var kinds []uint16
		buf := make([]byte, framing.MaxDatagram+1)
		_ = p.SetReadDeadline(time.Now().Add(2 * time.Second))
		for {
			n, _, err := p.ReadFromUDP(buf)
			if err != nil {
				t.Fatalf("no packet of kind %d (got %v): %v", kind, kinds, err)
			}
			if h, _, err := framing.DecodeDatagram(buf[:n]); err == nil {
				kinds = append(kinds, h.Type)
				if h.Type == kind || h.Type == adapter.Failure {
					return kinds
				}
			}
		}
	}

	// (a) Joining.
	joiner, joinerPeer := reserve(t, s, "g10-joining"), peer(t)
	sendPacket(t, joinerPeer, joiner, 1, adapter.Hello, &client.Hello{SessionToken: joiner.Token})
	if join := f.next(t).GetJoin(); join == nil || join.PlayerId != joiner.PlayerID {
		t.Fatal("missing join")
	}
	sendPacket(t, joinerPeer, joiner, 2, adapter.Actions, shotBatch)
	select {
	case e := <-f.got:
		t.Fatalf("a joining player's actions reached the runtime: %v", e)
	case <-time.After(60 * time.Millisecond):
	}
	results(joiner.PlayerID)
	joined := envelope()
	joined.Message = &runtime.RuntimeEnvelope_JoinResult{JoinResult: &runtime.JoinResult{PlayerId: joiner.PlayerID, Accepted: true}}
	f.send(joined)
	// The runtime link handles its messages in order: the Welcome comes after
	// the results were handled.
	if kinds := until(joinerPeer, adapter.Welcome); slices.Contains(kinds, adapter.Failure) {
		t.Fatal("a joining player's results failed the room")
	} else if slices.Contains(kinds, adapter.ActionResults) {
		t.Fatal("a joining player's results reached its Client")
	}
	s.mu.Lock()
	held := s.players[joiner.PlayerID].results.pending
	s.mu.Unlock()
	if held != nil {
		t.Fatal("a joining player's results were held for its Client")
	}

	// (b) Left.
	leaver, leaverPeer := reserve(t, s, "g10-leaver"), peer(t)
	sendPacket(t, leaverPeer, leaver, 1, adapter.Hello, &client.Hello{SessionToken: leaver.Token})
	accept(t, f, leaver)
	receivePacket(t, leaverPeer, adapter.Welcome)
	sendPacket(t, leaverPeer, leaver, 2, adapter.Actions, shotBatch)
	for in := f.next(t); in.GetActions() == nil || in.GetActions().PlayerId != leaver.PlayerID; in = f.next(t) {
	}
	if status, _ := post(t, s, "/rooms/1/leave", map[string]any{"session_id": leaver.SessionID, "session_token": leaver.Token}); status != 200 {
		t.Fatal(status)
	}
	for in := f.next(t); in.GetLeave() == nil; in = f.next(t) {
	}
	results(leaver.PlayerID)
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 5, Players: []*runtime.PlayerState{{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1, PlayerId: joiner.PlayerID}}}}
	f.send(e)
	if kinds := until(joinerPeer, adapter.Snapshot); slices.Contains(kinds, adapter.Failure) {
		t.Fatal("a removed player's results failed the room")
	}
	s.mu.Lock()
	available := s.available
	s.mu.Unlock()
	s.link.mu.Lock()
	_, kept := s.link.actionStats[leaver.PlayerID]
	s.link.mu.Unlock()
	if !available || kept {
		t.Fatalf("after a removed player's results: available=%v link statistics kept=%v", available, kept)
	}

	// (c) The same request id again.
	if again := reserve(t, s, "g10-leaver"); again.PlayerID == leaver.PlayerID || again.PlayerID <= joiner.PlayerID {
		t.Fatalf("a removed player's id was reused: %d after %d", again.PlayerID, leaver.PlayerID)
	}
}

// sameWorld compares two peers' snapshot payloads apart from gateway_hold_us,
// which each write fills for its own peer (pv7 §5); both must carry it.
func sameWorld(t *testing.T, a, b []byte) bool {
	t.Helper()
	var x, y client.WorldSnapshot
	if proto.Unmarshal(a, &x) != nil || proto.Unmarshal(b, &y) != nil || x.GatewayHoldUs == nil || y.GatewayHoldUs == nil {
		return false
	}
	x.GatewayHoldUs, y.GatewayHoldUs = nil, nil
	return proto.Equal(&x, &y)
}
