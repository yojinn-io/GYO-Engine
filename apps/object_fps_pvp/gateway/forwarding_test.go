package gateway

import (
	"context"
	"fmt"
	"net"
	"net/netip"
	"slices"
	"sync"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// batch takes one write pass as if every Write succeeded: tests without a
// runtime connection stand in for the writer.
func (l *runtimeLink) batch() []*runtime.RuntimeEnvelope {
	writes := l.take()
	l.mu.Lock()
	defer l.mu.Unlock()
	batch := make([]*runtime.RuntimeEnvelope, 0, len(writes))
	for _, w := range writes {
		l.wroteLocked(w)
		batch = append(batch, w.message)
	}
	return batch
}

func moves(first, last uint32) []*client.MovementCommand {
	var out []*client.MovementCommand
	for sequence := first; sequence <= last; sequence++ {
		out = append(out, &client.MovementCommand{Sequence: sequence, MoveForward: 1})
	}
	return out
}

func sendInputAt(t *testing.T, s *Server, p *reservation, seq uint32, now time.Time, in *client.PlayerInput) {
	t.Helper()
	payload, _ := proto.Marshal(in)
	if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Input, SessionID: p.session.ID, Sequence: seq}, payload, p.session.Endpoint(), now); err != nil {
		t.Fatal(err)
	}
}

func sendMoves(t *testing.T, s *Server, p *reservation, seq uint32, epoch uint32, commands []*client.MovementCommand) {
	t.Helper()
	sendInputAt(t, s, p, seq, time.Now(), &client.PlayerInput{MovementEpoch: epoch, LifeGeneration: 1, Commands: commands})
}

// sequences takes one write pass and lists the input commands in write order.
func sequences(l *runtimeLink) (out []uint32) {
	for _, e := range l.batch() {
		if in := e.GetInput(); in != nil {
			for _, command := range in.Commands {
				out = append(out, command.Sequence)
			}
		}
	}
	return out
}

func resolve(s *Server, tick, through uint32) {
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick,
		Players: []*runtime.PlayerState{{PlayerId: 7, MovementEpoch: 1, LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, LastResolvedCommand: through}},
		Combat:  []*runtime.CombatState{{PlayerId: 7, LifeGeneration: 1, Hp: 100000, MagazineAmmo: 12}}}}
	s.runtimeMessage(e)
}

// linkTotals accumulates what takeIngress hands over, per player.
type linkTotals map[uint32]*playerIngressCounts

func (totals linkTotals) take(l *runtimeLink) linkTotals {
	for player, counts := range l.takeIngress() {
		if totals[player] == nil {
			totals[player] = new(playerIngressCounts)
		}
		for i, n := range counts {
			totals[player][i] += n
		}
	}
	return totals
}

func (totals linkTotals) of(player uint32) playerIngressCounts {
	if counts := totals[player]; counts != nil {
		return *counts
	}
	return playerIngressCounts{}
}

// i0 returns the handed commands and what accounts for them; they are equal
// when every handed command was written or counted as removed.
func i0(c playerIngressCounts) (handed, accounted uint64) {
	handed = c[playerFwdHandedMainCommands] + c[playerFwdHandedRejectedLaneCommands]
	for _, term := range []playerIngressCounter{playerFwdLinkWrittenMainCommands, playerFwdLinkWrittenRejectedLaneCommands,
		playerFwdMergedDuplicateCommands, playerFwdWrittenDuplicateCommands, playerDropResolvedTrimCommands,
		playerDropRejectedLaneOverflowCommands, playerDropDiscardedOnLeaveCommands, playerDropClearedOnCloseCommands,
		playerDropAbandonedOnCloseCommands} {
		accounted += c[term]
	}
	return handed, accounted
}

// dialedLink is a link with a real runtime connection whose peer reads and
// discards every frame, so close() and write() run as in production.
func dialedLink(t *testing.T) *runtimeLink {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = listener.Close() })
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			return
		}
		defer conn.Close()
		for {
			if _, err := framing.ReadFrame(conn); err != nil {
				return
			}
		}
	}()
	conn, err := framing.DialTCP(context.Background(), listener.Addr().String(), time.Second)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = conn.Close() })
	return &runtimeLink{conn: conn, wake: make(chan struct{}, 1), done: make(chan struct{})}
}

