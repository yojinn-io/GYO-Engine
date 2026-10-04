"""v5 repeated-plan soak evidence: every cycle, every life, streamed.

The probe repeats the 16-second gameplay plan; player B dies once per cycle and
player A never dies. Clean runs only: there is no relay and no injected fault.
Frames and diagnostic traces are streamed, because a 1808-second 144 Hz run is
about a gigabyte of JSON lines. Per-life combat is checked with the same replay
as the short gameplay analyzer, limited to that life's decisions.
"""
from collections import Counter, defaultdict, deque
import json
import math
from pathlib import Path

from action_evidence import records, frames
from command_evidence import read_trace_events
from gameplay_evidence import (client_disturbance, combat_expected, life_reset_matches, percentile,
                               start_phase_by_player, validate_accepted_actions)
from start_phase_evidence import reseed_evidence
from acceptance_util import PROTOCOL_VERSION

PHASE_STATES = ('acquiring', 'settling', 'tracking')
DRIFT_BUCKET_CYCLES = 10
MOVEMENT_MARGIN_NS = 1_000_000_000


def cycle_of(time_ns, start_ns, cycle_ns):
    return (time_ns-start_ns)//cycle_ns


def expected_target_life(cycle, player_index, lives_per_cycle):
    """Life of the other player inside a cycle; every life starts at generation 1."""
    return 1+cycle*lives_per_cycle[1-player_index]


class PhaseTally:
    """Phase-tracking time and corrections from per-frame Client observations.

    The Client's counters restart with each movement epoch and life, so a
    decrease starts a new count instead of being subtracted."""

    def __init__(self):
        self.seconds = Counter()
        self.corrections = self.late_corrections = 0
        self.per_cycle = Counter()
        self.previous = None

    def observe(self, time_ns, state, corrections, late, cycle):
        if self.previous is not None:
            before_ns, before_state, before_corrections, before_late = self.previous
            self.seconds[PHASE_STATES[before_state]] += (time_ns-before_ns)/1e9
            added = corrections-before_corrections if corrections >= before_corrections else corrections
            self.corrections += added
            self.per_cycle[cycle] += added
            self.late_corrections += late-before_late if late >= before_late else late
        else:
            self.corrections, self.late_corrections = corrections, late
            self.per_cycle[cycle] += corrections
        self.previous = time_ns, state, corrections, late

    def summary(self, cycles):
        return {'state_seconds': dict(self.seconds), 'corrections': self.corrections,
                'late_corrections': self.late_corrections,
                'corrections_per_cycle': [self.per_cycle.get(cycle, 0) for cycle in range(cycles)]}


