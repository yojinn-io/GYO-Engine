// Package gateway composes the Object FPS room, adapter and runtime lifecycle.
// None of these product decisions belongs to the shared gateway module.
package gateway

import (
	"context"
	"encoding/json"
	"errors"
	"io"
	"log"
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
	client "gyo.local/object_fps_pvp/protocol/clientv6"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

const sessionTimeout = 5 * time.Second
const joinTimeout = 3 * time.Second

type phase uint8

const (
	reserved phase = iota
	joining
	active
)

type Config struct{ HTTPAddress, UDPAddress, RuntimeAddress, AdvertiseIP string }
type reservation struct {
	actions        *actionWindow
	packetWindow   time.Time
	packetCount    uint64
	session        *session.Session
	playerID       uint64
	requestID      string
	phase          phase
	joinStarted    time.Time
	lastResolved   uint64
	movementEpoch  uint64
	lifeGeneration uint64
	commands       map[uint64]*runtime.MovementCommand
	outSequence    uint32
	// Diagnostics only: datagrams of this session and the last arrival, and
	// the intervals between this session's writes to its Client.
	received     uint64
	lastReceived time.Time
	sent         sendStatistics
}
type outbound struct {
	actionPlayer uint64
	peer         netip.AddrPort
	packet       []byte
}

type Server struct {
	config                  Config
	mu                      sync.Mutex
	available               bool
	created                 bool
	ready                   *runtime.Ready
	link                    *runtimeLink
	players                 map[uint64]*reservation
	sessions                map[uint64]*reservation
	requests                map[string]*reservation
	nextPlayer              uint64
	lastSnapshot            uint64
	hasSnapshot             bool
	http                    *http.Server
	listener                net.Listener
	udp                     *net.UDPConn
	controlOut              chan outbound
	snapshotOut             chan []byte
	resultWake              chan struct{}
	closeOnce               sync.Once
	snapshotReplacements    atomic.Uint64
	rateAcceptedPackets     atomic.Uint64
	rateRejectedPackets     atomic.Uint64
	maxSessionWindowPackets atomic.Uint64
}

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
	s := &Server{config: cfg, available: true, ready: ready, link: link, players: make(map[uint64]*reservation),
		sessions: make(map[uint64]*reservation), requests: make(map[string]*reservation),
		udp: udp, listener: listener, controlOut: make(chan outbound, 64), snapshotOut: make(chan []byte, 1),
		resultWake: make(chan struct{}, 1)}
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
		s.link.close()
		_ = s.udp.Close()
		_ = s.http.Close()
		_ = s.listener.Close()
		log.Printf("gateway transport coalesced_snapshots=%d rate_accepted_packets=%d rate_limited_packets=%d max_session_window_packets=%d", s.snapshotReplacements.Load(), s.rateAcceptedPackets.Load(), s.rateRejectedPackets.Load(), s.maxSessionWindowPackets.Load())
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
	return map[string]any{"id": 1, "match_id": 1, "players": len(s.players), "capacity": s.ready.MaxPlayers,
		"status": status, "arena_id": s.ready.ArenaId, "arena_version": s.ready.ArenaVersion, "arena_digest": s.ready.ArenaDigest}
}

type joinRequest struct {
	RequestID       string `json:"request_id"`
	ProtocolVersion uint16 `json:"protocol_version"`
}

