package gateway

import (
	"errors"
	"math"
	"net/netip"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv7"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

func totalsOf(r *runtime.GatewayDropReport) map[runtime.DropReason]uint64 {
	out := map[runtime.DropReason]uint64{}
	for _, c := range r.Counts {
		out[c.Reason] = c.Total
	}
	return out
}

// pv7 §4: counts are cumulative and never go back; a report goes only when
// something changed; at most 4 samples, the rest counted as omitted.
func TestDropLedgerIsCumulativeAndBounded(t *testing.T) {
	d := newDropLedger()
	now := time.Now()
	for i := 0; i < 6; i++ {
		d.add(7, 2, dropEvent{reason: runtime.DropReason_DROP_MALFORMED_INPUT_PACKETS, at: now, transport: uint32(i + 1)})
	}
	d.add(7, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: now})
	r := d.report(7, now)
	if r == nil || r.ReportSequence != 1 || r.Final || len(r.Samples) != 4 || r.SamplesOmitted != 3 {
		t.Fatalf("first report %v", r)
	}
	if r.Counts[0].Reason != runtime.DropReason_DROP_MALFORMED_INPUT_PACKETS || r.Counts[0].Total != 12 ||
		r.Counts[1].Reason != runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS || r.Counts[1].Total != 1 {
		t.Fatalf("counts %v", r.Counts)
	}
	if r.Samples[0].TransportSequence != 1 || r.Samples[0].ItemCount != 2 {
		t.Fatalf("first sample %v", r.Samples[0])
	}
	if again := d.report(7, now); again != nil {
		t.Fatalf("a report without a change %v", again)
	}
	d.add(7, 1, dropEvent{reason: runtime.DropReason_DROP_MALFORMED_INPUT_PACKETS, at: now})
	second := d.report(7, now)
	if second == nil || second.ReportSequence != 2 || totalsOf(second)[runtime.DropReason_DROP_MALFORMED_INPUT_PACKETS] != 13 ||
		totalsOf(second)[runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS] != 1 || len(second.Samples) != 1 || second.SamplesOmitted != 3 {
		t.Fatalf("second report %v", second)
	}
}

// A removed player's stream sends exactly one final, then nothing; a stream
// that never counted anything sends none.
func TestDropLedgerFinalIsSentOnce(t *testing.T) {
	d := newDropLedger()
	d.add(3, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_ACTIONS_PACKETS, at: time.Now()})
	d.remove(3)
	d.remove(4) // never counted
	if due := d.due(false); len(due) != 1 || due[0] != 3 {
		t.Fatalf("due %v", due)
	}
	d.add(3, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_ACTIONS_PACKETS, at: time.Now()}) // after Leave, before the final
	final := d.report(3, time.Now())
	if final == nil || !final.Final || final.Counts[0].Total != 2 {
		t.Fatalf("final %v", final)
	}
	d.add(3, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_ACTIONS_PACKETS, at: time.Now()})
	d.add(4, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_ACTIONS_PACKETS, at: time.Now()})
	if due := d.due(false); len(due) != 0 || d.report(3, time.Now()) != nil {
		t.Fatalf("a stream reported after its final: %v", due)
	}
}

// 1000 joins and leaves cannot raise the reports per pass above the bound; the
// finals past the waiting room are merged into the Gateway stream's count, and
// nothing is lost: every stream ends with a final or in the merged count.
func TestDropLedgerPassesAreBoundedUnderChurn(t *testing.T) {
	d := newDropLedger()
	finals, merged := 0, uint64(0)
	settle := func(id uint64) {
		r := d.report(id, time.Now())
		switch {
		case r == nil:
		case r.PlayerId == 0:
			merged = r.MergedStreams
		case r.Final:
			finals++
		}
	}
	for player := uint64(1); player <= 1000; player++ {
		d.add(player, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: time.Now()})
		d.remove(player)
		if player%10 == 0 { // a timer pass now and then
			due := d.due(false)
			if len(due) > adapter.MaxPlayers*2 { // the contract's bound, not the constant

				t.Fatalf("pass of %d streams", len(due))
			}
			for _, id := range due {
				settle(id)
			}
		}
	}
	for due := d.due(false); len(due) > 0; due = d.due(false) {
		for _, id := range due {
			settle(id)
		}
	}
	if uint64(finals)+merged != 1000 || merged == 0 {
		t.Fatalf("%d finals and %d merged of 1000 streams", finals, merged)
	}
}

