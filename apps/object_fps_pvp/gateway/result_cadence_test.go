package gateway

import (
	"context"
	"math/rand/v2"
	"net"
	"net/netip"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv6"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

// resultLoop drives sendUDP's result path by hand on a virtual clock: a pass
// selects at now and writes each packet, re-anchoring its deadline at the
// completed write, and the next pass is due at the deadline actionPackets (or,
// after writes, resultDeadline) reports, never at a fixed tick.
type resultLoop struct {
	t        *testing.T
	s        *Server
	p        *reservation
	endpoint netip.AddrPort
	now      time.Time
	seq      uint32
	written  []time.Time
	deadline time.Time
	pending  bool
}

func newResultLoop(t *testing.T) *resultLoop {
	t.Helper()
	s, p, endpoint, now := actionFixture(t)
	return &resultLoop{t: t, s: s, p: p, endpoint: endpoint, now: now, seq: 1}
}

// The Client's side, through the Gateway's own receive paths.
func (r *resultLoop) clientBatch(ack uint64, ids ...uint64) {
	r.t.Helper()
	b := &client.ActionBatch{AcknowledgedThrough: ack}
	for _, id := range ids {
		b.Shots = append(b.Shots, &client.ShotRequest{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: id, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)})
	}
	r.seq++
	deliverActions(r.t, r.s, r.p, r.endpoint, r.seq, b, r.now)
}

