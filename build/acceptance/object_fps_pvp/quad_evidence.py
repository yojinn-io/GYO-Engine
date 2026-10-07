"""Room-capacity (N-Client) acceptance evidence; the two-Client analyzers are unchanged.

Command stage: ``command_metrics`` is an N-player form of
``command_evidence.analyze_commands`` for runs without an injected fault. It reuses
that module's trace reader and life-respawn helpers; only the aggregation over
players is its own. ``test_quad_evidence.py`` cross-validates it value by value
against ``analyze_commands`` on two-player corpora (CTest), and ``--cross-validate``
does the same on recorded two-Client run directories. The duplicated responsibility
is a known Code Smell to converge in v7 (batch 16 Architecture Delta).

Functional: room capacity, the refused extra join, one decision per action, the
allowed rejections, respawns at free spawns, identical snapshots across Clients and
the IPC transport statistics of the Match trace.
"""
import argparse
from collections import Counter, defaultdict, deque
import csv
import json
import math
from pathlib import Path
import shutil
import tempfile

from acceptance_capacity import MAX_PLAYERS
from command_evidence import (FRAME_INTERVAL_SOURCE, LIFE_RESPAWN_PRODUCTION_STEPS, START_PHASE_FRAME_CUT_SECONDS,
                              TICK_SECONDS, analyze_commands, finite, frame_interval_statistics,
                              lifecycle_cancellation_matches, match_life_seed_clamps, measurement_window,
                              nearest_rank, read_trace_events, trace_role)

# Racing outcomes of a bot's shot: its shooter or its life died before the authority resolved it.
REJECTION_NAMES = ('None', 'InvalidReference', 'Expired', 'Cooldown', 'Dead', 'StaleLife', 'InvalidLife',
                   'Reloading', 'EmptyMagazine', 'MagazineFull')
ALLOWED_REJECTIONS = frozenset(('Dead', 'StaleLife'))
LIFE_DEAD = 1
SPAWN_TOLERANCE = 1e-4
# Values compared with analyze_commands; every other key is either identical by construction or quad-only.
CROSS_VALIDATED_KEYS = (
    'commands', 'commands_per_player', 'expected_per_player', 'production_60hz_passed', 'production_allowance_steps',
    'seeded_per_player', 'actual', 'substituted', 'unresolved', 'actual_fraction', 'first_send_p95_ms', 'actual_p50_ms',
    'actual_p95_ms', 'host_accept_p50_ms', 'host_accept_p95_ms', 'host_to_execution_p50_ms', 'execution_sources',
    'queue_30_tick_sum_max', 'queue_30_tick_sum_tail', 'resets', 'unexpected_resets', 'life_respawn_resets',
    'life_seed_clamps', 'lifecycle_cancelled', 'reset_reasons', 'reset_causality', 'disturbed', 'runtime_gap_events',
    'snapshot_receipt_gaps', 'remote_stale_frames', 'transport_event_counts', 'transport_max_age_seconds',
    'trace_files', 'trace_events', 'presentation_frame_intervals')
# analyze_commands names two players; every other error text is shared.
TWO_PLAYER_ERROR = 'Expected both players to generate commands across the measurement window'
EVERY_PLAYER_ERROR = 'Expected every player to generate commands across the measurement window'


