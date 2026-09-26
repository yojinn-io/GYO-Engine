package gateway

import (
	"context"
	"errors"
	"log"
	"sort"
	"sync"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev3"
)

// This queue is product-owned: ordered lifecycle requests and bounded command
// windows intentionally have different semantics. Neither lane blocks the World.
type runtimeLink struct {
	conn            *framing.TCPConnection
	mu              sync.Mutex
	controls        []*runtime.RuntimeEnvelope
	inputs          map[uint64]*runtime.PlayerInput
	epochs          map[uint64]uint64 // Advanced only by authoritative snapshots.
	wake            chan struct{}
	done            chan struct{}
	closed          bool
	once            sync.Once
	coalescedInputs uint64
	maxWriteAge     time.Duration
}

func connectRuntime(ctx context.Context, address string) (*runtimeLink, *runtime.Ready, error) {
	conn, err := framing.DialTCP(ctx, address, 3*time.Second)
	if err != nil {
		return nil, nil, err
	}
	frame, err := conn.Read(3 * time.Second)
	var envelope runtime.RuntimeEnvelope
	if err == nil {
		err = proto.Unmarshal(frame, &envelope)
	}
	ready := envelope.GetReady()
	if err == nil && (envelope.ProtocolVersion != adapter.RuntimeVersion || ready == nil ||
		ready.ArenaId == "" || ready.ArenaVersion == 0 || ready.TickRate != adapter.AuthorityTickRate ||
		ready.SnapshotIntervalTicks != adapter.SnapshotIntervalTicks || ready.MaxPlayers != adapter.MaxPlayers) {
		err = errors.New("runtime readiness contract mismatch")
	}
	if err != nil {
		_ = conn.Close()
		return nil, nil, err
	}
	return &runtimeLink{conn: conn, inputs: make(map[uint64]*runtime.PlayerInput), epochs: make(map[uint64]uint64), wake: make(chan struct{}, 1), done: make(chan struct{})}, ready, nil
}

func envelope() *runtime.RuntimeEnvelope {
	return &runtime.RuntimeEnvelope{ProtocolVersion: adapter.RuntimeVersion}
}

func (l *runtimeLink) control(message *runtime.RuntimeEnvelope) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errors.New("runtime disconnected")
	}
	if len(l.controls) >= 64 {
		return errors.New("runtime lifecycle queue full")
	}
	if leave := message.GetLeave(); leave != nil {
		delete(l.inputs, leave.PlayerId)
		delete(l.epochs, leave.PlayerId)
	}
	l.controls = append(l.controls, message)
	l.notify()
	return nil
}

func (l *runtimeLink) input(in *runtime.PlayerInput) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.closed {
		return errors.New("runtime disconnected")
	}
	if in == nil || in.PlayerId == 0 || in.MovementEpoch == 0 || len(in.Commands) == 0 || len(in.Commands) > adapter.MaxPendingCommands {
		return adapter.ErrInput
	}
	epoch := l.epochs[in.PlayerId]
	if epoch == 0 {
		epoch = 1
	}
	if in.MovementEpoch < epoch {
		return nil
	}
	if in.MovementEpoch != epoch {
		return adapter.ErrInput
	}
	var previous uint64
	for _, command := range in.Commands {
		if command == nil || command.Sequence <= previous {
			return adapter.ErrInput
		}
		previous = command.Sequence
	}
	old := l.inputs[in.PlayerId]
	if old == nil && len(l.inputs) >= adapter.MaxPlayers {
		return errors.New("runtime input slots full")
	}
	merged := make(map[uint64]*runtime.MovementCommand)
	if old != nil {
		for _, command := range old.Commands {
			merged[command.Sequence] = command
		}
	}
	for _, command := range in.Commands {
		if existing := merged[command.Sequence]; existing != nil && !adapter.EqualCommand(existing, command) {
			return adapter.ErrInput
		}
		merged[command.Sequence] = command
	}
	if len(merged) > adapter.MaxFutureCommands {
		return adapter.ErrInput
	}
	window := &runtime.PlayerInput{PlayerId: in.PlayerId, MovementEpoch: epoch}
	for _, command := range merged {
		window.Commands = append(window.Commands, proto.Clone(command).(*runtime.MovementCommand))
	}
	sort.Slice(window.Commands, func(i, j int) bool { return window.Commands[i].Sequence < window.Commands[j].Sequence })
	l.inputs[in.PlayerId] = window
	if old != nil {
		l.coalescedInputs++
	}
	l.notify()
	return nil
}

