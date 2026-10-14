package gateway

import (
	"context"
	"errors"
	"net"
	"net/netip"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv6"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
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

func moves(first, last uint64) []*client.MovementCommand {
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

func sendMoves(t *testing.T, s *Server, p *reservation, seq uint32, epoch uint64, commands []*client.MovementCommand) {
	t.Helper()
	sendInputAt(t, s, p, seq, time.Now(), &client.PlayerInput{MovementEpoch: epoch, LifeGeneration: 1, Commands: commands})
}

// sequences takes one write pass and lists the input commands in write order.
func sequences(l *runtimeLink) (out []uint64) {
	for _, e := range l.batch() {
		if in := e.GetInput(); in != nil {
			for _, command := range in.Commands {
				out = append(out, command.Sequence)
			}
		}
	}
	return out
}

func resolve(s *Server, tick, through uint64) {
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Snapshot{Snapshot: &runtime.WorldSnapshot{Tick: tick,
		Players: []*runtime.PlayerState{{PlayerId: 7, MovementEpoch: 1, LifeGeneration: 1, LifeState: runtime.LifeState_LIFE_ALIVE, Grounded: true, LastResolvedCommand: through}},
		Combat:  []*runtime.CombatState{{PlayerId: 7, LifeGeneration: 1, Hp: 100, MagazineAmmo: 12}}}}
	s.runtimeMessage(e)
}

// linkTotals accumulates what takeIngress hands over, per player.
type linkTotals map[uint64]*playerIngressCounts

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

func (totals linkTotals) of(player uint64) playerIngressCounts {
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
	return &runtimeLink{conn: conn, inputs: make(map[uint64]*runtime.PlayerInput), epochs: make(map[uint64]uint64),
		lives: make(map[uint64]uint64), wake: make(chan struct{}, 1), done: make(chan struct{})}
}

func linkWindow(player, epoch, first, last uint64) *runtime.PlayerInput {
	in := &runtime.PlayerInput{LifeGeneration: 1, MovementEpoch: epoch, PlayerId: player}
	for sequence := first; sequence <= last; sequence++ {
		in.Commands = append(in.Commands, &runtime.MovementCommand{Sequence: sequence, MoveForward: 1})
	}
	return in
}

// D49/D51: the Match alone decides that a step is resolved and measures how
// late a substituted step arrived, so the Gateway forwards resolved steps.
func TestGatewayForwardsResolvedCommandsToTheMatch(t *testing.T) {
	s, p, _, _ := actionFixture(t)
	totals := linkTotals{}
	sendMoves(t, s, p, 2, 1, moves(1, 2))
	sequences(s.link)
	resolve(s, 10, 3) // 3 was substituted: its first copy is late, never written.
	sendMoves(t, s, p, 3, 1, moves(1, 4))
	got := sequences(s.link)
	totals.take(s.link)
	if p.commands[4] == nil || len(p.commands) != 1 || totals.of(7)[playerFwdRoutedFutureLimitInputs] != 0 {
		t.Fatalf("resolved sequence underflowed the future bound: %v %v", p.commands, got)
	}
	if len(got) != 2 || got[0] != 3 || got[1] != 4 {
		t.Fatalf("resolved command did not reach the runtime: %v", got)
	}
	resolve(s, 11, 6)
	sendMoves(t, s, p, 4, 1, moves(5, 6))
	if got := sequences(s.link); len(got) != 2 || got[0] != 5 {
		t.Fatalf("all-resolved window was not forwarded: %v", got)
	}
	if c := totals.take(s.link).of(7); c[playerFwdResolvedAtWriteCommands] != 3 {
		t.Fatalf("resolved forwarding was not counted: %d", c[playerFwdResolvedAtWriteCommands])
	}
}

func TestGatewayRoutesFarFutureWindowsToTheMatch(t *testing.T) {
	s, p, _, _ := actionFixture(t)
	totals := linkTotals{}
	sendMoves(t, s, p, 2, 1, moves(33, 33))
	if got := sequences(s.link); len(got) != 1 || got[0] != 33 || len(p.commands) != 0 ||
		totals.take(s.link).of(7)[playerFwdRoutedFutureLimitInputs] != 1 {
		t.Fatalf("far-future window did not reach the runtime: %v", got)
	}
	// A crafted link state: the defensive cap still reaches the Match.
	s.link.inputs[7] = &runtime.PlayerInput{PlayerId: 7, MovementEpoch: 1, LifeGeneration: 1}
	for sequence := uint64(2); sequence <= 33; sequence++ {
		s.link.inputs[7].Commands = append(s.link.inputs[7].Commands, &runtime.MovementCommand{Sequence: sequence, MoveForward: 1})
	}
	sendMoves(t, s, p, 3, 1, moves(1, 1))
	if got := sequences(s.link); len(got) != 33 || totals.take(s.link).of(7)[playerFwdRoutedLinkCapInputs] != 1 {
		t.Fatalf("link-cap window did not reach the runtime: %v", got)
	}
}

func TestRuntimeLinkCapCountsOnlyUnresolvedAndTrimsCounted(t *testing.T) {
	l := &runtimeLink{inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1)}
	l.acknowledge(1, 1, 1, 20)
	for first := uint64(1); first <= 49; first += 12 {
		if err := l.input(linkWindow(1, 1, first, min(first+11, 52))); err != nil {
			t.Fatalf("link cap counted resolved commands: %v", err)
		}
	}
	if err := l.input(linkWindow(1, 1, 53, 53)); !errors.Is(err, adapter.ErrInput) || linkRefusalReason(err) != playerFwdRoutedLinkCapInputs {
		t.Fatal("link admitted 33 unresolved commands")
	}
	l.acknowledge(1, 1, 1, 52)
	for first := uint64(53); first <= 77; first += 12 {
		if err := l.input(linkWindow(1, 1, first, min(first+11, 84))); err != nil {
			t.Fatal(err)
		}
	}
	l.acknowledge(1, 1, 1, 100)
	if err := l.input(linkWindow(1, 1, 101, 101)); err != nil {
		t.Fatal(err)
	}
	if c := (linkTotals{}).take(l).of(1); c[playerDropResolvedTrimCommands] != 52 {
		t.Fatalf("resolved trim was not counted: %d", c[playerDropResolvedTrimCommands])
	}
	batch := l.batch()
	var got []uint64
	for _, e := range batch {
		if len(e.GetInput().Commands) > adapter.MaxPendingCommands {
			t.Fatal("IPC batch exceeds12")
		}
		for _, command := range e.GetInput().Commands {
			got = append(got, command.Sequence)
		}
	}
	if len(got) != 33 || got[0] != 53 || got[32] != 101 {
		t.Fatalf("resolved retention kept the wrong commands: %v", got)
	}
}

// The rejected lane (xhigh): per player the main window is written first, then
// the rejected windows in arrival order, each cut into IPC inputs of at most
// 12 commands; beyond 4 windows the oldest is dropped and counted.
func TestRuntimeLinkRejectedLaneIsOrderedChunkedBoundedAndCounted(t *testing.T) {
	l := &runtimeLink{inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1)}
	for first := uint64(1); first <= 13; first += 12 {
		if err := l.input(linkWindow(1, 1, first, first+11)); err != nil {
			t.Fatal(err)
		}
	}
	l.acknowledge(1, 2, 1, 0) // The 24-command epoch-1 window moves to the rejected lane.
	if err := l.input(linkWindow(1, 2, 1, 1)); err != nil {
		t.Fatal(err)
	}
	batch := l.batch()
	for _, e := range batch {
		if len(e.GetInput().Commands) > adapter.MaxPendingCommands {
			t.Fatal("rejected lane IPC exceeds 12 commands")
		}
	}
	if len(batch) != 3 || batch[0].GetInput().MovementEpoch != 2 {
		t.Fatalf("rejected window overtook the main window: %v", batch)
	}
	for i, e := range batch[1:] {
		in := e.GetInput()
		if in.MovementEpoch != 1 || len(in.Commands) != 12 || in.Commands[0].Sequence != uint64(12*i+1) {
			t.Fatalf("rejected window was reordered or cut wrongly: %v", batch)
		}
	}
	totals := linkTotals{}
	if c := totals.take(l).of(1); c[playerFwdRoutedRotationInputs] != 1 {
		t.Fatalf("rotated window was not counted: %d", c[playerFwdRoutedRotationInputs])
	}
	for i := uint64(1); i <= maxRejectedWindows+1; i++ {
		if err := l.reject(linkWindow(1, 1, i, i+2), playerFwdRoutedEpochMismatchInputs); err != nil {
			t.Fatal(err)
		}
	}
	batch = l.batch()
	if c := totals.take(l).of(1); c[playerDropRejectedLaneOverflowCommands] != 3 || len(batch) != maxRejectedWindows {
		t.Fatalf("rejected lane overflow was not counted: %d", c[playerDropRejectedLaneOverflowCommands])
	}
	if batch[0].GetInput().Commands[0].Sequence != 2 || batch[maxRejectedWindows-1].GetInput().Commands[0].Sequence != maxRejectedWindows+1 {
		t.Fatalf("rejected lane overflow dropped the wrong window: %v", batch)
	}
}

