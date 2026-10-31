// Package gateway composes the Object FPS room, adapter and runtime lifecycle.
// None of these product decisions belongs to the shared gateway module.
package gateway

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"math"
	"net"
	"net/http"
	"net/netip"
	"slices"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/httpserver"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// sessionTimeout is T_max and connectionInterruptedAfter T_int (v7.1
// outage-resync; network-architecture §4): the same values as the Client's
// (Movement.hpp), bound by tests/object_fps_pvp/fixtures/connection_timing.json.
// The Gateway counts T_max from the last uplink packet that refreshed the
// session; T_int is the silence a rebinding needs (receivePacket) and the one that
// restarts a Client's clock window (echo.go noteVerified).
const sessionTimeout = 5 * time.Second
const connectionInterruptedAfter = 500 * time.Millisecond
const joinTimeout = 3 * time.Second

type phase uint8

const (
	reserved phase = iota
	joining
	active
)

type Config struct{ HTTPAddress, UDPAddress, RuntimeAddress, AdvertiseIP string }
type reservation struct {
	results      resultSlot // at most one results frame waiting for its release (pv7 §2a)
	packetWindow time.Time
	packetCount  uint64
	session      *session.Session
	playerID     uint32
	requestID    string
	phase        phase
	joinStarted  time.Time
	outSequence  uint32
	// Diagnostics only: datagrams of this session and the last arrival, and
	// the intervals between this session's writes to its Client.
	received     uint64
	lastReceived time.Time
	sent         sendStatistics
	echo         sessionEcho // the time echo on Hello and Welcome (echo.go)
}
type outbound struct {
	actionPlayer uint32
	peer         netip.AddrPort
	packet       []byte
	// The discard a failed write is (pv7 §4), and its identity.
	player      uint32
	writeFailed playerIngressCounter
	sequence    uint32
	tick        uint32
}

// snapshotFrame is a Client snapshot waiting for the send loop, with its tick
// and when the Gateway read its IPC frame: each player's write encodes it with
// that player's gateway_hold_us (pv7 §5, D-4; diagnostics only).
type snapshotFrame struct {
	snapshot *client.WorldSnapshot
	read     time.Time
	tick     uint32
}

type Server struct {
	config      Config
	mu          sync.Mutex
	available   bool
	created     bool
	ready       *runtime.Ready
	link        *runtimeLink
	players     map[uint32]*reservation
	sessions    map[uint64]*reservation
	requests    map[string]*reservation
	nextPlayer  uint64
	http        *http.Server
	listener    net.Listener
	udp         *net.UDPConn
	controlOut  chan queuedControl
	snapshotOut chan snapshotFrame
	resultWake  chan struct{}
	// Diagnostics only (under mu): results frames for a player that was not
	// active, logged at close (pv7 §4 (d), not a loss).
	resultsForInactive   uint64
	closeOnce            sync.Once
	diagnosticsOnce      sync.Once // the last diagnostic window is logged once
	snapshotReplacements atomic.Uint64
	// The datagram receivePacket handles (under s.mu): when it was read and
	// its GYOP sequence, the identity of a drop it ends in (pv7 §4).
	datagram                dropEvent
	rateAcceptedPackets     atomic.Uint64
	rateRejectedPackets     atomic.Uint64
	maxSessionWindowPackets atomic.Uint64
	// Ingress statistics (v7 batch 08c), under mu: the Gateway-wide counters
	// and each player's datagram outcomes (IG). A player's counters outlive
	// its reservation (remove, runtimeFailed) until taken, like the link's.
	ingress       gatewayIngressCounts
	playerIngress map[uint32]*playerIngressCounts
	// The ingress lines, under mu: where the current window started, the
	// players that had a line, and whether the final one was written. The
	// final window is written once (finalOnce), on the first end path.
	ingressWindowStart time.Time
	ingressPlayers     map[uint32]struct{}
	ingressFinal       bool
	finalOnce          sync.Once
}

// The reusable Session's admission checks. Tests replace them to reach an
// admission error this Gateway does not know: the Session returns only
// ErrUnauthorized, ErrRateLimit and ErrStale.
var (
	sessionAdmit = (*session.Session).Admit
	sessionHello = (*session.Session).HelloRebinding
)

func New(ctx context.Context, cfg Config) (*Server, error) {
	ip, err := netip.ParseAddr(cfg.AdvertiseIP)
	if err != nil || !ip.Is4() || ip.IsUnspecified() {
		return nil, errors.New("advertise-ip must be a reachable literal IPv4 address")
	}
	udpAddr, err := net.ResolveUDPAddr("udp", cfg.UDPAddress)
	if err != nil {
		return nil, err
	}
	udp, err := net.ListenUDP("udp", udpAddr)
	if err != nil {
		return nil, err
	}
	listener, err := net.Listen("tcp", cfg.HTTPAddress)
	if err != nil {
		_ = udp.Close()
		return nil, err
	}
	link, ready, err := connectRuntime(ctx, cfg.RuntimeAddress)
	if err != nil {
		_ = udp.Close()
		_ = listener.Close()
		return nil, err
	}
	s := &Server{config: cfg, available: true, ready: ready, link: link, players: make(map[uint32]*reservation),
		sessions: make(map[uint64]*reservation), requests: make(map[string]*reservation),
		udp: udp, listener: listener, controlOut: make(chan queuedControl, 64), snapshotOut: make(chan snapshotFrame, 1),
		resultWake: make(chan struct{}, 1), ingressWindowStart: time.Now()}
	mux := http.NewServeMux()
	mux.HandleFunc("GET /rooms", s.listRooms)
	mux.HandleFunc("POST /rooms", s.createRoom)
	mux.HandleFunc("POST /rooms/1/join", s.joinRoom)
	mux.HandleFunc("POST /rooms/1/leave", s.leaveRoom)
	s.http = httpserver.New(cfg.HTTPAddress, mux)
	return s, nil
}

