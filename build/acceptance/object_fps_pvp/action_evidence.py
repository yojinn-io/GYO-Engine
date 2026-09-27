"""Product probe evidence: streaming integrity and legal-rate coexistence gates."""
from bisect import bisect_left, bisect_right
from collections import Counter
import json
import math
from pathlib import Path

from command_evidence import analyze_commands, finite, nearest_rank, read_trace_events


def records(path, *, legacy=False):
    count, ended = 0, False
    with Path(path).open() as stream:
        for number, line in enumerate(stream, 1):
            try:
                value = json.loads(line)
                if ended:
                    raise ValueError('record after evidence_end')
                if value.get('kind') == 'evidence_end':
                    if value['records'] != count or value['lost'] != 0:
                        raise ValueError('record count mismatch or diagnostic loss')
                    ended = True
                else:
                    count += 1
                    yield value
            except (ValueError, KeyError, TypeError) as error:
                raise ValueError(f'{path}:{number}: corrupt action evidence: {error}') from error
    if not ended and not legacy:
        raise ValueError(f'{path}: missing evidence_end')


def frames(output):
    path = output/'action-frames.jsonl'
    if path.exists():
        yield from records(path)
    else:
        yield from json.loads((output/'action-frames.json').read_text())


def analyze_legal(output, *, injected=False):
    """Includes every legal shot; missing outcomes become infinity and fail."""
    output = Path(output)
    client = json.loads((output/'action-client.json').read_text())
    errors = []
    def check(condition, message):
        if not condition and message not in errors:
            errors.append(message)
    check(client['legal_shots'], 'Probe was not configured for legal-rate shots')
    check(client['passed'], 'Client submission/decision/retirement did not converge')
    start, end = client['measurement_start_ns'], client['measurement_end_ns']
    player_ids = client['player_ids']
    check(len(player_ids) == 2 and len(set(player_ids)) == 2,
          'Expected exactly two distinct player identities')
    check(len(client['clients']) == 2 and {c['player_id'] for c in client['clients']} == set(player_ids),
          'Client summaries do not contain exactly both players')
    check(end-start == round(client['duration']*1e9), 'Measurement duration differs from requested real seconds')
    period_ns = round(1e9/client['fps'])
    check(client['start_ns'] <= start and client['end_ns'] >= end+2_000_000_000-period_ns,
          'Actual probe clock does not cover measurement and two-second drain')
    submitted, decisions, received = {}, {}, {}
    prior_submission = {}
    for event in records(output/'actions.jsonl'):
        check(event['kind'] in ('submitted', 'decision'), 'Unknown action evidence kind')
        player, timestamp = event['player_id'], event['time_ns']
        if event['kind'] == 'submitted':
            key = player, event['request']['action_id']
            check(key not in submitted, 'Repeated original submission identity')
            check(start <= timestamp < end, 'Legal shot outside measurement window')
            check(timestamp-prior_submission.get(player, timestamp-400_000_000) >= 400_000_000,
                  'Legal shot schedule faster than 400ms')
            prior_submission[player] = timestamp
            submitted[key] = timestamp
        elif event['kind'] == 'decision':
            key = player, event['decision']['action_id']
            check(key not in decisions, 'Duplicate delivered decision/effect')
            check(key in submitted, 'Decision without original submission')
            decisions[key], received[key] = event['decision'], timestamp
    check(bool(submitted), 'No legal actions submitted')
    check(set(decisions) == set(submitted), 'Unknown/missing decision identities')
    ticks = {}
    all_match_gap_events = 0
    for event in read_trace_events(output/'match-commands.jsonl'):
        if event['kind'] == 'runtime_gap' and (event['frame_seconds'] >= .1 or event['dropped_seconds'] > 0):
            all_match_gap_events += 1
        # Empty-room teardown can restart Tick numbering after Leave. Only this
        # probe's live PlayerId interval can adjudicate its original actions.
        if event['kind'] == 'snapshot_produced' and client['start_ns'] <= event['time_ns'] <= client['end_ns']:
            tick = event['authority_tick']
            check(tick not in ticks, 'Duplicate authoritative Tick observation')
            ticks[tick] = event['time_ns']
    adjudication_ms, delivery_ms = [], []
    rejections = Counter()
    damage = {player: [] for player in client['player_ids']}
    for key, generated in submitted.items():
        d = decisions.get(key)
        resolved = ticks.get(d['resolved_tick']) if d else None
        adjudication_ms.append((resolved-generated)/1e6 if resolved is not None else math.inf)
        delivery_ms.append((received[key]-generated)/1e6 if key in received else math.inf)
        check(resolved is not None, 'Missing Match Tick for action adjudication')
        if resolved is not None:
            check(generated <= resolved <= received[key], 'Invalid same-host adjudication/delivery ordering')
        if d:
            rejections[str(d['rejection'])] += 1
            check((d['accepted'] and d['rejection'] == 0) or (not d['accepted'] and d['rejection'] in (1, 2, 3)),
                  'Unknown/inconsistent terminal decision status')
            check(d['damage'] >= 0 and d['damage'] <= client['clients'][0]['shot_damage'], 'Invalid shot damage')
            check(d['accepted'] or d['damage'] == 0, 'Rejected action produced damage')
            check(d['damage'] == 0 or (d['hit_kind'] == 2 and d['target_id'] in damage), 'Damage without valid player hit')
            if d['damage']:
                damage[d['target_id']].append((d['resolved_tick'], d['damage']))
    prefixes = {}
    for player, values in damage.items():
        check(bool(values), 'Player '+str(player)+' had no actual authoritative HP decrease')
        values.sort()
        tick_values, totals = [], [0]
        for tick, value in values:
            tick_values.append(tick); totals.append(totals[-1]+value)
        prefixes[player] = tick_values, totals
    frame_count = gaps = frozen = hp_observations = all_loop_gaps = 0
    maximum_frame = maximum_pending = maximum_retained = 0
    intervals = []
    first_frame = previous_frame = last_frame = None
    frame_times = []
    previous_snapshot_ticks = {}
    for frame in frames(output):
        timestamp = frame['time_ns']
        frame_times.append(timestamp)
        check(isinstance(timestamp, int) and not isinstance(timestamp, bool) and timestamp > 0,
              'Invalid frame monotonic timestamp')
        check(math.isfinite(frame['frame_seconds']) and frame['frame_seconds'] >= 0, 'Invalid frame interval')
        if first_frame is None:
            first_frame = timestamp
        actual_interval = None
        if previous_frame is not None:
            check(timestamp > previous_frame, 'Frame clock is nonmonotonic')
            actual_interval = (timestamp-previous_frame)/1e9
        previous_frame = last_frame = timestamp
        all_loop_gaps += frame['frame_seconds'] >= .1 or (actual_interval is not None and actual_interval >= .1)
        if start <= frame['time_ns'] < end:
            frame_count += 1
            intervals.append(frame['frame_seconds'])
            maximum_frame = max(maximum_frame, frame['frame_seconds'])
            gaps += frame['frame_seconds'] >= .1
            frozen += any(frame['frozen_'+suffix] for suffix in ('a', 'b'))
        for index, suffix in enumerate(('a', 'b')):
            maximum_pending = max(maximum_pending, frame['pending_'+suffix])
            maximum_retained = max(maximum_retained, frame['retained_'+suffix])
            tick = frame['snapshot_tick_'+suffix]
            check(type(tick) is int and tick > 0 and tick >= previous_snapshot_ticks.get(suffix, 0),
                  'Snapshot Tick is invalid or went backwards')
            previous_snapshot_ticks[suffix] = tick
            combat = frame['combat_'+suffix]
            check(len(combat) == 2 and {state['player_id'] for state in combat} == set(player_ids),
                  'Frame combat state must contain exactly both distinct players')
            for state in combat:
                if state['player_id'] not in prefixes:
                    continue
                values, totals = prefixes[state['player_id']]
                maximum_hp = client['clients'][index]['maximum_hp']
                expected = maximum_hp-totals[bisect_right(values, tick)]
                check(type(state['hp']) is int and 0 <= state['hp'] <= maximum_hp and state['hp'] == expected,
                      'Snapshot HP differs from unique authoritative effects')
                hp_observations += 1
    # The probe schedules the next shot from the current iteration's clock plus
    # 400 ms, and records a separately sampled time_ns on that same frame. Allow
    # the first eligible recorded frame or its immediate successor, rather than
    # inventing a new firing-rate policy or assuming nominal FPS was achieved.
    shot_coverage = {}
    for player in player_ids:
        stamps = sorted(timestamp for (identity, _), timestamp in submitted.items() if identity == player)
        deadline = start
        for timestamp in stamps:
            frame_index = bisect_left(frame_times, timestamp)
            eligible = bisect_left(frame_times, deadline)
            check(frame_index < len(frame_times) and frame_times[frame_index] == timestamp,
                  'Shot submission has no corresponding recorded frame')
            check(frame_index <= eligible+1, 'Legal shot generation skipped its next eligible frame')
            deadline = timestamp+400_000_000
        eligible = bisect_left(frame_times, deadline)
        check(bool(stamps) and (eligible+1 >= len(frame_times) or frame_times[eligible+1] >= end),
              'Legal shot generation stopped before measurement ended')
        shot_coverage[player] = {'count': len(stamps), 'first_ns': stamps[0] if stamps else None,
                                 'last_ns': stamps[-1] if stamps else None,
                                 'maximum_interval_ms': max((b-a)/1e6 for a, b in zip(stamps, stamps[1:])) if len(stamps)>1 else None}
    for c in client['clients']:
        check(c['submitted'] == sum(player == c['player_id'] for player, _ in submitted) and
              c['decisions'] == sum(player == c['player_id'] for player, _ in decisions),
              'Client summary differs from complete action records')
        check(c['maximum_batch_shots'] <= 8 and c['maximum_datagram_bytes'] <= 1200,
              'Action batch/datagram exceeds8/1200')
        check(c['submitted'] == c['decisions'] and c['retained'] == 0 and c['retired_through'] == c['submitted'],
              'Not every submitted action was decided, delivered and retired')
        combat = c['combat']
        check(len(combat) == 2 and {state['player_id'] for state in combat} == set(player_ids),
              'Terminal combat state must contain exactly both distinct players')
        for state in combat:
            if state['player_id'] in prefixes:
                expected = c['maximum_hp']-prefixes[state['player_id']][1][-1]
                check(type(state['hp']) is int and 0 <= state['hp'] <= c['maximum_hp'] and state['hp'] == expected,
                      'Terminal HP differs from all unique authoritative effects')
    check(maximum_pending <= 12 and maximum_retained <= 32, 'Movement/action window exceeded bound')
    check(frame_count > 0 and hp_observations > 0, 'No measured frame/HP evidence')
    check(first_frame is not None and first_frame <= start and last_frame >= end,
          'Raw frame coverage does not span the declared measurement window')
    check(not any(client['blocked_submissions']), 'Legal action submission was blocked')
    p95match, p95client = nearest_rank(adjudication_ms, .95), nearest_rank(delivery_ms, .95)
    accepted = sum(d['accepted'] for d in decisions.values())
    if not injected:
        check(p95match <= 100, 'Generated to Match adjudication P95 exceeds100ms')
        check(p95client <= 150, 'Generated to Client decision P95 exceeds150ms')
        check(accepted == len(submitted), 'Clean legal shots were not all accepted')
        check(gaps == 0 and frozen == 0, 'Clean scheduling gap>=100ms or frozen prediction')
        check(all_loop_gaps == 0 and all_match_gap_events == 0,
              'Warmup/measurement/drain had scheduling gap>=100ms or Match dropped simulation time')
    timing = {'start_ns': start, 'end_ns': end, 'player_ids': client['player_ids'],
              'fps': client['fps'], 'injected': injected, 'gaps_100ms': gaps,
              'measurement_policy': 'fixed two-second warmup and two-second drain; all measured frames retained'}
    (output/'timing.json').write_text(json.dumps(timing, indent=2)+'\n')
    movement = analyze_commands(output, enforce=not injected)
    check(movement['passed'], 'Movement evidence failed: '+str(movement['errors']))
    if not injected:
        check(not movement['disturbed'] and movement['resets'] == 0 and movement['production_60hz_passed'],
              'Run was disturbed, reset epoch or did not produce60Hz commands; cannot qualify as clean')
    result = {'passed': not errors, 'errors': errors, 'injected': injected,
              'measurement_seconds': (end-start)/1e9, 'nominal_fps': client['fps'],
              'measured_frames': frame_count, 'actual_loop_hz': frame_count/((end-start)/1e9),
              'raw_frame_coverage_ns': [first_frame, last_frame], 'actual_probe_end_ns': client['end_ns'],
              'frame_p95_ms': finite(nearest_rank(intervals, .95)*1000), 'maximum_frame_ms': maximum_frame*1000,
              'gaps_100ms': gaps, 'frozen_frames': frozen, 'submitted': len(submitted),
              'all_loop_gaps_100ms': all_loop_gaps, 'all_match_gap_or_drop_events': all_match_gap_events,
              'delivered': len(decisions), 'accepted': accepted, 'rejection_counts': dict(rejections),
              'shot_coverage': shot_coverage,
              'adjudication_p95_ms': finite(p95match), 'client_decision_p95_ms': finite(p95client),
              'adjudication_timestamp': 'Match snapshot-produced after same resolved Tick; conservative upper bound',
              'hp_observations': hp_observations, 'unique_damage': {p: sum(v for _, v in d) for p, d in damage.items()},
              'hp_saturation_limitation': 'After100HP is depleted, HP alone cannot expose repeated zero-damage hits; combine unique delivered ActionId/immutable decision evidence and existing domain/network dedup tests. No artificial heal or respawn.',
              'maximum_pending_movement': maximum_pending, 'maximum_retained_actions': maximum_retained,
              'movement': movement, 'full_30_minute_qualified': (end-start) >= 1_800_000_000_000 and not injected and not errors,
              'scope': 'same-host real sockets, headless; no GUI or input-to-photon claim'}
    (output/'legal-action-evidence.json').write_text(json.dumps(result, indent=2)+'\n')
    return result