def command_metrics(directory, *, enforce=True):
    """analyze_commands for N players and no injected recovery; returns its result shape."""
    directory = Path(directory)
    start, end, timing = measurement_window(directory)
    if timing.get('release_ns', 0):
        raise ValueError('quad command evidence has no fault recovery; use the two-Client analyzers')
    generated, all_generated, sent, sent_started, accepted, resolved = {}, {}, {}, {}, {}, {}
    sources, event_counts = Counter(), Counter()
    errors, trace_files, gaps, resets = [], [], [], []
    queue_history, queue_max, queue_tail = {}, 0, {}
    interference = []
    seeded = Counter()
    transport_max_age, transport_count = 0, 0
    receipt_gaps = []
    presented = {}
    cancelled, all_life_resets = {}, []
    for path in sorted(directory.glob('*commands.jsonl')):
        ended, count, schema = False, 0, None
        last_receipt = {}
        try:
            for event in read_trace_events(path):
                count += 1
                kind, timestamp = event['kind'], event['time_ns']
                schema = event['schema_version']
                event_counts[kind] += 1
                key = (event['player_id'], event['epoch'], event['sequence'])
                measured = start <= timestamp < end
                if kind == 'generated':
                    if key in all_generated:
                        errors.append(f'Duplicate generated identity {key}')
                    all_generated[key] = timestamp
                if kind == 'generated' and measured and event['seeded_neutral']:
                    seeded[event['player_id']] += 1
                if kind == 'generated' and measured and not event['seeded_neutral']:
                    generated[key] = timestamp
                elif kind == 'sent':
                    if key not in sent or timestamp < sent[key]:
                        sent[key] = timestamp
                        sent_started[key] = event['started_ns']
                elif kind == 'host_accepted':
                    accepted[key] = min(accepted.get(key, timestamp), timestamp)
                elif kind == 'resolved':
                    if key in resolved:
                        errors.append(f'Duplicate resolution {key}')
                    resolved[key] = (timestamp, event['source'])
                    if measured:
                        sources[event['source']] += 1
                    identity = key[:2]
                    history = queue_history.setdefault(identity, deque(maxlen=30))
                    history.append(event['queued'])
                    queue_tail[identity] = sum(history)
                    if measured:
                        queue_max = max(queue_max, sum(history))
                elif kind == 'snapshot_received':
                    prior = last_receipt.get(event['player_id'])
                    if prior and timestamp - prior >= 100000000:
                        interference.append({'kind': 'snapshot_receipt_gap', 'player_id': event['player_id'],
                                             'start_ns': prior, 'end_ns': timestamp})
                        if measured:
                            receipt_gaps.append({'file': path.name, 'player_id': event['player_id'],
                                                 'time_ns': timestamp, 'seconds': (timestamp - prior) / 1e9})
                    last_receipt[event['player_id']] = timestamp
                elif kind == 'runtime_gap':
                    if measured and (event['frame_seconds'] >= .1 or event['dropped_seconds'] > 0):
                        gaps.append(event)
                    if event['frame_seconds'] > .05:
                        interference.append({'kind': 'runtime_gap', 'player_id': event['player_id'],
                                             'start_ns': timestamp - round(event['frame_seconds'] * 1e9),
                                             'end_ns': timestamp})
                elif kind == 'reset':
                    if measured:
                        resets.append(event)
                    if event.get('reset_reason') == 'life_respawn':
                        all_life_resets.append(event)
                elif kind == 'lifecycle_cancelled':
                    cancelled[key] = event
                elif kind == 'presentation' and measured:
                    presented.setdefault(trace_role(path), []).append(event['frame_seconds'])
                elif kind == 'transport':
                    transport_max_age = max(transport_max_age, event['age_seconds'])
                    transport_count += event['count']
                    if event['age_seconds'] >= .1:
                        interference.append({'kind': 'transport', 'player_id': event['player_id'],
                                             'start_ns': timestamp - round(event['age_seconds'] * 1e9),
                                             'end_ns': timestamp})
            ended = True
        except ValueError as exc:
            errors.append(str(exc))
        trace_files.append({'name': path.name, 'events': count, 'complete': ended, 'schema_version': schema})
    # Any non-Match trace is a Client trace: the bots' shared file and a GUI participant's own.
    if not any(p['name'].startswith('match') for p in trace_files) or not any(
            not p['name'].startswith('match') for p in trace_files):
        errors.append('Missing Match or Client trace')

    def reset_reason(reset):
        reason = reset.get('reset_reason')
        return reason if isinstance(reason, str) and reason else 'unrecorded'
    life_resets = [reset for reset in resets if reset_reason(reset) == 'life_respawn']
    for reset in life_resets:
        if not isinstance(reset.get('life_generation'), int) or reset['life_generation'] < 2:
            errors.append(f'LifeRespawn reset for player {reset["player_id"]} lacks a new life generation')
    life_seed_clamps, gaps = match_life_seed_clamps(gaps, all_life_resets)
    per_player = Counter(key[0] for key in generated)
    lifecycle_cancelled = 0
    for key, event in cancelled.items():
        if not lifecycle_cancellation_matches(event, all_life_resets):
            errors.append(f'Lifecycle cancellation {key} lacks a matching LifeRespawn reset')
        elif key in generated and key not in resolved:
            del generated[key]
            lifecycle_cancelled += 1
    send_ms, actual_ms, host_ms, accepted_execution_ms = [], [], [], []
    actual, substitute, missing = 0, 0, 0
    for key, (timestamp, source) in resolved.items():
        if source != 'actual':
            continue
        if key not in all_generated:
            errors.append(f'Actual command {key} missing Generated evidence')
        elif key not in sent or key not in accepted:
            errors.append(f'Actual command {key} missing Sent or HostAccepted evidence')
        elif not all_generated[key] <= sent_started[key] <= accepted[key] <= timestamp:
            errors.append(f'Actual command {key} violates Generated <= Sent start <= HostAccepted <= Resolved')
    for key, timestamp in generated.items():
        send_ms.append((sent[key] - timestamp) / 1e6 if key in sent else math.inf)
        if key in accepted:
            host_ms.append((accepted[key] - timestamp) / 1e6)
        outcome = resolved.get(key)
        if outcome and outcome[1] == 'actual':
            actual += 1
            actual_ms.append((outcome[0] - timestamp) / 1e6)
            if key in accepted:
                accepted_execution_ms.append((outcome[0] - accepted[key]) / 1e6)
        else:
            actual_ms.append(math.inf)
            if outcome:
                substitute += 1
            else:
                missing += 1
    n = len(generated)
    expected_players = timing.get('player_ids', sorted(per_player))
    if set(expected_players) != set(per_player):
        errors.append(EVERY_PLAYER_ERROR)
    expected_count = (end - start) * 60 / 1e9
    respawns = Counter(reset['player_id'] for reset in life_resets)
    production_allowance = {p: 2 + LIFE_RESPAWN_PRODUCTION_STEPS * respawns[p] for p in expected_players}
    production_ok = all(abs(per_player[p] - expected_count) <= production_allowance[p] for p in expected_players)
    if not timing.get('injected') and not gaps and not timing.get('gaps_100ms') and not production_ok:
        errors.append('Clean-run command production differs from 60 Hz by more than two boundary steps per player')
    send95, actual50, actual95 = nearest_rank(send_ms, .95), nearest_rank(actual_ms, .5), nearest_rank(actual_ms, .95)
    ratio = actual / n if n else 0
    if not n:
        errors.append('No generated measurement commands')
    if any(value < 0 for value in send_ms + actual_ms + host_ms + accepted_execution_ms):
        errors.append('Negative command stage time; clocks or event identity invalid')
    if enforce:
        if send95 > 22: errors.append('Generated to first send P95 exceeds 22 ms')
        if actual50 > 50 or actual95 > 66.7: errors.append('Generated to actual execution exceeds 50/66.7 ms')
        if ratio < .99: errors.append('Original command actual execution rate below 99%')
    # Every remote-age column of a two-Client frames.csv (quad runs write none).
    remote_stale_frames = 0
    frames_file = directory / 'frames.csv'
    if frames_file.exists():
        with frames_file.open() as frames:
            for frame in csv.DictReader(frames):
                ages = [float(value) for column, value in frame.items() if column.startswith('remote_age_')]
                if ages and max(ages) >= .1:
                    remote_stale_frames += 1
    disturbed = bool(remote_stale_frames or gaps or receipt_gaps or timing.get('gaps_100ms') or timing.get('injected') or
                     timing.get('history_overflows') or transport_max_age >= .1)
    reset_causes = []
    for reset in resets:
        causes = [item for item in interference if item['player_id'] in (0, reset['player_id']) and
                  item['start_ns'] < reset['time_ns'] <= item['end_ns'] + 1_500_000_000]
        reason = reset_reason(reset)
        reset_causes.append({'player_id': reset['player_id'], 'epoch': reset['epoch'], 'reason': reason,
                             'time_ns': reset['time_ns'], 'preceding_interference': causes})
        if not causes and reason != 'life_respawn':
            errors.append(f'Unexplained epoch reset for player {reset["player_id"]} (reason {reason}): '
                          'no preceding substantive interference')
    if not disturbed and queue_max >= 105:
        errors.append('Clean-run 30-tick queued-command sum reaches the backlog reset threshold')
    return {'passed': not errors, 'latency_thresholds_enforced': enforce, 'errors': errors, 'window_ns': [start, end],
            'players': len(expected_players), 'commands': n, 'commands_per_player': dict(per_player),
            'expected_per_player': expected_count, 'production_60hz_passed': production_ok,
            'production_allowance_steps': {str(k): v for k, v in production_allowance.items()},
            'seeded_per_player': dict(seeded), 'recovery': None, 'actual': actual, 'substituted': substitute,
            'unresolved': missing, 'actual_fraction': ratio, 'first_send_p95_ms': finite(send95),
            'actual_p50_ms': finite(actual50), 'actual_p95_ms': finite(actual95),
            'host_accept_p50_ms': finite(nearest_rank(host_ms, .5)),
            'host_accept_p95_ms': finite(nearest_rank(host_ms, .95)),
            'host_to_execution_p50_ms': finite(nearest_rank(accepted_execution_ms, .5)),
            'execution_sources': dict(sources), 'queue_30_tick_sum_max': queue_max,
            'queue_30_tick_sum_tail': {str(k): v for k, v in queue_tail.items()},
            'resets': len(resets), 'unexpected_resets': len(resets) - len(life_resets),
            'life_respawn_resets': [{key: reset[key] for key in ('player_id', 'life_generation', 'epoch', 'authority_tick', 'time_ns')}
                                    for reset in all_life_resets],
            'life_seed_clamps': life_seed_clamps, 'lifecycle_cancelled': lifecycle_cancelled,
            'reset_reasons': dict(Counter(reset_reason(reset) for reset in resets)),
            'reset_causality': reset_causes, 'disturbed': disturbed, 'runtime_gap_events': gaps,
            'snapshot_receipt_gaps': receipt_gaps, 'remote_stale_frames': remote_stale_frames,
            'transport_event_counts': transport_count, 'transport_max_age_seconds': transport_max_age,
            'trace_files': trace_files, 'trace_events': dict(event_counts),
            'presentation_frame_intervals': {'source': FRAME_INTERVAL_SOURCE, 'tick_seconds': TICK_SECONDS,
                                             'cut_seconds': START_PHASE_FRAME_CUT_SECONDS,
                                             'roles': {role: frame_interval_statistics(values)
                                                       for role, values in sorted(presented.items())}}}


