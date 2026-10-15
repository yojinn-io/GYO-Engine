package gateway

import (
	"errors"
	"fmt"
	"maps"
	"math/rand"
	"net/netip"
	"slices"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev7"
)

// actionWire stands in for the runtime connection of a link's writer: it
// records the ActionBatches each successful write carried, per player, and
// fails every write while fail is set.
type actionWire struct {
	fail    bool
	written map[uint64]map[uint64]bool // player -> shot ids some successful write carried
	acked   map[uint64]uint64          // player -> highest AcknowledgedThrough written
}

func replaceActionWire(t *testing.T) *actionWire {
	t.Helper()
	wire := &actionWire{written: map[uint64]map[uint64]bool{}, acked: map[uint64]uint64{}}
	replaceRuntimeWrite(t, func(_ *framing.TCPConnection, payload []byte, _ time.Duration) error {
		if wire.fail {
			return errors.New("injected write failure")
		}
		var e runtime.RuntimeEnvelope
		if err := proto.Unmarshal(payload, &e); err != nil {
			return err
		}
		if in := e.GetActions(); in != nil {
			if wire.written[in.PlayerId] == nil {
				wire.written[in.PlayerId] = map[uint64]bool{}
			}
			for _, s := range in.Shots {
				wire.written[in.PlayerId][s.ActionId] = true
			}
			wire.acked[in.PlayerId] = max(wire.acked[in.PlayerId], in.AcknowledgedThrough)
		}
		return nil
	})
	return wire
}

// due makes every action window's deadline elapsed, so the next take selects
// each window with something to send.
func due(l *runtimeLink) {
	l.mu.Lock()
	defer l.mu.Unlock()
	for _, w := range l.actionWindows {
		w.nextSend = time.Time{}
	}
}

// actionDrops reads one player's six D56 counters from the link, in line order.
func actionDrops(l *runtimeLink, player uint64) [6]uint64 {
	l.mu.Lock()
	defer l.mu.Unlock()
	var out [6]uint64
	if c := l.ingress[player]; c != nil {
		copy(out[:], c[playerDropDiscardedOnLeaveActionsShots:playerDropAbandonedOnCloseActionsAcks+1])
	}
	return out
}

// linkShots reads one player's two D59 counters from the link: the shot
// requests handed to it and those a successful write carried.
func linkShots(l *runtimeLink, player uint64) (handed, written uint64) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if c := l.ingress[player]; c != nil {
		return c[playerFwdHandedActionsShots], c[playerFwdLinkWrittenActionsShots]
	}
	return 0, 0
}

func shotBatch(player, ack uint64, ids ...uint64) *runtime.ActionBatch {
	in := &runtime.ActionBatch{PlayerId: player, AcknowledgedThrough: ack}
	for _, id := range ids {
		in.Shots = append(in.Shots, shot(id))
	}
	return in
}

