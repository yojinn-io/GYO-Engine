package gateway

import (
	"strings"
	"testing"
	"time"

	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

// D180 (silent-branch-asserts): a JoinResult the Gateway cannot carry or did
// not ask for is an IPC contract violation; it fails the runtime instead of
// being dropped silently.

func failingFixture(t *testing.T) (*Server, *ingressLog) {
	t.Helper()
	logs := captureIngressLog(t)
	s, _, _, _ := actionFixture(t)
	link := dialedLink(t)
	link.drops = newDropLedger()
	s.link = link
	return s, logs
}

func waitUnavailable(t *testing.T, s *Server, logs *ingressLog, reason string) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for {
		s.mu.Lock()
		available := s.available
		s.mu.Unlock()
		if !available && strings.Contains(logText(logs), "runtime disconnected reason="+reason) {
			return
		}
		if time.Now().After(deadline) {
			t.Fatalf("the runtime did not fail with reason %s:\n%s", reason, logText(logs))
		}
		time.Sleep(time.Millisecond)
	}
}

func joinResult(player uint64, accepted bool, reason string) *runtime.RuntimeEnvelope {
	return &runtime.RuntimeEnvelope{Message: &runtime.RuntimeEnvelope_JoinResult{
		JoinResult: &runtime.JoinResult{PlayerId: player, Accepted: accepted, Reason: reason}}}
}

func TestAJoinRejectionTooLongForADatagramFailsTheRuntime(t *testing.T) {
	s, logs := failingFixture(t)
	s.players[7].phase = joining
	s.runtimeMessage(joinResult(7, false, strings.Repeat("x", 2000)))
	waitUnavailable(t, s, logs, "protocol")
	if !strings.Contains(logText(logs), "join result reason exceeds the datagram limit") {
		t.Fatal("the failure does not name the reason")
	}
}

func TestAJoinRejectionThatFitsIsRelayed(t *testing.T) {
	s, _ := failingFixture(t)
	s.players[7].phase = joining
	s.runtimeMessage(joinResult(7, false, strings.Repeat("x", 1000)))
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.available || s.players[7] != nil || len(s.controlOut) != 1 {
		t.Fatalf("available %v, player kept %v, %d controls queued", s.available, s.players[7] != nil, len(s.controlOut))
	}
}

func TestAJoinResultForAPlayerNotJoiningFailsTheRuntime(t *testing.T) {
	s, logs := failingFixture(t) // player 7 is active
	s.runtimeMessage(joinResult(7, true, ""))
	waitUnavailable(t, s, logs, "protocol")
}

func TestAJoinResultForAPlayerThatLeftIsIgnored(t *testing.T) {
	s, _ := failingFixture(t)
	s.runtimeMessage(joinResult(99, true, ""))
	time.Sleep(20 * time.Millisecond)
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.available {
		t.Fatal("a result for a player that left failed the runtime")
	}
}