func (l *runtimeLink) acknowledge(playerID, epoch, sequence uint64) {
	l.mu.Lock()
	defer l.mu.Unlock()
	previous := l.epochs[playerID]
	if previous == 0 {
		previous = 1
	}
	if epoch < previous {
		return
	}
	if l.epochs == nil {
		l.epochs = make(map[uint64]uint64)
	}
	l.epochs[playerID] = epoch
	if epoch > previous {
		delete(l.inputs, playerID)
	}
	if in := l.inputs[playerID]; in != nil {
		first := sort.Search(len(in.Commands), func(i int) bool { return in.Commands[i].Sequence > sequence })
		in.Commands = in.Commands[first:]
		if len(in.Commands) == 0 {
			delete(l.inputs, playerID)
		}
	}
}

func (l *runtimeLink) notify() {
	select {
	case l.wake <- struct{}{}:
	default:
	}
}

func (l *runtimeLink) batch() []*runtime.RuntimeEnvelope {
	l.mu.Lock()
	defer l.mu.Unlock()
	batch := l.controls
	l.controls = nil
	ids := make([]uint64, 0, len(l.inputs))
	for id := range l.inputs {
		ids = append(ids, id)
	}
	sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
	for _, id := range ids {
		commands := l.inputs[id].Commands
		// Merging overlapping windows can exceed a single client batch. Preserve
		// every command and keep each IPC input within the same 12-command bound.
		for len(commands) > 0 {
			count := min(len(commands), adapter.MaxPendingCommands)
			e := envelope()
			e.Message = &runtime.RuntimeEnvelope_Input{Input: &runtime.PlayerInput{PlayerId: id, Commands: commands[:count], MovementEpoch: l.inputs[id].MovementEpoch}}
			batch = append(batch, e)
			commands = commands[count:]
		}
		delete(l.inputs, id)
	}
	return batch
}

func (l *runtimeLink) run(ctx context.Context, receive func(*runtime.RuntimeEnvelope), failed func(error)) {
	fail := func(err error) { l.once.Do(func() { l.close(); failed(err) }) }
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case <-l.done:
				return
			case <-l.wake:
			}
			for _, message := range l.batch() {
				payload, err := proto.Marshal(message)
				if err == nil {
					started := time.Now()
					err = l.conn.Write(payload, time.Second)
					elapsed := time.Since(started)
					l.mu.Lock()
					if elapsed > l.maxWriteAge {
						l.maxWriteAge = elapsed
					}
					l.mu.Unlock()
				}
				if err != nil {
					fail(err)
					return
				}
			}
		}
	}()
	go func() {
		for {
			payload, err := l.conn.Read(5 * time.Second)
			var message runtime.RuntimeEnvelope
			if err == nil {
				err = proto.Unmarshal(payload, &message)
			}
			if err == nil && (message.ProtocolVersion != adapter.RuntimeVersion || message.Message == nil) {
				err = errors.New("runtime protocol mismatch")
			}
			if err != nil {
				if ctx.Err() == nil {
					fail(err)
				}
				return
			}
			switch message.Message.(type) {
			case *runtime.RuntimeEnvelope_JoinResult, *runtime.RuntimeEnvelope_Snapshot, *runtime.RuntimeEnvelope_Error:
				receive(&message)
			default:
				fail(errors.New("unexpected runtime message"))
				return
			}
		}
	}()
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
	clear(l.inputs)
	clear(l.epochs)
	log.Printf("runtime transport coalesced_input_windows=%d max_write_age_us=%d", l.coalescedInputs, l.maxWriteAge.Microseconds())
	l.mu.Unlock()
	_ = l.conn.Close()
	l.notify()
}