// Order across players and lanes: controls first, then per player (ascending
// id) its main window and its rejected lane, however they were queued.
func TestRuntimeLinkWritesEachPlayersMainWindowBeforeItsRejectedLane(t *testing.T) {
	l := &runtimeLink{inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1)}
	if err := l.reject(linkWindow(2, 3, 1, 1), playerFwdRoutedEpochMismatchInputs); err != nil {
		t.Fatal(err)
	}
	if err := l.reject(linkWindow(1, 3, 5, 5), playerFwdRoutedEpochMismatchInputs); err != nil {
		t.Fatal(err)
	}
	for _, player := range []uint64{2, 1} {
		if err := l.input(linkWindow(player, 1, 1, 1)); err != nil {
			t.Fatal(err)
		}
	}
	if err := l.reject(linkWindow(1, 3, 6, 6), playerFwdRoutedEpochMismatchInputs); err != nil {
		t.Fatal(err)
	}
	join := envelope()
	join.Message = &runtime.RuntimeEnvelope_Join{Join: &runtime.PlayerJoin{PlayerId: 3}}
	if err := l.control(join); err != nil {
		t.Fatal(err)
	}
	type step struct{ player, epoch, first uint64 }
	want := []step{{1, 1, 1}, {1, 3, 5}, {1, 3, 6}, {2, 1, 1}, {2, 3, 1}}
	batch := l.batch()
	if len(batch) != len(want)+1 || batch[0].GetJoin() == nil {
		t.Fatalf("control did not lead the pass: %v", batch)
	}
	for i, e := range batch[1:] {
		in := e.GetInput()
		if got := (step{in.PlayerId, in.MovementEpoch, in.Commands[0].Sequence}); got != want[i] {
			t.Fatalf("rejected window overtook a main window: %d %v want %v", i, got, want[i])
		}
	}
}

