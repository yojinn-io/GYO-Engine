package gateway

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"io"
	"net"
	"net/http"
	"net/netip"
	"sync"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv1"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev1"
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
}

func newTestServer(t *testing.T) (*Server, *fakeRuntime) {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	f := &fakeRuntime{listener: listener, got: make(chan *runtime.RuntimeEnvelope, 32)}
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			return
		}
		f.mu.Lock()
		f.conn = conn
		f.mu.Unlock()
		ready := envelope()
		ready.Message = &runtime.RuntimeEnvelope_Ready{Ready: &runtime.Ready{ArenaId: "test_arena", ArenaVersion: 1, TickRate: 60, SnapshotIntervalTicks: 3, MaxPlayers: 2}}
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
		s.Close()
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
	return s, f
}
func (f *fakeRuntime) send(e *runtime.RuntimeEnvelope) {
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
func (f *fakeRuntime) quiet(t *testing.T) {
	t.Helper()
	select {
	case e := <-f.got:
		t.Fatalf("unexpected runtime request %v", e)
	case <-time.After(40 * time.Millisecond):
	}
}

type credentials struct {
	MatchID   uint64 `json:"match_id"`
	PlayerID  uint64 `json:"player_id"`
	SessionID uint64 `json:"session_id"`
	Token     string `json:"session_token"`
	UDPIP     string `json:"udp_ip"`
	UDPPort   uint16 `json:"udp_port"`
	Version   uint16 `json:"protocol_version"`
	Arena     string `json:"arena_id"`
}

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
	status, body := post(t, s, "/rooms/1/join", map[string]any{"request_id": id, "protocol_version": 1})
	if status != http.StatusOK {
		t.Fatalf("join %d: %s", status, body)
	}
	var c credentials
	if err := json.Unmarshal(body, &c); err != nil {
		t.Fatal(err)
	}
	if c.SessionID == 0 || c.PlayerID == 0 || c.Token == "" || c.UDPPort == 0 || c.Arena != "test_arena" {
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
	packet, err := framing.EncodeDatagram(framing.Header{Version: 1, Type: kind, SessionID: c.SessionID, Sequence: seq}, b)
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
	status, _ := post(t, s, "/rooms/1/join", map[string]any{"request_id": "before_create", "protocol_version": 1})
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
	if err := proto.Unmarshal(receivePacket(t, p, adapter.Welcome), &welcome); err != nil || welcome.PlayerId != c.PlayerID || welcome.TickRate != 60 {
		t.Fatalf("welcome %v %v", &welcome, err)
	}
	sendPacket(t, p, c, 4, adapter.Hello, &client.Hello{SessionToken: c.Token})
	receivePacket(t, p, adapter.Welcome)
	f.quiet(t)
	sendPacket(t, p, c, 5, adapter.Input, &client.PlayerInput{InputSequence: 100, ClientTick: 201, MoveForward: 1, Yaw: .5})
	in := f.next(t).GetInput()
	if in == nil || in.PlayerId != c.PlayerID || in.InputSequence != 100 || in.ClientTick != 201 {
		t.Fatalf("bad input %v", in)
	}
	sendPacket(t, p, c, 6, adapter.Input, &client.PlayerInput{InputSequence: 99, MoveForward: -1})
	f.quiet(t)
	sendPacket(t, p, c, 7, adapter.Input, &client.PlayerInput{InputSequence: 100, MoveForward: -1})
	f.quiet(t)
	e = envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 3, Players: []*runtime.PlayerState{{PlayerId: c.PlayerID, X: 5, Z: 7, LastInputSequence: 100}}}}
	f.send(e)
	var snapshot client.WorldSnapshot
	if err := proto.Unmarshal(receivePacket(t, p, adapter.Snapshot), &snapshot); err != nil || snapshot.Tick != 3 || len(snapshot.Players) != 1 || snapshot.Players[0].X != 5 {
		t.Fatalf("snapshot %v %v", &snapshot, err)
	}
	status, _ = post(t, s, "/rooms/1/leave", map[string]any{"session_id": c.SessionID, "session_token": c.Token})
	if status != 200 {
		t.Fatal(status)
	}
	if leave := f.next(t).GetLeave(); leave == nil || leave.PlayerId != c.PlayerID {
		t.Fatal("leave missing")
	}
	sendPacket(t, p, c, 8, adapter.Input, &client.PlayerInput{InputSequence: 101, MoveForward: 1})
	f.quiet(t)
}