// The Match's side: decisions, then a retirement.
func (r *resultLoop) match(retired uint64, ids ...uint64) {
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

// One pass of the loop at r.now; each write completes writeDelay after the
// previous one. Returns the batches written.
func (r *resultLoop) pass(writeDelay time.Duration) []*client.ActionResults {
	r.t.Helper()
	packets, deadline, pending := r.s.actionPackets(r.now)
	var out []*client.ActionResults
	for _, packet := range packets {
		r.now = r.now.Add(writeDelay)
		r.s.actionWritten(packet.actionPlayer, r.now)
		r.written = append(r.written, r.now)
		h, payload, err := framing.DecodeDatagram(packet.packet)
		var results client.ActionResults
		if err != nil || h.Type != adapter.ActionResults || proto.Unmarshal(payload, &results) != nil {
			r.t.Fatal("bad result datagram")
		}
		out = append(out, &results)
	}
	if len(packets) > 0 {
		deadline, pending = r.s.resultDeadline()
	}
	r.deadline, r.pending = deadline, pending
	return out
}

// The time the loop's timer fires next: the deadline, or one interval later
// while nothing is pending.
func (r *resultLoop) nextTimer() time.Time {
	if !r.pending {
		return r.now.Add(actionSendInterval)
	}
	return maxTime(r.deadline, r.now)
}

func maxTime(a, b time.Time) time.Time {
	if a.After(b) {
		return a
	}
	return b
}

// G1: with the Client caught up nothing is sent, not even the retirement
// again; a new decision after an idle interval goes out on the same pass.
func TestResultsIdleWithoutRepeatsAndANewDecisionGoesOutAtOnce(t *testing.T) {
	r := newResultLoop(t)
	r.clientBatch(0, 1)
	r.match(0, 1)
	if sent := r.pass(200 * time.Microsecond); len(sent) != 1 || len(sent[0].Decisions) != 1 {
		t.Fatalf("first decision %v", sent)
	}
	r.clientBatch(1)
	r.match(1)
	r.now = r.nextTimer()
	if sent := r.pass(200 * time.Microsecond); len(sent) != 1 || sent[0].RetiredThrough != 1 || len(sent[0].Decisions) != 0 {
		t.Fatalf("retirement %v", sent)
	}
	for k := 0; k < 30; k++ {
		r.now = r.nextTimer()
		if sent := r.pass(0); len(sent) != 0 {
			t.Fatalf("retired-only repeat after %v", r.now.Sub(r.written[1]))
		}
	}
	last := r.written[len(r.written)-1]
	r.now = r.now.Add(7 * time.Millisecond)
	r.clientBatch(1, 2)
	r.match(1, 2)
	if !r.pending || r.deadline.After(r.now) {
		t.Fatalf("new decision %v after the last write waits until %v", r.now.Sub(last), r.deadline.Sub(last))
	}
	at := r.now
	if sent := r.pass(0); len(sent) != 1 || len(sent[0].Decisions) != 1 || sent[0].Decisions[0].ActionId != 2 || !r.written[len(r.written)-1].Equal(at) {
		t.Fatalf("new decision not sent at once: %v", sent)
	}
}

// G2: a decision less than an interval after the last write waits for its
// deadline, the last write plus one interval, exactly.
func TestResultsLessThanAnIntervalApartWaitForTheDeadline(t *testing.T) {
	r := newResultLoop(t)
	r.clientBatch(0, 1)
	r.match(0, 1)
	r.pass(200 * time.Microsecond)
	t0 := r.written[0]
	r.clientBatch(1)
	r.now = t0.Add(10 * time.Millisecond)
	r.clientBatch(1, 2)
	r.match(0, 2)
	if !r.pending || !r.deadline.Equal(t0.Add(actionSendInterval)) {
		t.Fatalf("deadline %v after the write, want one interval", r.deadline.Sub(t0))
	}
	r.now = r.deadline.Add(-1)
	if sent := r.pass(0); len(sent) != 0 {
		t.Fatalf("result sent %v after the previous write, before its deadline", r.now.Sub(t0))
	}
	r.now = t0.Add(actionSendInterval)
	if sent := r.pass(0); len(sent) != 1 {
		t.Fatal("result not sent at its deadline")
	}
}

// G3: random new decisions, ACKs, retirements and pokes with writes taking
// 0..30 ms, one of them blocked 250 ms. Writes stay a strict interval apart,
// so any half-open second holds at most 30, and the blocked write's release
// sends no catch-up.
func TestResultsNeverBurstUnderRandomEventsAndABlockedWrite(t *testing.T) {
	rng := rand.New(rand.NewPCG(8, 2))
	r := newResultLoop(t)
	start := r.now
	var requested, decided, acknowledged, retired uint64
	event := func() {
		switch rng.IntN(5) {
		case 0:
			if requested-retired < adapter.MaxActions {
				requested++
				r.clientBatch(acknowledged, requested)
			}
		case 1:
			if decided < requested {
				decided++
				r.match(retired, decided)
			}
		case 2:
			if acknowledged < decided {
				acknowledged = decided
				r.clientBatch(acknowledged)
			}
		case 3:
			if retired < acknowledged {
				retired = acknowledged
				r.match(retired)
			}
		case 4:
			if retired > 0 {
				r.clientBatch(retired) // A poke.
			}
		}
	}
	nextEvent := start.Add(time.Duration(rng.IntN(40)) * time.Millisecond)
	var release time.Time
	for r.now.Before(start.Add(20 * time.Second)) {
		wake := r.nextTimer()
		if nextEvent.Before(wake) {
			wake = nextEvent
		}
		r.now = maxTime(r.now, wake)
		for !nextEvent.After(r.now) {
			event()
			nextEvent = nextEvent.Add(time.Duration(5+rng.IntN(40)) * time.Millisecond)
		}
		delay := time.Duration(rng.IntN(30_001)) * time.Microsecond
		if release.IsZero() && len(r.written) == 40 {
			delay = 250 * time.Millisecond
		}
		if sent := r.pass(delay); len(sent) > 0 && delay == 250*time.Millisecond {
			release = r.now
		}
	}
	if release.IsZero() || len(r.written) < 150 {
		t.Fatalf("only %d results, blocked write %v", len(r.written), release)
	}
	for i := 1; i < len(r.written); i++ {
		if gap := r.written[i].Sub(r.written[i-1]); gap < actionSendInterval {
			t.Fatalf("results %v apart (write %d)", gap, i)
		}
		if r.written[i-1].Equal(release) && r.written[i].Before(release.Add(actionSendInterval)) {
			t.Fatalf("released write sent a catch-up result after %v", r.written[i].Sub(release))
		}
	}
	for i := range r.written {
		count := 0
		for j := i; j < len(r.written) && r.written[j].Sub(r.written[i]) < time.Second; j++ {
			count++
		}
		if count > 30 {
			t.Fatalf("%d results within one second", count)
		}
	}
}

// G4: a retirement goes out once; neither timer passes nor other wakes
// repeat it.
func TestARetirementIsSentOnce(t *testing.T) {
	rng := rand.New(rand.NewPCG(4, 4))
	r := newResultLoop(t)
	r.clientBatch(0, 1)
	r.match(0, 1)
	r.pass(0)
	r.clientBatch(1)
	r.match(1)
	r.now = r.nextTimer()
	if sent := r.pass(0); len(sent) != 1 || sent[0].RetiredThrough != 1 {
		t.Fatalf("retirement %v", sent)
	}
	end := r.now.Add(time.Second)
	for r.now.Before(end) {
		if rng.IntN(2) == 0 {
			r.now = r.nextTimer()
		} else {
			r.now = r.now.Add(time.Duration(rng.IntN(20_000)) * time.Microsecond) // Another wake.
		}
		if sent := r.pass(0); len(sent) != 0 {
			t.Fatalf("retirement repeated: %v", sent)
		}
	}
}

// G5: an ACK-only batch acknowledging no more than the retirement already
// sent shows the Client missed it: one retirement packet at the deadline. An
// ACK ahead of the retirement, a batch with shots, or a duplicate sequence
// asks for nothing.
func TestAnACKOnlyBatchBehindTheRetirementPokesItOnce(t *testing.T) {
	r := newResultLoop(t)
	r.clientBatch(0, 1)
	r.match(0, 1)
	r.pass(0)
	r.clientBatch(1)
	r.match(1)
	r.now = r.nextTimer()
	r.pass(0) // The retirement, lost on its way.
	last := r.now
	r.now = last.Add(5 * time.Millisecond)
	r.clientBatch(1)
	if r.deadline, r.pending = r.s.resultDeadline(); !r.pending || !r.deadline.Equal(last.Add(actionSendInterval)) {
		t.Fatal("ACK-only batch behind the retirement asked for nothing")
	}
	r.now = r.deadline.Add(-1)
	if sent := r.pass(0); len(sent) != 0 {
		t.Fatal("poke answered before the deadline")
	}
	r.now = last.Add(actionSendInterval)
	if sent := r.pass(0); len(sent) != 1 || sent[0].RetiredThrough != 1 || len(sent[0].Decisions) != 0 {
		t.Fatalf("poke answer %v", sent)
	}
	if r.pending {
		t.Fatal("poke answered more than once")
	}
	// An ACK ahead of the retirement: the Client knows of nothing missing.
	r.now = r.now.Add(time.Second)
	r.clientBatch(1, 2)
	r.match(1, 2)
	r.pass(0)
	r.clientBatch(2)
	if _, pending := r.s.resultDeadline(); pending {
		t.Fatal("ACK ahead of the retirement poked")
	}
	r.match(2)
	r.now = r.nextTimer()
	r.pass(0)
	r.now = r.now.Add(time.Second)
	// A batch with shots, even with an ACK behind the retirement.
	r.clientBatch(2, 3)
	if _, pending := r.s.resultDeadline(); pending {
		t.Fatal("batch with shots poked")
	}
	// A duplicate sequence that CommitSequence rejects.
	payload, _ := proto.Marshal(&client.ActionBatch{AcknowledgedThrough: 2})
	r.s.mu.Lock()
	err := r.s.receiveActions(r.p, framing.Header{Version: adapter.ClientVersion, Type: adapter.Actions, SessionID: r.p.session.ID, Sequence: r.seq}, payload, r.now)
	r.s.mu.Unlock()
	if err != nil {
		t.Fatal(err)
	}
	if _, pending := r.s.resultDeadline(); pending {
		t.Fatal("rejected duplicate sequence poked")
	}
}

// G6: unacknowledged decisions are resent every interval, more than eight in
// rotation, until the Client's contiguous ACK.
func TestUnacknowledgedDecisionsAreResentEachIntervalInRotation(t *testing.T) {
	r := newResultLoop(t)
	r.clientBatch(0, 1, 2, 3, 4, 5, 6, 7, 8)
	r.clientBatch(0, 9, 10)
	r.match(0, 1, 2, 3, 4, 5, 6, 7, 8)
	r.match(0, 9, 10)
	seen := map[uint64]bool{}
	for k := 0; k < 3; k++ {
		at := r.nextTimer()
		if k > 0 && !at.Equal(r.written[k-1].Add(actionSendInterval)) {
			t.Fatalf("resend %d due %v after the previous write", k, at.Sub(r.written[k-1]))
		}
		r.now = at
		sent := r.pass(0)
		if len(sent) != 1 || len(sent[0].Decisions) != adapter.MaxActionBatch {
			t.Fatalf("resend %d: %v", k, sent)
		}
		for _, d := range sent[0].Decisions {
			seen[d.ActionId] = true
		}
	}
	if len(seen) != 10 {
		t.Fatalf("rotation covered %d of 10 decisions", len(seen))
	}
	r.clientBatch(10)
	for k := 0; k < 30; k++ {
		r.now = r.nextTimer()
		if sent := r.pass(0); len(sent) != 0 {
			t.Fatal("acknowledged decisions resent")
		}
	}
}

// G7: for random ledgers, phases, availability and times, the reported
// deadline and the selection agree: a deadline at or before now selects a
// batch, a batch is only selected at or after a reported deadline, and
// nothing due is left unselected. Players leave and are evicted in between.
func TestResultDeadlineAgreesWithSelection(t *testing.T) {
	rng := rand.New(rand.NewPCG(7, 7))
	s, _, _, base := actionFixture(t)
	sessions := make([]*session.Session, 5)
	for i := range sessions {
		peer, err := session.New(base)
		if err != nil || peer.Hello(peer.Token, netip.AddrPortFrom(netip.MustParseAddr("127.0.0.1"), uint16(29010+i)), 1, base) != nil {
			t.Fatal("session")
		}
		sessions[i] = peer
	}
	jitter := func(span time.Duration) time.Duration { return time.Duration(rng.Int64N(int64(2*span))) - span }
	sent := 0
	for trial := 0; trial < 3000; trial++ {
		s.mu.Lock()
		s.available = rng.IntN(6) != 0
		clear(s.players)
		clear(s.sessions)
		for id := uint64(1); id < uint64(len(sessions)); id++ {
			p := &reservation{playerID: id, phase: phase(rng.IntN(3)), session: sessions[id], movementEpoch: 1, lifeGeneration: 1, commands: map[uint64]*runtime.MovementCommand{}}
			if rng.IntN(5) != 0 {
				w := newActionWindow()
				w.retired = uint64(rng.IntN(3))
				decided := uint64(rng.IntN(4))
				for k := uint64(1); k <= decided; k++ {
					w.decisions[w.retired+k] = decision(w.retired + k)
				}
				w.acknowledged = w.retired + uint64(rng.IntN(int(decided)+1))
				w.retiredSent = uint64(rng.IntN(int(w.retired) + 1))
				w.retirementAsked = rng.IntN(4) == 0
				w.nextSend = base.Add(jitter(50 * time.Millisecond))
				p.actions = w
			}
			s.players[id] = p
			s.sessions[p.session.ID] = p
		}
		if rng.IntN(4) == 0 {
			s.remove(s.players[uint64(1+rng.IntN(len(sessions)-1))]) // A Leave.
		}
		s.mu.Unlock()
		if rng.IntN(4) == 0 {
			e := envelope()
			e.Message = &runtime.RuntimeEnvelope_Evicted{Evicted: &runtime.PlayerEvicted{PlayerId: uint64(1 + rng.IntN(len(sessions)-1)), Reason: runtime.EvictionReason_EVICTION_HIGH_LATENCY}}
			s.runtimeMessage(e)
			for len(s.controlOut) > 0 {
				<-s.controlOut
			}
		}
		deadline, pending := s.resultDeadline()
		now := base.Add(jitter(60 * time.Millisecond))
		if pending {
			switch rng.IntN(3) {
			case 0:
				now = deadline
			case 1:
				now = deadline.Add(-1)
			}
		}
		packets, after, stillPending := s.actionPackets(now)
		due := pending && !deadline.After(now)
		if due && len(packets) == 0 {
			t.Fatalf("trial %d: deadline %v reported at or before now but nothing was sent", trial, now.Sub(deadline))
		}
		if !due && len(packets) > 0 {
			t.Fatalf("trial %d: %d batches sent without a reported deadline at or before now", trial, len(packets))
		}
		if stillPending && !after.After(now) {
			t.Fatalf("trial %d: a deadline was left due after selection", trial)
		}
		sent += len(packets)
	}
	if sent < 300 {
		t.Fatalf("only %d batches over all trials", sent)
	}
}

// G8 (virtual clock): the send loop waits until the deadline, not a fixed
// interval. A decision 30 ms after a write waits the remaining 3.3 ms.
func TestResultWaitEndsAtTheDeadline(t *testing.T) {
	r := newResultLoop(t)
	r.clientBatch(0, 1)
	r.match(0, 1)
	r.pass(0)
	t0 := r.now
	r.clientBatch(1)
	r.now = t0.Add(30 * time.Millisecond)
	r.clientBatch(1, 2)
	r.match(0, 2)
	packets, deadline, pending := r.s.actionPackets(r.now)
	if len(packets) != 0 {
		t.Fatal("sent before the deadline")
	}
	if wait := resultWait(deadline, pending, r.now); wait != t0.Add(actionSendInterval).Sub(r.now) {
		t.Fatalf("result wait %v after a decision 30 ms after the write, want %v", wait, t0.Add(actionSendInterval).Sub(r.now))
	}
	if wait := resultWait(deadline, pending, deadline.Add(time.Millisecond)); wait != 0 {
		t.Fatalf("result wait %v past the deadline", wait)
	}
	if wait := resultWait(time.Time{}, false, r.now); wait != actionSendInterval {
		t.Fatalf("idle result wait %v", wait)
	}
}

// G8: only new content for the Client wakes the send loop: a new decision, a
// retirement, a poke. Repeats and ACKs do not.
func TestOnlyNewResultContentWakesTheSendLoop(t *testing.T) {
	r := newResultLoop(t)
	woken := func() bool {
		select {
		case <-r.s.resultWake:
			return true
		default:
			return false
		}
	}
	r.clientBatch(0, 1)
	if woken() {
		t.Fatal("a request woke the send loop")
	}
	r.match(0, 1)
	if !woken() {
		t.Fatal("a new decision did not wake the send loop")
	}
	r.match(0, 1)
	if woken() {
		t.Fatal("a repeated decision woke the send loop")
	}
	r.clientBatch(1)
	if woken() {
		t.Fatal("an ACK ahead of the retirement woke the send loop")
	}
	r.match(1)
	if !woken() {
		t.Fatal("a retirement did not wake the send loop")
	}
	r.match(1)
	if woken() {
		t.Fatal("a repeated retirement woke the send loop")
	}
	r.clientBatch(1)
	if !woken() {
		t.Fatal("a poke did not wake the send loop")
	}
}

// The real sendUDP loop over loopback UDP for the fixture's player, whose
// Client is the returned socket.
func resultLoopFixture(t *testing.T) (*Server, *reservation, *net.UDPConn, netip.AddrPort, context.CancelFunc, chan struct{}) {
	t.Helper()
	s, p, _, now := actionFixture(t)
	c := peer(t)
	endpoint := c.LocalAddr().(*net.UDPAddr).AddrPort()
	clientSession, err := session.New(now)
	if err != nil || clientSession.Hello(clientSession.Token, endpoint, 1, now) != nil {
		t.Fatal("session")
	}
	p.session = clientSession
	s.sessions = map[uint64]*reservation{clientSession.ID: p}
	if s.udp, err = net.ListenUDP("udp", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)}); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() { s.sendUDP(ctx); close(done) }()
	t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(2 * time.Second):
			t.Error("send loop did not stop")
		}
		_ = s.udp.Close()
	})
	return s, p, c, endpoint, cancel, done
}

