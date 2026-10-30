package gateway

import (
	"fmt"
	"log"
	"sort"
	"strings"
	"sync"
	"time"

	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev8"
)

// The pv7 GatewayDropReport (contract §4, D49 second stage): the discards the
// Gateway executed, by reason, reported to the Match per stream (each player,
// and the Gateway as player 0). Diagnostics only: identity, never content;
// bounded; cumulative, so a merged or lost report loses no count (D32: no
// value of it reaches a decision).

type dropScope uint8

const (
	dropPlayerScope dropScope = iota + 1
	dropGatewayScope
)

// dropReasons binds each DropReason to its ingress key: the only place the
// enum meets the key words. tests/object_fps_pvp/fixtures/drop_reasons.json is
// its golden, read in Go, C++ and Python. The transitional reasons (16, 17,
// 20-26, 43, 44) go with their exits in the edge batches.
var dropReasons = []struct {
	reason runtime.DropReason
	key    ingressKey
	scope  dropScope
}{
	{runtime.DropReason_DROP_RATE_LIMITED_HELLO_PACKETS, playerIngressKeys[playerDropRateLimitedHelloPackets], dropPlayerScope},
	{runtime.DropReason_DROP_RATE_LIMITED_INPUT_PACKETS, playerIngressKeys[playerDropRateLimitedInputPackets], dropPlayerScope},
	{runtime.DropReason_DROP_RATE_LIMITED_ACTIONS_PACKETS, playerIngressKeys[playerDropRateLimitedActionsPackets], dropPlayerScope},
	{runtime.DropReason_DROP_RATE_LIMITED_OTHER_PACKETS, playerIngressKeys[playerDropRateLimitedOtherPackets], dropPlayerScope},
	{runtime.DropReason_DROP_UNAUTHORIZED_HELLO_PACKETS, playerIngressKeys[playerDropUnauthorizedHelloPackets], dropPlayerScope},
	{runtime.DropReason_DROP_UNAUTHORIZED_INPUT_PACKETS, playerIngressKeys[playerDropUnauthorizedInputPackets], dropPlayerScope},
	{runtime.DropReason_DROP_UNAUTHORIZED_ACTIONS_PACKETS, playerIngressKeys[playerDropUnauthorizedActionsPackets], dropPlayerScope},
	{runtime.DropReason_DROP_UNAUTHORIZED_OTHER_PACKETS, playerIngressKeys[playerDropUnauthorizedOtherPackets], dropPlayerScope},
	{runtime.DropReason_DROP_STALE_SEQUENCE_ACTIONS_PACKETS, playerIngressKeys[playerDropStaleSequenceActionsPackets], dropPlayerScope},
	{runtime.DropReason_DROP_STALE_SEQUENCE_OTHER_PACKETS, playerIngressKeys[playerDropStaleSequenceOtherPackets], dropPlayerScope},
	{runtime.DropReason_DROP_MALFORMED_HELLO_PACKETS, playerIngressKeys[playerDropMalformedHelloPackets], dropPlayerScope},
	{runtime.DropReason_DROP_MALFORMED_INPUT_PACKETS, playerIngressKeys[playerDropMalformedInputPackets], dropPlayerScope},
	{runtime.DropReason_DROP_MALFORMED_ACTIONS_PACKETS, playerIngressKeys[playerDropMalformedActionsPackets], dropPlayerScope},
	{runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, playerIngressKeys[playerDropInactiveInputPackets], dropPlayerScope},
	{runtime.DropReason_DROP_INACTIVE_ACTIONS_PACKETS, playerIngressKeys[playerDropInactiveActionsPackets], dropPlayerScope},
	{runtime.DropReason_DROP_UNKNOWN_KIND_PACKETS, playerIngressKeys[playerDropUnknownKindPackets], dropPlayerScope},
	{runtime.DropReason_DROP_ADMISSION_UNKNOWN_PACKETS, playerIngressKeys[playerDropAdmissionUnknownPackets], dropPlayerScope},
	{runtime.DropReason_DROP_WRITE_FAILED_SNAPSHOT_PACKETS, playerIngressKeys[playerDropWriteFailedSnapshotPackets], dropPlayerScope},
	{runtime.DropReason_DROP_WRITE_FAILED_RESULTS_PACKETS, playerIngressKeys[playerDropWriteFailedResultsPackets], dropPlayerScope},
	{runtime.DropReason_DROP_WRITE_FAILED_CONTROL_PACKETS, playerIngressKeys[playerDropWriteFailedControlPackets], dropPlayerScope},
	{runtime.DropReason_DROP_CONTROL_QUEUE_FULL_CONTROL_PACKETS, playerIngressKeys[playerDropControlQueueFullControlPackets], dropPlayerScope},
	{runtime.DropReason_DROP_REPLACED_SNAPSHOT_PACKETS, playerIngressKeys[playerDropReplacedSnapshotPackets], dropPlayerScope},
	{runtime.DropReason_DROP_IPC_QUEUE_OVERFLOW_INPUT_PACKETS, playerIngressKeys[playerDropIpcQueueOverflowInputPackets], dropPlayerScope},
	{runtime.DropReason_DROP_IPC_QUEUE_OVERFLOW_ACTIONS_PACKETS, playerIngressKeys[playerDropIpcQueueOverflowActionsPackets], dropPlayerScope},
	{runtime.DropReason_DROP_RESULTS_OVER_RATE_PACKETS, playerIngressKeys[playerDropResultsOverRatePackets], dropPlayerScope},
	{runtime.DropReason_DROP_UNDECODABLE_DATAGRAMS, gatewayIngressKeys[gatewayDropUndecodableDatagrams], dropGatewayScope},
	{runtime.DropReason_DROP_OTHER_VERSION_DATAGRAMS, gatewayIngressKeys[gatewayDropOtherVersionDatagrams], dropGatewayScope},
	{runtime.DropReason_DROP_UNKNOWN_SESSION_DATAGRAMS, gatewayIngressKeys[gatewayDropUnknownSessionDatagrams], dropGatewayScope},
	{runtime.DropReason_DROP_COALESCED_SNAPSHOTS, gatewayIngressKeys[gatewayDropCoalescedSnapshots], dropGatewayScope},
}

