package gateway

import (
	"context"
	"errors"
	"fmt"
	"log"
	"os"
	"sort"
	"sync"
	"time"

	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

// This queue is product-owned: ordered lifecycle requests and bounded command
// windows intentionally have different semantics. Neither lane blocks the World.
var errRuntimeDisconnected = errors.New("runtime disconnected")

type runtimeLink struct {
	actionWindows   map[uint64]*actionWindow
	conn            *framing.TCPConnection
	mu              sync.Mutex
	controls        []*runtime.RuntimeEnvelope
	inputs          map[uint64]*runtime.PlayerInput
	epochs          map[uint64]uint64 // Advanced only by authoritative snapshots.
	lives           map[uint64]uint64
	wake            chan struct{}
	done            chan struct{}
	closed          bool
	once            sync.Once
	coalescedInputs uint64
	maxWriteAge     time.Duration
	// The Match decides what is resolved, stale or conflicting (D49), so the
	// link forwards every window it is handed. resolved is the authority cursor
	// last acknowledged, used only to bound the merged main window; rejected is
	// the lane of windows the Match may refuse as a whole, never merged.
	resolved map[uint64]uint64
	rejected map[uint64][]*runtime.PlayerInput
	// written is a transport fact, not a judgment: the main-lane commands this
	// link took for the TCP writer in the current epoch/life, by sequence. A
	// copy with the same identity and content carries nothing new for the
	// Match (it only uses the first arrival), so it is not written again.
	written       map[uint64]map[uint64]*runtime.MovementCommand
	newestWritten map[uint64]uint64
	// Per-player I0 counters since the last takeIngress; kept across forget
	// and close so the commands those discard stay accounted for.
	ingress map[uint64]*playerIngressCounts
	// Diagnostics only: intervals between each player's ActionBatch writes.
	actionStats map[uint64]*intervalStats
	// The writer goroutine run started, and a channel closed once it exited:
	// close() does not wait for it, and a pass it took still counts its
	// commands written or abandoned (I0) until then.
	writerStarted bool
	writerExited  chan struct{}
	writerOnce    sync.Once
	// Diagnostics only (pv7 §2, §3). The single diagnostic slot (newest
	// wins), written by the writer on its own wake-up: it never touches the
	// data lanes, l.wake or the action timer. clock is the sampling with the
	// Match, under its own leaf lock (after s.mu and l.mu).
	diagnostic     *runtime.RuntimeEnvelope
	diagnosticWake chan struct{}
	clockMu        sync.Mutex
	clock          *clockSync
	// now is the reader's clock for liveness; tests replace it.
	now func() time.Time
	// Diagnostics only: write passes taken; a diagnostic write takes none.
	takes uint64
}

// runtimeLiveness: the reader fails the link when no Snapshot, JoinResult,
// Evicted or Error arrived for this long (pv7 §2). Tests shorten it.
var runtimeLiveness = 5 * time.Second

// writerIdleWait is how long the writer sleeps with no action deadline; tests
// lengthen it to count the passes something else caused.
var writerIdleWait = actionSendInterval

// The write of one runtime frame. Tests replace it to block or fail a write
// the writer goroutine is in.
var runtimeWrite = (*framing.TCPConnection).Write

func connectRuntime(ctx context.Context, address string) (*runtimeLink, *runtime.Ready, error) {
	conn, err := framing.DialTCP(ctx, address, 3*time.Second)
	if err != nil {
		return nil, nil, err
	}
	frame, err := conn.Read(3 * time.Second)
	var envelope runtime.RuntimeEnvelope
	if err == nil {
		var version uint64
		var other bool
		if version, other, err = decodeRuntime(frame, &envelope); other {
			// The Gateway reads Ready first, so it is the side that detects
			// another version; it tells the Match its own version before
			// closing.
			writeVersionNotice(conn)
			err = fmt.Errorf("runtime readiness contract mismatch: protocol version expected %d, received %d", adapter.RuntimeVersion, version)
		}
	}
	ready := envelope.GetReady()
	if err == nil && (ready == nil ||
		ready.ArenaId == "" || ready.ArenaVersion == 0 || ready.ArenaDigest == 0 || ready.TickRate != adapter.AuthorityTickRate ||
		ready.SnapshotIntervalTicks != adapter.SnapshotIntervalTicks || ready.MaxPlayers != adapter.MaxPlayers || !adapter.ValidRules(ready.CombatRules) || !adapter.ReadyFitsWelcome(ready)) {
		err = errors.New("runtime readiness contract mismatch")
	}
	if err != nil {
		_ = conn.Close()
		return nil, nil, err
	}
	return &runtimeLink{conn: conn, inputs: make(map[uint64]*runtime.PlayerInput), epochs: make(map[uint64]uint64), lives: make(map[uint64]uint64), wake: make(chan struct{}, 1), done: make(chan struct{}),
		diagnosticWake: make(chan struct{}, 1), clock: newClockSync(gatewayMicros(time.Now())), now: time.Now}, ready, nil
}

// decodeRuntime checks the version before the content (pv7 contract §1). Field
// 1 is always serialized first and never moves, so it is read on its own first:
// another version whose content this schema cannot parse still names its
// version. other is true for any version but this Gateway's; message is then
// left unparsed.
func decodeRuntime(payload []byte, message *runtime.RuntimeEnvelope) (version uint64, other bool, err error) {
	if number, kind, n := protowire.ConsumeTag(payload); n > 0 && number == 1 && kind == protowire.VarintType {
		if value, m := protowire.ConsumeVarint(payload[n:]); m > 0 && value != uint64(adapter.RuntimeVersion) {
			return value, true, nil
		}
	}
	if err := proto.Unmarshal(payload, message); err != nil {
		return 0, false, err
	}
	return uint64(message.ProtocolVersion), message.ProtocolVersion != adapter.RuntimeVersion, nil
}

// writeVersionNotice tells the peer this Gateway's protocol version: an
// envelope with only field 1, whose position never changes. Best effort: the
// link is closing anyway.
func writeVersionNotice(conn *framing.TCPConnection) {
	if payload, err := proto.Marshal(&runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion}); err == nil {
		_ = conn.Write(payload, 500*time.Millisecond)
	}
}