type resultArrival struct {
	at      time.Time
	results *client.ActionResults
}

// Result packets reaching the Client until the deadline, or until one that
// stop accepts.
func resultArrivals(t *testing.T, c *net.UDPConn, until time.Time, stop func(*client.ActionResults) bool) []resultArrival {
	t.Helper()
	var out []resultArrival
	buf := make([]byte, framing.MaxDatagram+1)
	for {
		_ = c.SetReadDeadline(until)
		n, _, err := c.ReadFromUDP(buf)
		if err != nil {
			return out
		}
		h, payload, err := framing.DecodeDatagram(buf[:n])
		var results client.ActionResults
		if err == nil && h.Type == adapter.ActionResults && proto.Unmarshal(payload, &results) == nil {
			out = append(out, resultArrival{time.Now(), &results})
			if stop != nil && stop(&results) {
				return out
			}
		}
	}
}

// G8 (real time): a new decision after an idle interval wakes the loop at
// once (an idle timer alone would take up to an interval), and 1 kHz of
// spurious wakes neither adds resends nor brings them closer.
func TestSendLoopWakesForNewResultsAndIgnoresSpuriousWakes(t *testing.T) {
	s, p, c, endpoint, _, _ := resultLoopFixture(t)
	rng := rand.New(rand.NewPCG(8, 8))
	seq := uint32(1)
	clientBatch := func(ack uint64, ids ...uint64) {
		b := &client.ActionBatch{AcknowledgedThrough: ack}
		for _, id := range ids {
			b.Shots = append(b.Shots, &client.ShotRequest{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: id, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)})
		}
		seq++
		deliverActions(t, s, p, endpoint, seq, b, time.Now())
	}
	slow := 0
	for id := uint64(1); id <= 16; id++ {
		// Idle for more than an interval, at a random phase of the idle timer.
		time.Sleep(40*time.Millisecond + time.Duration(rng.IntN(34_000))*time.Microsecond)
		clientBatch(id-1, id)
		started := time.Now()
		if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, RetiredThrough: id - 1, Decisions: []*runtime.ShotDecision{decision(id)}}); err != nil {
			t.Fatal(err)
		}
		decided := func(r *client.ActionResults) bool { return len(r.Decisions) == 1 && r.Decisions[0].ActionId == id }
		if got := resultArrivals(t, c, started.Add(10*time.Millisecond), decided); len(got) == 0 || !decided(got[len(got)-1].results) {
			slow++
			if got = resultArrivals(t, c, time.Now().Add(time.Second), decided); len(got) == 0 || !decided(got[len(got)-1].results) {
				t.Fatalf("decision %d never sent", id)
			}
		}
		clientBatch(id)
		if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, RetiredThrough: id}); err != nil {
			t.Fatal(err)
		}
		retired := func(r *client.ActionResults) bool { return r.RetiredThrough == id }
		if got := resultArrivals(t, c, time.Now().Add(time.Second), retired); len(got) == 0 || !retired(got[len(got)-1].results) {
			t.Fatalf("retirement %d never sent", id)
		}
	}
	// One slow pass of 16 is tolerated for a loaded host; a loop the wake does
	// not reach is slower than 10 ms in about 70% of passes.
	if slow > 1 {
		t.Fatalf("%d of 16 new decisions waited more than 10 ms for the send loop", slow)
	}
	// An unacknowledged decision resends at the interval under spurious wakes.
	clientBatch(16, 17)
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, RetiredThrough: 16, Decisions: []*runtime.ShotDecision{decision(17)}}); err != nil {
		t.Fatal(err)
	}
	stop := make(chan struct{})
	go func() {
		ticker := time.NewTicker(time.Millisecond)
		defer ticker.Stop()
		for {
			select {
			case <-stop:
				return
			case <-ticker.C:
				s.wakeResults()
			}
		}
	}()
	got := resultArrivals(t, c, time.Now().Add(250*time.Millisecond), nil)
	close(stop)
	// 250 ms at 30 Hz is about 7 resends; one per wake would be about 250.
	if len(got) < 5 || len(got) > 9 {
		t.Fatalf("%d results in 250 ms of spurious wakes", len(got))
	}
	for i := 1; i < len(got); i++ {
		if gap := got[i].at.Sub(got[i-1].at); gap < actionSendInterval-3*time.Millisecond {
			t.Fatalf("results %v apart under spurious wakes", gap)
		}
	}
}