func linkWindow(player, epoch, first, last uint32) *runtime.PlayerInput {
	in := &runtime.PlayerInput{LifeGeneration: 1, MovementEpoch: epoch, PlayerId: player}
	for sequence := first; sequence <= last; sequence++ {
		in.Commands = append(in.Commands, &runtime.MovementCommand{Sequence: sequence, MoveForward: 1})
	}
	return in
}

// pv7 §2a: each Client datagram's window reaches the Match as one IPC
// message, exactly as decoded: resolved commands, copies, another epoch or
// life, a far future or a conflicting command alike. The Match decides (D49).
func TestGatewayForwardsEachInputDatagramAsItCame(t *testing.T) {
	s, p, _, now := actionFixture(t)
	conflict := moves(2, 3)
	conflict[0].MoveForward = -1
	sent := []*client.PlayerInput{
		{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(1, 3), ObservedAuthorityTick: 4},
		{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(1, 3), ObservedAuthorityTick: 5}, // a copy
		{MovementEpoch: 1, LifeGeneration: 1, Commands: conflict},
		{MovementEpoch: 2, LifeGeneration: 1, Commands: moves(1, 1)},
		{MovementEpoch: 1, LifeGeneration: 2, Commands: moves(1, 1)},
		{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(500, 511)},
	}
	resolve(s, 3, 3) // the Match resolved 1..3: no Gateway cursor drops or trims them
	for k, in := range sent {
		sendInputAt(t, s, p, uint32(k+2), now.Add(time.Duration(k)*time.Millisecond), in)
	}
	batch := s.link.batch()
	if len(batch) != len(sent) {
		t.Fatalf("%d IPC messages for %d datagrams", len(batch), len(sent))
	}
	for k, e := range batch {
		got, want := e.GetInput(), sent[k]
		if got == nil || got.PlayerId != 7 || got.MovementEpoch != want.MovementEpoch || got.LifeGeneration != want.LifeGeneration ||
			got.ObservedAuthorityTick != want.ObservedAuthorityTick || len(got.Commands) != len(want.Commands) {
			t.Fatalf("message %d is not datagram %d: %v", k, k, got)
		}
		for i, command := range got.Commands {
			if command.Sequence != want.Commands[i].Sequence || command.MoveForward != want.Commands[i].MoveForward {
				t.Fatalf("message %d command %d changed: %v", k, i, command)
			}
		}
	}
	counts := s.playerIngress[7]
	if counts[playerFwdMainInputPackets] != uint64(len(sent)) || counts[playerFwdRejectedLaneInputPackets] != 0 ||
		counts[playerFwdWrittenDuplicateInputPackets] != 0 {
		t.Fatalf("datagram outcomes: %v", counts)
	}
}

// pv7 §2a (D-10): the link writes in arrival order, controls and inputs alike:
// a Leave never overtakes the player's queued inputs, and inputs queued
// before an eviction (forget, no Leave) still reach the Match.
func TestRuntimeLinkKeepsArrivalOrderAndALeaveDoesNotOvertake(t *testing.T) {
	l := dialedLink(t)
	leave := envelope()
	leave.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 1}}
	join := envelope()
	join.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: 3}}
	for _, step := range []func() error{
		func() error { return l.input(linkWindow(1, 1, 1, 1), dropEvent{}) },
		func() error { return l.control(join) },
		func() error { return l.input(linkWindow(2, 1, 1, 1), dropEvent{}) },
		func() error { return l.input(linkWindow(1, 1, 2, 2), dropEvent{}) },
		func() error { return l.control(leave) },
		func() error { return l.input(linkWindow(2, 1, 2, 2), dropEvent{}) },
	} {
		if err := step(); err != nil {
			t.Fatal(err)
		}
	}
	l.forget(2) // an eviction: no Leave, the queued inputs stay
	var order []string
	for _, e := range l.batch() {
		switch {
		case e.GetInput() != nil:
			order = append(order, fmt.Sprintf("input %d/%d", e.GetInput().PlayerId, e.GetInput().Commands[0].Sequence))
		case e.GetLeave() != nil:
			order = append(order, fmt.Sprintf("leave %d", e.GetLeave().PlayerId))
		case e.GetJoin() != nil:
			order = append(order, fmt.Sprintf("join %d", e.GetJoin().PlayerId))
		}
	}
	want := []string{"input 1/1", "join 3", "input 2/1", "input 1/2", "leave 1", "input 2/2"}
	if !slices.Equal(order, want) {
		t.Fatalf("written %v, want %v", order, want)
	}
}