// The largest report (every reason at its maximum, four maximal samples) fits
// one IPC frame.
func TestLargestDropReportFitsAFrame(t *testing.T) {
	r := &runtime.GatewayDropReport{PlayerId: math.MaxUint64, ReportSequence: math.MaxUint64, Final: true,
		SamplesOmitted: math.MaxUint64, MergedStreams: math.MaxUint64}
	for _, entry := range dropReasons {
		r.Counts = append(r.Counts, &runtime.DropCount{Reason: entry.reason, Total: math.MaxUint64})
	}
	for i := 0; i < dropSamplesPerReport; i++ {
		r.Samples = append(r.Samples, &runtime.DropSample{Reason: runtime.DropReason_DROP_COALESCED_SNAPSHOTS, DwellUs: math.MaxUint32,
			TransportSequence: math.MaxUint32, MovementEpoch: math.MaxUint64, LifeGeneration: math.MaxUint64,
			FirstCommandSequence: math.MaxUint64, LastCommandSequence: math.MaxUint64, FirstActionId: math.MaxUint64,
			LastActionId: math.MaxUint64, SnapshotTick: math.MaxUint64, ItemCount: math.MaxUint32})
	}
	size := proto.Size(&runtime.RuntimeEnvelope{ProtocolVersion: math.MaxUint32, Message: &runtime.RuntimeEnvelope_DropReport{DropReport: r}})
	if size > framing.MaxFrame {
		t.Fatalf("largest report %d B > %d", size, framing.MaxFrame)
	}
}

// Each reported counter reaches the ledger from the entry that counts its 10 s
// line: exactly once, under its reason, in the next report.
func TestEveryReportedCounterReachesTheNextReport(t *testing.T) {
	for counter, reason := range playerDropReason {
		if reason == 0 {
			continue
		}
		s := &Server{link: &runtimeLink{drops: newDropLedger()}}
		s.countIngressAt(9, playerIngressCounter(counter), 1, dropEvent{at: time.Now()})
		r := s.link.drops.report(9, time.Now())
		if s.playerIngress[9][counter] != 1 || r == nil || len(r.Counts) != 1 || r.Counts[0].Reason != reason || r.Counts[0].Total != 1 {
			t.Fatalf("%s: line %d, report %v", ingressKeyName(playerIngressKeys[counter]), s.playerIngress[9][counter], r)
		}
	}
	for counter, reason := range gatewayDropReason {
		if reason == 0 {
			continue
		}
		s := &Server{link: &runtimeLink{drops: newDropLedger()}}
		s.countGatewayLocked(gatewayIngressCounter(counter), dropEvent{at: time.Now()})
		r := s.link.drops.report(0, time.Now())
		if s.ingress[counter] != 1 || r == nil || len(r.Counts) != 1 || r.Counts[0].Reason != reason {
			t.Fatalf("%s: line %d, report %v", ingressKeyName(gatewayIngressKeys[counter]), s.ingress[counter], r)
		}
	}
	l := &runtimeLink{drops: newDropLedger()}
	l.countLocked(5, playerDropResolvedTrimCommands, 3)
	l.countLocked(5, playerDropClearedOnCloseCommands, 2) // logged only
	if r := l.drops.report(5, time.Now()); r == nil || len(r.Counts) != 1 || r.Counts[0].Total != 3 {
		t.Fatalf("link counters: %v", r)
	}
}

// A sample's dwell ends when its report is written: with the first report's
// write blocked 500 ms, the second report's samples waited at least as long.
func TestDropSampleDwellEndsAtItsWrite(t *testing.T) {
	link, r := linkToScriptedRuntime(t)
	var first atomic.Bool
	first.Store(true)
	write := runtimeWrite
	replaceRuntimeWrite(t, func(c *framing.TCPConnection, payload []byte, timeout time.Duration) error {
		if first.CompareAndSwap(true, false) {
			time.Sleep(500 * time.Millisecond)
		}
		return write(c, payload, timeout)
	})
	at := time.Now()
	link.drops.add(1, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: at})
	link.drops.add(2, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: at})
	if err := link.writeDrops(false); err != nil {
		t.Fatal(err)
	}
	deadline := time.Now().Add(2 * time.Second)
	for {
		r.mu.Lock()
		var reports []*runtime.GatewayDropReport
		for _, e := range r.got {
			if d := e.GetDropReport(); d != nil {
				reports = append(reports, d)
			}
		}
		r.mu.Unlock()
		if len(reports) == 2 {
			if reports[1].PlayerId != 2 || reports[1].Samples[0].DwellUs < 500_000 {
				t.Fatalf("second report %v", reports[1])
			}
			return
		}
		if time.Now().After(deadline) {
			t.Fatalf("%d reports", len(reports))
		}
		time.Sleep(5 * time.Millisecond)
	}
}