func (s *Server) HTTPAddress() string { return s.listener.Addr().String() }
func (s *Server) UDPAddress() string  { return s.udp.LocalAddr().String() }

// Serve keeps HTTP available after a runtime failure so room state reports the
// failure. Recovery means restarting Runtime/Gateway and joining again.
func (s *Server) Serve(ctx context.Context) error {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	defer s.Close()
	s.link.run(ctx, s.runtimeMessage, s.runtimeFailed)
	go s.runDiagnostics(ctx)
	go s.receiveUDP(ctx)
	go s.sendUDP(ctx)
	go s.expire(ctx)
	go s.logStatistics(ctx)
	httpErr := make(chan error, 1)
	go func() { httpErr <- s.http.Serve(s.listener) }()
	select {
	case <-ctx.Done():
		return nil
	case err := <-httpErr:
		if errors.Is(err, http.ErrServerClosed) {
			return nil
		}
		return err
	}
}

func (s *Server) Close() {
	s.closeOnce.Do(func() {
		s.link.closeGracefully() // every stream's final drop report first (pv7 §4)
		_ = s.udp.Close()
		_ = s.http.Close()
		_ = s.listener.Close()
		s.mu.Lock()
		inactive := s.resultsForInactive
		s.mu.Unlock()
		// results_for_inactive_frames appended (pv7 §4 (d)): logged only.
		log.Printf("gateway transport coalesced_snapshots=%d rate_accepted_packets=%d rate_limited_packets=%d max_session_window_packets=%d results_for_inactive_frames=%d", s.snapshotReplacements.Load(), s.rateAcceptedPackets.Load(), s.rateRejectedPackets.Load(), s.maxSessionWindowPackets.Load(), inactive)
		s.flushDiagnostics()
		s.flushFinal()
	})
}

func (s *Server) listRooms(w http.ResponseWriter, _ *http.Request) {
	s.mu.Lock()
	rooms := []any{}
	if s.created {
		rooms = append(rooms, s.room())
	}
	s.mu.Unlock()
	writeJSON(w, http.StatusOK, map[string]any{"rooms": rooms})
}
func (s *Server) createRoom(w http.ResponseWriter, r *http.Request) {
	// Consume the bounded request before replying. In particular, a client
	// using Connection: close may deliver the JSON after its headers; replying
	// early leaves unread TCP data and can reset that connection on Windows.
	r.Body = http.MaxBytesReader(w, r.Body, 4096)
	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(new(struct{})); err != nil {
		writeError(w, http.StatusBadRequest, "invalid_create")
		return
	}
	if err := decoder.Decode(new(any)); err != io.EOF {
		writeError(w, http.StatusBadRequest, "invalid_create")
		return
	}
	s.mu.Lock()
	if s.available {
		s.created = true
	}
	available, room := s.available, s.room()
	s.mu.Unlock()
	if !available {
		writeError(w, http.StatusServiceUnavailable, "runtime_unavailable")
		return
	}
	writeJSON(w, http.StatusOK, room)
}
func (s *Server) room() map[string]any {
	status := "ready"
	if !s.available {
		status = "unavailable"
	}
	return map[string]any{"id": 1, "players": len(s.players), "capacity": s.ready.MaxPlayers,
		"status": status, "arena_id": s.ready.ArenaId, "arena_version": s.ready.ArenaVersion, "arena_digest": s.ready.ArenaDigest}
}

type joinRequest struct {
	RequestID       string `json:"request_id"`
	ProtocolVersion uint16 `json:"protocol_version"`
}

func (s *Server) joinRoom(w http.ResponseWriter, r *http.Request) {
	r.Body = http.MaxBytesReader(w, r.Body, 4096)
	body, err := io.ReadAll(r.Body)
	if err != nil {
		writeError(w, http.StatusBadRequest, "invalid_join")
		return
	}
	version, ok := joinVersion(body)
	if !ok {
		writeError(w, http.StatusBadRequest, "invalid_join")
		return
	}
	if version != adapter.ClientVersion {
		// The version is read before the rest (pv7 contract §1): a Client of
		// another version may send keys this version rejects. The Gateway
		// states its own version so the Client can name both; every
		// protocol_version key is the sender's.
		log.Printf("join refused remote=%s code=protocol_version expected=%d received=%d", r.RemoteAddr, adapter.ClientVersion, version)
		writeJSON(w, http.StatusConflict, map[string]any{"error": "protocol_version", "protocol_version": adapter.ClientVersion})
		return
	}
	decoder := json.NewDecoder(bytes.NewReader(body))
	decoder.DisallowUnknownFields()
	var request joinRequest
	if err := decoder.Decode(&request); err != nil {
		writeError(w, http.StatusBadRequest, "invalid_join")
		return
	}
	if err := decoder.Decode(new(any)); err != io.EOF {
		writeError(w, http.StatusBadRequest, "invalid_join")
		return
	}
	if strings.TrimSpace(request.RequestID) == "" || len(request.RequestID) > 128 {
		writeError(w, http.StatusBadRequest, "invalid_request_id")
		return
	}
	data, status, code := s.reserveSlot(request)
	if code != "" {
		log.Printf("join refused remote=%s code=%s", r.RemoteAddr, code)
		writeError(w, status, code)
		return
	}
	log.Printf("join reserved player=%v remote=%s", data["player_id"], r.RemoteAddr)
	writeJSON(w, status, data)
}

