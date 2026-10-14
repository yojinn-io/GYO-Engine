package gateway

import (
	"errors"
	"fmt"
	"net/netip"
	"strings"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	"gyo.local/gateway/session"
	"gyo.local/object_fps_pvp/gateway/adapter"
	client "gyo.local/object_fps_pvp/protocol/clientv6"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev6"
)

// ingressTake is what the Server and its link hand over, per player and
// Gateway-wide; the two parts of a player's counters have disjoint keys.
type ingressTake struct {
	players map[uint64]*playerIngressCounts
	gateway gatewayIngressCounts
}

func takeAllIngress(s *Server) ingressTake {
	players, gateway := s.takeIngress()
	if players == nil {
		players = map[uint64]*playerIngressCounts{}
	}
	for player, counts := range s.link.takeIngress() {
		if players[player] == nil {
			players[player] = new(playerIngressCounts)
		}
		players[player].add(counts)
	}
	return ingressTake{players, gateway}
}

func (total *ingressTake) add(take ingressTake) {
	if total.players == nil {
		total.players = map[uint64]*playerIngressCounts{}
	}
	for player, counts := range take.players {
		if total.players[player] == nil {
			total.players[player] = new(playerIngressCounts)
		}
		total.players[player].add(counts)
	}
	for i, n := range take.gateway {
		total.gateway[i] += n
	}
}

// checkIngressConservation checks IG per player, the Gateway-wide datagram
// conservation, T3 against the Session limiter's own counter, and I0.
func checkIngressConservation(t *testing.T, name string, total ingressTake, rateLimited uint64) {
	t.Helper()
	var received, rate uint64
	for player, c := range total.players {
		if outcomes := sumIngress(c[:], playerFwdMainInputPackets, playerDropAdmissionUnknownPackets); outcomes != c[playerReceivedDatagrams] {
			t.Errorf("%s: player %d: datagram outcomes %d != received %d", name, player, outcomes, c[playerReceivedDatagrams])
		}
		if handed, accounted := i0(*c); handed != accounted {
			t.Errorf("%s: player %d: link settled %d of %d handed commands", name, player, accounted, handed)
		}
		received += c[playerReceivedDatagrams]
		rate += sumIngress(c[:], playerDropRateLimitedHelloPackets, playerDropRateLimitedOtherPackets)
	}
	g := total.gateway
	if read := sumIngress(g[:], gatewayDropUndecodableDatagrams, gatewayDropUnknownSessionDatagrams) + received; read != g[gatewayReceivedDatagrams] {
		t.Errorf("%s: gateway-wide drops and players' datagrams %d != received %d", name, read, g[gatewayReceivedDatagrams])
	}
	if rate != g[gatewayDropRateLimitedPackets] || rate != rateLimited {
		t.Errorf("%s: per-player rate drops %d != gateway %d (limiter %d)", name, rate, g[gatewayDropRateLimitedPackets], rateLimited)
	}
}

// countedNames lists the non-zero counters by key name, or "none".
func countedNames[C ~uint8](values []uint64, keys []ingressKey, skip C) string {
	var names []string
	for i, n := range values {
		if n != 0 && C(i) != skip {
			names = append(names, fmt.Sprintf("%s=%d", ingressKeyName(keys[i]), n))
		}
	}
	if len(names) == 0 {
		return "none"
	}
	return strings.Join(names, " ")
}

type outcomeFixture struct {
	s        *Server
	p        *reservation
	endpoint netip.AddrPort
	now      time.Time
}

func (f *outcomeFixture) datagram(kind uint16, sequence uint32, message proto.Message) []byte {
	payload, _ := proto.Marshal(message)
	return f.raw(kind, sequence, payload)
}

func (f *outcomeFixture) raw(kind uint16, sequence uint32, payload []byte) []byte {
	packet, err := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: kind, SessionID: f.p.session.ID, Sequence: sequence}, payload)
	if err != nil {
		panic(err)
	}
	return packet
}

