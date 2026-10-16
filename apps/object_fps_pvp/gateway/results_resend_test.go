package gateway

import (
	"bytes"
	"fmt"
	"math/rand/v2"
	"slices"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	"gyo.local/gateway/framing"
	client "gyo.local/object_fps_pvp/protocol/clientv7"
)

// One result datagram as the Client would read it: when it was written, its
// GYOP sequence and its payload (each run has its own random session id).
type resultDatagram struct {
	at       time.Duration
	sequence uint32
	payload  []byte
}

// resendScenario drives the Gateway's result path on a virtual clock with a
// seeded Client and Match. The Match sends each decision and each retirement
// once, the moment it exists; with resend it also sends copies, every action
// interval while anything is unacknowledged: the retirement and the smallest
// unacknowledged decisions again. This isolates the Gateway's merge from the
// Match's schedule (lanePace below models that). The Client loses some results,
// acknowledges contiguously and asks again with an ACK-only batch. Returns
// every result datagram the Gateway wrote.
func resendScenario(t *testing.T, seed uint64, resend bool) []resultDatagram {
	t.Helper()
	r := newResultLoop(t)
	start := r.now
	random := rand.New(rand.NewPCG(seed, seed))
	var out []resultDatagram
	// Client state.
	nextID, received, knownRetired := uint64(1), map[uint64]bool{}, uint64(0)
	nextShot, nextBatch := time.Duration(0), time.Duration(0)
	var unsent []uint64
	// Match state: decision and retirement times, what it decided and retired.
	decideAt, retireAt := map[uint64]time.Duration{}, map[time.Duration]uint64{}
	decided, retired, lastResend := []uint64{}, uint64(0), time.Duration(0)
	send := func(ids ...uint64) { r.match(retired, ids...) }
	for step := time.Duration(0); step < 3*time.Second; step += time.Millisecond {
		r.now = start.Add(step)
		// The Client: a new shot now and then, a batch every 33 ms carrying
		// the new shots and its contiguous ACK; ACK-only while ahead of the
		// retirement it knows, or when that retirement looks lost.
		if step >= nextShot {
			unsent = append(unsent, nextID)
			nextID++
			nextShot = step + time.Duration(20+random.IntN(130))*time.Millisecond
		}
		if step >= nextBatch {
			ack := knownRetired
			for received[ack+1] {
				ack++
			}
			if len(unsent) > 0 || ack > knownRetired || (ack > 0 && random.IntN(10) == 0) {
				r.clientBatch(ack, unsent...)
				for _, id := range unsent {
					decideAt[id] = step + time.Duration(5+random.IntN(40))*time.Millisecond
				}
				unsent = nil
				retireAt[step+time.Duration(1+random.IntN(30))*time.Millisecond] = ack
			}
			nextBatch = step + 33*time.Millisecond
		}
		// The Match: each decision and each retirement once, when it exists.
		var fresh []uint64
		for id, at := range decideAt {
			if at <= step {
				fresh = append(fresh, id)
				delete(decideAt, id)
			}
		}
		slices.Sort(fresh)
		decided = append(decided, fresh...)
		if through, ok := retireAt[step]; ok && through > retired {
			retired = through
			decided = slices.DeleteFunc(decided, func(id uint64) bool { return id <= retired })
			send()
		}
		if len(fresh) > 0 {
			send(fresh...)
		}
		if resend && len(decided) > 0 && step-lastResend >= actionSendInterval {
			lastResend = step
			send(decided[:min(len(decided), 8)]...)
		}
		// The Gateway's send loop at its deadline.
		if deadline, pending := r.s.resultDeadline(); pending && !deadline.After(r.now) {
			packets, _, _ := r.s.actionPackets(r.now)
			for _, packet := range packets {
				r.s.actionWritten(packet.actionPlayer, r.now)
				h, payload, err := framing.DecodeDatagram(packet.packet)
				if err != nil {
					t.Fatal(err)
				}
				out = append(out, resultDatagram{at: step, sequence: h.Sequence, payload: payload})
				// The Client loses one result datagram in five.
				if random.IntN(5) == 0 {
					continue
				}
				var results client.ActionResults
				if err := proto.Unmarshal(payload, &results); err != nil {
					t.Fatal(err)
				}
				knownRetired = max(knownRetired, results.RetiredThrough)
				for _, d := range results.Decisions {
					received[d.ActionId] = true
				}
			}
		}
	}
	return out
}

// pv7 §9 (edge-results-resend): while the Gateway still keeps its result
// ledger, a copy of a decision or a retirement merges as nothing new: added to
// the same first sends, copies leave the Client's datagrams unchanged, datagram
// for datagram, at the same virtual times.
func TestMatchResultCopiesChangeNothingAtTheGateway(t *testing.T) {
	for seed := uint64(1); seed <= 8; seed++ {
		t.Run(fmt.Sprint("seed=", seed), func(t *testing.T) {
			once := resendScenario(t, seed, false)
			again := resendScenario(t, seed, true)
			if len(once) < 50 {
				t.Fatalf("only %d result datagrams: the scenario exercised too little", len(once))
			}
			if len(once) != len(again) {
				t.Fatalf("%d datagrams with each result sent once, %d with resends", len(once), len(again))
			}
			for k := range once {
				a, b := once[k], again[k]
				if a.at != b.at || a.sequence != b.sequence || !bytes.Equal(a.payload, b.payload) {
					t.Fatalf("datagram %d differs: once at %v sequence %d, resent at %v sequence %d", k, a.at, a.sequence, b.at, b.sequence)
				}
			}
		})
	}
}