// joinVersion reads only protocol_version from a join body that is one JSON
// object: missing or null is 0, and anything but an integer in 0..65535 is not
// a version (400 invalid_join).
func joinVersion(body []byte) (uint16, bool) {
	var peek struct {
		ProtocolVersion json.RawMessage `json:"protocol_version"`
	}
	if err := json.Unmarshal(body, &peek); err != nil {
		return 0, false
	}
	var version uint16
	if len(peek.ProtocolVersion) > 0 && json.Unmarshal(peek.ProtocolVersion, &version) != nil {
		return 0, false
	}
	return version, true
}

func (s *Server) reserveSlot(request joinRequest) (map[string]any, int, string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.available {
		return nil, http.StatusServiceUnavailable, "runtime_unavailable"
	}
	if !s.created {
		return nil, http.StatusNotFound, "room_not_found"
	}
	if existing := s.requests[request.RequestID]; existing != nil {
		return s.reservationData(existing), http.StatusOK, ""
	}
	if len(s.players) >= int(s.ready.MaxPlayers) {
		return nil, http.StatusConflict, "room_full"
	}
	createdAt := time.Now()
	peer, err := session.New(createdAt)
	if err != nil {
		return nil, http.StatusInternalServerError, "session_creation_failed"
	}
	for s.sessions[peer.ID] != nil {
		peer, err = session.New(createdAt)
		if err != nil {
			return nil, http.StatusInternalServerError, "session_creation_failed"
		}
	}
	if s.nextPlayer >= math.MaxUint32 { // protocol-v8 §2a: player ids end at the uint32 maximum
		return nil, http.StatusServiceUnavailable, "player_ids_exhausted"
	}
	s.nextPlayer++
	player := &reservation{packetWindow: createdAt, session: peer, playerID: uint32(s.nextPlayer), requestID: request.RequestID}
	s.players[player.playerID] = player
	s.sessions[peer.ID] = player
	s.requests[request.RequestID] = player
	return s.reservationData(player), http.StatusOK, ""
}
func (s *Server) reservationData(p *reservation) map[string]any {
	return map[string]any{"player_id": p.playerID, "session_id": p.session.ID,
		"session_token": p.session.Token, "udp_ip": s.config.AdvertiseIP, "udp_port": s.udp.LocalAddr().(*net.UDPAddr).Port,
		"protocol_version": adapter.ClientVersion, "arena_id": s.ready.ArenaId, "arena_version": s.ready.ArenaVersion, "arena_digest": s.ready.ArenaDigest}
}
func (s *Server) leaveRoom(w http.ResponseWriter, r *http.Request) {
	r.Body = http.MaxBytesReader(w, r.Body, 4096)
	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()
	var request struct {
		SessionID    uint64 `json:"session_id"`
		SessionToken string `json:"session_token"`
	}
	if decoder.Decode(&request) != nil || request.SessionID == 0 || request.SessionToken == "" {
		writeError(w, http.StatusBadRequest, "invalid_leave")
		return
	}
	if err := decoder.Decode(new(any)); err != io.EOF {
		writeError(w, http.StatusBadRequest, "invalid_leave")
		return
	}
	s.mu.Lock()
	p := s.sessions[request.SessionID]
	if p == nil {
		s.mu.Unlock()
		writeJSON(w, http.StatusOK, map[string]bool{"left": true})
		return
	}
	if !p.session.MatchesToken(request.SessionToken) {
		s.mu.Unlock()
		writeError(w, http.StatusForbidden, "invalid_session")
		return
	}
	var failure error
	if p.phase != reserved {
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: p.playerID,
			Reason: runtime.LeaveReason_LEAVE_REASON_CLIENT_REQUEST}}
		failure = s.link.control(e)
	}
	log.Printf("player left player=%d", p.playerID)
	s.remove(p)
	s.mu.Unlock()
	if failure != nil {
		s.runtimeFailed(failure)
		writeError(w, http.StatusServiceUnavailable, "runtime_unavailable")
		return
	}
	writeJSON(w, http.StatusOK, map[string]bool{"left": true})
}
func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}
func writeError(w http.ResponseWriter, status int, code string) {
	writeJSON(w, status, map[string]string{"error": code})
}

func (s *Server) receiveUDP(ctx context.Context) {
	buffer := make([]byte, framing.MaxDatagram+1)
	for {
		n, peer, err := s.udp.ReadFromUDPAddrPort(buffer)
		if err != nil {
			if ctx.Err() == nil {
				log.Printf("UDP stopped: %v", err)
			}
			return
		}
		if err = s.receiveDatagram(buffer[:n], peer, time.Now()); err != nil {
			s.runtimeFailed(err)
		}
	}
}

// receiveDatagram handles one datagram read from the UDP socket; an error
// fails the runtime.
func (s *Server) receiveDatagram(datagram []byte, peer netip.AddrPort, now time.Time) error {
	header, payload, err := framing.DecodeDatagram(datagram)
	if err != nil {
		s.mu.Lock()
		s.ingress[gatewayReceivedDatagrams]++
		s.countGatewayLocked(gatewayDropUndecodableDatagrams, dropEvent{at: now})
		s.mu.Unlock()
		return nil
	}
	return s.receivePacket(header, payload, peer, now)
}