// send delivers a setup datagram from the session's endpoint at f.now.
func (f *outcomeFixture) send(t *testing.T, packet []byte) {
	t.Helper()
	if err := f.s.receiveDatagram(packet, f.endpoint, f.now); err != nil {
		t.Fatal(err)
	}
}

// fillRateWindow spends the rest of the session's 120 packets in f.now's
// second (the fixture's Hello spent one) on admitted unknown-kind packets.
func (f *outcomeFixture) fillRateWindow(t *testing.T) {
	for sequence := uint32(100); sequence < 219; sequence++ {
		f.send(t, f.raw(99, sequence, nil))
	}
}

func inputMessage(epoch uint64, commands []*client.MovementCommand) *client.PlayerInput {
	return &client.PlayerInput{MovementEpoch: epoch, LifeGeneration: 1, Commands: commands}
}

func shotsMessage(ids ...uint64) *client.ActionBatch {
	b := &client.ActionBatch{}
	for _, id := range ids {
		b.Shots = append(b.Shots, &client.ShotRequest{LifeGeneration: 1, Kind: client.ActionKind_ACTION_SHOT, Pitch: proto.Float32(0), ActionId: id, ObservedAuthorityTick: 1, Yaw: proto.Float32(.25)})
	}
	return b
}

func replaceSessionAdmit(t *testing.T, admit func(*session.Session, netip.AddrPort, uint32, time.Time) error) {
	previous := sessionAdmit
	sessionAdmit = admit
	t.Cleanup(func() { sessionAdmit = previous })
}

func replaceSessionHello(t *testing.T, hello func(*session.Session, string, netip.AddrPort, uint32, time.Time) error) {
	previous := sessionHello
	sessionHello = hello
	t.Cleanup(func() { sessionHello = previous })
}

var errFutureAdmission = errors.New("an admission error the Gateway does not know")

var garbage = []byte{0x0a, 0x05} // a length-delimited field cut short

// The fixture's player 7 is active, its session saw a Hello with sequence 1
// from f.endpoint at f.now; fresh sequences start at 2.
type outcomeCase struct {
	name  string
	setup func(t *testing.T, f *outcomeFixture)
	// The measured datagram and its source.
	packet func(f *outcomeFixture) ([]byte, netip.AddrPort)
	// What the measured datagram counts besides received_datagrams (player 7
	// for player outcomes, the Gateway for the others).
	player  map[playerIngressCounter]uint64
	gateway gatewayIngressCounter
	fatal   bool // the runtime link fails on it
}

func fromEndpoint(packet func(f *outcomeFixture) []byte) func(f *outcomeFixture) ([]byte, netip.AddrPort) {
	return func(f *outcomeFixture) ([]byte, netip.AddrPort) { return packet(f), f.endpoint }
}

var spoofedEndpoint = netip.MustParseAddrPort("127.0.0.1:29002")

func one(counter playerIngressCounter) map[playerIngressCounter]uint64 {
	return map[playerIngressCounter]uint64{counter: 1}
}

// reservePlayer8 adds a reserved player whose session has seen no Hello.
func reservePlayer8(t *testing.T, f *outcomeFixture) {
	peer, err := session.New(f.now)
	if err != nil {
		t.Fatal(err)
	}
	p := &reservation{playerID: 8, phase: reserved, session: peer, movementEpoch: 1, lifeGeneration: 1, commands: map[uint64]*runtime.MovementCommand{}}
	f.s.players[8], f.s.sessions[peer.ID] = p, p
	f.p = p
	f.endpoint = netip.MustParseAddrPort("127.0.0.1:29008")
}

func closeLink(_ *testing.T, f *outcomeFixture) { f.s.link.closed = true }