// unknownRuntimeMessage names the field numbers of a message this version does
// not know, apart from an envelope that carries no message at all.
func unknownRuntimeMessage(message *runtime.RuntimeEnvelope) error {
	unknown := message.ProtoReflect().GetUnknown()
	var fields []int32
	for len(unknown) > 0 {
		number, _, length := protowire.ConsumeField(unknown)
		if length < 0 {
			break
		}
		fields = append(fields, int32(number))
		unknown = unknown[length:]
	}
	if len(fields) == 0 {
		return errors.New("runtime protocol mismatch: empty message")
	}
	return fmt.Errorf("unknown runtime message fields=%v", fields)
}

func envelope() *runtime.RuntimeEnvelope {
	return &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion}
}

func (l *runtimeLink) control(message *runtime.RuntimeEnvelope) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errRuntimeDisconnected
	}
	if len(l.controls) >= 64 {
		return errors.New("runtime lifecycle queue full")
	}
	if leave := message.GetLeave(); leave != nil {
		l.forgetLocked(leave.PlayerId)
	}
	l.controls = append(l.controls, message)
	l.notify()
	return nil
}

// forget drops a player's pending link state: input window, epoch, life and
// action window. A Leave does it before queuing the control; an eviction sends
// no Leave (the Match already removed the player), so the Gateway calls it.
// The commands, and the action requests and acknowledgement no write pass
// took, are counted into the player's ingress counters, which outlive it.
func (l *runtimeLink) forget(player uint64) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.forgetLocked(player)
}

func (l *runtimeLink) forgetLocked(player uint64) {
	discarded := uint64(0)
	if in := l.inputs[player]; in != nil {
		discarded += uint64(len(in.Commands))
	}
	for _, in := range l.rejected[player] {
		discarded += uint64(len(in.Commands))
	}
	l.countLocked(player, playerDropDiscardedOnLeaveCommands, discarded)
	if w := l.actionWindows[player]; w != nil {
		shots, acks := w.untaken()
		l.countLocked(player, playerDropDiscardedOnLeaveActionsShots, shots)
		l.countLocked(player, playerDropDiscardedOnLeaveActionsAcks, acks)
	}
	delete(l.inputs, player)
	delete(l.resolved, player)
	delete(l.rejected, player)
	delete(l.written, player)
	delete(l.newestWritten, player)
	delete(l.epochs, player)
	delete(l.lives, player)
	delete(l.actionWindows, player)
	delete(l.actionStats, player)
}

