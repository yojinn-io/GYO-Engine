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
	"strings"
	"sync"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/httpserver"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv1"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev1"
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
	session     *session.Session
	playerID    uint64
	requestID   string
	phase       phase
	joinStarted time.Time
	lastInput   uint64
	outSequence uint32
}
type outbound struct {
	peer   netip.AddrPort
	packet []byte
}

type Server struct {
	config       Config
	mu           sync.Mutex
	available    bool
	created      bool
	ready        *runtime.Ready
	link         *runtimeLink
	players      map[uint64]*reservation
	sessions     map[uint64]*reservation
	requests     map[string]*reservation
	nextPlayer   uint64
	lastSnapshot uint64
	hasSnapshot  bool
	http         *http.Server
	listener     net.Listener
	udp          *net.UDPConn
	controlOut   chan outbound
	snapshotOut  chan []byte
	closeOnce    sync.Once
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
		udp: udp, listener: listener, controlOut: make(chan outbound, 64), snapshotOut: make(chan []byte, 1)}
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
// failure. Recovery in v1 means restarting Runtime/Gateway and joining again.
func (s *Server) Serve(ctx context.Context) error {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	defer s.Close()
	s.link.run(ctx, s.runtimeMessage, s.runtimeFailed)
	go s.receiveUDP(ctx)
	go s.sendUDP(ctx)
	go s.expire(ctx)
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
		"status": status, "arena_id": s.ready.ArenaId, "arena_version": s.ready.ArenaVersion}
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
		writeError(w, status, code)
		return
	}
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
	peer, err := session.New(time.Now())
	if err != nil {
		return nil, http.StatusInternalServerError, "session_creation_failed"
	}
	for s.sessions[peer.ID] != nil {
		peer, err = session.New(time.Now())
		if err != nil {
			return nil, http.StatusInternalServerError, "session_creation_failed"
		}
	}
	if s.nextPlayer == ^uint64(0) {
		return nil, http.StatusServiceUnavailable, "player_ids_exhausted"
	}
	s.nextPlayer++
	player := &reservation{session: peer, playerID: s.nextPlayer, requestID: request.RequestID}
	s.players[player.playerID] = player
	s.sessions[peer.ID] = player
	s.requests[request.RequestID] = player
	return s.reservationData(player), http.StatusOK, ""
}
func (s *Server) reservationData(p *reservation) map[string]any {
	return map[string]any{"match_id": 1, "player_id": p.playerID, "session_id": p.session.ID,
		"session_token": p.session.Token, "udp_ip": s.config.AdvertiseIP, "udp_port": s.udp.LocalAddr().(*net.UDPAddr).Port,
		"protocol_version": adapter.ClientVersion, "arena_id": s.ready.ArenaId, "arena_version": s.ready.ArenaVersion}
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
	if !s.available {
		return nil
	}
	p := s.sessions[h.SessionID]
	if p == nil {
		return nil
	}
	switch h.Type {
	case adapter.Hello:
		var hello client.Hello
		if proto.Unmarshal(payload, &hello) != nil || p.session.Hello(hello.SessionToken, peer, h.Sequence, now) != nil {
			return nil
		}
		switch p.phase {
		case reserved:
			p.phase = joining
			p.joinStarted = now
			e := envelope()
			e.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: p.playerID}}
			return s.link.control(e)
		case active:
			s.welcome(p)
		}
	case adapter.Input:
		if p.phase != active {
			return nil
		}
		if p.session.Admit(peer, h.Sequence, now) != nil {
			return nil
		}
		in, err := adapter.DecodeInput(payload, p.playerID)
		if err != nil || in.InputSequence <= p.lastInput {
			return nil
		}
		if p.session.CommitSequence(h.Sequence, now) != nil {
			return nil
		}
		p.lastInput = in.InputSequence
		return s.link.input(in)
	}
	return nil
}
func (s *Server) welcome(p *reservation) {
	s.sendControl(p, adapter.Welcome, &client.Welcome{PlayerId: p.playerID, MatchId: 1, TickRate: s.ready.TickRate,
		SnapshotRate: s.ready.TickRate / s.ready.SnapshotIntervalTicks, ArenaId: s.ready.ArenaId, ArenaVersion: s.ready.ArenaVersion})
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
	case s.controlOut <- outbound{p.session.Endpoint(), packet}:
	default:
	}
}

func (s *Server) runtimeMessage(e *runtime.RuntimeEnvelope) {
	if result := e.GetJoinResult(); result != nil {
		s.mu.Lock()
		defer s.mu.Unlock()
		p := s.players[result.PlayerId]
		if p == nil || p.phase != joining {
			return
		}
		if !result.Accepted {
			s.sendControl(p, adapter.Failure, &client.Error{Code: "join_rejected", Message: result.Reason})
			s.remove(p)
			return
		}
		p.phase = active
		s.welcome(p)
		return
	}
	if snapshot := e.GetSnapshot(); snapshot != nil {
		out, err := adapter.SnapshotForClient(snapshot)
		if err != nil {
			s.runtimeFailed(err)
			return
		}
		s.mu.Lock()
		if !s.available || (s.hasSnapshot && out.Tick <= s.lastSnapshot) {
			s.mu.Unlock()
			return
		}
		s.lastSnapshot = out.Tick
		s.hasSnapshot = true
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
			default:
			}
			select {
			case s.snapshotOut <- payload:
			default:
			}
		}
		return
	}
	if problem := e.GetError(); problem != nil {
		log.Printf("runtime error player=%d code=%s", problem.PlayerId, problem.Code)
	}
}

func (s *Server) sendUDP(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			return
		case packet := <-s.controlOut:
			s.writeUDP(packet)
		case payload := <-s.snapshotOut:
			s.mu.Lock()
			packets := make([]outbound, 0, len(s.players))
			if s.available {
				for _, p := range s.players {
					if p.phase == active {
						p.outSequence++
						packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: adapter.Snapshot, SessionID: p.session.ID, Sequence: p.outSequence}, payload)
						if err == nil {
							packets = append(packets, outbound{p.session.Endpoint(), packet})
						}
					}
				}
			}
			s.mu.Unlock()
			for _, packet := range packets {
				s.writeUDP(packet)
			}
		}
	}
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
		if p.session.Expired(now, sessionTimeout) || (p.phase == joining && now.Sub(p.joinStarted) >= joinTimeout) {
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