// receivePacket gives every datagram exactly one outcome. One that belongs to
// no session is a Gateway-wide drop; a session's datagram is counted received
// for its player and, at each return, in exactly one fwd_ or drop_ counter, so
// the player's received_datagrams equals the sum of its outcomes (IG).
func (s *Server) receivePacket(h framing.Header, payload []byte, peer netip.AddrPort, now time.Time) (err error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.datagram = dropEvent{at: now, transport: h.Sequence}
	defer func() { s.datagram = dropEvent{} }()
	s.ingress[gatewayReceivedDatagrams]++
	if h.Version != adapter.ClientVersion {
		s.countGatewayLocked(gatewayDropOtherVersionDatagrams, s.datagram)
		return nil
	}
	if !s.available {
		s.ingress[gatewayDropRoomUnavailableDatagrams]++
		return nil
	}
	p := s.sessions[h.SessionID]
	if p == nil {
		s.countGatewayLocked(gatewayDropUnknownSessionDatagrams, s.datagram)
		return nil
	}
	p.received++
	p.lastReceived = now
	s.countIngress(p, playerReceivedDatagrams, 1)
	defer func() {
		// The runtime link failed on this datagram (receiveUDP then makes the
		// room unavailable). No outcome was counted for it: it leaves its
		// player's IG and is dropped as for an unavailable room.
		if err != nil {
			s.playerIngress[p.playerID][playerReceivedDatagrams]--
			s.ingress[gatewayDropRoomUnavailableDatagrams]++
		}
	}()
	switch h.Type {
	case adapter.Hello:
		var hello client.Hello
		if proto.Unmarshal(payload, &hello) != nil {
			// Before authentication: the session it claims.
			s.countIngress(p, playerDropMalformedHelloPackets, 1)
			return nil
		}
		before := p.session.LastVerified()
		// The product's rule for moving a session to a new endpoint
		// (network-architecture §4): T_int of silence, within T_max.
		rebound, admission := sessionHello(p.session, hello.SessionToken, peer, h.Sequence, now,
			session.Rebinding{Silence: connectionInterruptedAfter, Timeout: sessionTimeout})
		s.noteVerified(p, before, now)
		s.trackAdmission(p, ingressHello, now, admission)
		if admission != nil {
			return nil
		}
		s.noteHello(p, &hello, now)
		if rebound.Previous.IsValid() {
			// Ports only: no token and no full address in the log.
			log.Printf("session rebound version=1 player=%d old_port=%d new_port=%d silent_ms=%d",
				p.playerID, rebound.Previous.Port(), peer.Port(), rebound.Silent.Milliseconds())
		}
		switch p.phase {
		case reserved:
			p.phase = joining
			p.joinStarted = now
			log.Printf("player hello player=%d udp=%s", p.playerID, peer)
			e := envelope()
			e.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: p.playerID}}
			if err := s.link.control(e); err != nil {
				return err
			}
		case active:
			s.welcome(p)
		}
		s.countIngress(p, playerFwdAdmittedHelloPackets, 1)
	case adapter.Actions:
		if p.phase != active {
			s.countIngress(p, playerDropInactiveActionsPackets, 1)
			return nil
		}
		if s.admit(p, ingressActions, peer, h.Sequence, now) != nil {
			return nil
		}
		return s.receiveActions(p, h, payload, now)
	case adapter.Input:
		if p.phase != active {
			s.countIngress(p, playerDropInactiveInputPackets, 1)
			return nil
		}
		// A stale transport sequence still carries a complete payload (Hello,
		// Input and Actions share the Client's packet sequence): it is decoded
		// and forwarded, but never commits the sequence, so it neither extends
		// liveness nor moves the sequence. The rate limit already counted it.
		admission := s.admit(p, ingressInput, peer, h.Sequence, now)
		stale := errors.Is(admission, session.ErrStale)
		if admission != nil && !stale {
			return nil
		}
		in, err := adapter.DecodeInput(payload, p.playerID)
		if err != nil {
			s.countIngress(p, playerDropMalformedInputPackets, 1)
			return nil
		}
		return s.forwardInput(p, in, h.Sequence, stale, now)
	default:
		if s.admit(p, ingressOther, peer, h.Sequence, now) == nil {
			s.countIngress(p, playerDropUnknownKindPackets, 1)
		}
	}
	return nil
}

// countIngress adds n to one of a player's ingress counters. Called with s.mu
// held.
func (s *Server) countIngress(p *reservation, counter playerIngressCounter, n uint64) {
	e := s.datagram // a drop of the datagram being handled starts its dwell at its receipt
	if e.at.IsZero() {
		e.at = time.Now()
	}
	s.countIngressAt(p.playerID, counter, n, e)
}

// countIngressAt counts into the player's 10 s line and, from the same entry,
// a reported discard into the cumulative ledger (pv7 §4). Under s.mu.
func (s *Server) countIngressAt(player uint32, counter playerIngressCounter, n uint64, e dropEvent) {
	if s.playerIngress == nil {
		s.playerIngress = make(map[uint32]*playerIngressCounts)
	}
	counts := s.playerIngress[player]
	if counts == nil {
		counts = new(playerIngressCounts)
		s.playerIngress[player] = counts
	}
	counts[counter] += n
	s.link.countDrop(player, counter, n, e)
}

// countGatewayLocked counts one Gateway-wide item, the same way. Under s.mu.
func (s *Server) countGatewayLocked(counter gatewayIngressCounter, e dropEvent) {
	s.ingress[counter]++
	if reason := gatewayDropReason[counter]; reason != runtime.DropReason_DROP_REASON_UNSPECIFIED {
		e.reason = reason
		s.link.ledger().add(0, 1, e)
	}
}