// Why input() refused a window. Each wraps adapter.ErrInput; the Server then
// hands the window to the rejected lane, unmerged, for the Match to judge.
var (
	errLinkEpoch           = fmt.Errorf("%w: window of another movement epoch or life", adapter.ErrInput)
	errLinkConflict        = fmt.Errorf("%w: command conflicts with the pending window", adapter.ErrInput)
	errLinkWrittenConflict = fmt.Errorf("%w: command conflicts with a written command", adapter.ErrInput)
	errLinkCap             = fmt.Errorf("%w: merged window exceeds the unresolved command bound", adapter.ErrInput)
)

// linkRefusalReason names the routing attribute of a window input() refused.
// An epoch refusal is the link disagreeing with the Server, which routes
// windows of another epoch itself, so it has its own reason; any other
// refusal is a schema error.
func linkRefusalReason(err error) playerIngressCounter {
	switch {
	case errors.Is(err, errLinkEpoch):
		return playerFwdRoutedLinkEpochMismatchInputs
	case errors.Is(err, errLinkConflict):
		return playerFwdRoutedConflictInputs
	case errors.Is(err, errLinkWrittenConflict):
		return playerFwdRoutedWrittenConflictInputs
	case errors.Is(err, errLinkCap):
		return playerFwdRoutedLinkCapInputs
	}
	return playerFwdRoutedMalformedInputs
}

// writtenRetention covers the Match ingress ledger (600 sequences) plus the
// future bound: a written sequence is at most MaxFutureCommands ahead of the
// Match cursor, so a copy older than this is already aged there and is
// forwarded again for the Match to classify. Measured from the newest
// sequence this link wrote, never from the Gateway's copy of the cursor.
const writtenRetention = 600 + adapter.MaxFutureCommands

// The two drops the link still executes, bounded and counted per player: a
// stalled writer can overflow the rejected lane or the resolved part of the
// merged main window.
const (
	maxRejectedWindows  = 4
	maxRetainedResolved = adapter.MaxFutureCommands
)

func (l *runtimeLink) countLocked(player uint64, counter playerIngressCounter, n uint64) {
	if n == 0 {
		return
	}
	if l.ingress == nil {
		l.ingress = make(map[uint64]*playerIngressCounts)
	}
	counts := l.ingress[player]
	if counts == nil {
		counts = new(playerIngressCounts)
		l.ingress[player] = counts
	}
	counts[counter] += n
}

// takeIngress hands over each player's link counters accumulated since the
// previous call. I0 holds per player over the sum of all takes: the handed
// commands equal the written ones plus the counted removals.
func (l *runtimeLink) takeIngress() map[uint64]*playerIngressCounts {
	l.mu.Lock()
	defer l.mu.Unlock()
	taken := l.ingress
	l.ingress = nil
	return taken
}

// input merges a window into the player's main lane. Commands this link
// already wrote with the same content are dropped and counted; a refused
// window changes nothing, and the caller routes it to the rejected lane.
func (l *runtimeLink) input(in *runtime.PlayerInput) error {
	return l.inputWindow(in, new(bool))
}

