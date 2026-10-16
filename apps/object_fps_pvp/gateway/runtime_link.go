package gateway

import (
	"context"
	"errors"
	"fmt"
	"log"
	"os"
	"slices"
	"sync"
	"time"

	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

// This queue is product-owned: lifecycle requests are never dropped, input
// windows are bounded per player (pv7 §2a). Neither blocks the World.
var errRuntimeDisconnected = errors.New("runtime disconnected")

type runtimeLink struct {
	actionWindows map[uint64]*actionWindow
	conn          *framing.TCPConnection
	mu            sync.Mutex
	wake          chan struct{}
	done          chan struct{}
	closed        bool
	once          sync.Once
	maxWriteAge   time.Duration
	// pv7 §2a: controls and input windows in arrival order, so a Leave never
	// overtakes the player's queued inputs. The Match decides what is
	// resolved, stale, copied or conflicting (D49); the link forwards each
	// Client datagram's window as it came. Controls are never dropped (more
	// than maxQueuedControls fail the link); each player keeps at most
	// maxQueuedInputs windows, the oldest dropped and reported beyond that.
	queue          []queuedMessage
	queuedControls int
	queuedInputs   map[uint64]int
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
	// The pv7 GatewayDropReport (drop_report.go): the ledger, and whether the
	// timer asked for a pass of reports with the heartbeat.
	drops    *dropLedger
	dropPass bool
	// When the writer last wrote a pass of reports: a stream reports at most
	// once a second (pv7 §4). Only the writer goroutine touches it.
	lastDropPass time.Time
}

// errLinkClosing ends a write pass the close interrupted: what it had not
// written is abandoned, and the close (not the writer) ends the link.
var errLinkClosing = errors.New("runtime link closing")

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
	return &runtimeLink{conn: conn, wake: make(chan struct{}, 1), done: make(chan struct{}),
		diagnosticWake: make(chan struct{}, 1), clock: newClockSync(gatewayMicros(time.Now())), now: time.Now, drops: newDropLedger()}, ready, nil
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

// One queued control or input window. An input keeps the identity its
// overflow is reported with (pv7 §4, reason 32).
type queuedMessage struct {
	message  *runtime.RuntimeEnvelope
	input    bool
	player   uint64
	commands uint64
	event    dropEvent
}

const (
	maxQueuedControls = 64
	// Not less than 64 windows per player (pv7 §2a); pv6 bounded 32 resolved
	// commands.
	maxQueuedInputs = 64
)

func envelope() *runtime.RuntimeEnvelope {
	return &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion}
}

func (l *runtimeLink) control(message *runtime.RuntimeEnvelope) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errRuntimeDisconnected
	}
	if l.queuedControls >= maxQueuedControls {
		return errors.New("runtime lifecycle queue full")
	}
	if leave := message.GetLeave(); leave != nil {
		l.forgetLocked(leave.PlayerId)
	}
	l.queue = append(l.queue, queuedMessage{message: message})
	l.queuedControls++
	l.notify()
	return nil
}

// forget drops a player's action window. Its queued input windows stay: they
// go to the Match in arrival order, which counts what comes after the player
// left (pv7 §2a). A Leave does it before queuing the control; an eviction
// sends no Leave (the Match already removed the player), so the Gateway calls
// it. The action requests and acknowledgement no write pass took are counted
// into the player's ingress counters, which outlive it.
func (l *runtimeLink) forget(player uint64) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.forgetLocked(player)
}

func (l *runtimeLink) forgetLocked(player uint64) {
	if w := l.actionWindows[player]; w != nil {
		shots, acks := w.untaken()
		l.countLocked(player, playerDropDiscardedOnLeaveActionsShots, shots)
		l.countLocked(player, playerDropDiscardedOnLeaveActionsAcks, acks)
	}
	delete(l.actionWindows, player)
	delete(l.actionStats, player)
}

func (l *runtimeLink) countLocked(player uint64, counter playerIngressCounter, n uint64) {
	l.countEventLocked(player, counter, n, dropEvent{at: time.Now()})
}