// The link counts, by exit, the shot requests and the acknowledgement that no
// write pass took: a pass that took them writes them or counts them
// abandoned, so a request is never counted twice nor counted once written.
func TestLinkCountsTheActionsNoPassTookByExit(t *testing.T) {
	const player = 7
	// Keys in line order: leave shots, leave acks, close shots, close acks,
	// abandoned shots, abandoned acks.
	cases := []struct {
		name string
		run  func(t *testing.T, l *runtimeLink, wire *actionWire)
		want [6]uint64
		// D59: the distinct requests handed to the link and those written.
		handed, written uint64
	}{
		{name: "leave", run: func(t *testing.T, l *runtimeLink, _ *actionWire) {
			// Shots 1 and 2 written, 1 decided; the ACK of 1 and shot 3 never taken.
			mustActions(t, l, shotBatch(player, 0, 1, 2))
			mustWrite(t, l)
			l.actionResults(&runtime.ActionResults{PlayerId: player, Decisions: []*runtime.ShotDecision{decision(1)}})
			mustActions(t, l, shotBatch(player, 1, 2, 3))
			leave(t, l, player)
		}, want: [6]uint64{1, 1, 0, 0, 0, 0}, handed: 3, written: 2},
		{name: "leave_after_the_ack_was_written", run: func(t *testing.T, l *runtimeLink, _ *actionWire) {
			mustActions(t, l, shotBatch(player, 0, 1))
			mustWrite(t, l)
			l.actionResults(&runtime.ActionResults{PlayerId: player, Decisions: []*runtime.ShotDecision{decision(1)}})
			mustActions(t, l, shotBatch(player, 1))
			mustWrite(t, l)
			leave(t, l, player)
		}, want: [6]uint64{}, handed: 1, written: 1},
		{name: "leave_after_the_match_retired_the_ack", run: func(t *testing.T, l *runtimeLink, _ *actionWire) {
			mustActions(t, l, shotBatch(player, 0, 1))
			mustWrite(t, l)
			l.actionResults(&runtime.ActionResults{PlayerId: player, Decisions: []*runtime.ShotDecision{decision(1)}})
			mustActions(t, l, shotBatch(player, 1))
			mustWrite(t, l)
			l.actionResults(&runtime.ActionResults{PlayerId: player, RetiredThrough: 1})
			leave(t, l, player)
		}, want: [6]uint64{}, handed: 1, written: 1},
		{name: "leave_while_a_pass_holds_the_batch", run: func(t *testing.T, l *runtimeLink, _ *actionWire) {
			// The pass took shot 1 before the Leave: it writes it.
			mustActions(t, l, shotBatch(player, 0, 1))
			due(l)
			pass := l.take()
			mustActions(t, l, shotBatch(player, 0, 1, 2))
			leave(t, l, player)
			if err := l.write(pass); err != nil {
				t.Fatal(err)
			}
		}, want: [6]uint64{1, 0, 0, 0, 0, 0}, handed: 2, written: 1},
		{name: "close", run: func(t *testing.T, l *runtimeLink, _ *actionWire) {
			mustActions(t, l, shotBatch(player, 0, 1, 2))
			mustWrite(t, l)
			l.actionResults(&runtime.ActionResults{PlayerId: player, Decisions: []*runtime.ShotDecision{decision(1)}})
			mustActions(t, l, shotBatch(player, 1, 2, 3, 4))
			l.close()
		}, want: [6]uint64{0, 0, 2, 1, 0, 0}, handed: 4, written: 2},
		{name: "write_failure", run: func(t *testing.T, l *runtimeLink, wire *actionWire) {
			// Shot 1 written; the failed pass first took 2 and 3 and the ACK
			// of 1; shot 4 and the ACK of 2 came after it and close clears them.
			mustActions(t, l, shotBatch(player, 0, 1))
			mustWrite(t, l)
			l.actionResults(&runtime.ActionResults{PlayerId: player, Decisions: []*runtime.ShotDecision{decision(1)}})
			mustActions(t, l, shotBatch(player, 1, 1, 2, 3))
			due(l)
			pass := l.take()
			l.actionResults(&runtime.ActionResults{PlayerId: player, Decisions: []*runtime.ShotDecision{decision(2)}})
			mustActions(t, l, shotBatch(player, 2, 3, 4))
			wire.fail = true
			if err := l.write(pass); err == nil {
				t.Fatal("injected failure did not fail the write")
			}
			l.close()
		}, want: [6]uint64{0, 0, 1, 1, 2, 1}, handed: 4, written: 1},
		{name: "write_failure_of_a_resend", run: func(t *testing.T, l *runtimeLink, wire *actionWire) {
			// Every shot of the failed pass was written before: nothing is lost.
			mustActions(t, l, shotBatch(player, 0, 1, 2))
			mustWrite(t, l)
			wire.fail = true
			due(l)
			if err := l.write(l.take()); err == nil {
				t.Fatal("injected failure did not fail the write")
			}
			l.close()
		}, want: [6]uint64{}, handed: 2, written: 2},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			wire := replaceActionWire(t)
			l := dialedLink(t)
			c.run(t, l, wire)
			if got := actionDrops(l, player); got != c.want {
				t.Fatalf("drops (leave shots, acks; close shots, acks; abandoned shots, acks) %v, want %v", got, c.want)
			}
			if handed, written := linkShots(l, player); handed != c.handed || written != c.written {
				t.Fatalf("link shots handed %d, written %d; want %d, %d", handed, written, c.handed, c.written)
			}
		})
	}
}

func mustActions(t *testing.T, l *runtimeLink, in *runtime.ActionBatch) {
	t.Helper()
	if err := l.actions(in); err != nil {
		t.Fatalf("link refused %v: %v", in, err)
	}
}

func mustWrite(t *testing.T, l *runtimeLink) {
	t.Helper()
	due(l)
	if err := l.write(l.take()); err != nil {
		t.Fatal(err)
	}
}

func leave(t *testing.T, l *runtimeLink, player uint64) {
	t.Helper()
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: player}}
	if err := l.control(e); err != nil {
		t.Fatal(err)
	}
}