class LifeTally:
    """Idempotent per-life aggregates; both Clients report the same authority ticks."""

    def __init__(self):
        self.first = {}
        self.dead = {}

    def observe(self, time_ns, tick, player):
        identity = player['player_id'], player['life_generation']
        first = self.first.get(identity)
        if first is None or tick < first[0]:
            self.first[identity] = tick, player
        if player['life_state'] != 1:
            return
        dead = self.dead.get(identity)
        x, z = player['position'][0], player['position'][2]
        if dead is None:
            self.dead[identity] = dead = {'tick': tick, 'time_ns': time_ns, 'sample': player, 'state_ticks': set(),
                                          'waits': set(), 'x': [x, x], 'z': [z, z], 'epoch': player['epoch']}
        elif tick < dead['tick']:
            dead.update(tick=tick, time_ns=time_ns, sample=player)
        dead['state_ticks'].add(player['life_state_tick'])
        dead['waits'].add(player['respawn_tick']-player['life_state_tick'])
        dead['x'] = [min(dead['x'][0], x), max(dead['x'][1], x)]
        dead['z'] = [min(dead['z'][0], z), max(dead['z'][1], z)]
        dead['epoch'] = max(dead['epoch'], player['epoch'])

    def finish(self, player_ids, lives_per_cycle, cycles, start_ns, cycle_ns):
        errors, deaths, respawns = [], [], []
        def check(ok, message):
            if not ok and message not in errors:
                errors.append(message)
        for index, player in enumerate(player_ids):
            expected = set(range(1, 2+cycles*lives_per_cycle[index]))
            observed = {life for owner, life in self.first if owner == player}
            check(observed == expected, f'Player {player} lives {sorted(observed)} differ from planned {sorted(expected)}')
            for life in sorted(observed):
                dead = self.dead.get((player, life))
                if dead:
                    sample = dead['sample']
                    deaths.append({'player_id': player, 'life': life, 'tick': sample['life_state_tick'],
                                   'respawn_tick': sample['respawn_tick'],
                                   'cycle': cycle_of(dead['time_ns'], start_ns, cycle_ns)})
                    check(dead['waits'] == {180}, 'Death wait is not180 authority ticks')
                    check(dead['x'][1]-dead['x'][0] < 1e-5 and dead['z'][1]-dead['z'][0] < 1e-5,
                          'Dead player moved horizontally')
                    check(len(dead['state_ticks']) == 1, 'Death occurred more than once within life')
                    check(life == max(expected) or deaths[-1]['cycle'] == (life-1)//max(lives_per_cycle[index], 1),
                          'Death did not occur inside its planned cycle')
                else:
                    check(life == max(expected), f'Player {player} life {life} ended without an observed death')
                if life > 1:
                    old = self.dead.get((player, life-1))
                    check(old is not None, 'New life lacks preceding observed death')
                    if old:
                        first = self.first[(player, life)][1]
                        check(first['life_state_tick'] >= old['sample']['respawn_tick'], 'Respawn occurred before deadline')
                        check(first['epoch'] > old['epoch'], 'Respawn did not establish new movement epoch')
                        respawns.append({'player_id': player, 'life': life, 'tick': first['life_state_tick']})
            check(self.dead.get((player, max(expected))) is None, f'Player {player} final life died outside the plan')
        return errors, deaths, respawns


class JumpTally:
    """Each player must leave the ground and land again inside each cycle's alive window."""

    def __init__(self, cycles):
        self.peak = defaultdict(float)
        self.landed = set()
        self.cycles = cycles

    def observe(self, offset_seconds, cycle, player):
        identity = player['player_id'], cycle
        if offset_seconds < 6:
            self.peak[identity] = max(self.peak[identity], player['position'][1])
            if offset_seconds >= 2.5 and player['grounded']:
                self.landed.add(identity)

    def errors(self, player_ids):
        missing = [(player, cycle) for player in player_ids for cycle in range(self.cycles)
                   if self.peak.get((player, cycle), 0) <= .4 or (player, cycle) not in self.landed]
        return ([f'Jump did not raise and land authoritative feet in {len(missing)} player-cycle(s): {missing[:8]}']
                if missing else [])


def drift_windows(per_cycle_actual, bucket=DRIFT_BUCKET_CYCLES):
    """The approved movement thresholds applied to consecutive cycle windows."""
    windows = []
    for first in range(0, len(per_cycle_actual), bucket):
        values = [value for cycle in per_cycle_actual[first:first+bucket] for value in cycle]
        windows.append({'cycles': [first, min(first+bucket, len(per_cycle_actual))-1], 'commands': len(values),
                        'actual_p50_ms': percentile(values, 50), 'actual_p95_ms': percentile(values, 95),
                        'passed': bool(values) and percentile(values, 50) <= 50 and percentile(values, 95) <= 66.7})
    return windows


def finite_or_none(value):
    return value if isinstance(value, (int, float)) and math.isfinite(value) else None