// pv7 §2a: each player's queue holds maxQueuedInputs windows (not less than
// 64); beyond that the oldest is dropped, counted and reported with its
// identity (§4, reason 32). Another player's queue is untouched.
func TestRuntimeLinkInputQueueDropsTheOldestWindowAndReportsIt(t *testing.T) {
	if maxQueuedInputs < 64 {
		t.Fatalf("the input queue holds %d windows, fewer than 64", maxQueuedInputs)
	}
	l := dialedLink(t)
	l.drops = newDropLedger()
	start := time.Now()
	// Player 2's window waits first: player 1's overflow must not take it.
	if err := l.input(linkWindow(2, 1, 1, 1), dropEvent{}); err != nil {
		t.Fatal(err)
	}
	for k := uint32(1); k <= maxQueuedInputs+2; k++ {
		e := dropEvent{at: start.Add(time.Duration(k) * time.Millisecond), transport: uint32(100 + k), epoch: 1, life: 1, firstCommand: k, lastCommand: k, items: 1}
		if err := l.input(linkWindow(1, 1, k, k), e); err != nil {
			t.Fatal(err)
		}
	}
	totals := linkTotals{}.take(l)
	if got := totals.of(1)[playerDropIpcQueueOverflowInputPackets]; got != 2 {
		t.Fatalf("overflowed windows counted %d, want 2", got)
	}
	if got := totals.of(1)[playerDropIpcQueueOverflowInputCommands]; got != 2 {
		t.Fatalf("overflowed commands counted %d, want 2", got)
	}
	if got := totals.of(2)[playerDropIpcQueueOverflowInputPackets]; got != 0 {
		t.Fatalf("player 2 lost %d windows to player 1's overflow", got)
	}
	got := sequences(l)
	want := []uint32{1}
	for k := uint32(3); k <= maxQueuedInputs+2; k++ {
		want = append(want, k)
	}
	if !slices.Equal(got, want) {
		t.Fatalf("written %v, want player 2's window, then the newest %d of player 1", got, maxQueuedInputs)
	}
	report := l.drops.report(1, start.Add(time.Second))
	if report == nil || len(report.Samples) != 2 {
		t.Fatalf("overflow report %v", report)
	}
	for k, sample := range report.Samples {
		id := uint64(k + 1)
		if sample.Reason != runtime.DropReason_DROP_IPC_QUEUE_OVERFLOW_INPUT_PACKETS || sample.TransportSequence != uint32(100+id) ||
			sample.MovementEpoch != 1 || sample.LifeGeneration != 1 || sample.FirstCommandSequence != id || sample.LastCommandSequence != id || sample.ItemCount != 1 {
			t.Fatalf("sample %d does not carry the dropped window: %v", k, sample)
		}
	}
}

