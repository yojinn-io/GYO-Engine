package gateway

import (
	"fmt"
	"math"
	"slices"
	"strconv"
	"time"
)

// Diagnostics only (pv7 contract §2, §3): four-timestamp clock sampling with
// the Match, the same algorithm as the Match's ClockSync
// (apps/object_fps_pvp/src/Pvp/ClockSync.cpp). No value it computes may reach a
// decision (D32, guarded by TestClockStaysOutOfDecisions). Times are
// microseconds; the caller passes them, so tests use any clock.

const (
	clockWindowUs                = 10_000_000
	clockRememberedTxs           = 8
	clockDriftWindows            = 30
	clockJumpFloorUs             = 1_000
	clockJumpAllowancePpm        = 100
	clockMaxTimeUs        uint64 = 1 << 53 // larger is not a monotonic time in µs; keeps int64 exact
)

// gatewayClockOrigin is the G clock domain's one reference point (pv7 §2): the
// runtime link and every session read time.Since of it.
var gatewayClockOrigin = time.Now()

// gatewayMicros is t in the G clock domain, from Go's monotonic reading.
func gatewayMicros(t time.Time) uint64 {
	if d := t.Sub(gatewayClockOrigin); d > 0 {
		return uint64(d / time.Microsecond)
	}
	return 0
}

// clockStamp is one ClockSample as it travels, in its sender's clock.
type clockStamp struct {
	origin, receive *uint64
	transmit        uint64
}

// clockWindow summarizes one window of samples, as logged.
type clockWindow struct {
	windowUs, samples, invalid, negativeDelay, replaced uint64
	offsetUs, delayMinUs, delayMaxUs                    *int64
	driftPpm                                            *float64
	jump, final                                         bool
}

func optionalInt(value *int64) string {
	if value == nil {
		return "na"
	}
	return strconv.FormatInt(*value, 10)
}

func boolDigit(value bool) string {
	if value {
		return "1"
	}
	return "0"
}

// line is the "clock sync" line both ends log (pv7 §3). The reason= key
// (log-contract §1.4): a Gateway window ends after clockWindowUs or with its
// link (final); the Client's resume reason has no Gateway writer yet.
func (w clockWindow) line() string {
	reason := "window"
	if w.final {
		reason = "final"
	}
	drift := "na"
	if w.driftPpm != nil && !math.IsNaN(*w.driftPpm) && !math.IsInf(*w.driftPpm, 0) {
		drift = strconv.FormatFloat(*w.driftPpm, 'f', 3, 64)
	}
	return fmt.Sprintf("clock sync window_ms=%d samples=%d missing=%s invalid=%d negative_delay=%d replaced=%d offset_us=%s delay_min_us=%s delay_max_us=%s drift_ppm=%s jump=%s final=%s reason=%s",
		w.windowUs/1000, w.samples, boolDigit(w.samples == 0), w.invalid, w.negativeDelay, w.replaced,
		optionalInt(w.offsetUs), optionalInt(w.delayMinUs), optionalInt(w.delayMaxUs), drift, boolDigit(w.jump), boolDigit(w.final), reason)
}

type clockPoint struct {
	atUs              uint64
	offsetUs, delayUs int64
}

// clockSync is not safe for concurrent use; its owner locks it.
type clockSync struct {
	windowStartUs, lastTransmitUs uint64
	transmits                     []uint64
	lastPeerTransmit              *uint64
	echoTransmit, echoReceive     uint64
	echoed                        bool
	best                          *clockPoint
	delayMaxUs                    *int64
	samples, invalid, negative    uint64
	replaced                      uint64
	segment                       []clockPoint
}

func newClockSync(nowUs uint64) *clockSync { return &clockSync{windowStartUs: nowUs} }

// stamp is the transmit_us of an outgoing sample serialized now: strictly
// increasing, at least 1, and remembered for origin matching.
func (c *clockSync) stamp(nowUs uint64) uint64 {
	c.lastTransmitUs = max(nowUs, c.lastTransmitUs+1)
	c.transmits = append(c.transmits, c.lastTransmitUs)
	if len(c.transmits) > clockRememberedTxs {
		c.transmits = c.transmits[1:]
	}
	return c.lastTransmitUs
}