func (s *Server) joinRoom(w http.ResponseWriter, r *http.Request) {
	r.Body = http.MaxBytesReader(w, r.Body, 4096)
	decoder := json.NewDecoder(r.Body)
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
	if request.ProtocolVersion != adapter.ClientVersion {
		writeError(w, http.StatusConflict, "protocol_version")
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
	if s.nextPlayer == ^uint64(0) {
		return nil, http.StatusServiceUnavailable, "player_ids_exhausted"
	}
	s.nextPlayer++
	player := &reservation{packetWindow: createdAt, session: peer, playerID: s.nextPlayer, requestID: request.RequestID, movementEpoch: 1, lifeGeneration: 1,
		commands: make(map[uint64]*runtime.MovementCommand)}
	s.players[player.playerID] = player
	s.sessions[peer.ID] = player
	s.requests[request.RequestID] = player
	return s.reservationData(player), http.StatusOK, ""
}
func (s *Server) reservationData(p *reservation) map[string]any {
	return map[string]any{"match_id": 1, "player_id": p.playerID, "session_id": p.session.ID,
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
		e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: p.playerID}}
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
		header, payload, err := framing.DecodeDatagram(buffer[:n])
		if err != nil || header.Version != adapter.ClientVersion {
			continue
		}
		if err = s.receivePacket(header, payload, peer, time.Now()); err != nil {
			s.runtimeFailed(err)
		}
	}
}
func (s *Server) receivePacket(h framing.Header, payload []byte, peer netip.AddrPort, now time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.available || h.Version != adapter.ClientVersion {
		return nil
	}
	p := s.sessions[h.SessionID]
	if p == nil {
		return nil
	}
	p.received++
	p.lastReceived = now
	switch h.Type {
	case adapter.Hello:
		var hello client.Hello
		if proto.Unmarshal(payload, &hello) != nil {
			return nil
		}
		admission := p.session.Hello(hello.SessionToken, peer, h.Sequence, now)
		s.trackAdmission(p, now, admission)
		if admission != nil {
			return nil
		}
		switch p.phase {
		case reserved:
			p.phase = joining
			p.joinStarted = now
			log.Printf("player hello player=%d udp=%s", p.playerID, peer)
			e := envelope()
			e.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: p.playerID}}
			return s.link.control(e)
		case active:
			s.welcome(p)
		}
	case adapter.Actions:
		if p.phase != active || s.admit(p, peer, h.Sequence, now) != nil {
			return nil
		}
		return s.receiveActions(p, h, payload, now)
	case adapter.Input:
		if p.phase != active {
			return nil
		}
		// A stale transport sequence still carries a complete payload (Hello,
		// Input and Actions share the Client's packet sequence): it is decoded
		// and forwarded, but never commits the sequence, so it neither extends
		// liveness nor moves the sequence. The rate limit already counted it.
		admission := s.admit(p, peer, h.Sequence, now)
		stale := errors.Is(admission, session.ErrStale)
		if admission != nil && !stale {
			return nil
		}
		in, err := adapter.DecodeInput(payload, p.playerID)
		if err != nil {
			return nil
		}
		return s.forwardInput(p, in, h.Sequence, stale, now)
	default:
		_ = s.admit(p, peer, h.Sequence, now)
	}
	return nil
}
func (s *Server) welcome(p *reservation) {
	s.sendControl(p, adapter.Welcome, &client.Welcome{PlayerId: p.playerID, MatchId: 1, TickRate: s.ready.TickRate,
		SnapshotRate: s.ready.TickRate / s.ready.SnapshotIntervalTicks, ArenaId: s.ready.ArenaId, ArenaVersion: s.ready.ArenaVersion, ArenaDigest: s.ready.ArenaDigest, CombatRules: adapter.RulesForClient(s.ready.CombatRules), JumpHeight: s.ready.JumpHeight, Gravity: s.ready.Gravity})
}
func (s *Server) sendControl(p *reservation, kind uint16, message proto.Message) {
	payload, err := proto.Marshal(message)
	if err != nil {
		return
	}
	p.outSequence++
	packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: kind, SessionID: p.session.ID, Sequence: p.outSequence}, payload)
	if err != nil {
		return
	}
	select {
	case s.controlOut <- outbound{peer: p.session.Endpoint(), packet: packet}:
	default:
	}
}