# What both command analyzers read; a recorded run is copied so neither writes into its evidence.
COMMAND_INPUTS = ('*commands.jsonl', 'timing.json', 'latency-plan.csv', 'frames.csv')


def cross_validate_recorded(directory):
    """cross_validate on a copy; a gameplay run without timing.json gets gameplay_evidence's fixed window."""
    directory = Path(directory)
    with tempfile.TemporaryDirectory(prefix='pvp-quad-cross-') as temporary:
        for pattern in COMMAND_INPUTS:
            for path in directory.glob(pattern):
                shutil.copy2(path, temporary)
        copy = Path(temporary)
        client = directory / 'action-client.json'
        if not (copy / 'timing.json').exists() and not (copy / 'latency-plan.csv').exists() and client.exists():
            client = json.loads(client.read_text())
            (copy / 'timing.json').write_text(json.dumps({
                'start_ns': client['start_ns'] + 1_000_000_000, 'end_ns': client['start_ns'] + 15_000_000_000,
                'player_ids': client['player_ids'], 'fps': client.get('fps', 60), 'injected': False}))
        return cross_validate(copy)


def cross_validate(directory):
    """Differences between command_metrics and analyze_commands on one two-player run directory."""
    reference = analyze_commands(directory, enforce=False)
    quad = command_metrics(directory, enforce=False)
    differences = {key: {'command_evidence': reference.get(key), 'quad_evidence': quad.get(key)}
                   for key in CROSS_VALIDATED_KEYS if reference.get(key) != quad.get(key)}
    expected_errors = [EVERY_PLAYER_ERROR if error == TWO_PLAYER_ERROR else error for error in reference['errors']]
    if expected_errors != quad['errors']:
        differences['errors'] = {'command_evidence': reference['errors'], 'quad_evidence': quad['errors']}
    return differences


