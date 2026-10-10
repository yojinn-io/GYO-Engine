package gateway

import (
	"errors"
	"net/netip"
	"sort"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

const actionSendInterval = (time.Second + adapter.ActionSendRate - 1) / adapter.ActionSendRate

// Each layer owns a bounded immutable action ledger. Movement acknowledgements
// and snapshot replacement never touch this state.
type actionWindow struct {
	requests                      map[uint64]*runtime.ShotRequest
	decisions                     map[uint64]*runtime.ShotDecision
	retired, acknowledged, cursor uint64
	nextSend                      time.Time
	// Session direction only (Gateway to Client); the runtime link's windows
	// never use them: the retirement last sent to the Client, and whether an
	// ACK-only batch showed that the Client missed it.
	retiredSent     uint64
	retirementAsked bool
}

func newActionWindow() *actionWindow {
	return &actionWindow{requests: map[uint64]*runtime.ShotRequest{}, decisions: map[uint64]*runtime.ShotDecision{}}
}
func (w *actionWindow) validate(in *runtime.ActionBatch) error {
	if in == nil || in.PlayerId == 0 || len(in.Shots) > adapter.MaxActionBatch {
		return adapter.ErrActions
	}
	if in.AcknowledgedThrough > w.acknowledged {
		if in.AcknowledgedThrough-w.retired > adapter.MaxActions {
			return adapter.ErrActions
		}
		for id := w.retired; id < in.AcknowledgedThrough; {
			id++
			if w.decisions[id] == nil {
				return adapter.ErrActions
			}
		}
	}
	seen := map[uint64]*runtime.ShotRequest{}
	additional := 0
	for _, shot := range in.Shots {
		if !adapter.ValidShot(shot) {
			return adapter.ErrActions
		}
		if prior := seen[shot.ActionId]; prior != nil {
			if !adapter.EqualShot(prior, shot) {
				return adapter.ErrActions
			}
			continue
		}
		seen[shot.ActionId] = shot
		if shot.ActionId <= w.retired {
			continue
		}
		if shot.ActionId-w.retired > adapter.MaxActions {
			return adapter.ErrActions
		}
		if old := w.requests[shot.ActionId]; old != nil {
			if !adapter.EqualShot(old, shot) {
				return adapter.ErrActions
			}
		} else {
			additional++
		}
	}
	if len(w.requests)+additional > adapter.MaxActions {
		return adapter.ErrActions
	}
	return nil
}
func (w *actionWindow) merge(in *runtime.ActionBatch) {
	for _, shot := range in.Shots {
		if shot.ActionId > w.retired && w.requests[shot.ActionId] == nil {
			w.requests[shot.ActionId] = proto.Clone(shot).(*runtime.ShotRequest)
		}
	}
	w.acknowledged = max(w.acknowledged, in.AcknowledgedThrough)
}
func (w *actionWindow) validateResults(in *runtime.ActionResults) error {
	if in.RetiredThrough > w.acknowledged {
		return adapter.ErrActions
	}
	for _, d := range in.Decisions {
		if d.ActionId <= w.retired {
			continue
		}
		request := w.requests[d.ActionId]
		if request == nil || d.ActionId-w.retired > adapter.MaxActions ||
			d.Kind != request.Kind || d.LifeGeneration != request.LifeGeneration {
			return adapter.ErrActions
		}
		if old := w.decisions[d.ActionId]; old != nil && !adapter.EqualDecision(old, d) {
			return adapter.ErrActions
		}
	}
	return nil
}
func (w *actionWindow) mergeResults(in *runtime.ActionResults) {
	w.retired = max(w.retired, in.RetiredThrough)
	for _, d := range in.Decisions {
		if d.ActionId > w.retired {
			w.decisions[d.ActionId] = proto.Clone(d).(*runtime.ShotDecision)
		}
	}
	for id := range w.requests {
		if id <= w.retired {
			delete(w.requests, id)
			delete(w.decisions, id)
		}
	}
}
func circularIDs(ids []uint64, cursor uint64) []uint64 {
	sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
	if len(ids) == 0 {
		return ids
	}
	first := sort.Search(len(ids), func(i int) bool { return ids[i] > cursor })
	ordered := append(append(make([]uint64, 0, len(ids)), ids[first:]...), ids[:first]...)
	return ordered[:min(len(ordered), adapter.MaxActionBatch)]
}
func (l *runtimeLink) actions(in *runtime.ActionBatch) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errRuntimeDisconnected
	}
	if in == nil || in.PlayerId == 0 {
		return adapter.ErrActions
	}
	if l.actionWindows == nil {
		l.actionWindows = map[uint64]*actionWindow{}
	}
	w := l.actionWindows[in.PlayerId]
	if w == nil {
		if len(l.actionWindows) >= adapter.MaxPlayers {
			return adapter.ErrActions
		}
		w = newActionWindow()
	}
	if err := w.validate(in); err != nil {
		return err
	}
	w.merge(in)
	l.actionWindows[in.PlayerId] = w
	l.notify()
	return nil
}
func (l *runtimeLink) actionResults(in *runtime.ActionResults) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if w := l.actionWindows[in.PlayerId]; w != nil {
		w.mergeResults(in)
	}
	l.notify()
}