// The queue depth holds while the writer is blocked in a runtime write: the
// windows that arrive meanwhile wait, up to maxQueuedInputs per player, and
// the Match then reads the newest of them in order.
func TestRuntimeLinkQueuesWindowsBehindABlockedWrite(t *testing.T) {
	block, entered := make(chan struct{}), make(chan struct{}, 1)
	var mu sync.Mutex
	var written []uint32
	setForTest(t, &runtimeWrite, func(_ *framing.TCPConnection, payload []byte, _ time.Duration) error {
		var e runtime.RuntimeEnvelope
		if err := proto.Unmarshal(payload, &e); err != nil {
			return err
		}
		if in := e.GetInput(); in != nil {
			if in.Commands[0].Sequence == 1 {
				entered <- struct{}{}
				<-block
			}
			mu.Lock()
			written = append(written, in.Commands[0].Sequence)
			mu.Unlock()
		}
		return nil
	})
	l := dialedLink(t)
	l.drops, l.now, l.diagnosticWake = newDropLedger(), time.Now, make(chan struct{}, 1)
	l.clock = newClockSync(gatewayMicros(time.Now()))
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	l.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	defer l.close()
	if err := l.input(linkWindow(1, 1, 1, 1), dropEvent{}); err != nil {
		t.Fatal(err)
	}
	<-entered
	const arrived = maxQueuedInputs + 10
	for k := uint32(2); k <= arrived+1; k++ {
		if err := l.input(linkWindow(1, 1, k, k), dropEvent{}); err != nil {
			t.Fatal(err)
		}
	}
	close(block)
	deadline := time.Now().Add(2 * time.Second)
	for {
		mu.Lock()
		n := len(written)
		mu.Unlock()
		if n == maxQueuedInputs+1 || time.Now().After(deadline) {
			break
		}
		time.Sleep(time.Millisecond)
	}
	mu.Lock()
	defer mu.Unlock()
	want := []uint32{1}
	for k := uint32(arrived + 1 - maxQueuedInputs + 1); k <= arrived+1; k++ {
		want = append(want, k)
	}
	if !slices.Equal(written, want) {
		t.Fatalf("the Match read %v, want %v", written, want)
	}
	if got := (linkTotals{}).take(l).of(1)[playerDropIpcQueueOverflowInputPackets]; got != arrived-maxQueuedInputs {
		t.Fatalf("overflow counted %d, want %d", got, arrived-maxQueuedInputs)
	}
}

// D-11: decoding succeeded commits the sequence and liveness; a datagram that
// does not decode commits nothing.
func TestGatewayCommitsAnInputSequenceOnlyOnceItDecoded(t *testing.T) {
	s, p, _, now := actionFixture(t)
	malformed := &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(2, 1)} // empty: undecodable
	sendInputAt(t, s, p, 5, now.Add(4*time.Second), malformed)
	if len(s.link.batch()) != 0 || s.playerIngress[7][playerDropMalformedInputPackets] != 1 {
		t.Fatal("a malformed input reached the Match or was not counted")
	}
	if !p.session.Expired(now.Add(6500*time.Millisecond), sessionTimeout) {
		t.Fatal("a malformed input refreshed liveness")
	}
	if p.session.CommitSequence(5, now) != nil {
		t.Fatal("a malformed input committed its sequence")
	}
	sendInputAt(t, s, p, 6, now.Add(4*time.Second), &client.PlayerInput{MovementEpoch: 9, LifeGeneration: 9, Commands: moves(1, 1)})
	if p.session.Expired(now.Add(6500*time.Millisecond), sessionTimeout) {
		t.Fatal("a decoded input of any epoch did not refresh liveness")
	}
}

