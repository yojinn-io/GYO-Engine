package gateway

import (
	"slices"
	"testing"
	"time"

	runtime "gyo.local/object_fps_pvp/protocol/runtimev8"
)

// matchLane models IpcHost's ActionLane (pv7 §2a): one frame per action
// interval at most, carrying the retirement, the four smallest unacknowledged
// decisions and four cycling slots; due while anything is unacknowledged, the
// retirement advanced, or an ACK-only batch asked for it again.
type matchLane struct {
	sent              bool
	last              time.Duration
	sentRetired, next uint64 // next: the cursor (the last cycled id)
	asked             bool
}

func (m *matchLane) frame(now time.Duration, decided []uint64, retired uint64) ([]uint64, bool) {
	if len(decided) == 0 && retired <= m.sentRetired && !m.asked || m.sent && now-m.last < actionSendInterval {
		return nil, false
	}
	m.sent, m.last, m.sentRetired, m.asked = true, now, retired, false
	first := min(len(decided), 4)
	ids := slices.Clone(decided[:first])
	rest := decided[first:]
	start, _ := slices.BinarySearch(rest, m.next+1)
	for k := 0; k < min(len(rest), 8-first); k++ {
		id := rest[(start+k)%len(rest)]
		ids = append(ids, id)
		m.next = id
	}
	slices.Sort(ids)
	return ids, true
}

// sendFrame hands the Gateway one frame of the lane.
func (r *resultLoop) sendFrame(retired uint64, ids []uint64) {
	r.t.Helper()
	in := &runtime.ActionResults{PlayerId: 7, RetiredThrough: retired}
	for _, id := range ids {
		in.Decisions = append(in.Decisions, decision(id))
	}
	if err := r.s.receiveActionResults(in); err != nil {
		r.t.Fatal(err)
	}
	r.deadline, r.pending = r.s.resultDeadline()
}

// pv7 §2a (D-10): frames arriving one action interval apart, each up to
// ±0.5 ms off, are all released: a frame is replaced only when a second one
// arrives before the first's release, and releases follow the arrivals.
// (Dropping a frame that arrives within an interval of the last send would
// lose about half of them.)
func TestResultSlotReleasesFramesArrivingAtTheIntervalWithoutDrops(t *testing.T) {
	r := newResultLoop(t)
	r.clientBatch(0, 1)
	start := r.now
	jitter := []time.Duration{500, -500, 300, -200, 0, 450, -450, 100} // µs
	frames := 0
	for k := 1; k <= 300; k++ {
		arrival := start.Add(time.Duration(k)*actionSendInterval + jitter[k%len(jitter)]*time.Microsecond)
		// The loop wakes for a release due before this arrival.
		for r.pending && r.deadline.Before(arrival) {
			r.now = maxTime(r.now, r.deadline)
			r.pass(0)
		}
		r.now = maxTime(r.now, arrival)
		r.sendFrame(0, []uint64{1})
		frames++
		if !r.deadline.After(r.now) {
			r.pass(0)
		}
	}
	for r.pending {
		r.now = maxTime(r.now, r.deadline)
		r.pass(0)
	}
	if replaced := r.s.playerIngress[7][playerDropResultsOverRatePackets]; replaced != 0 || len(r.written) != frames {
		t.Fatalf("%d frames, %d released, %d replaced", frames, len(r.written), replaced)
	}
}