func (s *Server) runtimeMessage(e *runtime.RuntimeEnvelope) {
	if results := e.GetActionResults(); results != nil {
		if err := s.receiveActionResults(results); err != nil {
			s.runtimeFailed(err)
		}
		return
	}
	if result := e.GetJoinResult(); result != nil {
		s.mu.Lock()
		defer s.mu.Unlock()
		p := s.players[result.PlayerId]
		if p == nil || p.phase != joining {
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
		if !s.available || (s.hasSnapshot && out.Tick <= s.lastSnapshot) {
			s.mu.Unlock()
			return
		}
		for _, state := range out.Players {
			if p := s.players[state.PlayerId]; p != nil &&
				(state.LifeGeneration < p.lifeGeneration || state.MovementEpoch < p.movementEpoch ||
					(state.LifeGeneration > p.lifeGeneration && state.MovementEpoch <= p.movementEpoch) ||
					(state.MovementEpoch == p.movementEpoch && state.LastResolvedCommand < p.lastResolved)) {
				s.mu.Unlock()
				return
			}
		}
		s.lastSnapshot = out.Tick
		s.hasSnapshot = true
		for _, state := range out.Players {
			if p := s.players[state.PlayerId]; p != nil &&
				(state.LifeGeneration > p.lifeGeneration || state.MovementEpoch > p.movementEpoch || state.LastResolvedCommand > p.lastResolved) {
				if state.MovementEpoch > p.movementEpoch || state.LifeGeneration > p.lifeGeneration {
					clear(p.commands)
				}
				p.movementEpoch = state.MovementEpoch
				p.lifeGeneration = state.LifeGeneration
				p.lastResolved = state.LastResolvedCommand
				for sequence := range p.commands {
					if sequence <= p.lastResolved {
						delete(p.commands, sequence)
					}
				}
				s.link.acknowledge(p.playerID, p.movementEpoch, p.lifeGeneration, p.lastResolved)
			}
		}
		s.mu.Unlock()
		payload, err := proto.Marshal(out)
		if err != nil {
			s.runtimeFailed(err)
			return
		}
		if len(payload) > framing.MaxDatagram-framing.HeaderSize {
			s.runtimeFailed(errors.New("snapshot exceeds datagram limit"))
			return
		}
		select {
		case s.snapshotOut <- payload:
		default:
			select {
			case <-s.snapshotOut:
				s.snapshotReplacements.Add(1)
			default:
			}
			select {
			case s.snapshotOut <- payload:
			default:
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
		// Without this an evicted player's action window would keep a slot:
		// a replacement could not deliver shots or reloads, silently.
		s.link.forget(evicted.PlayerId)
		s.remove(p)
		return
	}
	if problem := e.GetError(); problem != nil {
		log.Printf("runtime error player=%d code=%s", problem.PlayerId, problem.Code)
	}
}

// Wakes the send loop when a Client has something new: a decision, a
// retirement, or a retirement its ACK-only batch showed it missed.
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
	// Results go out on a wake (something new for a Client) or at the earliest
	// result deadline, a strict ActionSendRate interval from that player's last
	// completed write: at once after an idle interval, never sooner.
	timer := time.NewTimer(actionSendInterval)
	defer timer.Stop()
	results := func() {
		packets, deadline, pending := s.actionPackets(time.Now())
		for _, packet := range packets {
			s.writeUDP(packet)
			s.actionWritten(packet.actionPlayer, time.Now())
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
		case packet := <-s.controlOut:
			s.writeUDP(packet)
		case payload := <-s.snapshotOut:
			s.mu.Lock()
			players := make([]uint64, 0, len(s.players))
			if s.available {
				for _, p := range s.players {
					if p.phase == active {
						players = append(players, p.playerID)
					}
				}
			}
			s.mu.Unlock()
			for _, playerID := range players {
				var packet outbound
				payload, packet = s.snapshotForPeer(playerID, payload)
				s.writeUDP(packet)
				if packet.peer.IsValid() {
					s.snapshotWritten(playerID, time.Now())
				}
			}
		}
	}
}

func (s *Server) snapshotForPeer(playerID uint64, payload []byte) ([]byte, outbound) {
	// A previous peer's write may have blocked while a newer publication arrived.
	// Replace this still-unwritten datagram before assigning its transport sequence.
	select {
	case payload = <-s.snapshotOut:
		s.snapshotReplacements.Add(1)
	default:
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	p := s.players[playerID]
	if !s.available || p == nil || p.phase != active {
		return payload, outbound{}
	}
	p.outSequence++
	packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: adapter.Snapshot, SessionID: p.session.ID, Sequence: p.outSequence}, payload)
	if err != nil {
		return payload, outbound{}
	}
	return payload, outbound{peer: p.session.Endpoint(), packet: packet}
}

func (s *Server) writeUDP(packet outbound) {
	if packet.peer.IsValid() {
		_ = s.udp.SetWriteDeadline(time.Now().Add(time.Second))
		_, _ = s.udp.WriteToUDPAddrPort(packet.packet, packet.peer)
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
			reason := "session_timeout"
			if joinExpired {
				reason = "join_timeout"
			}
			log.Printf("player expired player=%d reason=%s", p.playerID, reason)
			if p.phase != reserved {
				e := envelope()
				e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: p.playerID}}
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
			links := s.link.takeActionStatistics()
			s.mu.Lock()
			ids := make([]uint64, 0, len(s.players))
			for id := range s.players {
				ids = append(ids, id)
			}
			slices.Sort(ids)
			log.Printf("gateway statistics players=%d rate_accepted_packets=%d rate_limited_packets=%d", len(ids),
				s.rateAcceptedPackets.Load(), s.rateRejectedPackets.Load())
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
				log.Print(sendStatisticsLine(id, now.Sub(windowStart), p.sent.results.take(), p.sent.snapshots.take(), link))
			}
			s.mu.Unlock()
			windowStart = now
		}
	}
}