// G8 (real time): a decision that arrives 20 ms after a write wakes the loop
// before its deadline, and the loop then waits only until that deadline (the
// write + I), not a fixed interval from the wake (the write + 20 ms + I).
func TestSendLoopSendsADecisionWithinAnIntervalAtItsDeadline(t *testing.T) {
	s, p, c, endpoint, _, _ := resultLoopFixture(t)
	seq := uint32(1)
	clientBatch := func(ack uint64, ids ...uint64) {
		b := &client.ActionBatch{AcknowledgedThrough: ack}
		for _, id := range ids {
			b.Shots = append(b.Shots, &client.ShotRequest{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: id, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)})
		}
		seq++
		deliverActions(t, s, p, endpoint, seq, b, time.Now())
	}
	arrival := func(id uint64) time.Time {
		decided := func(r *client.ActionResults) bool {
			for _, d := range r.Decisions {
				if d.ActionId == id {
					return true
				}
			}
			return false
		}
		got := resultArrivals(t, c, time.Now().Add(time.Second), decided)
		if len(got) == 0 || !decided(got[len(got)-1].results) {
			t.Fatalf("decision %d never sent", id)
		}
		return got[len(got)-1].at
	}
	clientBatch(0, 1)
	if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}}); err != nil {
		t.Fatal(err)
	}
	previous := arrival(1)
	slow := 0
	for id := uint64(1); id <= 16; id++ {
		clientBatch(id)
		time.Sleep(time.Until(previous.Add(20 * time.Millisecond)))
		clientBatch(id, id+1)
		if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, RetiredThrough: id, Decisions: []*runtime.ShotDecision{decision(id + 1)}}); err != nil {
			t.Fatal(err)
		}
		next := arrival(id + 1)
		if next.Sub(previous) > actionSendInterval+10*time.Millisecond {
			slow++
		}
		previous = next
	}
	// One slow pass of 16 is tolerated for a loaded host; a loop that waits a
	// fixed interval from the wake sends each about 53 ms after the last.
	if slow > 1 {
		t.Fatalf("%d of 16 decisions 20 ms after a write waited more than 10 ms past the deadline", slow)
	}
}

