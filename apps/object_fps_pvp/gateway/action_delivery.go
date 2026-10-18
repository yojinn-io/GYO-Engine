package gateway

import (
	"errors"
	"net/netip"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

const actionSendInterval = (time.Second + adapter.ActionSendRate - 1) / adapter.ActionSendRate

// actions queues one Client datagram's action batch as it came (pv7 §2a): one
// IPC message per datagram, behind everything queued before it, bounded per
// player apart from the input windows (maxQueuedActions). The Match judges
// duplicates, conflicts, the window and the ACK (D-1 is moot); the link
// neither merges, validates against a ledger, holds nor resends. e is the
// datagram's identity for an overflow report (33).
func (l *runtimeLink) actions(in *runtime.ActionBatch, e dropEvent) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errRuntimeDisconnected
	}
	if in == nil || in.PlayerId == 0 || len(in.Shots) > adapter.MaxActionBatch {
		return adapter.ErrActions
	}
	if e.transport != 0 { // diagnostics only (D96): never deduplicates or orders
		in.ClientTransportSequence = &e.transport
	}
	message := envelope()
	message.Message = &runtime.RuntimeEnvelope_Actions{Actions: in}
	l.queueDataLocked(queuedMessage{message: message, actions: true, player: in.PlayerId, event: e})
	// The send statistics entry lives from the first batch to forget: a
	// batch written after the player left records nothing.
	if l.actionStats == nil {
		l.actionStats = map[uint64]*intervalStats{}
	}
	if l.actionStats[in.PlayerId] == nil {
		l.actionStats[in.PlayerId] = &intervalStats{}
	}
	l.notify()
	return nil
}

// pv7 §2a: the Gateway holds at most one results frame per player, as the
// Match wrote it (latest wins: the Match resends until the Client's ACK), and
// releases it a strict interval after that player's last send. A frame
// replaced before its release is counted and reported (§4, reason 34). The
// Gateway keeps no ledger: it neither selects by content nor resends.
type resultSlot struct {
	pending  []byte // the Client ActionResults payload, nil when empty
	event    dropEvent
	nextSend time.Time
}

// The due results frames, each encoded with its GYOP sequence here, in the one
// UDP writer (sendUDP), right before its write. Also reports the earliest
// deadline left.
func (s *Server) actionPackets(now time.Time) ([]outbound, time.Time, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var packets []outbound
	if !s.available {
		return nil, time.Time{}, false
	}
	for _, p := range s.players {
		slot := &p.results
		if p.phase != active || slot.pending == nil || now.Before(slot.nextSend) {
			continue
		}
		// Anchor before encoding: a frame that fails to encode must not leave
		// a due deadline behind, or the send loop would wait zero time on it.
		slot.nextSend = now.Add(actionSendInterval)
		payload := slot.pending
		slot.pending = nil
		p.outSequence++
		packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: adapter.ActionResults, SessionID: p.session.ID, Sequence: p.outSequence}, payload)
		if err != nil {
			continue
		}
		packets = append(packets, outbound{peer: p.session.Endpoint(), packet: packet, actionPlayer: p.playerID,
			player: p.playerID, writeFailed: playerDropWriteFailedResultsPackets, sequence: p.outSequence})
	}
	deadline, ok := s.nextResultDeadline()
	return packets, deadline, ok
}

