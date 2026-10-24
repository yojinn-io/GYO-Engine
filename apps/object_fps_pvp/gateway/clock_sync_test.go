package gateway

import (
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

func u64(v uint64) *uint64 { return &v }

// One measured sample: our transmit t1, the peer's receive t2 and transmit t3,
// our read t4 (pv7 §3: delay=(t4-t1)-(t3-t2), offset=((t2-t1)+(t3-t4))/2).
func TestClockSampleFollowsTheFourTimestampFormula(t *testing.T) {
	c := newClockSync(0)
	t1 := c.stamp(1_000)
	c.observe(clockStamp{origin: u64(t1), receive: u64(5_000), transmit: 5_100}, 1_300)
	w, ok := c.roll(clockWindowUs, false)
	if !ok || w.samples != 1 || *w.delayMinUs != 200 || *w.delayMaxUs != 200 || *w.offsetUs != 3_900 {
		t.Fatalf("window %+v", w)
	}
	if origin, receive, ok := c.echo(); !ok || origin != 5_100 || receive != 1_300 {
		t.Fatalf("echo %d %d %v", origin, receive, ok)
	}
}

func TestClockSampleViolationsAreCountedAndIgnored(t *testing.T) {
	c := newClockSync(0)
	for i := uint64(1); i <= clockRememberedTxs+1; i++ {
		c.stamp(i * 100)
	}
	c.observe(clockStamp{origin: u64(100), receive: u64(10), transmit: 20}, 2_000)        // forgotten origin
	c.observe(clockStamp{origin: u64(900), transmit: 30}, 2_000)                          // origin without receive
	c.observe(clockStamp{origin: u64(900), receive: u64(50), transmit: 40}, 2_000)        // receive after transmit
	c.observe(clockStamp{transmit: 40}, 2_000)                                            // not increasing
	c.observe(clockStamp{transmit: 0}, 2_000)                                             // transmit below 1
	c.observe(clockStamp{origin: u64(900), receive: u64(50), transmit: 60}, 2_000)        // measured
	c.observe(clockStamp{origin: u64(900), receive: u64(70), transmit: 2_000_000}, 1_000) // negative delay
	w, _ := c.roll(clockWindowUs, false)
	if w.invalid != 5 || w.samples != 1 || w.negativeDelay != 1 {
		t.Fatalf("window %+v", w)
	}
	// The last valid peer transmit is echoed, not the refused one.
	if origin, _, _ := c.echo(); origin != 2_000_000 {
		t.Fatalf("echo %d", origin)
	}
}

func TestClockStampIsStrictlyIncreasingFromOne(t *testing.T) {
	c := newClockSync(0)
	if a, b, d := c.stamp(0), c.stamp(5), c.stamp(5); a != 1 || b != 5 || d != 6 {
		t.Fatalf("stamps %d %d %d", a, b, d)
	}
}

func TestClockWindowTakesTheMinimumDelaySample(t *testing.T) {
	c := newClockSync(0)
	for i, delay := range []uint64{300, 100, 200} {
		t1 := c.stamp(uint64(i+1) * 1_000_000)
		// The peer clock is 40 µs ahead; a symmetric path of delay/2 each way.
		c.observe(clockStamp{origin: u64(t1), receive: u64(t1 + 40 + delay/2), transmit: t1 + 40 + delay/2}, t1+delay)
	}
	w, _ := c.roll(clockWindowUs, false)
	if w.samples != 3 || *w.delayMinUs != 100 || *w.delayMaxUs != 300 || *w.offsetUs != 40 {
		t.Fatalf("window %+v", w)
	}
}

// measure adds one sample with the given offset and delay at time at.
func measure(c *clockSync, at uint64, offset, delay int64) {
	t1 := c.stamp(at)
	t2 := uint64(int64(t1) + offset + delay/2)
	c.observe(clockStamp{origin: u64(t1), receive: u64(t2), transmit: t2}, t1+uint64(delay))
}

func TestClockWindowsReportMissingJumpsAndDrift(t *testing.T) {
	c := newClockSync(0)
	if w, _ := c.roll(clockWindowUs, false); w.samples != 0 || w.offsetUs != nil || w.driftPpm != nil ||
		w.line() != "clock sync window_ms=10000 samples=0 missing=1 invalid=0 negative_delay=0 replaced=0 offset_us=na delay_min_us=na delay_max_us=na drift_ppm=na jump=0 final=0 reason=window" {
		t.Fatalf("missing window %q", w.line())
	}
	// 1 ppm of drift: the offset grows 10 µs per 10 s window.
	for i := uint64(2); i <= 4; i++ {
		measure(c, i*clockWindowUs-5_000_000, int64(i)*10, 100)
		w, _ := c.roll(i*clockWindowUs, false)
		if w.jump {
			t.Fatalf("window %d jumped", i)
		}
		if i >= 3 && (w.driftPpm == nil || *w.driftPpm < 0.999 || *w.driftPpm > 1.001) {
			t.Fatalf("window %d drift %v", i, w.driftPpm)
		}
	}
	// A step beyond both uncertainties (50 µs each) and the 1 ms floor.
	measure(c, 5*clockWindowUs-5_000_000, 40+1_200, 100)
	w, _ := c.roll(5*clockWindowUs, false)
	if !w.jump || w.driftPpm != nil {
		t.Fatalf("jump window %+v", w)
	}
	// Within the floor: no jump, and the drift restarts from the jump.
	measure(c, 6*clockWindowUs-5_000_000, 40+1_200+900, 100)
	w, _ = c.roll(6*clockWindowUs, true)
	if w.jump || !w.final || w.driftPpm == nil {
		t.Fatalf("after jump %+v", w)
	}
	if got := w.line(); got != "clock sync window_ms=10000 samples=1 missing=0 invalid=0 negative_delay=0 replaced=0 offset_us=2140 delay_min_us=100 delay_max_us=100 drift_ppm=90.000 jump=0 final=1 reason=final" {
		t.Fatalf("line %q", got)
	}
}

// The jump rule's two allowances: each sample's uncertainty (delay / 2) and
// 100 ppm of the time between the samples.
func TestClockJumpAllowsUncertaintyAndDrift(t *testing.T) {
	c := newClockSync(0)
	measure(c, 5_000_000, 0, 2_000)
	c.roll(clockWindowUs, false)
	measure(c, 15_000_000, 2_500, 2_000) // within 1 ms + (1000 + 1000) µs
	if w, _ := c.roll(2*clockWindowUs, false); w.jump {
		t.Fatalf("uncertainty: %+v", w)
	}
	c = newClockSync(0)
	measure(c, 5_000_000, 0, 100)
	c.roll(clockWindowUs, false)
	for i := uint64(2); i <= 10; i++ { // nine windows without samples
		c.roll(i*clockWindowUs, false)
	}
	measure(c, 105_000_000, 5_000, 100) // 100 s later: within 10 ms + 100 µs
	if w, _ := c.roll(11*clockWindowUs, false); w.jump {
		t.Fatalf("drift allowance: %+v", w)
	}
}

// The order of the checks: a refused sample changes nothing, a first transmit
// of 0 is refused as such, and a delay of exactly 0 is a sample.
func TestClockChecksComeFirst(t *testing.T) {
	c := newClockSync(0)
	c.observe(clockStamp{transmit: 0}, 10)
	if _, _, ok := c.echo(); ok || c.invalid != 1 {
		t.Fatalf("transmit 0 taken: invalid %d", c.invalid)
	}
	c.observe(clockStamp{transmit: 5}, 20)
	c.observe(clockStamp{transmit: 5}, 30) // a repeat, read later
	if origin, receive, _ := c.echo(); origin != 5 || receive != 20 {
		t.Fatalf("echo %d %d", origin, receive)
	}
	t1 := c.stamp(100)
	c.observe(clockStamp{origin: u64(t1), receive: u64(150), transmit: 160}, 110) // delay 0
	if w, _ := c.roll(clockWindowUs, false); w.samples != 1 || w.negativeDelay != 0 || *w.delayMinUs != 0 {
		t.Fatalf("delay 0: %+v", w)
	}
}

// The clock_sync family's shared golden (log-contract): the same rows as
// tests/object_fps_pvp/ClockSyncTests.cpp.
var clockSyncGolden = filepath.Join("..", "..", "..", "tests", "object_fps_pvp", "fixtures", "log_v1", "clock-sync.tsv")

func TestClockWindowLineMatchesTheSharedGolden(t *testing.T) {
	data, err := os.ReadFile(clockSyncGolden)
	if err != nil {
		t.Fatal(err)
	}
	rows := 0
	for _, row := range strings.Split(strings.TrimRight(string(data), "\n"), "\n") {
		if row == "" || strings.HasPrefix(row, "#") {
			continue
		}
		f := strings.Split(row, "\t")
		if len(f) != 13 {
			t.Fatalf("row %q has %d columns", row, len(f))
		}
		u := func(i int) uint64 {
			v, err := strconv.ParseUint(f[i], 10, 64)
			if err != nil {
				t.Fatalf("row %q column %d: %v", row, i, err)
			}
			return v
		}
		optional := func(i int) *int64 {
			if f[i] == "na" {
				return nil
			}
			v, err := strconv.ParseInt(f[i], 10, 64)
			if err != nil {
				t.Fatalf("row %q column %d: %v", row, i, err)
			}
			return &v
		}
		w := clockWindow{windowUs: u(0), samples: u(1), invalid: u(2), negativeDelay: u(3), replaced: u(4),
			offsetUs: optional(5), delayMinUs: optional(6), delayMaxUs: optional(7), jump: f[9] == "1", final: f[10] == "1"}
		if f[8] != "na" {
			v, err := strconv.ParseFloat(f[8], 64)
			if err != nil {
				t.Fatalf("row %q drift: %v", row, err)
			}
			w.driftPpm = &v
		}
		got := w.line()
		if f[11] == "resume" {
			got = w.resumeLine()
		}
		if got != f[12] {
			t.Errorf("row %q:\n got  %q\n want %q", row, got, f[12])
		}
		rows++
	}
	if rows < 8 {
		t.Fatalf("only %d golden rows", rows)
	}
}