// D5: a failed UDP write to a player is that player's discard, with the
// packet's Gateway sequence; D6: a full control queue drops the control.
func TestDownlinkDiscardsAreCounted(t *testing.T) {
	setForTest(t, &udpWrite, func(*Server, []byte, netip.AddrPort) error { return errors.New("injected UDP failure") })
	s, _ := newTestServer(t)
	endpoint := netip.MustParseAddrPort("127.0.0.1:9")
	s.writeUDP(outbound{peer: endpoint, packet: []byte{1}, player: 4, writeFailed: playerDropWriteFailedResultsPackets, sequence: 17})
	s.writeUDP(outbound{peer: endpoint, packet: []byte{1}}) // nobody's: not counted
	r := s.link.drops.report(4, time.Now())
	if r == nil || totalsOf(r)[runtime.DropReason_DROP_WRITE_FAILED_RESULTS_PACKETS] != 1 || r.Samples[0].TransportSequence != 17 {
		t.Fatalf("write failure report %v", r)
	}
	// No send loop drains this Server's control queue.
	quiet := &Server{controlOut: make(chan outbound, 2), link: &runtimeLink{drops: newDropLedger()}}
	p := &reservation{playerID: 6, session: newActiveSession(t, 6), phase: active}
	quiet.mu.Lock()
	for i := 0; i < cap(quiet.controlOut)+3; i++ {
		quiet.sendControl(p, adapter.Failure, &client.Error{Code: "test"})
	}
	quiet.mu.Unlock()
	r = quiet.link.drops.report(6, time.Now())
	if r == nil || totalsOf(r)[runtime.DropReason_DROP_CONTROL_QUEUE_FULL_CONTROL_PACKETS] != 3 || r.Samples[0].TransportSequence != 3 {
		t.Fatalf("control queue report %v", r)
	}
}

// A newer snapshot that replaces the one the send loop is serving: the player
// already written is not counted, each later active player once (31), and the
// reader-side count (45) does not move.
func TestReplacedSnapshotIsCountedForThePlayersStillToServe(t *testing.T) {
	s := &Server{available: true, players: make(map[uint64]*reservation), snapshotOut: make(chan snapshotFrame, 1),
		link: &runtimeLink{drops: newDropLedger()}}
	for id := uint64(1); id <= 4; id++ {
		peer := newActiveSession(t, id)
		s.players[id] = &reservation{lifeGeneration: 1, playerID: id, session: peer, phase: active}
	}
	frame := snapshotFrame{payload: []byte{1}, tick: 5}
	players := []uint64{1, 2, 3, 4}
	frame, _ = s.snapshotForPeer(players, frame) // player 1 gets tick 5
	s.snapshotOut <- snapshotFrame{payload: []byte{2}, tick: 6}
	for i := 1; i < len(players); i++ {
		frame, _ = s.snapshotForPeer(players[i:], frame)
	}
	if frame.tick != 6 {
		t.Fatalf("served tick %d", frame.tick)
	}
	if r := s.link.drops.report(1, time.Now()); r != nil {
		t.Fatalf("player 1 got the replaced snapshot but counted %v", r)
	}
	for id := uint64(2); id <= 4; id++ {
		r := s.link.drops.report(id, time.Now())
		if r == nil || totalsOf(r)[runtime.DropReason_DROP_REPLACED_SNAPSHOT_PACKETS] != 1 || r.Samples[0].SnapshotTick != 5 {
			t.Fatalf("player %d: %v", id, r)
		}
	}
	if s.ingress[gatewayDropCoalescedSnapshots] != 0 {
		t.Fatal("the send loop's replacement counted as the reader's")
	}
}

