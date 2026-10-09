package gateway

import (
	"testing"
	"time"

	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

// A fixture whose player holds one unacknowledged decision, so that every
// eligible tick selects a result batch.
func resultCadenceFixture(t *testing.T) (*Server, time.Time) {
	t.Helper()
	s, p, _, now := actionFixture(t)
	in := &runtime.ActionBatch{PlayerId: 7, Shots: []*runtime.ShotRequest{shot(1)}}
	p.actions.merge(in)
	if s.link.actions(in) != nil {
		t.Fatal("admission")
	}
	if s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}}) != nil {
		t.Fatal("result")
	}
	return s, now
}

// Drives sendUDP's loop by hand: ticks at the given times, each selected batch
// written (and its deadline re-anchored) writeDelay after its tick.
func sendResults(s *Server, ticks []time.Time, writeDelay time.Duration) []time.Time {
	var sent []time.Time
	for _, tick := range ticks {
		for _, packet := range s.actionPackets(tick) {
			written := tick.Add(writeDelay)
			s.actionWritten(packet.actionPlayer, written)
			sent = append(sent, written)
		}
	}
	return sent
}

func TestResultChannelSendsOnEveryTickDespiteWriteReanchoring(t *testing.T) {
	s, start := resultCadenceFixture(t)
	ticks := make([]time.Time, 30)
	for k := range ticks {
		ticks[k] = start.Add(time.Duration(k) * actionSendInterval)
	}
	// The write completes 200 us after its tick, as a real UDP write does.
	if sent := sendResults(s, ticks, 200*time.Microsecond); len(sent) != len(ticks) {
		t.Fatalf("result channel sent %d of %d ticks", len(sent), len(ticks))
	}
}

func TestResultChannelKeepsHalfAnIntervalAndNoBurstAfterABlockedWrite(t *testing.T) {
	s, start := resultCadenceFixture(t)
	// Ticks with a fixed pattern of 0..30 ms delays (a busy send loop), plus a
	// buffered tick delivered right after a write blocked for 250 ms.
	delays := []time.Duration{0, 30, 2, 17, 29, 0, 11, 25, 3, 30}
	var ticks []time.Time
	for k := 0; k < 120; k++ {
		ticks = append(ticks, start.Add(time.Duration(k)*actionSendInterval+delays[k%len(delays)]*time.Millisecond))
	}
	sent := sendResults(s, ticks, 200*time.Microsecond)
	blockedUntil := sent[len(sent)-1].Add(250 * time.Millisecond)
	s.actionWritten(7, blockedUntil)
	sent = append(sent, sendResults(s, []time.Time{blockedUntil, blockedUntil.Add(resultSendTolerance - 1)}, 0)...)
	if last := sent[len(sent)-1]; !last.Before(blockedUntil) {
		t.Fatalf("released write sent a catch-up result at %v", last.Sub(blockedUntil))
	}
	for i := 1; i < len(sent); i++ {
		if gap := sent[i].Sub(sent[i-1]); gap < resultSendTolerance {
			t.Fatalf("results %v apart", gap)
		}
	}
	// action_probe's limit: any one-second window holds at most 31 results.
	for i := range sent {
		count := 0
		for j := i; j < len(sent) && sent[j].Sub(sent[i]) < time.Second; j++ {
			count++
		}
		if count > 31 {
			t.Fatalf("%d results within one second", count)
		}
	}
	// Delays stay below one interval, so two ticks apart are always more than
	// half an interval apart: at least every other tick sends.
	if len(sent) < 60 {
		t.Fatalf("only %d of 120 ticks sent", len(sent))
	}
}

// The real runtime link loop, woken by other events at 60 Hz (as inputs do),
// resends an undecided request at ActionSendRate: not on every wake, and not
// only on the wakes that happen to come after its deadline.
func TestLinkResendsAnUndecidedRequestAtTheActionRateDespiteOtherWakes(t *testing.T) {
	s, f := newTestServer(t)
	in := &runtime.ActionBatch{PlayerId: 7, Shots: []*runtime.ShotRequest{shot(1)}}
	if err := s.link.actions(in); err != nil {
		t.Fatal(err)
	}
	stop := make(chan struct{})
	defer close(stop)
	go func() {
		ticker := time.NewTicker(time.Second / 60)
		defer ticker.Stop()
		for {
			select {
			case <-stop:
				return
			case <-ticker.C:
				s.link.notify()
			}
		}
	}()
	var arrivals []time.Time
	deadline := time.Now().Add(500 * time.Millisecond)
	for time.Now().Before(deadline) {
		select {
		case e := <-f.got:
			if e.GetActions() != nil {
				arrivals = append(arrivals, time.Now())
			}
		case <-time.After(time.Until(deadline)):
		}
	}
	// 500 ms at 30 Hz is 15 batches; a resend on every wake would be about 30,
	// and one that waits for the next wake after its deadline about 10.
	if len(arrivals) < 13 || len(arrivals) > 17 {
		t.Fatalf("runtime link sent %d action batches in 500 ms", len(arrivals))
	}
	for i := 1; i < len(arrivals); i++ {
		if gap := arrivals[i].Sub(arrivals[i-1]); gap < actionSendInterval-3*time.Millisecond {
			t.Fatalf("action batches %v apart", gap)
		}
	}
}