// echo is the latest valid peer transmit and when it was read, for the next
// own sample.
func (c *clockSync) echo() (origin, receive uint64, ok bool) {
	return c.echoTransmit, c.echoReceive, c.echoed
}

func (c *clockSync) noteReplaced() { c.replaced++ }

// observe takes a peer sample read at receivedUs (this clock). A semantic
// violation is counted and ignored (no veto, pv7 §2); a sample whose origin
// matches a remembered transmit is measured.
func (c *clockSync) observe(peer clockStamp, receivedUs uint64) {
	if peer.transmit < 1 || peer.transmit >= clockMaxTimeUs || receivedUs >= clockMaxTimeUs ||
		(c.lastPeerTransmit != nil && peer.transmit <= *c.lastPeerTransmit) {
		c.invalid++
		return
	}
	transmit := peer.transmit
	c.lastPeerTransmit = &transmit
	c.echoTransmit, c.echoReceive, c.echoed = peer.transmit, receivedUs, true
	if (peer.origin == nil) != (peer.receive == nil) {
		c.invalid++
		return
	}
	if peer.origin == nil {
		return // nothing of ours to measure against yet
	}
	t1, t2, t3, t4 := *peer.origin, *peer.receive, peer.transmit, receivedUs
	if t2 > t3 || !slices.Contains(c.transmits, t1) {
		c.invalid++
		return
	}
	// t1 is our own transmit and t2 <= t3 < clockMaxTimeUs: exact in int64.
	delay := (int64(t4) - int64(t1)) - (int64(t3) - int64(t2))
	if delay < 0 {
		c.negative++
		return
	}
	offset := ((int64(t2) - int64(t1)) + (int64(t3) - int64(t4))) / 2
	c.samples++
	if c.best == nil || delay < c.best.delayUs {
		c.best = &clockPoint{atUs: t4, offsetUs: offset, delayUs: delay}
	}
	if c.delayMaxUs == nil || delay > *c.delayMaxUs {
		c.delayMaxUs = &delay
	}
}

// roll closes the window once it lasted clockWindowUs, or at once when final.
func (c *clockSync) roll(nowUs uint64, final bool) (clockWindow, bool) {
	var elapsed uint64
	if nowUs > c.windowStartUs {
		elapsed = nowUs - c.windowStartUs
	}
	if !final && elapsed < clockWindowUs {
		return clockWindow{}, false
	}
	w := clockWindow{windowUs: elapsed, samples: c.samples, invalid: c.invalid, negativeDelay: c.negative,
		replaced: c.replaced, final: final}
	if best := c.best; best != nil {
		offset, delayMin := best.offsetUs, best.delayUs
		w.offsetUs, w.delayMinUs, w.delayMaxUs = &offset, &delayMin, c.delayMaxUs
		if n := len(c.segment); n > 0 {
			previous := c.segment[n-1]
			var span uint64
			if best.atUs > previous.atUs {
				span = best.atUs - previous.atUs
			}
			allowance := max(int64(clockJumpFloorUs), int64(span/(1_000_000/clockJumpAllowancePpm)))
			uncertainty := (previous.delayUs + best.delayUs) / 2
			change := best.offsetUs - previous.offsetUs
			if change < 0 {
				change = -change
			}
			if change > allowance+uncertainty {
				w.jump = true
				c.segment = c.segment[:0]
			}
		}
		c.segment = append(c.segment, *best)
		if len(c.segment) > clockDriftWindows {
			c.segment = c.segment[1:]
		}
	}
	if len(c.segment) >= 2 {
		// Least squares of offset over time; x from the segment's first point.
		first := c.segment[0].atUs
		var meanX, meanY float64
		for _, p := range c.segment {
			meanX += float64(p.atUs - first)
			meanY += float64(p.offsetUs)
		}
		meanX /= float64(len(c.segment))
		meanY /= float64(len(c.segment))
		var covariance, variance float64
		for _, p := range c.segment {
			x := float64(p.atUs-first) - meanX
			covariance += x * (float64(p.offsetUs) - meanY)
			variance += x * x
		}
		if variance > 0 {
			drift := covariance / variance * 1e6
			w.driftPpm = &drift
		}
	}
	c.windowStartUs = nowUs
	c.best, c.delayMaxUs = nil, nil
	c.samples, c.invalid, c.negative, c.replaced = 0, 0, 0, 0
	return w, true
}