// The reader replacing an unsent snapshot counts 45 with the replaced tick.
func TestCoalescedSnapshotIsCountedAtTheReader(t *testing.T) {
	s := &Server{available: true, ready: &runtime.Ready{JumpHeight: .6, Gravity: 18, CombatRules: testRules()},
		players: make(map[uint64]*reservation), snapshotOut: make(chan snapshotFrame, 1), link: &runtimeLink{drops: newDropLedger()}}
	for tick := uint64(1); tick <= 3; tick++ {
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick}}
		s.runtimeMessage(e)
	}
	r := s.link.drops.report(0, time.Now())
	if r == nil || totalsOf(r)[runtime.DropReason_DROP_COALESCED_SNAPSHOTS] != 2 || r.Samples[0].SnapshotTick != 1 || r.Samples[1].SnapshotTick != 2 {
		t.Fatalf("coalesced report %v", r)
	}
}

// pv7 §4: on a graceful close every stream with content sends its last
// report, as final, the Gateway's included; and each written report is logged.
func TestGracefulCloseSendsEveryFinal(t *testing.T) {
	logs := captureIngressLog(t)
	s, f, cancel, _ := startTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "close"), peer(t)
	sendPacket(t, p, c, 1, adapter.Input, &client.PlayerInput{}) // inactive: the player's stream
	stranger := c
	stranger.SessionID += 1000
	sendPacket(t, p, stranger, 1, adapter.Hello, &client.Hello{}) // no session: the Gateway's
	deadline := time.Now().Add(2 * time.Second)
	for {
		s.mu.Lock()
		counted := s.playerIngress[c.PlayerID] != nil && s.playerIngress[c.PlayerID][playerDropInactiveInputPackets] == 1 &&
			s.ingress[gatewayDropUnknownSessionDatagrams] == 1
		s.mu.Unlock()
		if counted {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("drops not counted")
		}
		time.Sleep(5 * time.Millisecond)
	}
	cancel()
	s.Close()
	finals := map[uint64]bool{}
	for deadline := time.Now().Add(2 * time.Second); len(finals) < 2 && time.Now().Before(deadline); {
		select {
		case r := <-f.drops:
			if r.Final {
				finals[r.PlayerId] = true
			}
		case <-time.After(50 * time.Millisecond):
		}
	}
	if !finals[0] || !finals[c.PlayerID] {
		t.Fatalf("finals %v", finals)
	}
	logs.mu.Lock()
	text := logs.buf.String()
	logs.mu.Unlock()
	if !strings.Contains(text, "drop report player=0 ") || !strings.Contains(text, " final=1 ") {
		t.Fatalf("no report log in\n%s", text)
	}
}

func newActiveSession(t *testing.T, id uint64) *session.Session {
	t.Helper()
	peer, err := session.New(time.Now())
	if err != nil {
		t.Fatal(err)
	}
	if err := peer.Hello(peer.Token, netip.AddrPortFrom(netip.MustParseAddr("127.0.0.1"), uint16(29100+id)), 1, time.Now()); err != nil {
		t.Fatal(err)
	}
	return peer
}

// Counts go in reason order whatever order they came in.
func TestDropReportCountsAreInReasonOrder(t *testing.T) {
	d := newDropLedger()
	for _, reason := range []runtime.DropReason{31, 14, 27, 1, 30, 9, 22} {
		d.add(2, 1, dropEvent{reason: reason, at: time.Now()})
	}
	r := d.report(2, time.Now())
	for i := 1; i < len(r.Counts); i++ {
		if r.Counts[i-1].Reason >= r.Counts[i].Reason {
			t.Fatalf("counts out of order: %v", r.Counts)
		}
	}
	if len(r.Counts) != 7 {
		t.Fatalf("counts %v", r.Counts)
	}
}

// The report log line; the Match logs the same body for what it read
// (IpcHostTests "PvP IPC logs the Gateway's drop reports ..." pins the same text).
func TestDropReportLineIsPinned(t *testing.T) {
	r := &runtime.GatewayDropReport{PlayerId: 3, ReportSequence: 1, Samples: []*runtime.DropSample{{Reason: 14}},
		Counts: []*runtime.DropCount{{Reason: 14, Total: 2}}}
	if got := dropReportLine(r); got != "player=3 report_sequence=1 final=0 samples=1 samples_omitted=0 merged_streams=0 drop_inactive_input_packets=2" {
		t.Fatalf("line %q", got)
	}
	r.Final, r.MergedStreams, r.SamplesOmitted = true, 2, 5
	if got := dropReportLine(r); !strings.Contains(got, " final=1 samples=1 samples_omitted=5 merged_streams=2 ") {
		t.Fatalf("line %q", got)
	}
}