// inputWindow is input that also sets *copies when every command of the
// window was a written copy: nothing was merged, the packet's outcome is a
// written duplicate.
func (l *runtimeLink) inputWindow(in *runtime.PlayerInput, copies *bool) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errRuntimeDisconnected
	}
	if in == nil || in.PlayerId == 0 || in.MovementEpoch == 0 || in.LifeGeneration == 0 || len(in.Commands) == 0 || len(in.Commands) > adapter.MaxPendingCommands {
		return adapter.ErrInput
	}
	epoch := l.epochs[in.PlayerId]
	if epoch == 0 {
		epoch = 1
	}
	life := l.lives[in.PlayerId]
	if life == 0 {
		life = 1
	}
	// Old and future epochs alike: the Match judges them on the rejected lane.
	if in.MovementEpoch != epoch || in.LifeGeneration != life {
		return errLinkEpoch
	}
	var previous uint64
	for _, command := range in.Commands {
		if command == nil || command.Sequence <= previous {
			return adapter.ErrInput
		}
		previous = command.Sequence
	}
	written, newest := l.written[in.PlayerId], l.newestWritten[in.PlayerId]
	fresh := make([]*runtime.MovementCommand, 0, len(in.Commands))
	var expired uint64
	for _, command := range in.Commands {
		if existing := written[command.Sequence]; existing != nil {
			if !adapter.EqualCommand(existing, command) {
				return errLinkWrittenConflict
			}
			continue
		}
		if command.Sequence+writtenRetention <= newest {
			expired++
		}
		fresh = append(fresh, command)
	}
	duplicates := uint64(len(in.Commands) - len(fresh))
	old := l.inputs[in.PlayerId]
	if len(fresh) == 0 {
		// Nothing new for the Match. A pending window still reports the newest
		// snapshot this packet observed, as merging the copy used to.
		if old != nil {
			old.ObservedAuthorityTick = max(old.ObservedAuthorityTick, in.ObservedAuthorityTick)
		}
		*copies = true
		l.countLocked(in.PlayerId, playerFwdHandedMainCommands, uint64(len(in.Commands)))
		l.countLocked(in.PlayerId, playerFwdWrittenDuplicateCommands, duplicates)
		return nil
	}
	if old == nil && len(l.inputs) >= adapter.MaxPlayers {
		return errors.New("runtime input slots full")
	}
	merged := make(map[uint64]*runtime.MovementCommand)
	if old != nil {
		for _, command := range old.Commands {
			merged[command.Sequence] = command
		}
	}
	var mergedDuplicates uint64
	for _, command := range fresh {
		if existing := merged[command.Sequence]; existing != nil {
			if !adapter.EqualCommand(existing, command) {
				return errLinkConflict
			}
			mergedDuplicates++
			continue
		}
		merged[command.Sequence] = command
	}
	// Resolved commands stay in the window (the Match measures how late they
	// arrived); only the unresolved part is bounded.
	resolved := l.resolved[in.PlayerId]
	unresolved := 0
	for sequence := range merged {
		if sequence > resolved {
			unresolved++
		}
	}
	if unresolved > adapter.MaxFutureCommands {
		return errLinkCap
	}
	// A merged window reports the newest snapshot any of its parts observed.
	observed := in.ObservedAuthorityTick
	if old != nil && old.ObservedAuthorityTick > observed {
		observed = old.ObservedAuthorityTick
	}
	window := &runtime.PlayerInput{PlayerId: in.PlayerId, MovementEpoch: epoch, LifeGeneration: life, ObservedAuthorityTick: observed}
	for _, command := range merged {
		window.Commands = append(window.Commands, proto.Clone(command).(*runtime.MovementCommand))
	}
	sort.Slice(window.Commands, func(i, j int) bool { return window.Commands[i].Sequence < window.Commands[j].Sequence })
	// Keep the newest resolved commands; the oldest beyond the bound are a
	// drop the Gateway executes. Never marked written, a later copy of them is
	// forwarded again.
	var trimmed uint64
	if first := sort.Search(len(window.Commands), func(i int) bool { return window.Commands[i].Sequence > resolved }); first > maxRetainedResolved {
		trimmed = uint64(first - maxRetainedResolved)
		window.Commands = window.Commands[trimmed:]
	}
	l.inputs[in.PlayerId] = window
	if old != nil {
		l.coalescedInputs++
	}
	l.countLocked(in.PlayerId, playerFwdHandedMainCommands, uint64(len(in.Commands)))
	l.countLocked(in.PlayerId, playerFwdWrittenDuplicateCommands, duplicates)
	l.countLocked(in.PlayerId, playerFwdMergedDuplicateCommands, mergedDuplicates)
	l.countLocked(in.PlayerId, playerDropResolvedTrimCommands, trimmed)
	l.countLocked(in.PlayerId, playerFwdRetentionExpiredCommands, expired)
	l.notify()
	return nil
}

// reject queues a window the Match may refuse as a whole (old or future
// epoch/life, a conflict, beyond the bound): unmerged, unmarked, written after
// the player's main window. reason is the routing attribute it is counted by.
func (l *runtimeLink) reject(in *runtime.PlayerInput, reason playerIngressCounter) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errRuntimeDisconnected
	}
	if in == nil || in.PlayerId == 0 || len(in.Commands) == 0 {
		return adapter.ErrInput
	}
	l.countLocked(in.PlayerId, playerFwdHandedRejectedLaneCommands, uint64(len(in.Commands)))
	l.countLocked(in.PlayerId, reason, 1)
	l.rejectLocked(proto.Clone(in).(*runtime.PlayerInput))
	l.notify()
	return nil
}

