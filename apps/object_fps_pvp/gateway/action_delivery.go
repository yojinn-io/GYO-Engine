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

// A result batch may go out up to half an interval before its deadline. The
// deadline is re-anchored at each completed write, just after the tick that
// selected the batch, so without this the next tick is always a hair early
// and every other tick is skipped (half of ActionSendRate). Half an interval
// still keeps two sends that far apart: a released write never bursts.
const resultSendTolerance = actionSendInterval / 2

// Each layer owns a bounded immutable action ledger. Movement acknowledgements
// and snapshot replacement never touch this state.
type actionWindow struct {
	requests                      map[uint64]*runtime.ShotRequest
	decisions                     map[uint64]*runtime.ShotDecision
	retired, acknowledged, cursor uint64
	nextSend                      time.Time
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

// Results bypass the lossy control and latest-snapshot queues. They remain in
// the session ledger until the client's contiguous ACK retires them in Match.
func (s *Server) actionPackets(now time.Time) []outbound {
	s.mu.Lock()
	defer s.mu.Unlock()
	var packets []outbound
	if !s.available {
		return nil
	}
	for _, p := range s.players {
		w := p.actions
		if p.phase != active || w == nil || now.Add(resultSendTolerance).Before(w.nextSend) {
			continue
		}
		ids := make([]uint64, 0, len(w.decisions))
		for id := range w.decisions {
			if id > w.acknowledged {
				ids = append(ids, id)
			}
		}
		ids = circularIDs(ids, w.cursor)
		if len(ids) == 0 && w.retired == 0 {
			continue
		}
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
		w.nextSend = now.Add(actionSendInterval)
	}
	return packets
}
func (s *Server) receiveActions(p *reservation, h framing.Header, payload []byte, now time.Time) error {
	in, err := adapter.DecodeActions(payload, p.playerID)
	if err != nil {
		return nil
	}
	if p.actions == nil {
		p.actions = newActionWindow()
	}
	if p.actions.validate(in) != nil {
		return nil
	}
	// Link admission happens before committing the session's ledger or ACK. A
	// rejected batch can be retried unchanged without losing any request/result.
	if err = s.link.actions(in); err != nil {
		if err == adapter.ErrActions {
			return nil
		}
		return err
	}
	if p.session.CommitSequence(h.Sequence, now) != nil {
		return nil
	}
	p.actions.merge(in)
	return nil
}
func (s *Server) receiveActionResults(in *runtime.ActionResults) error {
	if _, err := adapter.ResultsForClient(in, s.ready.CombatRules); err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	p := s.players[in.PlayerId]
	if p == nil || p.phase != active {
		return nil
	}
	if p.actions == nil {
		p.actions = newActionWindow()
	}
	if err := p.actions.validateResults(in); err != nil {
		return err
	}
	p.actions.mergeResults(in)
	s.link.actionResults(in)
	return nil
}

// Product diagnostics observe the shared Session limiter; its120-packet policy
// and inclusion of Hello, stale and malformed authenticated traffic are intact.
// All per-reservation accounting occurs under Server.mu, matching receivePacket.
func (s *Server) admit(p *reservation, peer netip.AddrPort, sequence uint32, now time.Time) error {
	err := p.session.Admit(peer, sequence, now)
	s.trackAdmission(p, now, err)
	return err
}
func (s *Server) trackAdmission(p *reservation, now time.Time, err error) {
	if errors.Is(err, session.ErrUnauthorized) {
		return
	}
	if errors.Is(err, session.ErrRateLimit) {
		s.rateRejectedPackets.Add(1)
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