// ★ (xhigh): a stale transport sequence carries a full payload. It is decoded
// and forwarded, never committed: no liveness, no sequence movement, and the
// rate limit counts it once, as before.
func TestGatewayForwardsStaleSequenceInputWithoutCommitting(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	sendInputAt(t, s, p, 5, now.Add(time.Second), &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(3, 3)})
	if got := sequences(s.link); len(got) != 1 || got[0] != 3 {
		t.Fatalf("fresh input missing: %v", got)
	}
	accepted := s.rateAcceptedPackets.Load()
	// Never written before: dedup cannot hide a dropped stale packet.
	sendInputAt(t, s, p, 4, now.Add(4*time.Second), &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(2, 2)})
	if got := sequences(s.link); len(got) != 1 || got[0] != 2 || p.commands[2] == nil {
		t.Fatalf("stale-sequence input did not reach the runtime: %v", got)
	}
	if s.rateAcceptedPackets.Load() != accepted+1 {
		t.Fatal("stale-sequence input changed the rate accounting")
	}
	// Liveness is still the fresh packet's: 5.5 s after it, the session expires.
	if !p.session.Expired(now.Add(6500*time.Millisecond), sessionTimeout) {
		t.Fatal("stale-sequence input refreshed liveness")
	}
	// The sequence did not move back: 5 is still stale, 6 commits.
	if p.session.CommitSequence(5, now) == nil {
		t.Fatal("stale-sequence input moved the sequence")
	}
	// A stale epoch on the rejected lane is forwarded the same way.
	sendInputAt(t, s, p, 3, now.Add(4*time.Second), &client.PlayerInput{MovementEpoch: 2, LifeGeneration: 1, Commands: moves(1, 1)})
	if got := sequences(s.link); len(got) != 1 {
		t.Fatalf("stale-sequence rejected-lane input did not reach the runtime: %v", got)
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
		sendInputAt(t, s, p, seq, second, &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(5, 5)})
	}
	if got := sequences(s.link); len(got) != 1 || got[0] != 5 {
		t.Fatalf("admitted packets missing: %v", got)
	}
	sendInputAt(t, s, p, 130, second, &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(6, 6)})
	if got := sequences(s.link); len(got) != 0 || p.commands[6] != nil {
		t.Fatalf("rate-limited input reached the runtime: %v", got)
	}
}