// rejectLocked appends to the rejected lane; beyond maxRejectedWindows the
// oldest window is dropped and counted.
func (l *runtimeLink) rejectLocked(in *runtime.PlayerInput) {
	if l.rejected == nil {
		l.rejected = make(map[uint64][]*runtime.PlayerInput)
	}
	lane := append(l.rejected[in.PlayerId], in)
	if len(lane) > maxRejectedWindows {
		l.countLocked(in.PlayerId, playerDropRejectedLaneOverflowCommands, uint64(len(lane[0].Commands)))
		lane = lane[1:]
	}
	l.rejected[in.PlayerId] = lane
}

func (l *runtimeLink) acknowledge(playerID, epoch, life, sequence uint64) {
	l.mu.Lock()
	defer l.mu.Unlock()
	previous := l.epochs[playerID]
	if previous == 0 {
		previous = 1
	}
	previousLife := l.lives[playerID]
	if previousLife == 0 {
		previousLife = 1
	}
	if life == 0 || epoch < previous || life < previousLife || (life > previousLife && epoch <= previous) {
		return
	}
	if l.epochs == nil {
		l.epochs = make(map[uint64]uint64)
	}
	l.epochs[playerID] = epoch
	if l.lives == nil {
		l.lives = make(map[uint64]uint64)
	}
	l.lives[playerID] = life
	if epoch > previous || life > previousLife {
		// The pending window of the old epoch/life goes to the Match unmerged,
		// for it to refuse. Sequences restart at 1 in the new epoch, so the
		// written set must be cleared or the new epoch's first commands would
		// be swallowed as copies.
		if in := l.inputs[playerID]; in != nil {
			l.countLocked(playerID, playerFwdRoutedRotationInputs, 1)
			l.rejectLocked(in)
		}
		delete(l.inputs, playerID)
		delete(l.written, playerID)
		delete(l.newestWritten, playerID)
	}
	// Resolved commands stay until written: the Match measures late arrivals.
	if l.resolved == nil {
		l.resolved = make(map[uint64]uint64)
	}
	l.resolved[playerID] = sequence
}

func (l *runtimeLink) notify() {
	select {
	case l.wake <- struct{}{}:
	default:
	}
}

type inputLane uint8

const (
	noLane inputLane = iota // controls and action batches
	mainLane
	rejectedLane
)

// linkWrite is one message taken for the writer and what its write accounts
// for once l.conn.Write succeeded.
type linkWrite struct {
	message  *runtime.RuntimeEnvelope
	lane     inputLane
	player   uint64
	commands uint64
	resolved uint64 // main-lane commands at or below the resolved cursor when taken
	// An ActionBatch: the shots and the acknowledgement no earlier pass took.
	shots, acks uint64
}

// take empties the queues for one write pass: controls in order, then per
// player (ascending id) the main window followed by the rejected lane, then
// the due action batches. Only the main window is marked written, here, in
// the same critical section that takes it; trimmed or overflowed commands
// were removed before and are never marked. The action requests and the
// acknowledgement a batch carries are marked taken the same way.
func (l *runtimeLink) take() []linkWrite {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.takes++
	writes := make([]linkWrite, 0, len(l.controls)+len(l.inputs)+len(l.rejected))
	for _, message := range l.controls {
		writes = append(writes, linkWrite{message: message})
	}
	l.controls = nil
	ids := make([]uint64, 0, len(l.inputs)+len(l.rejected))
	for id := range l.inputs {
		ids = append(ids, id)
	}
	for id := range l.rejected {
		if l.inputs[id] == nil {
			ids = append(ids, id)
		}
	}
	sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
	for _, id := range ids {
		if in := l.inputs[id]; in != nil {
			l.markWrittenLocked(in)
			writes = appendWindow(writes, in, mainLane, l.resolved[id])
		}
		for _, in := range l.rejected[id] {
			writes = appendWindow(writes, in, rejectedLane, 0)
		}
		delete(l.inputs, id)
		delete(l.rejected, id)
	}
	for _, message := range l.actionBatch(time.Now()) {
		writes = append(writes, l.takeActionsLocked(message))
	}
	return writes
}