// A stream reports at most once a second (pv7 §4): a pass that comes too soon
// after the previous one waits for the second to end.
func TestDropPassesAreAtMostOncePerSecond(t *testing.T) {
	link, r := linkToScriptedRuntime(t)
	reports := func() int {
		r.mu.Lock()
		defer r.mu.Unlock()
		n := 0
		for _, e := range r.got {
			if e.GetDropReport() != nil {
				n++
			}
		}
		return n
	}
	pass := func() {
		link.mu.Lock()
		link.dropPass = true
		link.mu.Unlock()
		if err := link.writeDiagnostic(); err != nil {
			t.Fatal(err)
		}
	}
	link.drops.add(1, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: time.Now()})
	pass()
	first := time.Now()
	link.drops.add(1, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: time.Now()})
	pass() // too soon: deferred, and the writer is woken when the second ends
	select {
	case <-link.diagnosticWake:
	case <-time.After(2 * time.Second):
		t.Fatal("the deferred pass was never woken")
	}
	if elapsed := time.Since(first); elapsed < time.Second-10*time.Millisecond {
		t.Fatalf("woken after %v", elapsed)
	}
	if err := link.writeDiagnostic(); err != nil { // the pass kept its request
		t.Fatal(err)
	}
	deadline := time.Now().Add(2 * time.Second)
	for reports() < 2 && time.Now().Before(deadline) {
		time.Sleep(5 * time.Millisecond)
	}
	if n := reports(); n != 2 {
		t.Fatalf("%d reports", n)
	}
}

// On a running Gateway: a changed stream reports once at the next timer pass,
// an unchanged one never again, and a Leave ends it with exactly one final.
func TestRunningGatewayReportsChangesAndOneFinal(t *testing.T) {
	setForTest(t, &diagnosticInterval, 100*time.Millisecond)
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "running"), peer(t)
	sendPacket(t, p, c, 1, adapter.Input, &client.PlayerInput{}) // inactive
	collect := func(window time.Duration) []*runtime.GatewayDropReport {
		var out []*runtime.GatewayDropReport
		for deadline := time.Now().Add(window); time.Now().Before(deadline); {
			select {
			case r := <-f.drops:
				if r.PlayerId == c.PlayerID {
					out = append(out, r)
				}
			case <-time.After(20 * time.Millisecond):
			}
		}
		return out
	}
	if got := collect(1500 * time.Millisecond); len(got) != 1 || got[0].Final || got[0].Samples[0].TransportSequence != 1 {
		t.Fatalf("reports after one drop: %v", got)
	}
	if status, body := post(t, s, "/rooms/1/leave", map[string]any{"session_id": c.SessionID, "session_token": c.Token}); status != 200 {
		t.Fatalf("leave %d %s", status, body)
	}
	if got := collect(2500 * time.Millisecond); len(got) != 1 || !got[0].Final || got[0].ReportSequence != 2 {
		t.Fatalf("reports after the leave: %v", got)
	}
}

// The graceful close's finals take every stream with content: one reported
// before and unchanged since, one removed and waiting for its final, and the
// Gateway's.
func TestClosingFinalsTakeReportedAndWaitingStreams(t *testing.T) {
	d := newDropLedger()
	d.add(1, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: time.Now()})
	d.add(0, 1, dropEvent{reason: runtime.DropReason_DROP_UNKNOWN_SESSION_DATAGRAMS, at: time.Now()})
	for _, id := range d.due(false) {
		d.report(id, time.Now())
	}
	d.add(2, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: time.Now()})
	d.remove(2)
	finals := map[uint64]bool{}
	for _, id := range d.due(true) {
		if r := d.report(id, time.Now()); r != nil && r.Final {
			finals[id] = true
		}
	}
	if !finals[0] || !finals[1] || !finals[2] || len(finals) != 3 {
		t.Fatalf("finals %v", finals)
	}
	if d.due(false) != nil {
		t.Fatal("a pass after the close")
	}
}

