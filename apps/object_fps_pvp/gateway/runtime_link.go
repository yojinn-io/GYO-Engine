package gateway

import (
	"context"
	"errors"
	"sort"
	"sync"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev1"
)

// This queue is product-owned: ordered lifecycle requests and replaceable input
// intentionally have different semantics. Neither lane blocks the World.
type runtimeLink struct {
	conn     *framing.TCPConnection
	mu       sync.Mutex
	controls []*runtime.RuntimeEnvelope
	inputs   map[uint64]*runtime.PlayerInput
	wake     chan struct{}
	done     chan struct{}
	closed   bool
	once     sync.Once
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
	return &runtimeLink{conn: conn, inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1), done: make(chan struct{})}, ready, nil
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
	if old := l.inputs[in.PlayerId]; old == nil || old.InputSequence < in.InputSequence {
		if old == nil && len(l.inputs) >= adapter.MaxPlayers {
			return errors.New("runtime input slots full")
		}
		l.inputs[in.PlayerId] = in
	}
	l.notify()
	return nil
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
		e := envelope()
		e.Message = &runtime.RuntimeEnvelope_Input{Input: l.inputs[id]}
		batch = append(batch, e)
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
					err = l.conn.Write(payload, time.Second)
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
	l.mu.Unlock()
	_ = l.conn.Close()
	l.notify()
}