// countEventLocked is countLocked with the identity a reported discard is
// sampled with.
func (l *runtimeLink) countEventLocked(player uint64, counter playerIngressCounter, n uint64, e dropEvent) {
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
	// The same entry counts the reported discards cumulatively (pv7 §4).
	l.countDrop(player, counter, n, e)
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

// input queues one Client datagram's window as it came (pv7 §2a): one IPC
// message per datagram, behind everything queued before it. Beyond
// maxQueuedInputs windows of the player, its oldest queued window is dropped
// and reported with its identity; e is this window's.
func (l *runtimeLink) input(in *runtime.PlayerInput, e dropEvent) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errRuntimeDisconnected
	}
	if in == nil || in.PlayerId == 0 || len(in.Commands) == 0 || len(in.Commands) > adapter.MaxPendingCommands {
		return adapter.ErrInput
	}
	if l.queuedInputs == nil {
		l.queuedInputs = make(map[uint64]int)
	}
	if l.queuedInputs[in.PlayerId] >= maxQueuedInputs {
		for i, q := range l.queue {
			if q.input && q.player == in.PlayerId {
				l.countEventLocked(q.player, playerDropIpcQueueOverflowInputPackets, 1, q.event)
				l.countLocked(q.player, playerDropIpcQueueOverflowInputCommands, q.commands)
				l.queue = slices.Delete(l.queue, i, i+1)
				l.queuedInputs[in.PlayerId]--
				break
			}
		}
	}
	message := envelope()
	message.Message = &runtime.RuntimeEnvelope_Input{Input: in}
	l.queue = append(l.queue, queuedMessage{message: message, input: true, player: in.PlayerId, commands: uint64(len(in.Commands)), event: e})
	l.queuedInputs[in.PlayerId]++
	l.countLocked(in.PlayerId, playerFwdHandedMainCommands, uint64(len(in.Commands)))
	l.notify()
	return nil
}

func (l *runtimeLink) notify() {
	select {
	case l.wake <- struct{}{}:
	default:
	}
}

type inputLane uint8

const (
	noLane   inputLane = iota // controls and action batches
	mainLane                  // input windows
)

// linkWrite is one message taken for the writer and what its write accounts
// for once l.conn.Write succeeded.
type linkWrite struct {
	message  *runtime.RuntimeEnvelope
	lane     inputLane
	player   uint64
	commands uint64
	// An ActionBatch: the shots and the acknowledgement no earlier pass took.
	shots, acks uint64
}

// take empties the queue for one write pass, in arrival order, then the due
// action batches. The action requests and the acknowledgement a batch carries
// are marked taken in the same critical section that takes them.
func (l *runtimeLink) take() []linkWrite {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.takes++
	writes := make([]linkWrite, 0, len(l.queue))
	for _, q := range l.queue {
		if q.input {
			writes = append(writes, linkWrite{message: q.message, lane: mainLane, player: q.player, commands: q.commands})
		} else {
			writes = append(writes, linkWrite{message: q.message})
		}
	}
	l.queue = nil
	l.queuedControls = 0
	clear(l.queuedInputs)
	for _, message := range l.actionBatch(time.Now()) {
		writes = append(writes, l.takeActionsLocked(message))
	}
	return writes
}