func TestGatewayForwardsStaleSequenceInputWithoutCommitting(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	sendInputAt(t, s, p, 5, now.Add(time.Second), &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(3, 3)})
	if got := sequences(s.link); len(got) != 1 || got[0] != 3 {
		t.Fatalf("fresh input missing: %v", got)
	}
	accepted := s.rateAcceptedPackets.Load()
	sendInputAt(t, s, p, 4, now.Add(4*time.Second), &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(2, 2)})
	if got := sequences(s.link); len(got) != 1 || got[0] != 2 {
		t.Fatalf("stale-sequence input did not reach the runtime: %v", got)
	}
	if s.rateAcceptedPackets.Load() != accepted+1 {
		t.Fatal("stale-sequence input changed the rate accounting")
	}
	if s.playerIngress[7][playerFwdStaleSequenceInputPackets] != 1 {
		t.Fatalf("stale-sequence outcome: %v", s.playerIngress[7])
	}
	// Liveness is still the fresh packet's: 5.5 s after it, the session expires.
	if !p.session.Expired(now.Add(6500*time.Millisecond), sessionTimeout) {
		t.Fatal("stale-sequence input refreshed liveness")
	}
	// The sequence did not move back: 5 is still stale, 6 commits.
	if p.session.CommitSequence(5, now) == nil {
		t.Fatal("stale-sequence input moved the sequence")
	}
	// Only a stale sequence is forwarded: another endpoint is not the session.
	payload, _ := proto.Marshal(&client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(4, 4)})
	spoofed := netip.MustParseAddrPort("127.0.0.1:29002")
	if spoofed == endpoint {
		t.Fatal("fixture endpoint")
	}
	if err := s.receivePacket(framing.Header{Version: adapter.ClientVersion, Type: adapter.Input, SessionID: p.session.ID, Sequence: 4}, payload, spoofed, now); err != nil {
		t.Fatal(err)
	}
	if got := sequences(s.link); len(got) != 0 {
		t.Fatalf("unauthorized input reached the runtime: %v", got)
	}
	// Nor is a rate-limited one: 120 packets fill the second, the 121st
	// carries a fresh sequence and a new command and must not reach the link.
	second := now.Add(10 * time.Second)
	for seq := uint32(10); seq < 130; seq++ {
		first := uint32(seq) * 3
		sendInputAt(t, s, p, seq, second, &client.PlayerInput{MovementEpoch: 2, LifeGeneration: 3, Commands: moves(first, first+2)})
	}
	// One player's queue holds maxQueuedInputs windows; the older admitted
	// ones overflowed, counted.
	if got := sequences(s.link); len(got) != 3*maxQueuedInputs {
		t.Fatalf("admitted packets missing: %d commands", len(got))
	}
	if got := (linkTotals{}).take(s.link).of(7)[playerDropIpcQueueOverflowInputPackets]; got != 120-maxQueuedInputs {
		t.Fatalf("overflowed %d, want %d", got, 120-maxQueuedInputs)
	}
	// The Server's sample of a dropped window is that datagram's: its GYOP
	// sequence, receipt, identity and commands (pv7 §4, reason 32).
	report := s.link.drops.report(7, second.Add(time.Second))
	if report == nil {
		t.Fatal("no overflow report")
	}
	var sample *runtime.DropSample // the earlier spoofed packet's sample comes first
	for _, candidate := range report.Samples {
		if candidate.Reason == runtime.DropReason_DROP_IPC_QUEUE_OVERFLOW_INPUT_PACKETS {
			sample = candidate
			break
		}
	}
	if sample == nil || sample.TransportSequence != 10 ||
		sample.MovementEpoch != 2 || sample.LifeGeneration != 3 || sample.FirstCommandSequence != 30 ||
		sample.LastCommandSequence != 32 || sample.ItemCount != 3 || sample.DwellUs < 900_000 {
		t.Fatalf("the first dropped datagram's sample: %v", sample)
	}
	sendInputAt(t, s, p, 130, second, &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(6, 6)})
	if got := sequences(s.link); len(got) != 0 {
		t.Fatalf("rate-limited input reached the runtime: %v", got)
	}
}

// D-12: the Gateway no longer refuses an old or regressed snapshot (the IPC
// is ordered; the Client applies only a newer tick), and keeps no copy of the
// Match's cursors.
func TestGatewayForwardsEverySnapshotWithoutTickOrCursorGuards(t *testing.T) {
	s, _, _, _ := actionFixture(t)
	resolve(s, 10, 5)
	<-s.snapshotOut
	resolve(s, 9, 2) // an older tick, a regressed cursor
	if frame := <-s.snapshotOut; frame.tick != 9 {
		t.Fatalf("snapshot %d forwarded, want 9", frame.tick)
	}
	if s.ingress[gatewayDropOldTickSnapshots] != 0 || s.ingress[gatewayDropRegressedSnapshots] != 0 {
		t.Fatalf("snapshot guards still count: %v", s.ingress)
	}
}