// takeIngress hands over the Server's ingress counters accumulated since the
// previous call: each player's datagram outcomes, and the Gateway-wide
// counters. The link's command counters (I0) are taken by its own
// takeIngress; a player's line adds both (disjoint keys).
func (s *Server) takeIngress() (map[uint32]*playerIngressCounts, gatewayIngressCounts) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.takeIngressLocked()
}
func (s *Server) takeIngressLocked() (map[uint32]*playerIngressCounts, gatewayIngressCounts) {
	players, gateway := s.playerIngress, s.ingress
	s.playerIngress, s.ingress = nil, gatewayIngressCounts{}
	return players, gateway
}
func (s *Server) welcome(p *reservation) {
	s.sendControl(p, adapter.Welcome, &client.Welcome{PlayerId: p.playerID, TickRate: s.ready.TickRate,
		SnapshotRate: s.ready.TickRate / s.ready.SnapshotIntervalTicks, ArenaId: s.ready.ArenaId, ArenaVersion: s.ready.ArenaVersion, ArenaDigest: s.ready.ArenaDigest, CombatRules: adapter.RulesForClient(s.ready.CombatRules), JumpHeight: s.ready.JumpHeight, Gravity: s.ready.Gravity,
		CrouchBodyHeight: s.ready.CrouchBodyHeight, CrouchEyeHeight: s.ready.CrouchEyeHeight, CrouchSpeedScale: s.ready.CrouchSpeedScale, SprintSpeedScale: s.ready.SprintSpeedScale,
		Clock: s.welcomeSample(p, time.Now())})
}

// A control (Welcome, Error) waiting for the UDP writer, which assigns its
// GYOP sequence right before the write (pv7 §2a: the Client drops a packet
// whose sequence is not newer, so no sequence is taken before then). It keeps
// its reservation: an Error may follow the player's removal.
type queuedControl struct {
	p       *reservation
	kind    uint16
	payload []byte
}

func (s *Server) sendControl(p *reservation, kind uint16, message proto.Message) {
	payload, err := proto.Marshal(message)
	if err != nil {
		// Strings were UTF-8-validated when the runtime message was read; the
		// other fields are numbers or constants.
		s.invariantFailed("a control message does not marshal: " + err.Error())
		return
	}
	select {
	case s.controlOut <- queuedControl{p: p, kind: kind, payload: payload}:
	default:
		s.countIngressAt(p.playerID, playerDropControlQueueFullControlPackets, 1, dropEvent{at: time.Now()})
	}
}

// controlPacket sequences and encodes a queued control in the UDP writer.
func (s *Server) controlPacket(c queuedControl) (outbound, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	c.p.outSequence++
	packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: c.kind, SessionID: c.p.session.ID, Sequence: c.p.outSequence}, c.payload)
	if err != nil {
		// Every control fits: a Welcome by ReadyFitsWelcome, a join_rejected
		// reason by the size check where the JoinResult is read, the other
		// errors are constant.
		s.invariantFailed("a control packet does not encode: " + err.Error())
		return outbound{}, false
	}
	return outbound{peer: c.p.session.Endpoint(), packet: packet, player: c.p.playerID,
		writeFailed: playerDropWriteFailedControlPackets, sequence: c.p.outSequence}, true
}

func (s *Server) runtimeMessage(e *runtime.RuntimeEnvelope) {
	if results := e.GetActionResults(); results != nil {
		if err := s.receiveActionResults(results); err != nil {
			s.runtimeFailed(err)
		}
		return
	}
	if result := e.GetJoinResult(); result != nil {
		// A rejection's reason travels to the Client in an Error control: it
		// must fit one datagram (C1).
		if proto.Size(&client.Error{Code: "join_rejected", Message: result.Reason}) > framing.MaxDatagram-framing.HeaderSize {
			s.runtimeFailed(protocolError{errors.New("join result reason exceeds the datagram limit")})
			return
		}
		s.mu.Lock()
		defer s.mu.Unlock()
		p := s.players[result.PlayerId]
		if p == nil {
			// The player left or timed out while its Join was in flight.
			return
		}
		if p.phase != joining {
			// The Gateway sends one Join per player, at its first Hello: a
			// second result is an IPC contract violation (C2).
			go s.runtimeFailed(protocolError{fmt.Errorf("join result for player %d, which is not joining", p.playerID)})
			return
		}
		if !result.Accepted {
			log.Printf("player join rejected player=%d reason=%s", p.playerID, result.Reason)
			s.sendControl(p, adapter.Failure, &client.Error{Code: "join_rejected", Message: result.Reason})
			s.remove(p)
			return
		}
		p.phase = active
		log.Printf("player active player=%d", p.playerID)
		s.welcome(p)
		return
	}
	if snapshot := e.GetSnapshot(); snapshot != nil {
		out, err := adapter.SnapshotForClient(snapshot, s.ready.CombatRules)
		if err != nil {
			s.runtimeFailed(err)
			return
		}
		s.mu.Lock()
		if !s.available {
			s.mu.Unlock()
			return
		}
		// No tick or cursor guard (D1, D2 removed, pv7 §2a D-12): the IPC is
		// ordered and the Client applies only a newer tick. The Gateway keeps
		// no copy of the Match's cursors.
		s.mu.Unlock()
		read := time.Now()
		// The bound with the largest hold any write can give it.
		widest := uint32(dropMaximumDwell / time.Microsecond)
		out.GatewayHoldUs = &widest
		payload, err := proto.Marshal(out)
		out.GatewayHoldUs = nil
		if err != nil {
			s.runtimeFailed(err)
			return
		}
		if len(payload) > framing.MaxDatagram-framing.HeaderSize {
			s.runtimeFailed(errors.New("snapshot exceeds datagram limit"))
			return
		}
		frame := snapshotFrame{snapshot: out, read: read, tick: out.Tick}
		select {
		case s.snapshotOut <- frame:
		default:
			select {
			case replaced := <-s.snapshotOut:
				// Sent to no Client (pv7 §4, 45).
				s.snapshotReplacements.Add(1)
				s.mu.Lock()
				s.countGatewayLocked(gatewayDropCoalescedSnapshots, dropEvent{at: time.Now(), tick: replaced.tick})
				s.mu.Unlock()
			default:
			}
			select {
			case s.snapshotOut <- frame:
			default:
				// The slot was just drained and only this reader goroutine
				// fills it.
				s.invariantFailed("the snapshot slot was refilled while the runtime reader held it")
			}
		}
		return
	}
	if evicted := e.GetEvicted(); evicted != nil {
		// The Match already removed the player for sustained poor connection
		// quality. Clear the reservation like a Leave and tell the Client why.
		s.mu.Lock()
		defer s.mu.Unlock()
		p := s.players[evicted.PlayerId]
		if p == nil {
			return
		}
		code, message := adapter.EvictionNotice(evicted)
		log.Printf("player evicted player=%d reason=%s reference_age_ms=%d substituted_permille=%d movement_resets=%d",
			evicted.PlayerId, code, evicted.ReferenceAgeMs, evicted.SubstitutedPermille, evicted.MovementResets)
		s.sendControl(p, adapter.Failure, &client.Error{Code: code, Message: message})
		// The link forgets the player's statistics; its queued batches still
		// reach the Match (pv7 §2a).
		s.link.forget(evicted.PlayerId)
		s.remove(p)
		return
	}
	if problem := e.GetError(); problem != nil {
		log.Printf("runtime error player=%d code=%s", problem.PlayerId, problem.Code)
	}
}