def analyze_soak(output, gateway_counters, *, soak=False):
    output = Path(output)
    errors = []
    def check(ok, message):
        if not ok and message not in errors:
            errors.append(message)
    client = json.loads((output/'action-client.json').read_text())
    plan = json.loads((output/'gameplay-plan.json').read_text())
    cycles, cycle_seconds = plan['cycles'], plan['cycle_seconds']
    cycle_ns, lives_per_cycle = round(cycle_seconds*1e9), plan['lives_per_cycle']
    start_ns, player_ids = client['start_ns'], client['player_ids']
    end_ns = start_ns+cycles*cycle_ns
    check(client.get('gameplay_v5') and client['protocol'] == PROTOCOL_VERSION and client['passed'],
          'v5 client did not finish/retire all actions')
    check(client.get('cycles') == cycles and client['duration'] == cycles*cycle_seconds and
          plan['protocol'] == PROTOCOL_VERSION and len(plan['actions']) == client['planned_actions'],
          'Predeclared cycle denominator missing or differs from the probe')
    check(client['end_ns'] >= end_ns, 'Probe ended before the declared cycles')
    rules = client['clients'][0]
    capacity, reload_ticks, maximum_hp = rules['magazine_capacity'], rules['reload_ticks'], rules['maximum_hp']

    # Actions: every declared instance exactly once, exact verdict, one decision.
    submitted, delivered, by_plan, decisions = {}, {}, {}, []
    for event in records(output/'actions.jsonl'):
        if event['kind'] == 'submitted':
            identity, ordinal = (event['player_id'], event['request']['action_id']), event['plan_ordinal']
            check(identity not in submitted and ordinal not in by_plan, 'Duplicate original action identity/plan ordinal')
            submitted[identity] = event; by_plan[ordinal] = event
        elif event['kind'] == 'decision':
            identity = event['player_id'], event['decision']['action_id']
            check(identity not in delivered, 'Duplicate delivered decision'); delivered[identity] = event
            decisions.append((event['player_id'], event['decision']))
        else:
            check(event['kind'] == 'jump', 'Unknown action evidence kind')
    check(set(by_plan) == set(range(len(plan['actions']))), 'A predeclared action was omitted')
    check(set(submitted) == set(delivered), 'Missing/unknown terminal decision')
    legal_ticks = {d['resolved_tick'] for _, d in decisions}
    verdicts, per_cycle_verdicts = Counter(), defaultdict(Counter)
    legal, unexpected = [], []
    for ordinal, declared in enumerate(plan['actions']):
        event = by_plan.get(ordinal)
        if not event:
            continue
        identity = event['player_id'], event['request']['action_id']
        result = delivered.get(identity)
        check(event['player_id'] == player_ids[declared['player_index']], 'Plan attributed to wrong Session')
        check(event['request']['life_generation'] == declared['life_generation'] and
              event['request']['action_kind'] == declared['action_kind'], 'Action immutable content differs from plan')
        check(event['time_ns'] >= start_ns+round(declared['at_seconds']*1e9), 'Action generated before scheduled edge')
        if not result:
            continue
        d = result['decision']
        verdicts[str(d['rejection'])] += 1
        if declared['name'] != 'cycle-reload':
            per_cycle_verdicts[declared['cycle']][declared['name'], d['rejection']] += 1
        check(d['life_generation'] == event['request']['life_generation'] and d['action_kind'] == event['request']['action_kind'],
              'Decision changed request kind/life')
        if d['rejection'] != declared['expected_rejection']:
            unexpected.append({'cycle': declared['cycle'], 'name': declared['name'], 'rejection': d['rejection']})
        check(d['accepted'] == (d['rejection'] == 0), 'Accepted/rejection contradiction')
        check(d['accepted'] or d['damage'] == 0, 'Rejected action damaged a player')
        check(d['damage'] in (0, 25) and (not d['damage'] or d['hit_kind'] == 2 and d['target_life_generation'] > 0),
              'Invalid v5 damage attribution')
        if declared['target']:
            life = expected_target_life(declared['cycle'], declared['player_index'], lives_per_cycle)
            check(d['accepted'] and d['damage'] == 25 and d['target_life_generation'] == life,
                  'Predeclared lethal hit did not damage the cycle life exactly once')
        if declared['expected_rejection'] == 0:
            legal.append((declared['cycle'], event['time_ns'], d['resolved_tick'], result['time_ns']))
    check(not unexpected, f'Unexpected verdicts: {unexpected[:8]}')

    # Match trace: tick times for legal actions, resets, backlog, runtime gaps.
    tick_times, resolved, resets, match_gaps = {}, {}, [], []
    queue, queue_max = defaultdict(lambda: deque(maxlen=30)), 0
    measure_start, measure_end = start_ns+MOVEMENT_MARGIN_NS, end_ns-MOVEMENT_MARGIN_NS
    for event in read_trace_events(output/'match-commands.jsonl'):
        kind, timestamp = event['kind'], event['time_ns']
        inside = start_ns <= timestamp <= client['end_ns']
        if kind == 'snapshot_produced' and inside and event['authority_tick'] in legal_ticks:
            tick_times[event['authority_tick']] = timestamp
        elif kind == 'resolved':
            key = event['player_id'], event['epoch'], event['sequence']
            check(key not in resolved, 'Duplicate resolution')
            resolved[key] = timestamp if event['source'] == 'actual' else None
            history = queue[event['player_id'], event['epoch']]; history.append(event['queued'])
            if measure_start <= timestamp < measure_end:
                queue_max = max(queue_max, sum(history))
        elif kind == 'reset' and inside:
            resets.append(event)
        elif kind == 'runtime_gap' and measure_start <= timestamp < measure_end and (
                event['frame_seconds'] >= .1 or event['dropped_seconds'] > 0):
            match_gaps.append(event)
    match_ms, client_ms = [], []
    per_cycle_match = defaultdict(list)
    for cycle, generated, tick, received in legal:
        at = tick_times.get(tick)
        check(at is not None, 'Missing Match tick for legal action')
        value = (at-generated)/1e6 if at is not None else math.inf
        match_ms.append(value); per_cycle_match[cycle].append(value); client_ms.append((received-generated)/1e6)
    check(percentile(match_ms, 95) <= 100, 'Legal generation-to-Match P95 exceeds100ms')
    check(percentile(client_ms, 95) <= 150, 'Legal generation-to-client P95 exceeds150ms')

    # Frames: windows, per-life combat, lives, jumps, phase tracking.
    lives, jumps = LifeTally(), JumpTally(cycles)
    phase = {suffix: PhaseTally() for suffix in ('a', 'b')}
    memo, observations, frame_count, previous_time, maximum_frame, slow_frames = {}, 0, 0, 0, 0.0, []
    groups = defaultdict(list)
    for owner, d in decisions:
        groups[owner, d['life_generation']].append((owner, d))
        if d['target_id']:
            groups[d['target_id'], d['target_life_generation']].append((owner, d))
    for frame in frames(output):
        time_ns, dt = frame['time_ns'], frame['frame_seconds']
        check(time_ns > previous_time and math.isfinite(dt) and dt >= 0, 'Invalid/nonmonotonic frame timestamp')
        if frame_count and (dt >= .1 or time_ns-previous_time >= 100_000_000):
            slow_frames.append({'time_ns': time_ns, 'frame_seconds': dt})
        previous_time = time_ns; frame_count += 1; maximum_frame = max(maximum_frame, dt)
        cycle = cycle_of(time_ns, start_ns, cycle_ns)
        offset = (time_ns-start_ns)/1e9-cycle*cycle_seconds
        check(not frame['drain_stalled'], 'Soak frame reported an injected drain stall')
        for suffix in ('a', 'b'):
            check(frame['pending_'+suffix] <= 12 and frame['queued_'+suffix] <= 32 and frame['retained_'+suffix] <= 32,
                  'Bounded command/action window exceeded')
            tick = frame['snapshot_tick_'+suffix]
            for combat in frame['combat_'+suffix]:
                key = combat['player_id'], combat['life_generation'], tick
                expected = memo.get(key)
                if expected is None:
                    expected = memo[key] = combat_expected(key[0], key[1], tick, groups.get(key[:2], []), maximum_hp,
                                                           capacity, reload_ticks)
                check(all(combat[name] == value for name, value in expected.items()),
                      'Authority HP/ammo/reload/last-shot differs from unique per-life effects')
                observations += 1
            for player in frame['players_'+suffix]:
                lives.observe(time_ns, tick, player)
                if 0 <= cycle < cycles:
                    jumps.observe(offset, cycle, player)
            if start_ns <= time_ns < end_ns:
                phase[suffix].observe(time_ns, frame['phase_state_'+suffix], frame['phase_corrections_'+suffix],
                                      frame['phase_late_corrections_'+suffix], cycle)
        if len(memo) > 8192:
            newest = max(key[2] for key in memo)
            memo = {key: value for key, value in memo.items() if key[2] > newest-600}
    life_errors, deaths, respawns = lives.finish(player_ids, lives_per_cycle, cycles, start_ns, cycle_ns)
    for error in life_errors+jumps.errors(player_ids):
        check(False, error)
    expected_deaths = cycles*sum(lives_per_cycle)
    check(len(deaths) == len(respawns) == expected_deaths, f'Expected {expected_deaths} deaths and respawns')
    life_resets = [event for event in resets if event['reset_reason'] == 'life_respawn']
    check(len(life_resets) == len(respawns), 'LifeRespawn reset count differs from actual new lives')
    check(all(life_reset_matches(event, respawns) for event in life_resets),
          'LifeRespawn reset does not match authoritative new life/tick')
    check(all(event['reset_reason'] == 'life_respawn' for event in resets), 'Clean soak had backlog/starvation reset')
    life_starts = {(player, 1): 0 for player in player_ids}
    life_starts.update({(r['player_id'], r['life']): r['tick'] for r in respawns})
    for error in validate_accepted_actions(decisions, life_starts, capacity=capacity, reload_ticks=reload_ticks,
                                           cooldown_ticks=rules['cooldown_ticks']):
        check(False, error)
    check(not slow_frames, 'Client full-run frame reached100ms')

    # Client trace: movement originals, first sends, cancellations, runtime gaps.
    # Stall reseeds are judged from seeded commands alone: a real command between
    # two seeded ones already breaks their consecutive sequence.
    generated, first_send, cancelled, runtime_gaps, seeded = {}, {}, {}, [], []
    second = player_ids[1]
    for event in read_trace_events(output/'clients-commands.jsonl'):
        kind, key = event['kind'], (event['player_id'], event['epoch'], event['sequence'])
        if kind == 'generated' and event['seeded_neutral']:
            seeded.append(event)
        elif kind == 'generated' and measure_start <= event['time_ns'] < measure_end:
            generated[key] = event['time_ns']
        elif kind == 'sent' and key not in first_send:
            first_send[key] = event['time_ns']
        elif kind == 'lifecycle_cancelled':
            check(event['player_id'] == second and any(
                r['player_id'] == second and r['life_generation'] == event['life_generation']+1 and
                r['epoch'] > event['epoch'] and r['time_ns'] <= event['time_ns'] for r in life_resets),
                'Lifecycle cancellation lacks matching old-life command/new-life reset')
            if key not in resolved:
                cancelled[key] = event
        elif kind == 'runtime_gap':
            runtime_gaps.append(event)
    gap_errors, life_clamps = client_disturbance(runtime_gaps, [], life_resets)
    for error in gap_errors:
        check(False, error)
    per_cycle_actual = [[] for _ in range(cycles)]
    actual, sends = [], []
    for key, at in generated.items():
        if key in cancelled:
            continue
        done = resolved.get(key)
        value = (done-at)/1e6 if done is not None else math.inf
        actual.append(value); per_cycle_actual[cycle_of(at, start_ns, cycle_ns)].append(value)
        sends.append((first_send[key]-at)/1e6 if key in first_send else math.inf)
    actual_count = sum(math.isfinite(value) for value in actual)
    check(bool(actual), 'No measured original movement commands')
    check(percentile(actual, 50) <= 50 and percentile(actual, 95) <= 66.7, 'Clean movement Actual exceeds50/66.7ms')
    check(percentile(sends, 95) <= 22, 'Clean movement first send P95 exceeds22ms')
    check(actual_count >= .99*len(actual), 'Clean original movement Actual below99%')
    check(queue_max < 105, 'Clean run reached30tick backlog threshold')
    check(not match_gaps, 'Clean Match runtime gap/dropped time')
    windows = drift_windows(per_cycle_actual)
    check(all(window['passed'] for window in windows), 'Movement Actual drifted past50/66.7ms in a cycle window')

    # Capacity without a relay: the Client's own transport maxima and the Gateway's limiter.
    for summary in client['clients']:
        check(summary['retained'] == 0 and summary['retired_through'] == summary['submitted'] == summary['decisions'],
              'Action IDs did not remain contiguous and retire across lives')
        check(summary['maximum_datagram_bytes'] <= 1200 and summary['maximum_batch_shots'] <= 8,
              'Action batch/datagram exceeds8/1200')
    check(gateway_counters.get('rate_limited_packets', 1) == 0, 'Actual Gateway rate rejection')
    check(gateway_counters.get('max_session_window_packets', 121) <= 120, 'Actual Gateway limiter exceeds120')

    measured_seconds = cycles*cycle_seconds
    result = {
        'passed': not errors, 'errors': errors, 'protocol': PROTOCOL_VERSION, 'fps': client['fps'], 'cycles': cycles,
        'measured_seconds': measured_seconds, 'soak_requested': soak,
        'full_30_minute_qualified': soak and measured_seconds >= 1800 and not errors,
        'scope': 'repeated 16-second v5 gameplay plan; same-host real sockets, headless, clean (no relay or fault); '
                 'no GUI, input-to-photon or Client/Host clock-drift claim',
        'planned_actions': len(plan['actions']), 'delivered_actions': len(delivered), 'verdicts': dict(verdicts),
        # The same plan must produce the same verdicts in every cycle; 1 is expected.
        'per_cycle_verdict_signatures': len({tuple(sorted(counts.items())) for counts in per_cycle_verdicts.values()}),
        'legal_actions': len(legal), 'legal_match_p95_ms': finite_or_none(percentile(match_ms, 95)),
        'legal_client_p95_ms': finite_or_none(percentile(client_ms, 95)),
        'legal_match_p95_ms_per_cycle': [finite_or_none(percentile(per_cycle_match[c], 95)) for c in range(cycles)],
        'deaths': len(deaths), 'respawns': len(respawns), 'life_resets': len(life_resets),
        'combat_observations': observations,
        'movement': {'window_seconds': [MOVEMENT_MARGIN_NS/1e9, measured_seconds-MOVEMENT_MARGIN_NS/1e9],
                     'originals': len(generated), 'lifecycle_cancelled': len(cancelled), 'remaining': len(actual),
                     'actual': actual_count, 'actual_fraction': actual_count/len(actual) if actual else 0,
                     'actual_p50_ms': finite_or_none(percentile(actual, 50)),
                     'actual_p95_ms': finite_or_none(percentile(actual, 95)),
                     'send_p95_ms': finite_or_none(percentile(sends, 95)), 'queue_30_tick_sum_max': queue_max,
                     'per_cycle_p95_ms': [finite_or_none(percentile(values, 95)) for values in per_cycle_actual],
                     'drift_windows': windows, 'drift_rule': f'approved 50/66.7 ms thresholds per {DRIFT_BUCKET_CYCLES}-cycle window',
                     'matched_life_seed_clamps': len(life_clamps)},
        'phase_tracking': {str(player_ids[index]): phase[suffix].summary(cycles) for index, suffix in enumerate(('a', 'b'))},
        'frames': frame_count, 'maximum_frame_seconds': maximum_frame, 'slow_frames': slow_frames[:20],
        'gateway_counters': gateway_counters,
        'start_phase': start_phase_by_player(client, reseed_evidence(seeded, 'clients-commands.jsonl')),
    }
    (output/'gameplay-soak-evidence.json').write_text(json.dumps(result, indent=2)+'\n')
    return result