// The reason of each counter that is reported; 0 for the rest (not drops, or
// only logged: room_unavailable, cleared_on_close, abandoned_on_close and the
// Gateway's rate-limited sum, pv7 §4).
var (
	playerDropReason  [playerIngressCounterCount]runtime.DropReason
	gatewayDropReason [gatewayIngressCounterCount]runtime.DropReason
	dropReasonKey     = map[runtime.DropReason]string{}
)

func init() {
	for _, entry := range dropReasons {
		dropReasonKey[entry.reason] = ingressKeyName(entry.key)
		for counter, key := range playerIngressKeys {
			if key == entry.key && entry.scope == dropPlayerScope {
				playerDropReason[counter] = entry.reason
			}
		}
		for counter, key := range gatewayIngressKeys {
			if key == entry.key && entry.scope == dropGatewayScope {
				gatewayDropReason[counter] = entry.reason
			}
		}
	}
}

const (
	dropSamplesPerReport = 4
	// A timer pass sends at most this many streams; further finals wait, and
	// past the waiting room they are merged into the Gateway stream's count.
	dropStreamsPerPass = adapter.MaxPlayers * 2
	dropWaitingFinals  = adapter.MaxPlayers * 2
	dropMaximumDwell   = 10 * time.Second
)

// dropEvent is one discard: its reason, when its dwell started (pv7 §2: the
// datagram's receipt, or the event) and the identity of what it dropped.
type dropEvent struct {
	reason                    runtime.DropReason
	at                        time.Time
	transport                 uint32
	epoch, life               uint64
	firstCommand, lastCommand uint64
	firstAction, lastAction   uint64
	tick                      uint64
	items                     uint32
}