// Rejected-lane windows keep HEAD's commit timing: epoch, future and conflict
// windows are routed before the sequence commits, so they extend no liveness.
func TestGatewayRejectedLaneWindowsDoNotCommitTheSequence(t *testing.T) {
	conflict := moves(1, 1)
	conflict[0].MoveForward = -1
	for _, c := range []struct {
		name string
		in   *client.PlayerInput
	}{
		{"epoch", &client.PlayerInput{MovementEpoch: 2, LifeGeneration: 1, Commands: moves(1, 1)}},
		{"future", &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(40, 40)}},
		{"conflict", &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: conflict}},
	} {
		s, p, _, now := actionFixture(t)
		sendInputAt(t, s, p, 2, now.Add(time.Second), &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(1, 1)})
		sendInputAt(t, s, p, 3, now.Add(4*time.Second), c.in)
		if got := sequences(s.link); len(got) != 2 {
			t.Fatalf("%s: windows missing: %v", c.name, got)
		}
		if !p.session.Expired(now.Add(6500*time.Millisecond), sessionTimeout) {
			t.Fatalf("rejected-lane %s input committed the sequence", c.name)
		}
	}
}

// A window the link refuses was validated by the Server and committed before
// the link saw it, as before 08c: it extends liveness on the rejected lane.
func TestGatewayLinkRefusedWindowsCommitTheSequence(t *testing.T) {
	s, p, _, now := actionFixture(t)
	sendInputAt(t, s, p, 2, now.Add(time.Second), &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: moves(1, 2)})
	sequences(s.link)
	resolve(s, 10, 2)
	changed := moves(1, 1)
	changed[0].MoveForward = -1
	sendInputAt(t, s, p, 3, now.Add(4*time.Second), &client.PlayerInput{MovementEpoch: 1, LifeGeneration: 1, Commands: changed})
	if got := sequences(s.link); len(got) != 1 {
		t.Fatalf("written conflict missing: %v", got)
	}
	if p.session.Expired(now.Add(6500*time.Millisecond), sessionTimeout) {
		t.Fatal("link-refused input did not commit the sequence")
	}
}