// appendWindow keeps each IPC input within the 12-command bound the Match
// enforces (IpcHost drops a larger input whole); a merge can exceed it. The
// main lane is cut at the resolved cursor first, so the resolved prefix does
// not shift the 12-command boundaries of the unresolved part: as many IPC
// inputs carry new commands as before 08c (each samples the reference age).
func appendWindow(writes []linkWrite, in *runtime.PlayerInput, lane inputLane, resolved uint64) []linkWrite {
	if lane == mainLane {
		first := sort.Search(len(in.Commands), func(i int) bool { return in.Commands[i].Sequence > resolved })
		writes = appendChunks(writes, in, in.Commands[:first], lane, resolved)
		return appendChunks(writes, in, in.Commands[first:], lane, resolved)
	}
	return appendChunks(writes, in, in.Commands, lane, resolved)
}

func appendChunks(writes []linkWrite, in *runtime.PlayerInput, commands []*runtime.MovementCommand, lane inputLane, resolved uint64) []linkWrite {
	for len(commands) > 0 {
		count := min(len(commands), adapter.MaxPendingCommands)
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Input{Input: &runtime.PlayerInput{PlayerId: in.PlayerId, Commands: commands[:count], MovementEpoch: in.MovementEpoch,
			LifeGeneration: in.LifeGeneration, ObservedAuthorityTick: in.ObservedAuthorityTick}}
		w := linkWrite{message: e, lane: lane, player: in.PlayerId, commands: uint64(count)}
		if lane == mainLane {
			for _, command := range commands[:count] {
				if command.Sequence <= resolved {
					w.resolved++
				}
			}
		}
		writes = append(writes, w)
		commands = commands[count:]
	}
	return writes
}

// markWrittenLocked records a main window taken for the writer and expires
// what is writtenRetention behind the newest written sequence.
func (l *runtimeLink) markWrittenLocked(in *runtime.PlayerInput) {
	if l.written == nil {
		l.written = make(map[uint64]map[uint64]*runtime.MovementCommand)
		l.newestWritten = make(map[uint64]uint64)
	}
	set := l.written[in.PlayerId]
	if set == nil {
		set = make(map[uint64]*runtime.MovementCommand)
		l.written[in.PlayerId] = set
	}
	newest := l.newestWritten[in.PlayerId]
	for _, command := range in.Commands {
		set[command.Sequence] = command
		newest = max(newest, command.Sequence)
	}
	l.newestWritten[in.PlayerId] = newest
	for sequence := range set {
		if sequence+writtenRetention <= newest {
			delete(set, sequence)
		}
	}
}

// wroteLocked counts a message whose l.conn.Write succeeded.
func (l *runtimeLink) wroteLocked(w linkWrite) {
	switch w.lane {
	case mainLane:
		l.countLocked(w.player, playerFwdLinkWrittenMainCommands, w.commands)
		l.countLocked(w.player, playerFwdResolvedAtWriteCommands, w.resolved)
	case rejectedLane:
		l.countLocked(w.player, playerFwdLinkWrittenRejectedLaneCommands, w.commands)
	}
	// The shot requests this pass first took (D59); a resent one was counted
	// by the pass that first took it.
	l.countLocked(w.player, playerFwdLinkWrittenActionsShots, w.shots)
}

// write sends one pass in order. A failed Marshal or Write fails the link;
// that message and the rest of the pass are abandoned and counted, because
// the Match may not have received them.
func (l *runtimeLink) write(writes []linkWrite) error {
	for i, w := range writes {
		payload, err := proto.Marshal(w.message)
		if err == nil {
			started := time.Now()
			err = runtimeWrite(l.conn, payload, 3*time.Second)
			elapsed := time.Since(started)
			l.mu.Lock()
			if elapsed > l.maxWriteAge {
				l.maxWriteAge = elapsed
			}
			if err == nil {
				l.wroteLocked(w)
			}
			l.mu.Unlock()
		}
		if actions := w.message.GetActions(); err == nil && actions != nil {
			l.actionWritten(actions.PlayerId, time.Now())
		}
		if err != nil {
			l.mu.Lock()
			for _, abandoned := range writes[i:] {
				if abandoned.lane != noLane {
					l.countLocked(abandoned.player, playerDropAbandonedOnCloseCommands, abandoned.commands)
				}
				l.countLocked(abandoned.player, playerDropAbandonedOnCloseActionsShots, abandoned.shots)
				l.countLocked(abandoned.player, playerDropAbandonedOnCloseActionsAcks, abandoned.acks)
			}
			l.mu.Unlock()
			return err
		}
	}
	return nil
}