def transport_statistics(match_trace):
    """IPC snapshot writes from the Match trace: coalesced replacements and the age of what waited."""
    events = [event for event in read_trace_events(match_trace) if event['kind'] == 'transport']
    ages = sorted(event['age_seconds'] for event in events if event['age_seconds'] > 0)
    return {'events': len(events), 'coalesced_snapshots': max((event['count'] for event in events), default=0),
            'aged_events': len(ages), 'age_p95_seconds': ages[max(0, math.ceil(len(ages) * .95) - 1)] if ages else 0,
            'age_max_seconds': ages[-1] if ages else 0,
            'source': 'Match trace transport events: count is the cumulative coalesced-snapshot counter, '
                      'age the wait of a replaced or partially written snapshot'}


def _respawns(snapshots, spawns, radius):
    """Every observed new life: (player, life, tick, at a spawn, clear of living players)."""
    by_tick = {}
    for snapshot in snapshots:
        by_tick.setdefault(snapshot['tick'], snapshot)
    results, seen = [], set()
    for tick in sorted(by_tick):
        for player in by_tick[tick]['players']:
            life = (player['player_id'], player['life_generation'])
            if player['life_generation'] < 2 or life in seen:
                continue
            seen.add(life)
            # Only the snapshot of the respawn tick itself still shows the untouched spawn position.
            exact = player['life_state'] != LIFE_DEAD and player['life_state_tick'] == tick
            position = player['position']
            at_spawn = exact and any(all(abs(a - b) <= SPAWN_TOLERANCE for a, b in zip(position, spawn)) for spawn in spawns)
            clear = exact and all(other['player_id'] == player['player_id'] or other['life_state'] == LIFE_DEAD or
                                  math.hypot(other['position'][0] - position[0], other['position'][2] - position[2]) >= 2 * radius
                                  for other in by_tick[tick]['players'])
            results.append({'player_id': life[0], 'life_generation': life[1], 'first_seen_tick': tick,
                            'respawn_tick_observed': exact, 'at_spawn': at_spawn, 'clear_of_living': clear})
    return results