func outcomeCases() []outcomeCase {
	// Sequence 5 carried command 3, which was written; sequence 4 is stale.
	stale := func(t *testing.T, f *outcomeFixture) {
		f.send(t, f.datagram(adapter.Input, 5, inputMessage(1, moves(3, 3))))
		sequences(f.s.link)
	}
	conflicting := moves(1, 1)
	conflicting[0].MoveForward = -1
	return []outcomeCase{
		// Datagrams of no session.
		{name: "undecodable", packet: func(f *outcomeFixture) ([]byte, netip.AddrPort) { return []byte("junk"), f.endpoint },
			gateway: gatewayDropUndecodableDatagrams},
		{name: "other_version", packet: func(f *outcomeFixture) ([]byte, netip.AddrPort) {
			packet, _ := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion + 1, Type: adapter.Input, SessionID: f.p.session.ID, Sequence: 2}, nil)
			return packet, f.endpoint
		}, gateway: gatewayDropOtherVersionDatagrams},
		{name: "room_unavailable", setup: func(_ *testing.T, f *outcomeFixture) { f.s.available = false },
			packet:  fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 1))) }),
			gateway: gatewayDropRoomUnavailableDatagrams},
		{name: "unknown_session", packet: func(f *outcomeFixture) ([]byte, netip.AddrPort) {
			packet, _ := framing.EncodeDatagram(framing.Header{Version: adapter.ClientVersion, Type: adapter.Input, SessionID: ^f.p.session.ID, Sequence: 2}, nil)
			return packet, f.endpoint
		}, gateway: gatewayDropUnknownSessionDatagrams},
		// The runtime link fails on the datagram: dropped as for an unavailable room.
		{name: "link_closed/input", setup: closeLink, fatal: true, gateway: gatewayDropRoomUnavailableDatagrams,
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 1))) })},
		{name: "link_closed/rejected_lane", setup: closeLink, fatal: true, gateway: gatewayDropRoomUnavailableDatagrams,
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(2, moves(1, 1))) })},
		{name: "link_closed/actions", setup: closeLink, fatal: true, gateway: gatewayDropRoomUnavailableDatagrams,
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 2, shotsMessage(1)) })},
		{name: "link_closed/hello", fatal: true, gateway: gatewayDropRoomUnavailableDatagrams,
			setup: func(t *testing.T, f *outcomeFixture) { reservePlayer8(t, f); closeLink(t, f) },
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				return f.datagram(adapter.Hello, 1, &client.Hello{SessionToken: f.p.session.Token})
			})},
		// Hello.
		{name: "admitted/hello/reserved", setup: reservePlayer8, player: one(playerFwdAdmittedHelloPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				return f.datagram(adapter.Hello, 1, &client.Hello{SessionToken: f.p.session.Token})
			})},
		{name: "admitted/hello/active", player: one(playerFwdAdmittedHelloPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				return f.datagram(adapter.Hello, 2, &client.Hello{SessionToken: f.p.session.Token})
			})},
		{name: "malformed/hello", player: one(playerDropMalformedHelloPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(adapter.Hello, 2, garbage) })},
		{name: "unauthorized/hello", player: one(playerDropUnauthorizedHelloPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				return f.datagram(adapter.Hello, 2, &client.Hello{SessionToken: "not the token"})
			})},
		{name: "rate_limited/hello", setup: func(t *testing.T, f *outcomeFixture) { f.fillRateWindow(t) },
			player: one(playerDropRateLimitedHelloPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				return f.datagram(adapter.Hello, 300, &client.Hello{SessionToken: f.p.session.Token})
			})},
		{name: "admission_unknown/hello", player: one(playerDropAdmissionUnknownPackets),
			setup: func(t *testing.T, _ *outcomeFixture) {
				replaceSessionHello(t, func(*session.Session, string, netip.AddrPort, uint32, time.Time) error { return errFutureAdmission })
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				return f.datagram(adapter.Hello, 2, &client.Hello{SessionToken: f.p.session.Token})
			})},
		{name: "admission_unknown/hello_stale", player: one(playerDropAdmissionUnknownPackets),
			setup: func(t *testing.T, _ *outcomeFixture) {
				replaceSessionHello(t, func(*session.Session, string, netip.AddrPort, uint32, time.Time) error { return session.ErrStale })
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				return f.datagram(adapter.Hello, 2, &client.Hello{SessionToken: f.p.session.Token})
			})},
		// Actions.
		{name: "handed/actions", player: one(playerFwdHandedActionsPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 2, shotsMessage(1)) })},
		{name: "handed/actions_commit_failed", player: one(playerFwdHandedActionsPackets),
			// Admit cannot pass a sequence CommitSequence refuses; a stand-in
			// reaches that return.
			setup: func(t *testing.T, _ *outcomeFixture) {
				replaceSessionAdmit(t, func(*session.Session, netip.AddrPort, uint32, time.Time) error { return nil })
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 1, shotsMessage(1)) })},
		{name: "inactive/actions", setup: func(_ *testing.T, f *outcomeFixture) { f.p.phase = joining },
			player: one(playerDropInactiveActionsPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 2, shotsMessage(1)) })},
		{name: "unauthorized/actions", player: one(playerDropUnauthorizedActionsPackets),
			packet: func(f *outcomeFixture) ([]byte, netip.AddrPort) {
				return f.datagram(adapter.Actions, 2, shotsMessage(1)), spoofedEndpoint
			}},
		{name: "rate_limited/actions", setup: func(t *testing.T, f *outcomeFixture) { f.fillRateWindow(t) },
			player: one(playerDropRateLimitedActionsPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 300, shotsMessage(1)) })},
		{name: "stale_sequence/actions", player: one(playerDropStaleSequenceActionsPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 1, shotsMessage(1)) })},
		{name: "admission_unknown/actions", player: one(playerDropAdmissionUnknownPackets),
			setup: func(t *testing.T, _ *outcomeFixture) {
				replaceSessionAdmit(t, func(*session.Session, netip.AddrPort, uint32, time.Time) error { return errFutureAdmission })
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 2, shotsMessage(1)) })},
		{name: "malformed/actions", player: one(playerDropMalformedActionsPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(adapter.Actions, 2, garbage) })},
		{name: "invalid/actions", // A3: acknowledges decisions the ledger never had
			player: map[playerIngressCounter]uint64{playerDropInvalidActionsPackets: 1, playerDropInvalidActionsShots: 2},
			packet: fromEndpoint(func(f *outcomeFixture) []byte {
				b := shotsMessage(1, 2)
				b.AcknowledgedThrough = 5
				return f.datagram(adapter.Actions, 2, b)
			})},
		{name: "link_rejected/actions", // A5: the link's action windows are full
			setup: func(_ *testing.T, f *outcomeFixture) {
				f.s.link.actionWindows = map[uint64]*actionWindow{}
				for player := uint64(100); player < 100+adapter.MaxPlayers; player++ {
					f.s.link.actionWindows[player] = newActionWindow()
				}
			},
			player: map[playerIngressCounter]uint64{playerDropLinkRejectedActionsPackets: 1, playerDropLinkRejectedActionsShots: 3},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Actions, 2, shotsMessage(1, 2, 3)) })},
		// Input.
		{name: "main/input", player: one(playerFwdMainInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 2))) })},
		{name: "rejected_lane/input_epoch", player: one(playerFwdRejectedLaneInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(2, moves(1, 1))) })},
		{name: "rejected_lane/input_future", player: one(playerFwdRejectedLaneInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(1, moves(40, 40))) })},
		{name: "rejected_lane/input_conflict", player: one(playerFwdRejectedLaneInputPackets),
			setup: func(t *testing.T, f *outcomeFixture) {
				f.send(t, f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 1))))
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 3, inputMessage(1, conflicting)) })},
		{name: "rejected_lane/input_link_refused", player: one(playerFwdRejectedLaneInputPackets),
			setup: func(t *testing.T, f *outcomeFixture) {
				f.send(t, f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 2))))
				sequences(f.s.link)
				resolve(f.s, 10, 2)
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 3, inputMessage(1, conflicting)) })},
		{name: "written_duplicate/input", player: one(playerFwdWrittenDuplicateInputPackets),
			setup: func(t *testing.T, f *outcomeFixture) {
				f.send(t, f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 2))))
				sequences(f.s.link)
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 3, inputMessage(1, moves(1, 2))) })},
		// ★ A stale sequence counts only as stale, on any lane (D55 ③). The
		// commands were never written, or dedup would hide a drop.
		{name: "stale_sequence/input_main", setup: stale, player: one(playerFwdStaleSequenceInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 4, inputMessage(1, moves(2, 2))) })},
		{name: "stale_sequence/input_rejected_lane", setup: stale, player: one(playerFwdStaleSequenceInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 4, inputMessage(2, moves(1, 1))) })},
		{name: "stale_sequence/input_written_duplicate", setup: stale, player: one(playerFwdStaleSequenceInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 4, inputMessage(1, moves(3, 3))) })},
		{name: "malformed/input_stale", setup: stale, player: one(playerDropMalformedInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(adapter.Input, 4, garbage) })},
		{name: "malformed/input", player: one(playerDropMalformedInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(adapter.Input, 2, garbage) })},
		{name: "inactive/input", setup: func(_ *testing.T, f *outcomeFixture) { f.p.phase = joining },
			player: one(playerDropInactiveInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 1))) })},
		{name: "unauthorized/input", player: one(playerDropUnauthorizedInputPackets),
			packet: func(f *outcomeFixture) ([]byte, netip.AddrPort) {
				return f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 1))), spoofedEndpoint
			}},
		{name: "rate_limited/input", setup: func(t *testing.T, f *outcomeFixture) { f.fillRateWindow(t) },
			player: one(playerDropRateLimitedInputPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 300, inputMessage(1, moves(1, 1))) })},
		{name: "admission_unknown/input", player: one(playerDropAdmissionUnknownPackets),
			setup: func(t *testing.T, _ *outcomeFixture) {
				replaceSessionAdmit(t, func(*session.Session, netip.AddrPort, uint32, time.Time) error { return errFutureAdmission })
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 1))) })},
		// Other kinds.
		{name: "unknown_kind", player: one(playerDropUnknownKindPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(99, 2, nil) })},
		{name: "unauthorized/other", player: one(playerDropUnauthorizedOtherPackets),
			packet: func(f *outcomeFixture) ([]byte, netip.AddrPort) { return f.raw(99, 2, nil), spoofedEndpoint }},
		{name: "rate_limited/other", setup: func(t *testing.T, f *outcomeFixture) { f.fillRateWindow(t) },
			player: one(playerDropRateLimitedOtherPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(99, 300, nil) })},
		{name: "stale_sequence/other", player: one(playerDropStaleSequenceOtherPackets),
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(99, 1, nil) })},
		{name: "admission_unknown/other", player: one(playerDropAdmissionUnknownPackets),
			setup: func(t *testing.T, _ *outcomeFixture) {
				replaceSessionAdmit(t, func(*session.Session, netip.AddrPort, uint32, time.Time) error { return errFutureAdmission })
			},
			packet: fromEndpoint(func(f *outcomeFixture) []byte { return f.raw(99, 2, nil) })},
	}
}