func (s *Server) remove(p *reservation) {
	delete(s.players, p.playerID)
	delete(s.sessions, p.session.ID)
	delete(s.requests, p.requestID)
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
	}
	clear(s.players)
	clear(s.sessions)
	clear(s.requests)
	s.mu.Unlock()
	s.link.close()
	log.Printf("runtime disconnected; room unavailable: %v", err)
}

// forwardInput hands a decoded window to the Match (D49: the Match decides what
// is stale, resolved or conflicting). The main lane merges windows and keeps
// resolved commands, whose late arrival the Match measures. A window the Match
// may refuse as a whole goes unmerged through the rejected lane, so it cannot
// make the Match refuse a merge with valid commands: another epoch or life,
// beyond the future bound, a conflict, or a window the link refused.
func (s *Server) forwardInput(p *reservation, in *runtime.PlayerInput, sequence uint32, stale bool, now time.Time) error {
	if in.MovementEpoch != p.movementEpoch || in.LifeGeneration != p.lifeGeneration {
		return s.link.reject(in, playerFwdRoutedEpochMismatchInputs)
	}
	// Validate the whole batch before committing the sequence. Unresolved
	// steps are immutable across packets; the bound counts only steps after
	// the cursor (a resolved step must not underflow it).
	for _, command := range in.Commands {
		if command.Sequence > p.lastResolved && command.Sequence-p.lastResolved > adapter.MaxFutureCommands {
			return s.link.reject(in, playerFwdRoutedFutureLimitInputs)
		}
		if existing := p.commands[command.Sequence]; existing != nil && !adapter.EqualCommand(existing, command) {
			return s.link.reject(in, playerFwdRoutedConflictInputs)
		}
	}
	// A stale sequence is never committed. Admit and CommitSequence both run
	// under s.mu and Admit found this sequence fresh, so the commit cannot
	// fail. A window the link refuses below is routed after the commit, as
	// before 08c.
	if !stale {
		_ = p.session.CommitSequence(sequence, now)
	}
	if err := s.link.input(in); err != nil {
		if errors.Is(err, adapter.ErrInput) {
			return s.link.reject(in, linkRefusalReason(err))
		}
		return err
	}
	for _, command := range in.Commands {
		if command.Sequence > p.lastResolved {
			p.commands[command.Sequence] = command
		}
	}
	return nil
}