// Called with the writer's mutex held. Only one current batch is selected per
// deadline; elapsed deadlines are never replayed after a blocked TCP write.
func (l *runtimeLink) actionBatch(now time.Time) []*runtime.RuntimeEnvelope {
	var out []*runtime.RuntimeEnvelope
	for player, w := range l.actionWindows {
		if now.Before(w.nextSend) {
			continue
		}
		ids := make([]uint64, 0, len(w.requests))
		for id := range w.requests {
			if w.decisions[id] == nil {
				ids = append(ids, id)
			}
		}
		ids = circularIDs(ids, w.cursor)
		if len(ids) == 0 && w.acknowledged <= w.retired {
			continue
		}
		in := &runtime.ActionBatch{PlayerId: player, AcknowledgedThrough: w.acknowledged}
		for _, id := range ids {
			in.Shots = append(in.Shots, proto.Clone(w.requests[id]).(*runtime.ShotRequest))
			w.cursor = id
		}
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Actions{Actions: in}
		out = append(out, e)
		w.nextSend = now.Add(actionSendInterval)
	}
	return out
}

// The earliest action deadline among players with something to send (an
// undecided request, or an acknowledgement the Match has not retired), so the
// link loop can wake at it. Called with the link's mutex held.
func (l *runtimeLink) nextActionDeadline() (time.Time, bool) {
	var next time.Time
	found := false
	for _, w := range l.actionWindows {
		pending := w.acknowledged > w.retired
		for id := range w.requests {
			if w.decisions[id] == nil {
				pending = true
				break
			}
		}
		if pending && (!found || w.nextSend.Before(next)) {
			next, found = w.nextSend, true
		}
	}
	return next, found
}

// Something new for the Client: a decision it has not acknowledged (resent
// every interval until it does), a retirement not sent yet, or one that an
// ACK-only batch showed it missed. Both the selection and the send loop's
// deadline use this, so a due deadline always selects a batch.
func (w *actionWindow) resultsPending() bool {
	if w.retired > w.retiredSent || w.retirementAsked {
		return true
	}
	for id := range w.decisions {
		if id > w.acknowledged {
			return true
		}
	}
	return false
}

// Results bypass the lossy control and latest-snapshot queues. They remain in
// the session ledger until the client's contiguous ACK retires them in Match.
// A player is selected only with something pending, at a strict interval from
// its last completed write. Also reports the earliest deadline left.
func (s *Server) actionPackets(now time.Time) ([]outbound, time.Time, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var packets []outbound
	if !s.available {
		return nil, time.Time{}, false
	}
	for _, p := range s.players {
		w := p.actions
		if p.phase != active || w == nil || now.Before(w.nextSend) || !w.resultsPending() {
			continue
		}
		// Anchor before encoding: a batch that fails to encode must not leave
		// a due deadline behind, or the send loop would wait zero time on it.
		w.nextSend = now.Add(actionSendInterval)
		w.retiredSent, w.retirementAsked = w.retired, false
		ids := make([]uint64, 0, len(w.decisions))
		for id := range w.decisions {
			if id > w.acknowledged {
				ids = append(ids, id)
			}
		}
		ids = circularIDs(ids, w.cursor)
		results := &runtime.ActionResults{PlayerId: p.playerID, RetiredThrough: w.retired}
		for _, id := range ids {
			results.Decisions = append(results.Decisions, w.decisions[id])
			w.cursor = id
		}
		mapped, err := adapter.ResultsForClient(results, s.ready.CombatRules)
		if err != nil {
			continue
		}
		payload, err := proto.Marshal(mapped)
		if err != nil {
			continue
		}
		p.outSequence++
		packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: adapter.ActionResults, SessionID: p.session.ID, Sequence: p.outSequence}, payload)
		if err != nil {
			continue
		}
		packets = append(packets, outbound{peer: p.session.Endpoint(), packet: packet, actionPlayer: p.playerID})
	}
	deadline, ok := s.nextResultDeadline()
	return packets, deadline, ok
}