func (l *runtimeLink) run(ctx context.Context, receive func(*runtime.RuntimeEnvelope), failed func(error)) {
	fail := func(err error) { l.once.Do(func() { l.close(); failed(err) }) }
	l.mu.Lock()
	l.writerStarted = true
	exited := l.writerExitedLocked()
	l.mu.Unlock()
	stopped := func() { l.writerOnce.Do(func() { close(exited) }) }
	go func() {
		defer stopped()
		// Inputs, controls, actions and results wake the loop (l.wake). Action
		// batches keep a strict ActionSendRate interval from each completed
		// write; this timer wakes the loop at the earliest such deadline, so a
		// resend neither waits for an unrelated wake nor rides every wake.
		timer := time.NewTimer(writerIdleWait)
		defer timer.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-l.done:
				return
			case <-l.wake:
			case <-timer.C:
			case <-l.diagnosticWake:
				// Only the diagnostic slot: no take(), no timer reset (pv7 §2).
				if err := l.writeDiagnostic(); err != nil {
					stopped()
					fail(err)
					return
				}
				continue
			}
			if err := l.write(l.take()); err != nil {
				// This goroutine counts nothing more. The failure reaches
				// runtimeFailed on it, whose final flush waits for it.
				stopped()
				fail(err)
				return
			}
			wait := writerIdleWait
			l.mu.Lock()
			if deadline, ok := l.nextActionDeadline(); ok {
				wait = max(time.Until(deadline), 0)
			}
			l.mu.Unlock()
			timer.Reset(wait)
		}
	}()
	// Liveness counts from run, not from when the reader goroutine starts.
	live := l.now()
	go func() {
		for {
			// Only Snapshot, JoinResult, Evicted and Error refresh liveness
			// (pv7 §2): results and clock replies are sent by the Match's IPC
			// thread, which answers even when the simulation is stuck.
			remaining := runtimeLiveness - l.now().Sub(live)
			if remaining <= 0 {
				// A timeout of zero or less would cancel the read deadline
				// (framing), so the link fails here as a timed-out read does.
				if ctx.Err() == nil {
					fail(fmt.Errorf("runtime liveness: %w", os.ErrDeadlineExceeded))
				}
				return
			}
			payload, err := l.conn.Read(remaining)
			var message runtime.RuntimeEnvelope
			if err == nil {
				var version uint64
				var other bool
				if version, other, err = decodeRuntime(payload, &message); other {
					err = fmt.Errorf("runtime protocol version mismatch: expected %d, received %d", adapter.RuntimeVersion, version)
				} else if err == nil && message.Message == nil {
					err = unknownRuntimeMessage(&message)
				}
			}
			if err != nil {
				if ctx.Err() == nil {
					fail(err)
				}
				return
			}
			switch message.Message.(type) {
			case *runtime.RuntimeEnvelope_JoinResult, *runtime.RuntimeEnvelope_Snapshot, *runtime.RuntimeEnvelope_Error, *runtime.RuntimeEnvelope_Evicted:
				live = l.now()
				receive(&message)
			case *runtime.RuntimeEnvelope_ActionResults:
				receive(&message)
			case *runtime.RuntimeEnvelope_Clock:
				l.observeClock(message.GetClock(), time.Now())
			default:
				fail(errors.New("unexpected runtime message"))
				return
			}
		}
	}()
}

// heartbeat queues the runtime link heartbeat built now (the Server's 1 Hz
// diagnostic timer, pv7 §3) and closes the clock window when it is due; the
// window is returned for the log. Diagnostics only.
func (l *runtimeLink) heartbeat(now time.Time) (clockWindow, bool) {
	l.clockMu.Lock()
	sample := &runtime.ClockSample{}
	if origin, receive, ok := l.clock.echo(); ok {
		sample.OriginUs, sample.ReceiveUs = &origin, &receive
	}
	window, rolled := l.clock.roll(gatewayMicros(now), false)
	l.clockMu.Unlock()
	l.queueDiagnostic(&runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion, Message: &runtime.RuntimeEnvelope_Clock{Clock: sample}})
	return window, rolled
}

