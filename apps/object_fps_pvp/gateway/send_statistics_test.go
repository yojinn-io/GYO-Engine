package gateway

import (
	"testing"
	"time"
)

func TestIntervalStatisticsSummarizeOneWindowAndCarryTheBoundary(t *testing.T) {
	start := time.Unix(100, 0)
	var stats intervalStats
	for _, ms := range []int{0, 33, 66, 133} {
		stats.record(start.Add(time.Duration(ms) * time.Millisecond))
	}
	// Intervals 33, 33, 67 ms: nearest-rank P50 is the 2nd, P90 and P99 the 3rd.
	if got := stats.take(); got != (intervalWindow{count: 4, summary: "33.0/67.0/67.0/67.0"}) {
		t.Fatalf("first window %+v", got)
	}
	if got := stats.take(); got != (intervalWindow{summary: "-"}) {
		t.Fatalf("empty window %+v", got)
	}
	// The first write of a window measures from the last write of the previous one.
	stats.record(start.Add(150 * time.Millisecond))
	if got := stats.take(); got != (intervalWindow{count: 1, summary: "17.0/17.0/17.0/17.0"}) {
		t.Fatalf("boundary window %+v", got)
	}
}

func TestIntervalSummaryUsesNearestRankOverManySamples(t *testing.T) {
	intervals := make([]time.Duration, 0, 100)
	for i := 100; i >= 1; i-- { // unsorted input
		intervals = append(intervals, time.Duration(i)*time.Millisecond)
	}
	if got := intervalSummary(intervals); got != "50.0/90.0/99.0/100.0" {
		t.Fatal(got)
	}
}

func TestSendStatisticsLineKeepsEveryKey(t *testing.T) {
	got := sendStatisticsLine(7, 10*time.Second+3*time.Millisecond, intervalWindow{count: 180, summary: "55.1/66.8/70.2/80.0"},
		intervalWindow{count: 300, summary: "33.3/33.5/34.1/40.0"}, noIntervals)
	want := "player send statistics player=7 window_ms=10003 results=180 results_interval_ms=55.1/66.8/70.2/80.0 " +
		"snapshots=300 snapshot_interval_ms=33.3/33.5/34.1/40.0 link_actions=0 link_action_interval_ms=-"
	if got != want {
		t.Fatalf("\n got %s\nwant %s", got, want)
	}
}

func TestSendStatisticsRecordCompletedWritesPerPlayerAndChannel(t *testing.T) {
	s, p, _, now := actionFixture(t)
	s.link.actionWindows = map[uint64]*actionWindow{7: newActionWindow()}
	for i := range 3 {
		at := now.Add(time.Duration(i) * 33 * time.Millisecond)
		s.actionWritten(7, at)
		s.snapshotWritten(7, at)
		s.link.actionWritten(7, at)
	}
	s.snapshotWritten(99, now) // not a player: ignored
	if got := p.sent.results.take(); got.count != 3 || got.summary != "33.0/33.0/33.0/33.0" {
		t.Fatalf("results %+v", got)
	}
	if got := p.sent.snapshots.take(); got.count != 3 {
		t.Fatalf("snapshots %+v", got)
	}
	links := s.link.takeActionStatistics()
	if got := links[7]; got.count != 3 || len(links) != 1 {
		t.Fatalf("link %+v", links)
	}
	// A leaving player's link statistics go with the rest of its link state.
	s.link.forget(7)
	if links := s.link.takeActionStatistics(); len(links) != 0 {
		t.Fatalf("forgotten player still reported: %+v", links)
	}
}