// pv7 §9 (edge-results-passthrough), end to end on a virtual clock: the
// Match's lane, the Gateway's slot and a Client that loses every other result
// datagram (either phase) and acknowledges contiguously at once. n
// unacknowledged decisions all arrive within 2⌈n/8⌉+2 datagrams.
//
// The bound is §9's since D112 (it first said 2⌈n/8⌉): measured 16→6, 24→8,
// 32→10 when the first datagram is lost, 24→7, 32→9 otherwise. With a gap the
// Client's contiguous ACK hides which later ids it holds, so one received
// frame cycles through ids it already has.
func TestUnacknowledgedDecisionsArriveDespiteEveryOtherDatagramLost(t *testing.T) {
	for _, n := range []uint64{8, 16, 24, 32} {
		for _, firstLost := range []bool{true, false} {
			r := newResultLoop(t)
			var ids []uint64
			for id := uint64(1); id <= n; id++ {
				ids = append(ids, id)
			}
			for first := 0; first < len(ids); first += 8 {
				r.clientBatch(0, ids[first:min(first+8, len(ids))]...)
			}
			start := r.now
			var m matchLane
			decided, retired := slices.Clone(ids), uint64(0)
			received := map[uint64]bool{}
			datagrams := 0
			for step := time.Duration(0); step < 2*time.Second && len(received) < int(n); step += time.Millisecond {
				r.now = start.Add(step)
				if frame, ok := m.frame(step, decided, retired); ok {
					r.sendFrame(retired, frame)
				}
				if !r.pending || r.deadline.After(r.now) {
					continue
				}
				for _, results := range r.pass(0) {
					datagrams++
					if (datagrams%2 == 1) == firstLost {
						continue // lost
					}
					for _, d := range results.Decisions {
						received[d.ActionId] = true
					}
					// The Client acknowledges contiguously; the Match retires at once.
					for received[retired+1] {
						retired++
					}
					decided = slices.DeleteFunc(decided, func(id uint64) bool { return id <= retired })
				}
			}
			if limit := 2*int((n+7)/8) + 2; len(received) != int(n) || datagrams > limit {
				t.Fatalf("n=%d, first lost %v: %d of %d decisions after %d datagrams (at most %d)", n, firstLost,
					len(received), n, datagrams, limit)
			}
		}
	}
}

// pv7 §9: the datagram with the last retirement is lost. The Client's next
// ACK-only batch (its ACK is ahead of the retirement it knows) reaches the
// Match through the link's FIFO like any other batch (pv7 §2a); the Match answers it with the retirement again, and the Client
// has it within two intervals of the loss.
func TestALostRetirementReachesTheClientWithinTwoIntervals(t *testing.T) {
	r := newResultLoop(t)
	r.clientBatch(0, 1)
	start := r.now
	var m matchLane
	decided, retired, knownRetired := []uint64{1}, uint64(0), uint64(0)
	received := map[uint64]bool{}
	var lostAt, gotAt time.Duration
	lost := false
	for step := time.Duration(0); step < 2*time.Second && gotAt == 0; step += time.Millisecond {
		r.now = start.Add(step)
		// The Client's batch every 33 ms: its ACK, ACK-only while ahead of the
		// retirement it knows.
		if step%(33*time.Millisecond) == 0 && received[1] && knownRetired < 1 {
			r.clientBatch(1)
		}
		// The link's pass: what it writes reaches the Match.
		for _, in := range actionMessages(r.s.link.batch()) {
			if len(in.Shots) == 0 && in.AcknowledgedThrough > 0 && in.AcknowledgedThrough <= retired {
				m.asked = true // IpcHost: an ACK-only batch behind the retirement
			}
			if in.AcknowledgedThrough > retired {
				retired = in.AcknowledgedThrough
				decided = slices.DeleteFunc(decided, func(id uint64) bool { return id <= retired })
			}
		}
		if frame, ok := m.frame(step, decided, retired); ok {
			r.sendFrame(retired, frame)
		}
		if !r.pending || r.deadline.After(r.now) {
			continue
		}
		for _, results := range r.pass(0) {
			if results.RetiredThrough == 1 && !lost {
				lost, lostAt = true, step // the first retirement is lost
				continue
			}
			for _, d := range results.Decisions {
				received[d.ActionId] = true
			}
			knownRetired = max(knownRetired, results.RetiredThrough)
			if knownRetired == 1 && gotAt == 0 {
				gotAt = step
			}
		}
	}
	if !lost || gotAt == 0 || gotAt-lostAt > 2*actionSendInterval {
		t.Fatalf("retirement lost at %v, received at %v (at most %v later)", lostAt, gotAt, 2*actionSendInterval)
	}
	t.Logf("the lost retirement arrived %v after the loss", gotAt-lostAt)
}

// Results for a player that is not active are not a loss (pv7 §4 (d)): no
// frame is held or sent, and they are only counted for the log.
func TestResultsForAnInactivePlayerAreCountedOnly(t *testing.T) {
	r := newResultLoop(t)
	r.s.mu.Lock()
	r.p.phase = joining
	r.s.mu.Unlock()
	r.sendFrame(0, nil)
	if r.pending || r.p.results.pending != nil || r.s.resultsForInactive != 1 {
		t.Fatalf("held %v, pending %v, counted %d", r.p.results.pending != nil, r.pending, r.s.resultsForInactive)
	}
	if sent := r.pass(0); len(sent) != 0 {
		t.Fatalf("%d results sent to an inactive player", len(sent))
	}
}