// I0 per player: the handed commands equal the written ones plus the counted
// removals (closed, abandoned), as long as no window overflowed (the overflow
// counts packets).
func TestRuntimeLinkConservesHandedCommands(t *testing.T) {
	l := dialedLink(t)
	totals := linkTotals{}
	read := func() playerIngressCounts { return totals.take(l).of(1) }
	balanced := func(stage string, c playerIngressCounts) {
		t.Helper()
		if handed, accounted := i0(c); handed != accounted {
			t.Fatalf("%s: I0 handed %d != accounted %d: %v", stage, handed, accounted, c)
		}
	}
	_ = l.input(linkWindow(1, 1, 1, 3), dropEvent{})
	_ = l.input(linkWindow(1, 1, 2, 4), dropEvent{}) // a copy is written again
	if err := l.write(l.take()); err != nil {
		t.Fatal(err)
	}
	c := read()
	if c[playerFwdHandedMainCommands] != 6 || c[playerFwdLinkWrittenMainCommands] != 6 {
		t.Fatalf("handed and written commands: %v", c)
	}
	balanced("written", c)
	// A Leave keeps the queued windows: they are written before it.
	_ = l.input(linkWindow(1, 1, 5, 6), dropEvent{})
	leave := envelope()
	leave.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 1}}
	if err := l.control(leave); err != nil {
		t.Fatal(err)
	}
	if batch := l.batch(); len(batch) != 2 || batch[0].GetInput() == nil || batch[1].GetLeave() == nil {
		t.Fatalf("leave overtook or dropped the queued input: %v", batch)
	}
	if c = read(); c[playerDropDiscardedOnLeaveCommands] != 0 {
		t.Fatalf("a Leave discarded commands: %v", c)
	}
	balanced("left", c)
	// A failed write abandons the whole pass; close clears what is queued.
	_ = l.input(linkWindow(1, 1, 7, 9), dropEvent{})
	_ = l.input(linkWindow(1, 1, 8, 9), dropEvent{})
	pass := l.take()
	_ = l.input(linkWindow(1, 1, 10, 14), dropEvent{})
	_ = l.conn.Close()
	written := read()[playerFwdLinkWrittenMainCommands]
	if err := l.write(pass); err == nil {
		t.Fatal("write on a closed connection succeeded")
	}
	c = read()
	if c[playerFwdLinkWrittenMainCommands] != written || c[playerDropAbandonedOnCloseCommands] != 5 {
		t.Fatalf("abandoned commands: %v", c)
	}
	l.close()
	if c = read(); c[playerDropClearedOnCloseCommands] != 5 {
		t.Fatalf("close cleared commands were not counted: %v", c)
	}
	balanced("closed", c)
	if err := l.input(linkWindow(1, 1, 15, 15), dropEvent{}); err == nil {
		t.Fatal("closed link accepted a window")
	}
	balanced("after close", read())
}

// Controls are never dropped: the 65th pending one fails (the link then
// fails), and a write pass frees the queue for the next 64.
func TestRuntimeLinkBoundsPendingControlsPerPass(t *testing.T) {
	l := dialedLink(t)
	join := func(player uint32) *runtime.RuntimeEnvelope {
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: player}}
		return e
	}
	for round := 0; round < 2; round++ {
		for k := uint32(0); k < maxQueuedControls; k++ {
			if err := l.control(join(k + 1)); err != nil {
				t.Fatalf("round %d: control %d refused: %v", round, k+1, err)
			}
		}
		if err := l.control(join(99)); err == nil {
			t.Fatalf("round %d: control %d accepted", round, maxQueuedControls+1)
		}
		if got := len(l.batch()); got != maxQueuedControls {
			t.Fatalf("round %d: %d controls written", round, got)
		}
	}
}

// An input wakes the writer at once: it does not wait for the idle timer.
func TestRuntimeLinkWritesAnInputAtOnce(t *testing.T) {
	setForTest(t, &writerIdleWait, 10*time.Second)
	written := make(chan struct{}, 4)
	setForTest(t, &runtimeWrite, func(_ *framing.TCPConnection, payload []byte, _ time.Duration) error {
		var e runtime.RuntimeEnvelope
		if proto.Unmarshal(payload, &e) == nil && e.GetInput() != nil {
			written <- struct{}{}
		}
		return nil
	})
	l := dialedLink(t)
	l.drops, l.now, l.diagnosticWake = newDropLedger(), time.Now, make(chan struct{}, 1)
	l.clock = newClockSync(gatewayMicros(time.Now()))
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	l.run(ctx, func(*runtime.RuntimeEnvelope) {}, func(error) {})
	defer l.close()
	time.Sleep(20 * time.Millisecond) // the writer is idle, waiting on its timer
	if err := l.input(linkWindow(1, 1, 1, 1), dropEvent{}); err != nil {
		t.Fatal(err)
	}
	select {
	case <-written:
	case <-time.After(time.Second):
		t.Fatal("an input waited for the writer's idle timer")
	}
}