// G9: a player removed (Leave or eviction) while the loop's timer waits for
// its resend gets no result, and no deadline is left for the loop to spin on;
// cancelling the context ends the loop while its timer waits.
func TestSendLoopStopsResultsOfARemovedPlayerAndEndsOnCancel(t *testing.T) {
	for _, removal := range []string{"leave", "evict"} {
		t.Run(removal, func(t *testing.T) {
			s, p, c, endpoint, cancel, done := resultLoopFixture(t)
			deliverActions(t, s, p, endpoint, 2, &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}}, time.Now())
			if err := s.receiveActionResults(&runtime.ActionResults{PlayerId: 7, Decisions: []*runtime.ShotDecision{decision(1)}}); err != nil {
				t.Fatal(err)
			}
			if got := resultArrivals(t, c, time.Now().Add(50*time.Millisecond), nil); len(got) == 0 {
				t.Fatal("no result before the removal")
			}
			if removal == "leave" {
				// leaveRoom's effect, under the Server's mutex.
				e := envelope()
				e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 7}}
				s.mu.Lock()
				if err := s.link.control(e); err != nil {
					t.Fatal(err)
				}
				s.remove(p)
				s.mu.Unlock()
				// A result selected just before may still be in flight.
				resultArrivals(t, c, time.Now().Add(5*time.Millisecond), nil)
			} else {
				e := envelope()
				e.Message = &runtime.RuntimeEnvelope_Evicted{Evicted: &runtime.PlayerEvicted{PlayerId: 7, Reason: runtime.EvictionReason_EVICTION_HIGH_LATENCY}}
				s.runtimeMessage(e)
				receivePacket(t, c, adapter.Failure)
			}
			if _, pending := s.resultDeadline(); pending {
				t.Fatal("removed player keeps a result deadline")
			}
			if got := resultArrivals(t, c, time.Now().Add(150*time.Millisecond), nil); len(got) != 0 {
				t.Fatalf("%d results after the removal", len(got))
			}
			cancel()
			select {
			case <-done:
			case <-time.After(500 * time.Millisecond):
				t.Fatal("send loop did not end on cancel")
			}
		})
	}
}

// G9: a runtime failure while the loop's timer waits for a resend ends the
// player's results, and no deadline is left for the loop to spin on.
func TestRuntimeFailureStopsPendingResults(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "failure"), peer(t)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c)
	receivePacket(t, p, adapter.Welcome)
	sendPacket(t, p, c, 2, adapter.Actions, &client.ActionBatch{Shots: []*client.ShotRequest{{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: 1, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)}}})
	for f.next(t).GetActions() == nil {
	}
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_ActionResults{ActionResults: &runtime.ActionResults{PlayerId: c.PlayerID, Decisions: []*runtime.ShotDecision{decision(1)}}}
	f.send(e)
	receivePacket(t, p, adapter.ActionResults)
	f.mu.Lock()
	_ = f.conn.Close()
	f.mu.Unlock()
	receivePacket(t, p, adapter.Failure)
	if _, pending := s.resultDeadline(); pending {
		t.Fatal("failed runtime leaves a result deadline")
	}
	if got := resultArrivals(t, p, time.Now().Add(150*time.Millisecond), nil); len(got) != 0 {
		t.Fatalf("%d results after the runtime failure", len(got))
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