// Over random interleavings of batches, write passes (some held while other
// things happen), Match results, Leaves, a failed write and the close, each
// player's shot requests are conserved exactly in the link, on its counters
// (D59):
//
//	fwd_handed_actions_shots == fwd_link_written_actions_shots
//	                          + drop_discarded_on_leave + drop_cleared_on_close
//	                          + drop_abandoned_on_close (_actions_shots)
//
// and the two D59 counters are the distinct ids the Client handed (a
// re-handed request, a copy within a batch and a retired id are merged into
// the one request) and the distinct ids a successful write carried. An
// acknowledgement is counted dropped, at least once, exactly when the highest
// one handed is neither written nor retired.
func TestLinkConservesActionRequestsAcrossEveryExit(t *testing.T) {
	players := []uint64{1, 2, 3}
	var exercised [6]int // seeds in which each counter was non-zero
	for seed := int64(1); seed <= 600; seed++ {
		t.Run(fmt.Sprint(seed), func(t *testing.T) {
			r := rand.New(rand.NewSource(seed))
			wire := replaceActionWire(t)
			l := dialedLink(t)
			type client struct {
				next, acked, retired uint64
				decided              map[uint64]bool // decisions the link has (the Match sent them)
				handed               map[uint64]bool
				left                 bool
			}
			clients := map[uint64]*client{}
			for _, p := range players {
				clients[p] = &client{next: 1, decided: map[uint64]bool{}, handed: map[uint64]bool{}}
			}
			// The writer takes one pass at a time: a held pass completes
			// before the next is taken.
			var held []linkWrite
			holding := false
			for step := 0; step < 60; step++ {
				p := players[r.Intn(len(players))]
				c := clients[p]
				switch op := r.Intn(10); {
				case op < 4 && !c.left: // the Client sends its unretired requests, maybe a new one, and its ACK
					var ids []uint64
					for id := c.retired + 1; id < c.next && len(ids) < adapter.MaxActionBatch; id++ {
						if r.Intn(3) > 0 {
							ids = append(ids, id)
						}
					}
					if len(ids) < adapter.MaxActionBatch && r.Intn(2) == 0 {
						ids = append(ids, c.next)
						c.next++
					}
					if len(ids) > 0 && r.Intn(4) == 0 {
						ids = append(ids, ids[0]) // a copy within the batch
					}
					ack := c.acked
					for ack < c.next && c.decided[ack+1] && r.Intn(2) == 0 {
						ack++
					}
					c.acked = ack
					if l.actions(shotBatch(p, ack, ids...)) == nil {
						for _, id := range ids {
							c.handed[id] = true
						}
					}
				case op < 7: // a held pass completes; a write pass, written now or held
					if holding {
						if err := l.write(held); err != nil {
							t.Fatal(err)
						}
						holding = false
						if op == 6 {
							break
						}
					}
					due(l)
					pass := l.take()
					if r.Intn(3) == 0 {
						held, holding = pass, true
					} else if err := l.write(pass); err != nil {
						t.Fatal(err)
					}
				case op < 9 && !c.left: // the Match decides written requests and retires through the ACK it read
					in := &runtime.ActionResults{PlayerId: p, RetiredThrough: max(c.retired, wire.acked[p])}
					// Sorted, so the seed alone decides which written requests are decided.
					for _, id := range slices.Sorted(maps.Keys(wire.written[p])) {
						if id > in.RetiredThrough && !c.decided[id] && r.Intn(2) == 0 {
							in.Decisions = append(in.Decisions, decision(id))
							c.decided[id] = true
						}
					}
					l.actionResults(in)
					c.retired = in.RetiredThrough
				case op == 9 && !c.left && r.Intn(3) == 0:
					leave(t, l, p)
					c.left = true
				}
			}
			// The end: the held pass completes or fails, or a last pass fails;
			// then the link closes.
			end := r.Intn(3)
			wire.fail = end == 0
			if holding {
				_ = l.write(held)
			}
			if end == 1 {
				wire.fail = true
				due(l)
				_ = l.write(l.take())
			}
			l.close()
			for _, p := range players {
				c := clients[p]
				d := actionDrops(l, p)
				for i, n := range d {
					if n > 0 {
						exercised[i]++
					}
				}
				handed, written := linkShots(l, p)
				if handed != written+d[0]+d[2]+d[4] {
					t.Fatalf("player %d: fwd_handed_actions_shots %d != fwd_link_written_actions_shots %d + drops %v",
						p, handed, written, d)
				}
				if distinct := uint64(len(c.handed)); handed != distinct {
					t.Fatalf("player %d: fwd_handed_actions_shots %d, %d distinct requests handed", p, handed, distinct)
				}
				if distinct := uint64(len(wire.written[p])); written != distinct {
					t.Fatalf("player %d: fwd_link_written_actions_shots %d, %d distinct requests written", p, written,
						distinct)
				}
				highest := c.acked
				lost := highest > max(wire.acked[p], c.retired)
				if dropped := d[1] + d[3] + d[5]; lost != (dropped > 0) {
					t.Fatalf("player %d: ACK %d (written %d, retired %d), counted dropped %d", p, highest,
						wire.acked[p], c.retired, dropped)
				}
			}
		})
	}
	for i, n := range exercised {
		if n < 10 {
			t.Errorf("counter %s was non-zero for only %d players: the interleavings miss that exit",
				ingressKeyName(playerIngressKeys[int(playerDropDiscardedOnLeaveActionsShots)+i]), n)
		}
	}
	t.Logf("players per non-zero counter (leave, close, abandoned; shots, acks): %v", exercised)
}