// Close without the context cancelled first sends the finals too.
func TestCloseWithoutCancelSendsTheFinals(t *testing.T) {
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "close-only"), peer(t)
	sendPacket(t, p, c, 1, adapter.Input, &client.PlayerInput{})
	for deadline := time.Now().Add(2 * time.Second); ; time.Sleep(5 * time.Millisecond) {
		s.mu.Lock()
		counted := s.playerIngress[c.PlayerID] != nil && s.playerIngress[c.PlayerID][playerDropInactiveInputPackets] == 1
		s.mu.Unlock()
		if counted {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("drop not counted")
		}
	}
	s.Close()
	for deadline := time.Now().Add(2 * time.Second); time.Now().Before(deadline); {
		select {
		case r := <-f.drops:
			if r.PlayerId == c.PlayerID && r.Final {
				return
			}
		case <-time.After(20 * time.Millisecond):
		}
	}
	t.Fatal("no final for the player")
}

// The production downlink builders carry their discard: a failed Welcome
// write is 29 with its Gateway sequence, a failed snapshot write 27 with its tick.
func TestProductionDownlinkCarriesItsDiscard(t *testing.T) {
	previous := udpWrite
	var failing atomic.Bool
	setForTest(t, &udpWrite, func(s *Server, packet []byte, peer netip.AddrPort) error {
		if failing.Load() {
			return errors.New("injected UDP failure")
		}
		return previous(s, packet, peer)
	})
	setForTest(t, &diagnosticInterval, time.Hour) // nothing takes the ledger's reports meanwhile
	s, f := newTestServer(t)
	post(t, s, "/rooms", map[string]any{})
	c, p := reserve(t, s, "down"), peer(t)
	failing.Store(true)
	sendPacket(t, p, c, 1, adapter.Hello, &client.Hello{SessionToken: c.Token})
	accept(t, f, c) // the Welcome's write fails
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: 41, Players: []*runtime.PlayerState{{LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, MovementEpoch: 1, PlayerId: c.PlayerID}}}}
	f.send(e)
	want := map[runtime.DropReason]bool{runtime.DropReason_DROP_WRITE_FAILED_CONTROL_PACKETS: false, runtime.DropReason_DROP_WRITE_FAILED_SNAPSHOT_PACKETS: false}
	var samples []*runtime.DropSample
	for deadline := time.Now().Add(3 * time.Second); time.Now().Before(deadline); time.Sleep(10 * time.Millisecond) {
		if r := s.link.drops.report(c.PlayerID, time.Now()); r != nil {
			samples = append(samples, r.Samples...)
			for reason := range totalsOf(r) {
				want[reason] = true
			}
		}
		if want[runtime.DropReason_DROP_WRITE_FAILED_CONTROL_PACKETS] && want[runtime.DropReason_DROP_WRITE_FAILED_SNAPSHOT_PACKETS] {
			break
		}
	}
	for reason, seen := range want {
		if !seen {
			t.Fatalf("%s not counted: %v", reason, samples)
		}
	}
	for _, sample := range samples {
		switch sample.Reason {
		case runtime.DropReason_DROP_WRITE_FAILED_CONTROL_PACKETS:
			if sample.TransportSequence != 1 {
				t.Fatalf("Welcome sample %v", sample)
			}
		case runtime.DropReason_DROP_WRITE_FAILED_SNAPSHOT_PACKETS:
			if sample.SnapshotTick != 41 || sample.TransportSequence == 0 {
				t.Fatalf("snapshot sample %v", sample)
			}
		}
	}
}

// An uplink drop's sample carries the datagram's GYOP sequence, and its dwell
// starts when the datagram was read.
func TestUplinkSampleCarriesTheDatagram(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now.Add(-300 * time.Millisecond)}
	p.phase = joining
	f.send(t, f.datagram(adapter.Input, 77, inputMessage(1, moves(1, 1)))) // inactive
	r := s.link.drops.report(p.playerID, time.Now())
	if r == nil || r.Samples[0].Reason != runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS || r.Samples[0].TransportSequence != 77 ||
		r.Samples[0].DwellUs < 300_000 {
		t.Fatalf("uplink sample %v", r)
	}
}

// A sample's dwell is clamped at 10 s (pv7 §2).
func TestDropSampleDwellIsClamped(t *testing.T) {
	d := newDropLedger()
	d.add(1, 1, dropEvent{reason: runtime.DropReason_DROP_INACTIVE_INPUT_PACKETS, at: time.Now().Add(-time.Minute)})
	if r := d.report(1, time.Now()); r.Samples[0].DwellUs != 10_000_000 {
		t.Fatalf("dwell %d", r.Samples[0].DwellUs)
	}
}