// Wakes the send loop when a results frame arrived for a Client.
func (s *Server) wakeResults() {
	select {
	case s.resultWake <- struct{}{}:
	default:
	}
}

// How long the send loop waits for the earliest result deadline: until it
// (none if it has passed), or one interval with nothing pending, a wake that
// sends nothing.
func resultWait(deadline time.Time, pending bool, now time.Time) time.Duration {
	if !pending {
		return actionSendInterval
	}
	return max(deadline.Sub(now), 0)
}

func (s *Server) sendUDP(ctx context.Context) {
	// Results go out on a wake (a frame arrived) or at the earliest release, a
	// strict ActionSendRate interval from that player's last completed write:
	// at once after an idle interval, never sooner. This goroutine is the one
	// UDP writer: every GYOP sequence is assigned here, right before its write.
	timer := time.NewTimer(actionSendInterval)
	defer timer.Stop()
	results := func() {
		packets, deadline, pending := s.actionPackets(time.Now())
		for _, packet := range packets {
			s.writeUDP(packet)
			s.actionWritten(packet.actionPlayer, time.Now(), len(packet.packet))
		}
		if len(packets) > 0 {
			// The writes re-anchored their players' deadlines.
			deadline, pending = s.resultDeadline()
		}
		timer.Reset(resultWait(deadline, pending, time.Now()))
	}
	for {
		select {
		case <-ctx.Done():
			return
		case <-s.resultWake:
			results()
		case <-timer.C:
			results()
		case control := <-s.controlOut:
			if packet, ok := s.controlPacket(control); ok {
				s.writeUDP(packet)
			}
		case frame := <-s.snapshotOut:
			s.mu.Lock()
			players := make([]uint32, 0, len(s.players))
			if s.available {
				for _, p := range s.players {
					if p.phase == active {
						players = append(players, p.playerID)
					}
				}
			}
			s.mu.Unlock()
			for i, playerID := range players {
				var packet outbound
				frame, packet = s.snapshotForPeer(players[i:], frame)
				s.writeUDP(packet)
				if packet.peer.IsValid() {
					s.snapshotWritten(playerID, time.Now(), len(packet.packet))
				}
			}
		}
	}
}

// snapshotForPeer encodes the frame for the first of the players the loop has
// still to serve (remaining).
func (s *Server) snapshotForPeer(remaining []uint32, frame snapshotFrame) (snapshotFrame, outbound) {
	// A previous peer's write may have blocked while a newer publication arrived.
	// Replace this still-unwritten datagram before assigning its transport sequence.
	replaced := false
	var old snapshotFrame
	select {
	case newer := <-s.snapshotOut:
		s.snapshotReplacements.Add(1)
		old, frame, replaced = frame, newer, true
	default:
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if replaced {
		// Every active player still to serve never gets the replaced one (pv7 §4, 31).
		now := time.Now()
		for _, id := range remaining {
			if p := s.players[id]; s.available && p != nil && p.phase == active {
				s.countIngressAt(id, playerDropReplacedSnapshotPackets, 1, dropEvent{at: now, tick: old.tick})
			}
		}
	}
	p := s.players[remaining[0]]
	if !s.available || p == nil || p.phase != active {
		return frame, outbound{}
	}
	// This player's hold: from reading the IPC frame to this write (pv7 §5).
	hold := dwellMicros(frame.read, time.Now())
	frame.snapshot.GatewayHoldUs = &hold
	// runtimeMessage marshalled this snapshot with the widest hold and checked
	// its size; this hold is no wider, so neither step can fail here.
	payload, err := proto.Marshal(frame.snapshot)
	if err != nil {
		s.invariantFailed("a client snapshot does not marshal: " + err.Error())
		return frame, outbound{}
	}
	p.outSequence++
	packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: adapter.Snapshot, SessionID: p.session.ID, Sequence: p.outSequence}, payload)
	if err != nil {
		s.invariantFailed("a client snapshot does not encode: " + err.Error())
		return frame, outbound{}
	}
	return frame, outbound{peer: p.session.Endpoint(), packet: packet, player: p.playerID,
		writeFailed: playerDropWriteFailedSnapshotPackets, sequence: p.outSequence, tick: frame.tick}
}