// matchLane models IpcHost's ActionLane (pv7 §2a): one frame per action
// interval at most, carrying the retirement, the four smallest unacknowledged
// decisions and four cycling slots; due while anything is unacknowledged or
// the retirement advanced.
type matchLane struct {
	sent              bool
	last              time.Duration
	sentRetired, next uint64 // next: the cursor (the last cycled id)
}

func (m *matchLane) frame(now time.Duration, decided []uint64, retired uint64) ([]uint64, bool) {
	if len(decided) == 0 && retired <= m.sentRetired || m.sent && now-m.last < actionSendInterval {
		return nil, false
	}
	m.sent, m.last, m.sentRetired = true, now, retired
	first := min(len(decided), 4)
	ids := slices.Clone(decided[:first])
	rest := decided[first:]
	start, _ := slices.BinarySearch(rest, m.next+1)
	for k := 0; k < min(len(rest), 8-first); k++ {
		id := rest[(start+k)%len(rest)]
		ids = append(ids, id)
		m.next = id
	}
	slices.Sort(ids)
	return ids, true
}

// lanePace runs a lossless Client against the Match either sending each
// decision and retirement the moment it exists, or through matchLane. Returns
// when the Client first received each decision.
func lanePace(t *testing.T, seed uint64, lane bool) map[uint64]time.Duration {
	t.Helper()
	r := newResultLoop(t)
	start := r.now
	random := rand.New(rand.NewPCG(seed, seed))
	// The shots and their decision delays are drawn up front, so both runs
	// see the same ones whatever the results do.
	shotAt, decideAfter := map[time.Duration]bool{}, map[uint64]time.Duration{}
	for at, id := time.Duration(0), uint64(1); at < 3*time.Second; id++ {
		shotAt[at] = true
		decideAfter[id] = time.Duration(5+random.IntN(40)) * time.Millisecond
		at += time.Duration(10+random.IntN(110)) * time.Millisecond
	}
	got := map[uint64]time.Duration{}
	nextID, knownRetired := uint64(1), uint64(0)
	var unsent []uint64
	decideAt, retireAt := map[uint64]time.Duration{}, map[time.Duration]uint64{}
	decided, retired := []uint64{}, uint64(0)
	var m matchLane
	for step := time.Duration(0); step < 3500*time.Millisecond; step += time.Millisecond {
		r.now = start.Add(step)
		if shotAt[step] {
			unsent = append(unsent, nextID)
			nextID++
		}
		if step%(33*time.Millisecond) == 0 {
			ack := knownRetired
			for {
				if _, seen := got[ack+1]; !seen {
					break
				}
				ack++
			}
			if len(unsent) > 0 || ack > knownRetired {
				r.clientBatch(ack, unsent...)
				for _, id := range unsent {
					decideAt[id] = step + decideAfter[id]
				}
				unsent = nil
				retireAt[step+10*time.Millisecond] = ack
			}
		}
		var fresh []uint64
		for id, at := range decideAt {
			if at <= step {
				fresh = append(fresh, id)
				delete(decideAt, id)
			}
		}
		slices.Sort(fresh)
		decided = append(decided, fresh...)
		advanced := false
		if through, ok := retireAt[step]; ok && through > retired {
			retired, advanced = through, true
			decided = slices.DeleteFunc(decided, func(id uint64) bool { return id <= retired })
		}
		if lane {
			if ids, ok := m.frame(step, decided, retired); ok {
				r.match(retired, ids...)
			}
		} else {
			if advanced {
				r.match(retired)
			}
			if len(fresh) > 0 {
				r.match(retired, fresh...)
			}
		}
		if deadline, pending := r.s.resultDeadline(); pending && !deadline.After(r.now) {
			packets, _, _ := r.s.actionPackets(r.now)
			for _, packet := range packets {
				r.s.actionWritten(packet.actionPlayer, r.now)
				_, payload, err := framing.DecodeDatagram(packet.packet)
				if err != nil {
					t.Fatal(err)
				}
				var results client.ActionResults
				if err := proto.Unmarshal(payload, &results); err != nil {
					t.Fatal(err)
				}
				knownRetired = max(knownRetired, results.RetiredThrough)
				for _, d := range results.Decisions {
					if _, seen := got[d.ActionId]; !seen {
						got[d.ActionId] = step
					}
				}
			}
		}
	}
	return got
}

// The Match's own schedule (one frame per interval, resends included) can
// hold a new decision back: measured here against sending it the moment it
// exists, lossless, with the Gateway's ledger still in between. Every decision
// still reaches the Client, and the extra delay is bounded.
func TestMatchLaneDelaysANewDecisionAtMostOneIntervalAtTheClient(t *testing.T) {
	worst, total, later := time.Duration(0), 0, 0
	for seed := uint64(1); seed <= 8; seed++ {
		once, lane := lanePace(t, seed, false), lanePace(t, seed, true)
		if len(once) < 20 || len(lane) != len(once) {
			t.Fatalf("seed %d: %d decisions received when sent once, %d through the lane", seed, len(once), len(lane))
		}
		for id, at := range once {
			extra := lane[id] - at
			worst = max(worst, extra)
			total++
			if extra > 0 {
				later++
			}
		}
	}
	t.Logf("decisions %d, later through the lane %d, worst extra delay %v", total, later, worst)
	if worst > actionSendInterval+time.Millisecond {
		t.Fatalf("a decision reached the Client %v later through the lane", worst)
	}
}