def analyze_quad(directory, *, judged=True):
    directory = Path(directory)
    errors = []

    def check(condition, message):
        if not condition:
            errors.append(message)
    client = json.loads((directory / 'quad-client.json').read_text())
    gui_path = directory / 'gui-quad.json'
    gui = json.loads(gui_path.read_text()) if gui_path.exists() else None
    check(client['capacity'] == MAX_PLAYERS, f'Probe capacity {client["capacity"]} differs from {MAX_PLAYERS}')
    ids = list(client['player_ids']) + ([gui['player_id']] if gui else [])
    check(len(ids) == len(set(ids)), 'Player IDs repeat')
    expected = client['expect_players']
    check(not expected or len(ids) == expected, f'{len(ids)} participants for an expected room of {expected}')
    for entry in client['clients']:
        check(not expected or entry['maximum_players_seen'] == expected,
              f'Player {entry["player_id"]} saw at most {entry["maximum_players_seen"]} players')
    fifth = client.get('fifth')
    if fifth is not None:
        check(fifth['error'] == 'room_full' and not fifth['joined'],
              f'The extra join was not refused as room_full: {fifth}')
        check(fifth['listed_players'] == fifth['listed_capacity'] == MAX_PLAYERS,
              f'The room list did not show a full room: {fifth}')
    rejections = Counter()
    for entry in client['clients']:
        submitted = {action['action_id'] for action in entry['submitted']}
        decided = [decision['action_id'] for decision in entry['decisions']]
        check(len(decided) == len(set(decided)) and set(decided) == submitted,
              f'Player {entry["player_id"]}: decisions do not match submissions one to one')
        check(entry['retained'] == 0, f'Player {entry["player_id"]} retained actions after the run')
        for decision in entry['decisions']:
            if not decision['accepted']:
                rejections[REJECTION_NAMES[decision['rejection']]] += 1
    unexpected = {name: count for name, count in rejections.items() if name not in ALLOWED_REJECTIONS}
    check(not unexpected, f'Rejections outside {sorted(ALLOWED_REJECTIONS)}: {unexpected}')
    snapshots = [json.loads(line) for line in (directory / 'quad-snapshots.jsonl').read_text().splitlines()]
    by_tick, mismatched = defaultdict(list), []
    for snapshot in snapshots:
        by_tick[snapshot['tick']].append(snapshot)
    for tick, copies in by_tick.items():
        first = {key: copies[0][key] for key in ('players', 'combat')}
        if any({key: copy[key] for key in ('players', 'combat')} != first for copy in copies[1:]):
            mismatched.append(tick)
    shared = sum(len({copy['client'] for copy in copies}) > 1 for copies in by_tick.values())
    check(shared > 0, 'No snapshot tick was observed by two Clients')
    check(not mismatched, f'Clients observed different payloads at ticks {sorted(mismatched)[:10]}')
    start, end = client['start_ns'] + 1_000_000_000, client['start_ns'] + round((client['duration'] - 1) * 1e9)
    if expected:
        short = sorted({snapshot['tick'] for snapshot in snapshots if len(snapshot['players']) != expected})
        check(not short, f'Snapshots without the full room at ticks {short[:10]}')
    respawns = _respawns(snapshots, client['arena']['spawns'], client['arena']['radius'])
    verified = [r for r in respawns if r['respawn_tick_observed']]
    check(respawns, 'No player died and respawned during the run')
    check(all(r['at_spawn'] and r['clear_of_living'] for r in verified),
          f'A respawn was not at a free spawn: {[r for r in verified if not (r["at_spawn"] and r["clear_of_living"])]}')
    check(verified, 'No respawn tick itself was observed')
    frames = [seconds for time_ns, seconds in zip(client['frame_time_ns'], client['frame_seconds']) if start <= time_ns < end]
    (directory / 'timing.json').write_text(json.dumps({
        'start_ns': start, 'end_ns': end, 'player_ids': ids, 'fps': client['fps'], 'injected': False,
        'gaps_100ms': sum(seconds >= .1 for seconds in frames),
        'measurement_policy': 'fixed start+1s/end-1s warmup/tail; no slow-frame filtering'}, indent=2) + '\n')
    commands = command_metrics(directory, enforce=judged)
    check(commands['passed'], 'Command stage: ' + str(commands['errors']))
    gui_summary = None
    if gui:
        times = gui['frame_time_ns']
        achieved = (len(times) - 1) / ((times[-1] - times[0]) / 1e9) if len(times) > 1 and times[-1] > times[0] else 0
        gui_summary = {'player_id': gui['player_id'], 'frames': gui['frames'], 'achieved_fps': achieved,
                       'presented_frames': gui['presented_frames'],
                       'skipped_presentation_frames': gui['skipped_presentation_frames'],
                       'players': [gui['minimum_players'], gui['maximum_players']], 'deaths': gui['deaths'],
                       'life_generations': gui['life_generations']}
        check(gui['minimum_players'] == gui['maximum_players'] == MAX_PLAYERS,
              f'The GUI Client did not keep a full room: {gui_summary["players"]}')
        check(gui['presented_frames'] > 0, 'The GUI Client presented no frame')
    result = {'passed': not errors, 'judged': judged, 'errors': errors, 'capacity': MAX_PLAYERS,
              'player_ids': ids, 'fifth': fifth, 'decisions': sum(len(entry['decisions']) for entry in client['clients']),
              'rejections': dict(rejections), 'snapshot_ticks': len(by_tick), 'shared_snapshot_ticks': shared,
              'respawns': respawns, 'deaths': len(respawns), 'transport': transport_statistics(directory / 'match-commands.jsonl'),
              'timer': client.get('timer'), 'gui': gui_summary, 'command_evidence': commands}
    (directory / 'quad-evidence.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('directory', type=Path, nargs='+')
    parser.add_argument('--record-only', action='store_true', help='report without the latency thresholds (clean-30)')
    parser.add_argument('--cross-validate', action='store_true',
                        help='compare the command stage with command_evidence on two-Client run directories')
    args = parser.parse_args()
    if args.cross_validate:
        differences = {str(directory): cross_validate_recorded(directory) for directory in args.directory}
        print(json.dumps(differences, indent=2))
        return 0 if not any(differences.values()) else 1
    results = [analyze_quad(directory, judged=not args.record_only) for directory in args.directory]
    print(json.dumps([{'directory': str(d), 'passed': r['passed'], 'errors': r['errors']}
                      for d, r in zip(args.directory, results)], indent=2))
    return 0 if all(r['passed'] for r in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