// udpWrite writes one datagram; tests replace it to fail a write.
var udpWrite = func(s *Server, packet []byte, peer netip.AddrPort) error {
	_ = s.udp.SetWriteDeadline(time.Now().Add(time.Second))
	_, err := s.udp.WriteToUDPAddrPort(packet, peer)
	return err
}

func (s *Server) writeUDP(packet outbound) {
	if packet.peer.IsValid() {
		if err := udpWrite(s, packet.packet, packet.peer); err != nil && packet.writeFailed != 0 {
			// A failed write to this player is a discard (pv7 §4, D5).
			s.mu.Lock()
			s.countIngressAt(packet.player, packet.writeFailed, 1, dropEvent{at: time.Now(), transport: packet.sequence, tick: packet.tick})
			s.mu.Unlock()
		}
	}
}

func (s *Server) expire(ctx context.Context) {
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case now := <-ticker.C:
			s.expireAt(now)
		}
	}
}
func (s *Server) expireAt(now time.Time) {
	s.mu.Lock()
	var failure error
	for _, p := range s.players {
		sessionExpired := p.session.Expired(now, sessionTimeout)
		joinExpired := p.phase == joining && now.Sub(p.joinStarted) >= joinTimeout
		if sessionExpired || joinExpired {
			reason, leave := "session_timeout", runtime.LeaveReason_LEAVE_REASON_SESSION_TIMEOUT
			if joinExpired {
				reason, leave = "join_timeout", runtime.LeaveReason_LEAVE_REASON_JOIN_TIMEOUT
			}
			log.Printf("player expired player=%d reason=%s", p.playerID, reason)
			if p.phase != reserved {
				e := envelope()
				e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: p.playerID, Reason: leave}}
				if err := s.link.control(e); err != nil {
					failure = err
				}
			}
			s.remove(p)
		}
	}
	s.mu.Unlock()
	if failure != nil {
		s.runtimeFailed(failure)
	}
}

// Every statisticsInterval: each player's phase, endpoint, datagrams and the
// age of the last one, with the Gateway-wide rate counters, then the player's
// send intervals (results, snapshots and runtime link action batches).
// Diagnostics only.
const statisticsInterval = 10 * time.Second

func (s *Server) logStatistics(ctx context.Context) {
	ticker := time.NewTicker(statisticsInterval)
	defer ticker.Stop()
	windowStart := time.Now()
	for {
		select {
		case <-ctx.Done():
			return
		case now := <-ticker.C:
			s.logStatisticsWindow(now, now.Sub(windowStart))
			windowStart = now
		}
	}
}

// logStatisticsWindow writes one window's lines. The ingress lines go with
// them until the final one: the Gateway's after the gateway statistics line,
// each current player's after its send statistics line, then those of removed
// players with counters left.
func (s *Server) logStatisticsWindow(now time.Time, window time.Duration) {
	links := s.link.takeActionStatistics()
	s.mu.Lock()
	defer s.mu.Unlock()
	ids := make([]uint32, 0, len(s.players))
	for id := range s.players {
		ids = append(ids, id)
	}
	slices.Sort(ids)
	log.Printf("gateway statistics players=%d rate_accepted_packets=%d rate_limited_packets=%d", len(ids),
		s.rateAcceptedPackets.Load(), s.rateRejectedPackets.Load())
	ingress := map[uint32]string{}
	var removed []playerIngressLine
	if !s.ingressFinal {
		gateway, lines := s.ingressWindowLocked(now, false)
		log.Print(gateway)
		for _, line := range lines {
			if s.players[line.player] != nil {
				ingress[line.player] = line.line
			} else {
				removed = append(removed, line)
			}
		}
	}
	for _, id := range ids {
		p := s.players[id]
		age := int64(-1)
		if !p.lastReceived.IsZero() {
			age = now.Sub(p.lastReceived).Milliseconds()
		}
		log.Printf("player statistics player=%d phase=%d udp=%s received=%d last_received_ms=%d", id, p.phase,
			p.session.Endpoint(), p.received, age)
		link, ok := links[id]
		if !ok {
			link = noIntervals
		}
		log.Print(sendStatisticsLine(id, window, p.sent.results.take(), p.sent.snapshots.take(), link,
			p.sent.resultBytes.take(), p.sent.snapshotBytes.take()))
		if line, ok := ingress[id]; ok {
			log.Print(line)
		}
	}
	for _, line := range removed {
		log.Print(line.line)
	}
}

// One player's ingress line of a window.
type playerIngressLine struct {
	player uint32
	line   string
}