// IG: every return of receivePacket and receiveActions counts exactly one
// outcome. Each case drives one path and checks that the datagram counted
// only its own key, and that IG, the Gateway-wide conservation, T3 and I0
// hold over everything the fixture handled.
func TestEveryDatagramCountsExactlyOneOutcome(t *testing.T) {
	for _, c := range outcomeCases() {
		t.Run(c.name, func(t *testing.T) {
			s, p, endpoint, now := actionFixture(t)
			f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
			if c.setup != nil {
				c.setup(t, f)
			}
			var total ingressTake
			total.add(takeAllIngress(s))
			packet, peer := c.packet(f)
			accepted := s.rateAcceptedPackets.Load()
			err := s.receiveDatagram(packet, peer, now)
			if (err != nil) != c.fatal {
				t.Fatalf("case %s: error %v", c.name, err)
			}
			if strings.HasPrefix(c.name, "admission_unknown/") && s.rateAcceptedPackets.Load() != accepted {
				t.Errorf("case %s: an unknown admission error was accepted by the rate accounting", c.name)
			}
			measured := takeAllIngress(s)
			total.add(measured)

			got := playerIngressCounts{}
			if counts := measured.players[f.p.playerID]; counts != nil {
				got = *counts
			}
			// Only the datagram outcomes (IG) and the shots of dropped batches.
			window := got[:playerDropLinkRejectedActionsShots+1]
			want := playerIngressCounts{}
			for counter, n := range c.player {
				want[counter] = n
			}
			if c.player != nil {
				want[playerReceivedDatagrams] = 1
			}
			if !slicesEqual(window, want[:playerDropLinkRejectedActionsShots+1]) {
				t.Errorf("case %s: counted %s", c.name, countedNames(window, playerIngressKeys[:], playerReceivedDatagrams))
			}
			wantGateway := gatewayIngressCounts{gatewayReceivedDatagrams: 1}
			if c.player == nil {
				wantGateway[c.gateway] = 1
			}
			if strings.HasPrefix(c.name, "rate_limited/") {
				wantGateway[gatewayDropRateLimitedPackets] = 1
			}
			if measured.gateway != wantGateway {
				t.Errorf("case %s: gateway counted %s", c.name, countedNames(measured.gateway[:], gatewayIngressKeys[:], gatewayReceivedDatagrams))
			}
			// Settle every handed command (the link's writer) before I0.
			if !s.link.closed {
				sequences(s.link)
			}
			total.add(takeAllIngress(s))
			checkIngressConservation(t, c.name, total, s.rateRejectedPackets.Load())
		})
	}
}

