package gateway

import (
	"errors"
	"testing"

	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

func dedupLink() *runtimeLink {
	return &runtimeLink{inputs: make(map[uint64]*runtime.PlayerInput), wake: make(chan struct{}, 1)}
}

func dedupWindow(epoch, first, last uint64, forward float32) *runtime.PlayerInput {
	in := &runtime.PlayerInput{LifeGeneration: 1, MovementEpoch: epoch, PlayerId: 1}
	for sequence := first; sequence <= last; sequence++ {
		in.Commands = append(in.Commands, &runtime.MovementCommand{Sequence: sequence, MoveForward: forward})
	}
	return in
}

// Upstream "event = new content" (08b): the first copy of each identity
// reaches the Match even when resolved; identical later copies do not.
func TestRuntimeLinkDropsCopiesOfWrittenCommandsAndCountsThem(t *testing.T) {
	l := dedupLink()
	if err := l.input(dedupWindow(1, 1, 3, 1)); err != nil {
		t.Fatal(err)
	}
	sequences(l)
	if err := l.input(dedupWindow(1, 1, 4, 1)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 1 || got[0] != 4 {
		t.Fatalf("written duplicate reached the runtime: %v", got)
	}
	if c := (linkTotals{}).take(l).of(1); c[playerFwdWrittenDuplicateCommands] != 3 {
		t.Fatalf("written duplicates were not counted: %d", c[playerFwdWrittenDuplicateCommands])
	}
	if err := l.input(dedupWindow(1, 2, 2, -1)); !errors.Is(err, adapter.ErrInput) || linkRefusalReason(err) != playerFwdRoutedWrittenConflictInputs {
		t.Fatalf("changed content of a written command was not refused: %v", err)
	}
}

// Marking happens when take() hands a window to the writer: a command trimmed
// before the write is not "written", so its next copy still reaches the Match.
func TestRuntimeLinkMarksOnlyTakenWindows(t *testing.T) {
	l := dedupLink()
	l.acknowledge(1, 1, 1, 100)
	for first := uint64(1); first <= 37; first += 12 {
		if err := l.input(dedupWindow(1, first, first+11, 1)); err != nil {
			t.Fatal(err)
		}
	}
	if got := sequences(l); len(got) != maxRetainedResolved || got[0] != 17 {
		t.Fatalf("unexpected retention: %v", got)
	}
	if err := l.input(dedupWindow(1, 1, 12, 1)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 12 {
		t.Fatalf("trimmed command was treated as written: %v", got)
	}
	// The rejected lane is never marked: the Match may refuse it whole.
	if err := l.reject(dedupWindow(1, 200, 201, 1), playerFwdRoutedFutureLimitInputs); err != nil {
		t.Fatal(err)
	}
	sequences(l)
	l.acknowledge(1, 1, 1, 199)
	if err := l.input(dedupWindow(1, 200, 201, 1)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 2 {
		t.Fatalf("rejected-lane window was treated as written: %v", got)
	}
}

func TestRuntimeLinkWrittenRetentionCoversTheLedger(t *testing.T) {
	l := dedupLink()
	for first := uint64(1); first <= 700; first += 10 {
		l.acknowledge(1, 1, 1, first-1)
		if err := l.input(dedupWindow(1, first, first+9, 1)); err != nil {
			t.Fatal(err)
		}
		sequences(l)
	}
	// Newest written is 700: 100 (600 behind) is inside the ledger's 600 + 32.
	if err := l.input(dedupWindow(1, 100, 101, 1)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 0 {
		t.Fatalf("copy within the ledger retention was forwarded again: %v", got)
	}
	totals := linkTotals{}
	if c := totals.take(l).of(1); c[playerFwdRetentionExpiredCommands] != 0 {
		t.Fatalf("retained copy counted as expired: %d", c[playerFwdRetentionExpiredCommands])
	}
	// 68 is exactly 632 behind 700: expired, forwarded again for the Match;
	// 69 is still held.
	if err := l.input(dedupWindow(1, 68, 69, 1)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 1 || got[0] != 68 {
		t.Fatalf("expired copy was not forwarded again: %v", got)
	}
	if c := totals.take(l).of(1); c[playerFwdRetentionExpiredCommands] != 1 {
		t.Fatalf("expired copy was not counted: %d", c[playerFwdRetentionExpiredCommands])
	}
}

// Each epoch restarts sequences at 1 with neutral bootstrap commands, so a
// set kept across rotation would swallow the new epoch's first window.
func TestRuntimeLinkRotationClearsTheWrittenSet(t *testing.T) {
	l := dedupLink()
	if err := l.input(dedupWindow(1, 1, 2, 0)); err != nil {
		t.Fatal(err)
	}
	sequences(l)
	l.acknowledge(1, 2, 1, 0)
	if err := l.input(dedupWindow(2, 1, 2, 0)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 2 {
		t.Fatalf("rotation kept the written set: %v", got)
	}
	// The newest written sequence restarts too, or the old epoch's 700 would
	// expire the new epoch's commands as soon as they are written.
	for first := uint64(1); first <= 700; first += 10 {
		l.acknowledge(1, 2, 1, first-1)
		_ = l.input(dedupWindow(2, first, first+9, 1))
		sequences(l)
	}
	l.acknowledge(1, 3, 1, 0)
	totals := linkTotals{}
	totals.take(l)
	for range 2 {
		if err := l.input(dedupWindow(3, 1, 2, 0)); err != nil {
			t.Fatal(err)
		}
	}
	if got := sequences(l); len(got) != 2 {
		t.Fatalf("rotation lost the new epoch's first commands: %v", got)
	}
	if err := l.input(dedupWindow(3, 1, 2, 0)); err != nil {
		t.Fatal(err)
	}
	if got, c := sequences(l), totals.take(l).of(1); len(got) != 0 || c[playerFwdRetentionExpiredCommands] != 0 {
		t.Fatalf("rotation kept the newest written sequence: %v %v", got, c)
	}
	l.forget(1)
	if len(l.written) != 0 || len(l.newestWritten) != 0 || len(l.resolved) != 0 || len(l.rejected) != 0 {
		t.Fatal("forget kept the written set")
	}
}

// Resolved commands are marked when written like any other: their next copy
// carries nothing new for the Match.
func TestRuntimeLinkMarksResolvedCommandsItWrites(t *testing.T) {
	l := dedupLink()
	l.acknowledge(1, 1, 1, 3)
	if err := l.input(dedupWindow(1, 1, 3, 1)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 3 {
		t.Fatalf("resolved commands missing: %v", got)
	}
	if err := l.input(dedupWindow(1, 1, 3, 1)); err != nil {
		t.Fatal(err)
	}
	if got := sequences(l); len(got) != 0 {
		t.Fatalf("written resolved command was not marked: %v", got)
	}
}

// A packet whose commands were all written creates no window, but a pending
// window still reports the newest snapshot it observed, as merging did.
func TestRuntimeLinkFoldsTheObservedTickOfADeduplicatedPacket(t *testing.T) {
	l := dedupLink()
	if err := l.input(dedupWindow(1, 1, 3, 1)); err != nil {
		t.Fatal(err)
	}
	sequences(l)
	copied := dedupWindow(1, 1, 3, 1)
	copied.ObservedAuthorityTick = 15
	if err := l.input(copied); err != nil {
		t.Fatal(err)
	}
	if batch := l.batch(); len(batch) != 0 {
		t.Fatalf("deduplicated packet created a window: %v", batch)
	}
	pending := dedupWindow(1, 4, 4, 1)
	pending.ObservedAuthorityTick = 10
	if err := l.input(pending); err != nil {
		t.Fatal(err)
	}
	copied.ObservedAuthorityTick = 20
	if err := l.input(copied); err != nil {
		t.Fatal(err)
	}
	if batch := l.batch(); len(batch) != 1 || batch[0].GetInput().ObservedAuthorityTick != 20 || len(batch[0].GetInput().Commands) != 1 {
		t.Fatalf("observed tick of a deduplicated packet was not folded: %v", batch)
	}
	if c := (linkTotals{}).take(l).of(1); c[playerFwdWrittenDuplicateCommands] != 6 || c[playerFwdHandedMainCommands] != 10 {
		t.Fatalf("whole deduplicated packet was not counted: %v", c)
	}
}