// Every link refusal has its own routing reason; a disagreement between the
// Server's and the link's epoch is not the Server's ordinary epoch routing.
func TestLinkRefusalReasonsAreDistinct(t *testing.T) {
	for err, want := range map[error]playerIngressCounter{
		errLinkEpoch:           playerFwdRoutedLinkEpochMismatchInputs,
		errLinkConflict:        playerFwdRoutedConflictInputs,
		errLinkWrittenConflict: playerFwdRoutedWrittenConflictInputs,
		errLinkCap:             playerFwdRoutedLinkCapInputs,
		adapter.ErrInput:       playerFwdRoutedMalformedInputs,
	} {
		if got := linkRefusalReason(err); got != want {
			t.Fatalf("link refusal %v counted as %s", err, ingressKeyName(playerIngressKeys[got]))
		}
	}
	s, p, _, _ := actionFixture(t)
	s.link.epochs[7] = 2 // A crafted divergence: the Server still holds epoch 1.
	sendMoves(t, s, p, 2, 1, moves(1, 1))
	c := (linkTotals{}).take(s.link).of(7)
	if got := sequences(s.link); len(got) != 1 || c[playerFwdRoutedLinkEpochMismatchInputs] != 1 || c[playerFwdRoutedEpochMismatchInputs] != 0 {
		t.Fatalf("epoch divergence was not counted apart: %v %v", got, c)
	}
}

// The resolved prefix of a main window is cut apart, so the unresolved part
// keeps the 12-command boundaries it had before 08c.
func TestRuntimeLinkCutsTheMainWindowAtTheResolvedCursor(t *testing.T) {
	l := &runtimeLink{inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1)}
	if err := l.input(linkWindow(1, 1, 1, 5)); err != nil {
		t.Fatal(err)
	}
	if err := l.input(linkWindow(1, 1, 6, 13)); err != nil {
		t.Fatal(err)
	}
	l.acknowledge(1, 1, 1, 5)
	var withNew, total int
	var mixed *runtime.PlayerInput
	for _, e := range l.batch() {
		in := e.GetInput()
		total += len(in.Commands)
		if in.Commands[len(in.Commands)-1].Sequence > 5 {
			withNew++
			if in.Commands[0].Sequence <= 5 {
				mixed = in
			}
		}
	}
	if withNew != 1 || total != 13 {
		t.Fatalf("resolved prefix shifted the chunk boundaries: %d inputs with new commands, %d commands", withNew, total)
	}
	if mixed != nil {
		t.Fatalf("resolved and new commands share an IPC input: %v", mixed)
	}
}

// A copy that changes a written command goes to the Match unmerged; only the
// Gateway knows it conflicted (the Match skips resolved steps unread).
func TestGatewayRoutesWrittenConflictsToTheRejectedLane(t *testing.T) {
	s, p, _, _ := actionFixture(t)
	sendMoves(t, s, p, 2, 1, moves(1, 2))
	sequences(s.link)
	resolve(s, 10, 2) // The Server no longer holds 1 and 2; only the link does.
	changed := moves(1, 3)
	changed[0].MoveForward = -1
	sendMoves(t, s, p, 3, 1, changed)
	batch := s.link.batch()
	if len(batch) != 1 || len(batch[0].GetInput().Commands) != 3 || batch[0].GetInput().Commands[0].MoveForward != -1 || p.commands[3] != nil {
		t.Fatalf("written conflict did not reach the runtime unmerged: %v", batch)
	}
	if c := (linkTotals{}).take(s.link).of(7); c[playerFwdRoutedWrittenConflictInputs] != 1 {
		t.Fatalf("written conflict was not counted: %d", c[playerFwdRoutedWrittenConflictInputs])
	}
}