func slicesEqual(a, b []uint64) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

// Every kind maps to the admission counter of its own reason and kind.
func TestAdmissionCountersMatchTheirKeys(t *testing.T) {
	for _, table := range []struct {
		reason   ingressReason
		counters []playerIngressCounter
		kinds    []ingressKind
	}{
		{reasonUnauthorized, unauthorizedPackets[:], []ingressKind{ingressHello, ingressInput, ingressActions, ingressOther}},
		{reasonRateLimited, rateLimitedPackets[:], []ingressKind{ingressHello, ingressInput, ingressActions, ingressOther}},
		{reasonStaleSequence, staleSequencePackets[:], []ingressKind{ingressActions, ingressOther}},
	} {
		for _, kind := range table.kinds {
			key := playerIngressKeys[table.counters[kind]]
			if key != (ingressKey{ingressDropped, table.reason, kind, unitPackets}) {
				t.Fatalf("%s for kind %d counts %s", ingressReasonNames[table.reason], kind, ingressKeyName(key))
			}
		}
	}
}

// T3 over two players and every kind: the players' rate-limited drops sum to
// the Gateway's, which is the Session limiter's own count. A stale packet
// stays in the limiter's accepted packets.
func TestRateLimitedDropsSumToTheLimiterCount(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
	second := &outcomeFixture{s: s, endpoint: endpoint, now: now}
	reservePlayer8(t, second)
	second.send(t, second.datagram(adapter.Hello, 1, &client.Hello{SessionToken: second.p.session.Token}))
	f.fillRateWindow(t)
	accepted := s.rateAcceptedPackets.Load()
	f.send(t, f.datagram(adapter.Input, 300, inputMessage(1, moves(1, 1))))
	f.send(t, f.datagram(adapter.Actions, 301, shotsMessage(1)))
	f.send(t, f.raw(99, 302, nil))
	second.fillRateWindow(t)
	second.send(t, second.datagram(adapter.Hello, 300, &client.Hello{SessionToken: second.p.session.Token}))
	second.send(t, second.raw(99, 301, nil))
	if s.rateAcceptedPackets.Load() != accepted+119 {
		t.Fatal("rate-limited packets entered the accepted packets")
	}
	// A stale packet in a new second is accepted by the limiter, as before.
	f.now = now.Add(2 * time.Second)
	f.send(t, f.raw(99, 1, nil))
	if s.rateAcceptedPackets.Load() != accepted+120 {
		t.Fatal("stale packet left the 120-packet accounting")
	}
	sequences(s.link)
	total := takeAllIngress(s)
	if s.rateRejectedPackets.Load() != 5 || total.gateway[gatewayDropRateLimitedPackets] != 5 {
		t.Fatalf("rate drops: limiter %d gateway %d, want 5", s.rateRejectedPackets.Load(), total.gateway[gatewayDropRateLimitedPackets])
	}
	checkIngressConservation(t, "two players", total, s.rateRejectedPackets.Load())
}