// ingressWindowLocked takes the ingress counters since the previous window,
// the Server's (datagram outcomes, Gateway-wide) and the link's (commands),
// and formats them: the Gateway line, and by ascending id a line for every
// current player and every player with counters taken, a removed one
// included. The final window (final=1) also has a line for every player that
// had one before, so each player's lines end with exactly one final line.
// Called with s.mu held.
func (s *Server) ingressWindowLocked(now time.Time, final bool) (string, []playerIngressLine) {
	var window time.Duration
	if !s.ingressWindowStart.IsZero() {
		window = max(now.Sub(s.ingressWindowStart), 0)
	}
	s.ingressWindowStart = now
	players, gateway := s.takeIngressLocked()
	if players == nil {
		players = map[uint32]*playerIngressCounts{}
	}
	for id, counts := range s.link.takeIngress() {
		if players[id] == nil {
			players[id] = new(playerIngressCounts)
		}
		players[id].add(counts)
	}
	for id := range s.players {
		if players[id] == nil {
			players[id] = new(playerIngressCounts)
		}
	}
	if final {
		s.ingressFinal = true
		for id := range s.ingressPlayers {
			if players[id] == nil {
				players[id] = new(playerIngressCounts)
			}
		}
	}
	ids := make([]uint32, 0, len(players))
	for id := range players {
		ids = append(ids, id)
	}
	slices.Sort(ids)
	lines := make([]playerIngressLine, 0, len(ids))
	for _, id := range ids {
		s.noteIngressPlayerLocked(id)
		lines = append(lines, playerIngressLine{id, playerIngressStatisticsLine(id, window, final, players[id])})
	}
	return gatewayIngressStatisticsLine(window, final, &gateway), lines
}

func (s *Server) noteIngressPlayerLocked(id uint32) {
	if s.ingressPlayers == nil {
		s.ingressPlayers = map[uint32]struct{}{}
	}
	s.ingressPlayers[id] = struct{}{}
}

// flushFinal writes the last ingress window (final=1) once, on the first end
// path that reaches it: Close (the context or the HTTP server ended Serve) or
// runtimeFailed, each after link.close(). A concurrent second caller returns
// once the first wrote it.
func (s *Server) flushFinal() {
	s.finalOnce.Do(s.writeFinalIngress)
}

func (s *Server) writeFinalIngress() {
	// close() does not wait for the writer: a pass it took counts its commands
	// written or abandoned until it exits, and I0 needs them in this window.
	s.link.waitWriter()
	s.mu.Lock()
	defer s.mu.Unlock()
	gateway, lines := s.ingressWindowLocked(time.Now(), true)
	log.Print(gateway)
	for _, line := range lines {
		log.Print(line.line)
	}
}

func (s *Server) remove(p *reservation) {
	s.endEcho(p, time.Now())
	s.link.ledger().remove(p.playerID) // its stream ends with one final report
	delete(s.players, p.playerID)
	delete(s.sessions, p.session.ID)
	delete(s.requests, p.requestID)
}

// invariantFailed reports a broken internal invariant of the Gateway, a
// Programmer Error (docs/architecture/error-handling.md; D180 (g2)): the
// runtime fails as it does for a protocol error, so the room becomes
// unavailable instead of going on with a silently dropped packet. The log
// line is "runtime disconnected reason=other error=\"gateway invariant
// failed: ...\"". The failure runs on its own goroutine, so callers may hold
// s.mu (runtimeFailed takes it, and calls sendControl).
func (s *Server) invariantFailed(what string) {
	go s.runtimeFailed(fmt.Errorf("gateway invariant failed: %s", what))
}

func (s *Server) runtimeFailed(err error) {
	s.mu.Lock()
	if !s.available {
		s.mu.Unlock()
		return
	}
	s.available = false
	for _, p := range s.players {
		if p.phase != reserved {
			s.sendControl(p, adapter.Failure, &client.Error{Code: "runtime_unavailable", Message: "Match ended; restart services and join again"})
		}
		// The final ingress window writes them although the reservations go.
		s.noteIngressPlayerLocked(p.playerID)
	}
	clear(s.players)
	clear(s.sessions)
	clear(s.requests)
	s.mu.Unlock()
	s.link.close()
	log.Printf("runtime disconnected reason=%s error=%q", disconnectReason(err), err.Error())
	s.flushFinal()
}

// forwardInput hands a decoded window to the Match as it came, one IPC message
// per datagram (pv7 §2a): the Match decides what is resolved, stale, copied
// or conflicting (D49). Decoding succeeded, so the sequence and liveness are
// committed first (D-11), unless the sequence is stale.
//
// The datagram's one outcome (IG) is fwd_main_input_packets, or its stale
// sequence (D55 ③). A failed link counts none (receivePacket counts the
// datagram for the unavailable room).
func (s *Server) forwardInput(p *reservation, in *runtime.PlayerInput, sequence uint32, stale bool, now time.Time) error {
	// Admit and CommitSequence both run under s.mu and Admit found this
	// sequence fresh, so the commit cannot fail.
	if !stale {
		if err := p.session.CommitSequence(sequence, now); err != nil {
			s.invariantFailed("an admitted input sequence did not commit: " + err.Error())
		}
	}
	e := s.datagram
	e.epoch, e.life = in.MovementEpoch, in.LifeGeneration
	e.firstCommand, e.lastCommand = in.Commands[0].Sequence, in.Commands[len(in.Commands)-1].Sequence
	e.items = uint32(len(in.Commands))
	if err := s.link.input(in, e); err != nil {
		if errors.Is(err, adapter.ErrInput) {
			// DecodeInput checked the same bounds (D183).
			s.invariantFailed("a decoded input window was refused by the link: " + err.Error())
			s.countIngress(p, playerDropMalformedInputPackets, 1)
			return nil
		}
		return err
	}
	outcome := playerFwdMainInputPackets
	if stale {
		outcome = playerFwdStaleSequenceInputPackets
	}
	s.countIngress(p, outcome, 1)
	return nil
}