// The earliest release among active players holding a frame, so the send loop
// can wake at it. Called with s.mu held.
func (s *Server) nextResultDeadline() (time.Time, bool) {
	var next time.Time
	found := false
	if !s.available {
		return next, false
	}
	for _, p := range s.players {
		if p.phase != active || p.results.pending == nil {
			continue
		}
		if !found || p.results.nextSend.Before(next) {
			next, found = p.results.nextSend, true
		}
	}
	return next, found
}
func (s *Server) resultDeadline() (time.Time, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.nextResultDeadline()
}
func (s *Server) receiveActions(p *reservation, h framing.Header, payload []byte, now time.Time) error {
	in, err := adapter.DecodeActions(payload, p.playerID)
	if err != nil {
		s.countIngress(p, playerDropMalformedActionsPackets, 1)
		return nil
	}
	// D-11: decoding succeeded, so the sequence and liveness commit (Admit
	// found the sequence fresh; stale action packets were dropped there).
	_ = p.session.CommitSequence(h.Sequence, now)
	// The overflow sample (§4, 33) carries the bounds and count of the
	// batch's action ids: an ACK-only batch has none (ids 0; the drop ledger
	// counts the batch itself as its one item).
	e := s.datagram
	e.items = uint32(len(in.Shots))
	for _, shot := range in.Shots {
		if e.firstAction == 0 || shot.ActionId < e.firstAction {
			e.firstAction = shot.ActionId
		}
		e.lastAction = max(e.lastAction, shot.ActionId)
	}
	if err := s.link.actions(in, e); err != nil {
		if errors.Is(err, adapter.ErrActions) {
			// Unreachable after DecodeActions (same bounds): a malformed packet.
			s.countIngress(p, playerDropMalformedActionsPackets, 1)
			return nil
		}
		return err
	}
	s.countIngress(p, playerFwdHandedActionsPackets, 1)
	return nil
}
func (s *Server) receiveActionResults(in *runtime.ActionResults) error {
	mapped, err := adapter.ResultsForClient(in, s.ready.CombatRules)
	if err != nil {
		return err
	}
	payload, err := proto.Marshal(mapped)
	if err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	p := s.players[in.PlayerId]
	// Results for a player that is not active are not a loss (§4 (d)): the
	// Match resends until it reads the Leave, and a removal is final (phases
	// only move forward, ids are never reused). Logged as a count only.
	if p == nil || p.phase != active {
		s.resultsForInactive++
		return nil
	}
	e := dropEvent{at: time.Now(), items: uint32(len(in.Decisions))}
	for _, d := range in.Decisions {
		if e.firstAction == 0 || d.ActionId < e.firstAction {
			e.firstAction = d.ActionId
		}
		e.lastAction = max(e.lastAction, d.ActionId)
	}
	if p.results.pending != nil {
		// Replaced before its release: the Match sends it again (§2a, 34).
		s.countIngressAt(p.playerID, playerDropResultsOverRatePackets, 1, p.results.event)
	}
	p.results.pending, p.results.event = payload, e
	s.wakeResults()
	return nil
}

// Product diagnostics observe the shared Session limiter; its120-packet policy
// and inclusion of Hello, stale and malformed authenticated traffic are intact.
// All per-reservation accounting occurs under Server.mu, matching receivePacket.
func (s *Server) admit(p *reservation, kind ingressKind, peer netip.AddrPort, sequence uint32, now time.Time) error {
	err := sessionAdmit(p.session, peer, sequence, now)
	s.trackAdmission(p, kind, now, err)
	return err
}

// trackAdmission counts the drop an admission error means for a datagram of
// kind, classified by the reusable layer's typed errors (session.go), and
// keeps the limiter diagnostics. Every caller drops a datagram whose
// admission failed, except a stale Input: it is forwarded and counted by its
// forwarding outcome. An error this Gateway does not know (the Session
// returns none today, nor ErrStale for a Hello) is dropped and counted, never
// accepted (fail-closed, D55 ④).
func (s *Server) trackAdmission(p *reservation, kind ingressKind, now time.Time, err error) {
	switch {
	case err == nil:
	case errors.Is(err, session.ErrUnauthorized):
		// Unauthenticated: the session it claims, which a forger can name.
		s.countIngress(p, unauthorizedPackets[kind], 1)
		return
	case errors.Is(err, session.ErrRateLimit):
		s.rateRejectedPackets.Add(1)
		s.ingress[gatewayDropRateLimitedPackets]++
		s.countIngress(p, rateLimitedPackets[kind], 1)
		return
	case errors.Is(err, session.ErrStale) && kind != ingressHello:
		// The limiter accepted it, as before 08c.
		if kind != ingressInput {
			s.countIngress(p, staleSequencePackets[kind], 1)
		}
	default:
		s.countIngress(p, playerDropAdmissionUnknownPackets, 1)
		return
	}
	if now.Sub(p.packetWindow) >= time.Second {
		p.packetWindow = now
		p.packetCount = 0
	}
	p.packetCount++
	s.rateAcceptedPackets.Add(1)
	for old := s.maxSessionWindowPackets.Load(); p.packetCount > old; old = s.maxSessionWindowPackets.Load() {
		if s.maxSessionWindowPackets.CompareAndSwap(old, p.packetCount) {
			break
		}
	}
}

// actionWritten records the interval between a player's action batches the
// link wrote (send statistics only; the link no longer paces them), while
// the player's entry exists (from its first batch until forget).
func (l *runtimeLink) actionWritten(playerID uint64, now time.Time) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if stats := l.actionStats[playerID]; stats != nil {
		stats.record(now)
	}
}
func (s *Server) actionWritten(playerID uint64, now time.Time) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if p := s.players[playerID]; p != nil {
		p.results.nextSend = now.Add(actionSendInterval)
		p.sent.results.record(now)
	}
}