type dropStream struct {
	totals   map[runtime.DropReason]uint64
	samples  []dropEvent // since the previous written report
	omitted  uint64
	sequence uint64 // of the last written report
	dirty    bool
	final    bool // the player was removed: the next report is the last
}

// dropLedger keeps every stream's cumulative counts and pending samples. Its
// lock is a leaf: the Server (under s.mu) and the link (under l.mu) count
// into it from the same entry that updates their 10 s lines.
type dropLedger struct {
	mu      sync.Mutex
	streams map[uint64]*dropStream
	waiting []uint64 // removed players whose final is due, oldest first
	ended   map[uint64]bool
	merged  uint64
	closed  bool
}

func newDropLedger() *dropLedger {
	return &dropLedger{streams: make(map[uint64]*dropStream), ended: make(map[uint64]bool)}
}

// add counts n items of one event. Counting stops for a stream once its final
// is written, and for every stream after the graceful close.
func (d *dropLedger) add(player uint64, n uint64, e dropEvent) {
	if d == nil || n == 0 || e.reason == runtime.DropReason_DROP_REASON_UNSPECIFIED {
		return
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.closed || d.ended[player] {
		return
	}
	s := d.streams[player]
	if s == nil {
		s = &dropStream{totals: make(map[runtime.DropReason]uint64)}
		d.streams[player] = s
	}
	s.totals[e.reason] += n
	s.dirty = true
	if len(s.samples) < dropSamplesPerReport {
		if e.items == 0 {
			e.items = uint32(min(n, uint64(^uint32(0))))
		}
		s.samples = append(s.samples, e)
	} else {
		s.omitted++
	}
}

// remove ends a player's stream: one final report, if it ever counted
// anything; past the waiting room, the stream is merged into the Gateway
// stream's merged_streams.
func (d *dropLedger) remove(player uint64) {
	if d == nil {
		return
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	s := d.streams[player]
	if player == 0 || d.ended[player] || s == nil || s.final {
		d.ended[player] = player != 0
		return
	}
	if len(d.waiting) >= dropWaitingFinals {
		delete(d.streams, player)
		d.ended[player] = true
		d.merged++
		d.gatewayLocked().dirty = true
		return
	}
	s.final, s.dirty = true, true
	d.waiting = append(d.waiting, player)
}

func (d *dropLedger) gatewayLocked() *dropStream {
	s := d.streams[0]
	if s == nil {
		s = &dropStream{totals: make(map[runtime.DropReason]uint64)}
		d.streams[0] = s
	}
	return s
}

// due lists the streams a timer pass writes: the live streams that changed,
// then the waiting finals, at most dropStreamsPerPass in all. At the graceful
// close (closing) every stream with content goes, as a final, and the ledger
// closes.
func (d *dropLedger) due(closing bool) []uint64 {
	if d == nil {
		return nil
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.closed {
		return nil
	}
	var live []uint64
	for player, s := range d.streams {
		if !s.final && (s.dirty || closing) {
			live = append(live, player)
		}
	}
	sort.Slice(live, func(i, j int) bool { return live[i] < live[j] })
	if closing {
		for player := range d.streams {
			d.streams[player].final = true
		}
		d.closed = true
		return append(live, d.waiting...)
	}
	out := append(live, d.waiting...)
	if len(out) > dropStreamsPerPass {
		out = out[:dropStreamsPerPass]
	}
	return out
}

// report builds the stream's next report now, just before it is written, and
// settles it: the sequence advances, the samples are taken, a final ends the
// stream. Nil when the stream has nothing to send.
func (d *dropLedger) report(player uint64, now time.Time) *runtime.GatewayDropReport {
	d.mu.Lock()
	defer d.mu.Unlock()
	s := d.streams[player]
	if s == nil || !s.dirty && !s.final {
		return nil
	}
	s.sequence++
	r := &runtime.GatewayDropReport{PlayerId: player, ReportSequence: s.sequence, Final: s.final, SamplesOmitted: s.omitted}
	reasons := make([]runtime.DropReason, 0, len(s.totals))
	for reason := range s.totals {
		reasons = append(reasons, reason)
	}
	sort.Slice(reasons, func(i, j int) bool { return reasons[i] < reasons[j] })
	for _, reason := range reasons {
		r.Counts = append(r.Counts, &runtime.DropCount{Reason: reason, Total: s.totals[reason]})
	}
	for _, e := range s.samples {
		r.Samples = append(r.Samples, &runtime.DropSample{Reason: e.reason, DwellUs: dwellMicros(e.at, now),
			TransportSequence: e.transport, MovementEpoch: e.epoch, LifeGeneration: e.life,
			FirstCommandSequence: e.firstCommand, LastCommandSequence: e.lastCommand,
			FirstActionId: e.firstAction, LastActionId: e.lastAction, SnapshotTick: e.tick, ItemCount: e.items})
	}
	if player == 0 {
		r.MergedStreams = d.merged
	}
	s.samples, s.dirty = nil, false
	if s.final {
		delete(d.streams, player)
		d.ended[player] = player != 0
		for i, waiting := range d.waiting {
			if waiting == player {
				d.waiting = append(d.waiting[:i], d.waiting[i+1:]...)
				break
			}
		}
	}
	return r
}

// dropReportLine is the Gateway's log of a written report, and the body the
// Match logs for each report it reads ("gw_drop report ..."): L2 compares them
// by report_sequence.
func dropReportLine(r *runtime.GatewayDropReport) string {
	var b strings.Builder
	fmt.Fprintf(&b, "player=%d report_sequence=%d final=%d samples=%d samples_omitted=%d merged_streams=%d",
		r.PlayerId, r.ReportSequence, finalFlag(r.Final), len(r.Samples), r.SamplesOmitted, r.MergedStreams)
	for _, c := range r.Counts {
		fmt.Fprintf(&b, " %s=%d", dropReasonKey[c.Reason], c.Total)
	}
	return b.String()
}

// writeDrops writes the due streams' reports, each built just before its
// write (so its samples' dwell ends there, pv7 §2), and logs each one.
func (l *runtimeLink) writeDrops(closing bool) error {
	for _, player := range l.drops.due(closing) {
		if !closing {
			select {
			case <-l.done: // the close writes what is left, as finals
				return errLinkClosing
			default:
			}
		}
		r := l.drops.report(player, time.Now())
		if r == nil {
			continue
		}
		if err := l.writeEnvelope(&runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion,
			Message: &runtime.RuntimeEnvelope_DropReport{DropReport: r}}); err != nil {
			return err
		}
		log.Printf("drop report %s", dropReportLine(r))
	}
	return nil
}

// countDrop counts a player counter's discard into the ledger, if the reason
// is reported.
func (l *runtimeLink) countDrop(player uint64, counter playerIngressCounter, n uint64, e dropEvent) {
	if l == nil {
		return
	}
	if reason := playerDropReason[counter]; reason != runtime.DropReason_DROP_REASON_UNSPECIFIED {
		e.reason = reason
		l.drops.add(player, n, e)
	}
}

// ledger is the link's drop ledger; nil for a missing link (tests).
func (l *runtimeLink) ledger() *dropLedger {
	if l == nil {
		return nil
	}
	return l.drops
}

// dwellMicros is the pv7 §2 dwell from an item's start to now in microseconds,
// clamped at dropMaximumDwell: the one definition behind DropSample.dwell_us
// and the gateway_dwell_us of the IPC input and action messages.
func dwellMicros(start, now time.Time) uint32 {
	return uint32(min(max(now.Sub(start), 0), dropMaximumDwell) / time.Microsecond)
}