// wroteLocked counts a message whose l.conn.Write succeeded.
func (l *runtimeLink) wroteLocked(w linkWrite) {
	switch w.lane {
	case mainLane:
		l.countLocked(w.player, playerFwdLinkWrittenMainCommands, w.commands)
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
		select {
		case <-l.done: // closing: the rest is abandoned, not written
			err = errLinkClosing
		default:
		}
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
				if err := l.writeDiagnostic(); errors.Is(err, errLinkClosing) {
					return
				} else if err != nil {
					stopped()
					fail(err)
					return
				}
				continue
			}
			if err := l.write(l.take()); errors.Is(err, errLinkClosing) {
				return
			} else if err != nil {
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
	l.mu.Lock()
	l.dropPass = true // the drop reports go with each timer tick
	l.mu.Unlock()
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
	l.wakeDiagnostic()
}

// writeDiagnostic writes the slot's envelope, if any, then the drop reports
// the timer asked for. A heartbeat's transmit is stamped here, when it is
// serialized (pv7 §3).
func (l *runtimeLink) writeDiagnostic() error {
	l.mu.Lock()
	message, pass := l.diagnostic, l.dropPass
	l.diagnostic, l.dropPass = nil, false
	l.mu.Unlock()
	select {
	case <-l.done:
		return errLinkClosing
	default:
	}
	if pass && !l.lastDropPass.IsZero() {
		if wait := time.Second - time.Since(l.lastDropPass); wait > 0 {
			// Too soon after the previous pass: it waits for the second to end.
			l.mu.Lock()
			l.dropPass = true
			l.mu.Unlock()
			time.AfterFunc(wait, l.wakeDiagnostic)
			pass = false
		}
	}
	if message != nil {
		if sample := message.GetClock(); sample != nil {
			l.clockMu.Lock()
			sample.TransmitUs = l.clock.stamp(gatewayMicros(time.Now()))
			l.clockMu.Unlock()
		}
		if err := l.writeEnvelope(message); err != nil {
			return err
		}
	}
	if pass {
		l.lastDropPass = time.Now()
		return l.writeDrops(false)
	}
	return nil
}

// wakeDiagnostic wakes the writer for the diagnostic slot only.
func (l *runtimeLink) wakeDiagnostic() {
	select {
	case l.diagnosticWake <- struct{}{}:
	default:
	}
}

// writeEnvelope writes one diagnostic envelope.
func (l *runtimeLink) writeEnvelope(message *runtime.RuntimeEnvelope) error {
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
// gracefulWriterWait bounds a graceful close's wait for the writer: longer
// than one runtime write's deadline. Tests shorten it.
var gracefulWriterWait = 3500 * time.Millisecond

// waitWriterFor is waitWriter with a bound; false when the writer still runs.
func (l *runtimeLink) waitWriterFor(limit time.Duration) bool {
	l.mu.Lock()
	started, exited := l.writerStarted, l.writerExitedLocked()
	l.mu.Unlock()
	if !started {
		return true
	}
	select {
	case <-exited:
		return true
	case <-time.After(limit):
		return false
	}
}

func (l *runtimeLink) waitWriter() {
	l.mu.Lock()
	started, exited := l.writerStarted, l.writerExitedLocked()
	l.mu.Unlock()
	if started {
		<-exited
	}
}

func (l *runtimeLink) close() { l.shutdown(false) }

// closeGracefully closes the link after writing every stream's final drop
// report (pv7 §4), once the writer exited: only on a graceful close.
func (l *runtimeLink) closeGracefully() { l.shutdown(true) }

func (l *runtimeLink) shutdown(graceful bool) {
	l.mu.Lock()
	if l.closed {
		l.mu.Unlock()
		return
	}
	l.closed = true
	close(l.done)
	l.diagnostic = nil
	for _, q := range l.queue {
		if q.input {
			l.countLocked(q.player, playerDropClearedOnCloseCommands, q.commands)
		}
	}
	l.queue = nil
	l.queuedControls = 0
	clear(l.queuedInputs)
	for id, w := range l.actionWindows {
		shots, acks := w.untaken()
		l.countLocked(id, playerDropClearedOnCloseActionsShots, shots)
		l.countLocked(id, playerDropClearedOnCloseActionsAcks, acks)
	}
	clear(l.actionWindows)
	clear(l.actionStats)
	// The link no longer merges windows (pv7 §2a); the key stays, always 0.
	log.Printf("runtime transport coalesced_input_windows=0 max_write_age_us=%d", l.maxWriteAge.Microseconds())
	l.mu.Unlock()
	if graceful {
		// The writer stops at its next frame (done is closed); a write in
		// progress ends within its own deadline. Only then is the connection
		// free for the finals: one writer at a time.
		if l.waitWriterFor(gracefulWriterWait) {
			if err := l.writeDrops(true); err != nil {
				log.Printf("drop report final writes failed: %v", err)
			}
		} else {
			log.Print("drop report finals not written: the writer did not stop in time")
		}
	}
	_ = l.conn.Close()
	l.notify()
}