func TestCapacityExpiryAndRuntimeFailure(t *testing.T) {
	s, f := newTestServer(t)
	status, _ := post(t, s, "/rooms", map[string]any{})
	if status != 200 {
		t.Fatal(status)
	}
	a := reserve(t, s, "a")
	_ = reserve(t, s, "b")
	status, _ = post(t, s, "/rooms/1/join", map[string]any{"request_id": "c", "protocol_version": 1})
	if status != 409 {
		t.Fatalf("capacity status %d", status)
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
	if leave := f.next(t).GetLeave(); leave == nil || leave.PlayerId != c.PlayerID {
		t.Fatal("expiry leave missing")
	}
	f.mu.Lock()
	_ = f.conn.Close()
	f.mu.Unlock()
	deadline := time.Now().Add(2 * time.Second)
	for {
		status, _ = post(t, s, "/rooms/1/join", map[string]any{"request_id": "after_failure", "protocol_version": 1})
		if status == 503 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("runtime failure status %d", status)
		}
		time.Sleep(5 * time.Millisecond)
	}
}

func TestRuntimeMailboxPreservesControlAndCoalescesInput(t *testing.T) {
	l := &runtimeLink{inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1)}
	join := envelope()
	join.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: 1}}
	if err := l.control(join); err != nil {
		t.Fatal(err)
	}
	for _, seq := range []uint64{10, 12, 11} {
		if err := l.input(&runtime.PlayerInput{PlayerId: 1, InputSequence: seq}); err != nil {
			t.Fatal(err)
		}
	}
	batch := l.batch()
	if len(batch) != 2 || batch[0].GetJoin() == nil || batch[1].GetInput().InputSequence != 12 {
		t.Fatalf("batch %v", batch)
	}
	_ = l.input(&runtime.PlayerInput{PlayerId: 1, InputSequence: 13})
	leave := envelope()
	leave.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 1}}
	_ = l.control(leave)
	batch = l.batch()
	if len(batch) != 1 || batch[0].GetLeave() == nil {
		t.Fatalf("leave retained stale input %v", batch)
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
		{PlayerId: a.PlayerID, X: 1, Z: 2}, {PlayerId: b.PlayerID, X: 3, Z: 4},
	}}}
	f.send(e)
	first, second := receivePacket(t, pa, adapter.Snapshot), receivePacket(t, pb, adapter.Snapshot)
	if !bytes.Equal(first, second) {
		t.Fatal("peers observed different world payloads")
	}
	var snapshot client.WorldSnapshot
	if err := proto.Unmarshal(first, &snapshot); err != nil || len(snapshot.Players) != 2 || snapshot.Tick != 3 {
		t.Fatalf("full snapshot: %v %v", &snapshot, err)
	}
	// No producer clock in Gateway: neither elapsed time nor a repeated old
	// publication causes another datagram.
	f.send(e)
	_ = pa.SetReadDeadline(time.Now().Add(120 * time.Millisecond))
	if _, _, err := pa.ReadFromUDP(make([]byte, framing.MaxDatagram)); err == nil {
		t.Fatal("gateway repeated a snapshot without a new authoritative publication")
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
	if leave := f.next(t).GetLeave(); leave == nil || leave.PlayerId != c.PlayerID {
		t.Fatal("pending timeout must send ordered leave")
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