// A player's action drops reach its ingress lines although forget removed its
// link state: a Leave's in the window after it (the removed player's line)
// and a close's in the final window.
func TestActionDropsReachTheWindowAndFinalLines(t *testing.T) {
	logs := captureIngressLog(t)
	s, p, endpoint, now := actionFixture(t)
	s.link = dialedLink(t)
	s.ingressWindowStart = now
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
	f.send(t, f.datagram(adapter.Actions, 2, shotsMessage(1, 2)))
	second, err := session.New(now)
	if err != nil {
		t.Fatal(err)
	}
	stayEndpoint := netip.MustParseAddrPort("127.0.0.1:29009")
	if second.Hello(second.Token, stayEndpoint, 1, now) != nil {
		t.Fatal("hello")
	}
	stay := &reservation{lifeGeneration: 1, playerID: 9, phase: active, session: second, movementEpoch: 1,
		commands: map[uint64]*runtime.MovementCommand{}, actions: newActionWindow()}
	s.mu.Lock()
	s.players[9], s.sessions[second.ID] = stay, stay
	s.mu.Unlock()
	g := &outcomeFixture{s: s, p: stay, endpoint: stayEndpoint, now: now}
	g.send(t, g.datagram(adapter.Actions, 2, shotsMessage(1, 2, 3)))

	// Player 7 leaves as leaveRoom does: the link forgets it, the Server removes it.
	s.mu.Lock()
	e := envelope()
	e.Message = &runtime.RuntimeEnvelope_Leave{Leave: &runtime.PlayerLeave{PlayerId: 7}}
	if err := s.link.control(e); err != nil {
		t.Fatal(err)
	}
	s.remove(p)
	s.mu.Unlock()
	s.logStatisticsWindow(now.Add(10*time.Second), 10*time.Second)
	s.runtimeFailed(errors.New("test end"))

	lines := logs.lines(t)
	checkIngressLines(t, "action drops", lines, s.rateRejectedPackets.Load())
	type seen struct{ leave, cleared uint64 }
	got := map[uint64][]seen{}
	for _, line := range lines {
		if line.player != 0 {
			got[line.player] = append(got[line.player], seen{line.counts[playerDropDiscardedOnLeaveActionsShots],
				line.counts[playerDropClearedOnCloseActionsShots]})
		}
	}
	if want := []seen{{2, 0}, {0, 0}}; fmt.Sprint(got[7]) != fmt.Sprint(want) {
		t.Errorf("player 7 (left) windows %v, want %v", got[7], want)
	}
	if want := []seen{{0, 0}, {0, 3}}; fmt.Sprint(got[9]) != fmt.Sprint(want) {
		t.Errorf("player 9 (closed) windows %v, want %v", got[9], want)
	}
	// D59: the handed shots reach the lines too; nothing was written.
	handed := map[uint64][2]uint64{}
	for _, line := range lines {
		if line.player != 0 {
			h := handed[line.player]
			handed[line.player] = [2]uint64{h[0] + line.counts[playerFwdHandedActionsShots],
				h[1] + line.counts[playerFwdLinkWrittenActionsShots]}
		}
	}
	if want := map[uint64][2]uint64{7: {2, 0}, 9: {3, 0}}; fmt.Sprint(handed) != fmt.Sprint(want) {
		t.Errorf("players' (handed, written) shots %v, want %v", handed, want)
	}
}