// The earliest result deadline among active players with something pending,
// so the send loop can wake at it. Called with s.mu held.
func (s *Server) nextResultDeadline() (time.Time, bool) {
	var next time.Time
	found := false
	if !s.available {
		return next, false
	}
	for _, p := range s.players {
		if p.phase != active || p.actions == nil || !p.actions.resultsPending() {
			continue
		}
		if !found || p.actions.nextSend.Before(next) {
			next, found = p.actions.nextSend, true
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
	if p.actions == nil {
		p.actions = newActionWindow()
	}
	// The two semantic action drops (A3 here, A5 below) are only counted in
	// P2 (D53 item 8): batches and their shots.
	if p.actions.validate(in) != nil {
		s.countIngress(p, playerDropInvalidActionsPackets, 1)
		s.countIngress(p, playerDropInvalidActionsShots, uint64(len(in.Shots)))
		return nil
	}
	// Link admission happens before committing the session's ledger or ACK. A
	// rejected batch can be retried unchanged without losing any request/result.
	if err = s.link.actions(in); err != nil {
		if err == adapter.ErrActions {
			s.countIngress(p, playerDropLinkRejectedActionsPackets, 1)
			s.countIngress(p, playerDropLinkRejectedActionsShots, uint64(len(in.Shots)))
			return nil
		}
		return err
	}
	// The link holds the batch: it reaches the Match whatever follows.
	s.countIngress(p, playerFwdHandedActionsPackets, 1)
	if p.session.CommitSequence(h.Sequence, now) != nil {
		return nil
	}
	p.actions.merge(in)
	// The Client sends an ACK-only batch only while its ACK is ahead of the
	// retirement it knows. One that acknowledges no more than the retirement
	// already here shows the Client missed it: send it once more (a poke). A
	// batch with shots does not tell which retirement the Client knows.
	if len(in.Shots) == 0 && in.AcknowledgedThrough > 0 && in.AcknowledgedThrough <= p.actions.retired {
		p.actions.retirementAsked = true
		s.wakeResults()
	}
	return nil
}
func (s *Server) receiveActionResults(in *runtime.ActionResults) error {
	if _, err := adapter.ResultsForClient(in, s.ready.CombatRules); err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	p := s.players[in.PlayerId]
	// Dropping the results of a player that is not active loses nothing to
	// resend later. Before its join completes the Match has none: only an
	// active player's actions are forwarded. A removal is final: phases only
	// move forward, player ids are never reused and the runtime link never
	// reconnects. The Match sends each result once, so a rejoin under the same
	// id, or a link reconnect, would also need the Match to resend every
	// unretired result once the player is active again.
	if p == nil || p.phase != active {
		return nil
	}
	if p.actions == nil {
		p.actions = newActionWindow()
	}
	w := p.actions
	if err := w.validateResults(in); err != nil {
		return err
	}
	// Only new content (a decision or a retirement) wakes the send loop.
	fresh := in.RetiredThrough > w.retired
	for _, d := range in.Decisions {
		if d.ActionId > max(w.retired, in.RetiredThrough) && w.decisions[d.ActionId] == nil {
			fresh = true
		}
	}
	w.mergeResults(in)
	s.link.actionResults(in)
	if fresh {
		s.wakeResults()
	}
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

// A selected frame may sit behind a blocked earlier write. Reanchor after its
// completed write as well, so release never triggers an immediate catch-up send.
func (l *runtimeLink) actionWritten(playerID uint64, now time.Time) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if w := l.actionWindows[playerID]; w != nil {
		w.nextSend = now.Add(actionSendInterval)
		if l.actionStats == nil {
			l.actionStats = map[uint64]*intervalStats{}
		}
		if l.actionStats[playerID] == nil {
			l.actionStats[playerID] = &intervalStats{}
		}
		l.actionStats[playerID].record(now)
	}
}
func (s *Server) actionWritten(playerID uint64, now time.Time) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if p := s.players[playerID]; p != nil && p.actions != nil {
		p.actions.nextSend = now.Add(actionSendInterval)
		p.sent.results.record(now)
	}
}