// I0: every command handed to the link is written once or counted once as
// removed; a move between lanes is not counted again.
func TestRuntimeLinkConservesHandedCommands(t *testing.T) {
	l := dialedLink(t)
	totals := linkTotals{}
	read := func() playerIngressCounts { return totals.take(l).of(1) }
	// I0 is checked whenever nothing is queued or in flight, after the
	// stage's own assertions so that each names the term it lost.
	balanced := func(stage string, c playerIngressCounts) {
		t.Helper()
		if handed, accounted := i0(c); handed != accounted {
			t.Fatalf("%s: I0 handed %d != accounted %d: %v", stage, handed, accounted, c)
		}
	}
	// Written (main), merged and written duplicates.
	_ = l.input(linkWindow(1, 1, 1, 3))
	_ = l.input(linkWindow(1, 1, 2, 4))
	if err := l.write(l.take()); err != nil {
		t.Fatal(err)
	}
	_ = l.input(linkWindow(1, 1, 3, 5))
	_ = l.reject(linkWindow(1, 2, 1, 2), playerFwdRoutedEpochMismatchInputs)
	if err := l.write(l.take()); err != nil {
		t.Fatal(err)
	}
	c := read()
	if c[playerFwdLinkWrittenMainCommands] != 5 {
		t.Fatalf("link written commands were not counted: %v", c)
	}
	if c[playerFwdLinkWrittenRejectedLaneCommands] != 2 {
		t.Fatalf("rejected-lane written commands were not counted: %v", c)
	}
	if c[playerFwdMergedDuplicateCommands] != 2 {
		t.Fatalf("merged duplicate commands were not counted: %v", c)
	}
	if c[playerFwdWrittenDuplicateCommands] != 2 {
		t.Fatalf("written duplicates were not counted: %v", c)
	}
	balanced("written", c)
	// Rotation moves the main window to the rejected lane without recounting;
	// overflow and Leave remove what is pending.
	_ = l.input(linkWindow(1, 1, 6, 7))
	l.acknowledge(1, 2, 1, 0)
	for i := uint64(1); i <= maxRejectedWindows; i++ {
		_ = l.reject(linkWindow(1, 1, 10*i, 10*i), playerFwdRoutedEpochMismatchInputs)
	}
	_ = l.input(linkWindow(1, 2, 1, 1))
	c = read()
	if c[playerFwdHandedRejectedLaneCommands] != 2+maxRejectedWindows || c[playerFwdHandedMainCommands] != 12 {
		t.Fatalf("rotation was recounted as handed: %v", c)
	}
	if c[playerDropRejectedLaneOverflowCommands] != 2 {
		t.Fatalf("rejected lane overflow was not counted: %v", c)
	}
	leave := envelope()
	leave.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 1}}
	if err := l.control(leave); err != nil {
		t.Fatal(err)
	}
	if c = read(); c[playerDropDiscardedOnLeaveCommands] != 1+maxRejectedWindows {
		t.Fatalf("leave discarded commands were not counted: %v", c)
	}
	balanced("left", c)
	if batch := l.batch(); len(batch) != 1 || batch[0].GetLeave() == nil {
		t.Fatalf("leave missing: %v", batch)
	}
	// Resolved trim.
	l.acknowledge(1, 2, 1, 100)
	for first := uint64(1); first <= 37; first += 12 {
		_ = l.input(linkWindow(1, 2, first, first+11))
	}
	if c = read(); c[playerDropResolvedTrimCommands] != 16 {
		t.Fatalf("resolved trim was not counted: %v", c)
	}
	// A failed write abandons the whole pass; close clears what is queued.
	_ = l.reject(linkWindow(1, 2, 200, 200), playerFwdRoutedFutureLimitInputs)
	pass := l.take()
	_ = l.input(linkWindow(1, 2, 101, 102))
	_ = l.reject(linkWindow(1, 3, 1, 3), playerFwdRoutedEpochMismatchInputs)
	_ = l.conn.Close()
	written := read()[playerFwdLinkWrittenMainCommands]
	if err := l.write(pass); err == nil {
		t.Fatal("write on a closed connection succeeded")
	}
	c = read()
	if c[playerFwdLinkWrittenMainCommands] != written {
		t.Fatal("written commands were counted before the write succeeded")
	}
	if c[playerDropAbandonedOnCloseCommands] == 0 {
		t.Fatalf("abandoned commands were not counted: %v", c)
	}
	if c[playerDropAbandonedOnCloseCommands] != 33 {
		t.Fatalf("the rest of a failed pass was not abandoned: %v", c)
	}
	l.close()
	if c = read(); c[playerDropClearedOnCloseCommands] != 5 {
		t.Fatalf("close cleared commands were not counted: %v", c)
	}
	balanced("closed", c)
	if err := l.input(linkWindow(1, 2, 103, 103)); err == nil || l.reject(linkWindow(1, 2, 103, 103), playerFwdRoutedEpochMismatchInputs) == nil {
		t.Fatal("closed link accepted a window")
	}
	balanced("after close", read())
}