// finalClockWindow closes the clock window with the link.
func (l *runtimeLink) finalClockWindow(now time.Time) clockWindow {
	l.clockMu.Lock()
	defer l.clockMu.Unlock()
	window, _ := l.clock.roll(gatewayMicros(now), true)
	return window
}

// queueDiagnostic puts a diagnostic envelope in the single slot (newest wins,
// a replaced one is counted) and wakes the writer on its own channel only.
func (l *runtimeLink) queueDiagnostic(message *runtime.RuntimeEnvelope) {
	l.mu.Lock()
	if l.closed {
		l.mu.Unlock()
		return
	}
	replaced := l.diagnostic != nil
	l.diagnostic = message
	l.mu.Unlock()
	if replaced {
		l.clockMu.Lock()
		l.clock.noteReplaced()
		l.clockMu.Unlock()
	}
	select {
	case l.diagnosticWake <- struct{}{}:
	default:
	}
}

// writeDiagnostic writes the slot's envelope, if any. A heartbeat's transmit
// is stamped here, when it is serialized (pv7 §3).
func (l *runtimeLink) writeDiagnostic() error {
	l.mu.Lock()
	message := l.diagnostic
	l.diagnostic = nil
	l.mu.Unlock()
	if message == nil {
		return nil
	}
	if sample := message.GetClock(); sample != nil {
		l.clockMu.Lock()
		sample.TransmitUs = l.clock.stamp(gatewayMicros(time.Now()))
		l.clockMu.Unlock()
	}
	payload, err := proto.Marshal(message)
	if err == nil {
		err = runtimeWrite(l.conn, payload, 3*time.Second)
	}
	return err
}

// observeClock measures a Match clock reply read at receivedAt. Diagnostics
// only: nothing here refreshes liveness or reaches a decision.
func (l *runtimeLink) observeClock(sample *runtime.ClockSample, receivedAt time.Time) {
	l.clockMu.Lock()
	defer l.clockMu.Unlock()
	l.clock.observe(clockStamp{origin: sample.OriginUs, receive: sample.ReceiveUs, transmit: sample.TransmitUs}, gatewayMicros(receivedAt))
}

func (l *runtimeLink) writerExitedLocked() chan struct{} {
	if l.writerExited == nil {
		l.writerExited = make(chan struct{})
	}
	return l.writerExited
}

// waitWriter returns once the writer goroutine exited, at once if run never
// started it. Only after close(): the writer runs until the link closes or the
// context ends.
func (l *runtimeLink) waitWriter() {
	l.mu.Lock()
	started, exited := l.writerStarted, l.writerExitedLocked()
	l.mu.Unlock()
	if started {
		<-exited
	}
}

func (l *runtimeLink) close() {
	l.mu.Lock()
	if l.closed {
		l.mu.Unlock()
		return
	}
	l.closed = true
	close(l.done)
	l.controls = nil
	l.diagnostic = nil
	for id, in := range l.inputs {
		l.countLocked(id, playerDropClearedOnCloseCommands, uint64(len(in.Commands)))
	}
	for id, lane := range l.rejected {
		for _, in := range lane {
			l.countLocked(id, playerDropClearedOnCloseCommands, uint64(len(in.Commands)))
		}
	}
	clear(l.inputs)
	clear(l.resolved)
	clear(l.rejected)
	clear(l.written)
	clear(l.newestWritten)
	clear(l.epochs)
	clear(l.lives)
	for id, w := range l.actionWindows {
		shots, acks := w.untaken()
		l.countLocked(id, playerDropClearedOnCloseActionsShots, shots)
		l.countLocked(id, playerDropClearedOnCloseActionsAcks, acks)
	}
	clear(l.actionWindows)
	clear(l.actionStats)
	log.Printf("runtime transport coalesced_input_windows=%d max_write_age_us=%d", l.coalescedInputs, l.maxWriteAge.Microseconds())
	l.mu.Unlock()
	_ = l.conn.Close()
	l.notify()
}
