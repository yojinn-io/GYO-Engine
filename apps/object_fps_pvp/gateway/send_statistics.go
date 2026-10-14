package gateway

import (
	"fmt"
	"math"
	"slices"
	"time"
)

// Diagnostics only: the intervals between consecutive completed writes of one
// channel, summarized once per statisticsInterval. Recording never changes
// when or what anything sends.
type intervalStats struct {
	last      time.Time
	count     uint64
	intervals []time.Duration
}

// One window of a channel: writes, the "p50/p90/p99/max" interval summary
// and the shortest interval (how close two writes came), in milliseconds.
type intervalWindow struct {
	count   uint64
	summary string
	minimum string
}

var noIntervals = intervalWindow{summary: "-", minimum: "-"}

func (s *intervalStats) record(at time.Time) {
	if !s.last.IsZero() {
		s.intervals = append(s.intervals, at.Sub(s.last))
	}
	s.last = at
	s.count++
}

// take ends the window. The interval across the boundary belongs to the next one.
func (s *intervalStats) take() intervalWindow {
	window := intervalWindow{count: s.count, summary: intervalSummary(s.intervals), minimum: "-"}
	if len(s.intervals) > 0 {
		window.minimum = fmt.Sprintf("%.1f", float64(slices.Min(s.intervals))/float64(time.Millisecond))
	}
	s.count = 0
	s.intervals = s.intervals[:0]
	return window
}

// intervalSummary is "p50/p90/p99/max" in milliseconds (nearest rank), or "-"
// for a window without intervals.
func intervalSummary(intervals []time.Duration) string {
	if len(intervals) == 0 {
		return "-"
	}
	sorted := slices.Clone(intervals)
	slices.Sort(sorted)
	rank := func(q float64) time.Duration {
		return sorted[max(int(math.Ceil(q*float64(len(sorted))))-1, 0)]
	}
	ms := func(d time.Duration) float64 { return float64(d) / float64(time.Millisecond) }
	return fmt.Sprintf("%.1f/%.1f/%.1f/%.1f", ms(rank(.50)), ms(rank(.90)), ms(rank(.99)), ms(sorted[len(sorted)-1]))
}

// A session's writes to its Client: ActionResults and snapshots.
type sendStatistics struct{ results, snapshots intervalStats }

// The shortest intervals come last: they were added after the first logs
// (v7 batch 06), so the earlier keys keep their order.
func sendStatisticsLine(player uint64, window time.Duration, results, snapshots, linkActions intervalWindow) string {
	return fmt.Sprintf("player send statistics player=%d window_ms=%d results=%d results_interval_ms=%s "+
		"snapshots=%d snapshot_interval_ms=%s link_actions=%d link_action_interval_ms=%s "+
		"results_min_interval_ms=%s snapshot_min_interval_ms=%s link_action_min_interval_ms=%s",
		player, window.Milliseconds(), results.count, results.summary, snapshots.count, snapshots.summary,
		linkActions.count, linkActions.summary, results.minimum, snapshots.minimum, linkActions.minimum)
}

// The Match side: each player's ActionBatch writes on the runtime link.
func (l *runtimeLink) takeActionStatistics() map[uint64]intervalWindow {
	l.mu.Lock()
	defer l.mu.Unlock()
	windows := make(map[uint64]intervalWindow, len(l.actionStats))
	for player, stats := range l.actionStats {
		windows[player] = stats.take()
	}
	return windows
}

func (s *Server) snapshotWritten(playerID uint64, now time.Time) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if p := s.players[playerID]; p != nil {
		p.sent.snapshots.record(now)
	}
}