// D1 and D2 (D55 ②): a snapshot whose tick is not after the last one, or
// in which a player's cursor goes back, is refused and counted.
func TestGatewayCountsRefusedSnapshots(t *testing.T) {
	s, _, _, _ := actionFixture(t)
	resolve(s, 10, 2)
	resolve(s, 10, 3) // same tick
	resolve(s, 9, 3)  // older tick
	resolve(s, 11, 1) // cursor 2 goes back to 1
	resolve(s, 12, 3)
	_, gateway := s.takeIngress()
	if gateway[gatewayDropOldTickSnapshots] != 2 || gateway[gatewayDropRegressedSnapshots] != 1 {
		t.Fatalf("refused snapshots counted %s", countedNames(gateway[:], gatewayIngressKeys[:], gatewayReceivedDatagrams))
	}
	if s.lastSnapshot != 12 {
		t.Fatalf("snapshot after the refused ones not accepted: last tick %d", s.lastSnapshot)
	}
}

// A player's counters outlive its reservation until taken: a Leave or a
// runtime failure loses none of its outcomes.
func TestPlayerIngressOutlivesTheReservation(t *testing.T) {
	s, p, endpoint, now := actionFixture(t)
	f := &outcomeFixture{s: s, p: p, endpoint: endpoint, now: now}
	f.send(t, f.datagram(adapter.Input, 2, inputMessage(1, moves(1, 1))))
	s.mu.Lock()
	s.remove(p)
	s.mu.Unlock()
	players, _ := s.takeIngress()
	if c := players[7]; c == nil || c[playerReceivedDatagrams] != 1 || c[playerFwdMainInputPackets] != 1 {
		t.Fatalf("removed player's outcomes were lost: %v", players[7])
	}
}

// The link tells the Server when a window held only written copies: its
// datagram is then a written duplicate, not a main-lane forward.
func TestRuntimeLinkReportsAWholeCopy(t *testing.T) {
	l := &runtimeLink{inputs: map[uint64]*runtime.PlayerInput{}, epochs: map[uint64]uint64{}, wake: make(chan struct{}, 1)}
	for i, c := range []struct {
		first, last uint64
		copies      bool
	}{{1, 2, false}, {1, 2, true}, {1, 3, false}} {
		var copies bool
		if err := l.inputWindow(linkWindow(1, 1, c.first, c.last), &copies); err != nil {
			t.Fatal(err)
		}
		if copies && !c.copies {
			t.Fatalf("window %d: link reported a partial copy as whole", i)
		}
		if !copies && c.copies {
			t.Fatalf("window %d: link did not report a whole copy", i)
		}
		sequences(l)
	}
}
