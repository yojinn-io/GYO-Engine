"""Same-host command-stage evidence. Missing outcomes remain in the denominator."""
import argparse
from collections import Counter, deque
import csv
import json
import math
from pathlib import Path


def nearest_rank(values, fraction):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)] if ordered else math.inf


def finite(value):
    return value if math.isfinite(value) else None


def measurement_window(directory):
    timing = directory / 'timing.json'
    if timing.exists():
        data = json.loads(timing.read_text())
        return data['start_ns'], data['end_ns'], data
    with (directory / 'latency-plan.csv').open() as source:
        rows = list(csv.DictReader(source))
    return int(float(rows[0]['scheduled_host_seconds']) * 1e9), int(float(rows[-1]['end_host_seconds']) * 1e9), {}


def read_trace_events(path):
    """Read complete, lossless product diagnostics; reject out-of-contract bounds."""
    path = Path(path)
    count, ended = 0, False
    file_schema = None
    allowed = {'generated', 'sent', 'host_accepted', 'resolved', 'reset', 'snapshot_produced',
               'snapshot_received', 'presentation', 'runtime_gap', 'transport'}
    with path.open() as stream:
        for line_number, line in enumerate(stream, 1):
            try:
                event = json.loads(line)
                if ended:
                    raise ValueError('data after trace_end')
                schema = event.get('schema_version', 1)
                if not isinstance(schema, int) or isinstance(schema, bool) or schema not in (1, 2):
                    raise ValueError('unsupported diagnostic schema')
                if file_schema is not None and schema != file_schema:
                    raise ValueError('mixed diagnostic schemas')
                file_schema = schema
                if event['kind'] == 'trace_end':
                    ended = True
                    if event['events'] != count or event['dropped']:
                        raise ValueError('diagnostic count mismatch or loss')
                    continue
                if event['kind'] not in allowed:
                    raise ValueError('unknown event kind')
                count += 1
                timestamp = event['time_ns']
                if not isinstance(timestamp, int) or isinstance(timestamp, bool) or timestamp <= 0:
                    raise ValueError('invalid monotonic timestamp')
                started = event['started_ns'] if schema == 2 else event.get('started_ns', 0)
                if not isinstance(started, int) or isinstance(started, bool) or started < 0:
                    raise ValueError('invalid send-call start')
                if event['kind'] == 'sent':
                    if schema == 1 and started == 0:
                        started = timestamp  # Explicit legacy single-timestamp schema.
                    if not 0 < started <= timestamp:
                        raise ValueError('invalid successful send-call interval')
                elif started != 0:
                    raise ValueError('send-call interval on non-Sent event')
                event['started_ns'] = started
                event['schema_version'] = schema
                if event['source'] not in ('none', 'actual', 'held', 'neutral'):
                    raise ValueError('invalid execution source')
                if event['kind'] == 'resolved' and event['source'] == 'none':
                    raise ValueError('resolved without source')
                for field in ('frame_seconds', 'dropped_seconds', 'age_seconds'):
                    if not math.isfinite(event[field]) or event[field] < 0:
                        raise ValueError('invalid diagnostic duration')
                for field in ('player_id', 'epoch', 'sequence', 'authority_tick', 'queued', 'pending', 'count'):
                    if not isinstance(event[field], int) or isinstance(event[field], bool) or event[field] < 0:
                        raise ValueError('invalid diagnostic integer')
                if event['queued'] > 32 or event['pending'] > 12:
                    raise ValueError('command window exceeds queued <= 32 or pending <= 12')
                if event['kind'] in ('generated', 'sent', 'host_accepted', 'resolved') and not all(
                        event[key] > 0 for key in ('player_id', 'epoch', 'sequence')):
                    raise ValueError('invalid command identity')
                yield event
            except (ValueError, TypeError, KeyError) as exc:
                raise ValueError(f'{path}:{line_number}: corrupt diagnostic record: {exc}') from exc
    if not ended:
        raise ValueError(f'{path}: missing trace_end (unclean shutdown/truncated evidence)')


def recovery_actual_intervals(resolutions, generated, player_ids, release_ns):
    """Find uninterrupted actual execution, including every intervening authority step."""
    result = {player: [] for player in player_ids}
    for player in player_ids:
        history = deque(maxlen=30)
        previous, active_epoch, interval = None, None, None
        for event in sorted((e for e in resolutions if e['player_id'] == player), key=lambda e: e['time_ns']):
            epoch, timestamp = event['epoch'], event['time_ns']
            key = (player, epoch, event['sequence'])
            consecutive = previous is not None and epoch == active_epoch and (
                event['sequence'] == previous['sequence'] + 1 and
                event['authority_tick'] == previous['authority_tick'] + 1 and
                0 < timestamp - previous['time_ns'] < 100_000_000)
            # Epochs are authority-owned and monotonic. A late older epoch cannot
            # manufacture another valid interval after a newer epoch was observed.
            stale_epoch = active_epoch is not None and epoch < active_epoch
            if epoch != active_epoch:
                history.clear()
            if epoch != active_epoch or not consecutive:
                interval = None
            active_epoch = max(active_epoch or epoch, epoch)
            history.append(event['queued'])
            queue_sum = sum(history)
            valid = not stale_epoch and timestamp >= release_ns and event['source'] == 'actual' and (
                release_ns <= generated.get(key, 0) <= timestamp) and queue_sum < 105
            if valid:
                if interval is None:
                    interval = {'start_ns': timestamp, 'end_ns': timestamp, 'epoch': epoch,
                                'commands': 1, 'queue_sum_max': queue_sum}
                    result[player].append(interval)
                else:
                    interval['end_ns'] = timestamp
                    interval['commands'] += 1
                    interval['queue_sum_max'] = max(interval['queue_sum_max'], queue_sum)
            else:
                interval = None
            previous = event
    return result


def analyze_commands(directory, *, enforce=True):
    directory = Path(directory)
    start, end, timing = measurement_window(directory)
    generated, all_generated, sent, sent_started, accepted, resolved = {}, {}, {}, {}, {}, {}
    sources, event_counts = Counter(), Counter()
    errors, trace_files, gaps, resets = [], [], [], []
    queue_history, queue_max, queue_tail = {}, 0, {}
    resolutions = []
    interference = []
    seeded = Counter()
    transport_max_age, transport_count = 0, 0
    receipt_gaps = []
    for path in sorted(directory.glob('*commands.jsonl')):
        ended = False
        count = 0
        schema = None
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
                    if timing.get("release_ns", 0):
                        resolutions.append(event)
                    if measured:
                        queue_max = max(queue_max, sum(history))
                elif kind == 'snapshot_received':
                    prior=last_receipt.get(event['player_id'])
                    if prior and timestamp-prior>=100000000:
                        interference.append({'kind':'snapshot_receipt_gap', 'player_id':event['player_id'],
                            'start_ns':prior, 'end_ns':timestamp})
                        if measured:
                            receipt_gaps.append({'file':path.name,'player_id':event['player_id'],
                                'time_ns':timestamp,'seconds':(timestamp-prior)/1e9})
                    last_receipt[event['player_id']]=timestamp
                elif kind == 'runtime_gap':
                    if measured and (event['frame_seconds'] >= .1 or event['dropped_seconds'] > 0):
                        gaps.append(event)
                    # A reset's short fresh-seed clamp is a consequence, not
                    # evidence of the preceding interference that caused it.
                    if event['frame_seconds'] > .05:
                        interference.append({'kind':'runtime_gap', 'player_id':event['player_id'],
                            'start_ns':timestamp-round(event['frame_seconds']*1e9), 'end_ns':timestamp})
                elif kind == 'reset' and measured:
                    resets.append(event)
                elif kind == 'transport':
                    transport_max_age = max(transport_max_age, event['age_seconds'])
                    transport_count += event['count']
                    if event['age_seconds'] >= .1:
                        interference.append({'kind':'transport', 'player_id':event['player_id'],
                            'start_ns':timestamp-round(event['age_seconds']*1e9), 'end_ns':timestamp})
            ended = True
        except ValueError as exc:
            errors.append(str(exc))
        trace_files.append({'name': path.name, 'events': count, 'complete': ended, 'schema_version':schema})
    if not trace_files or not any(p['name'].startswith('match') for p in trace_files) or not any(
            p['name'] in ('clients-commands.jsonl','create-commands.jsonl','join-commands.jsonl') for p in trace_files):
        errors.append('Missing Match or Client trace')
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
    per_player = Counter(key[0] for key in generated)
    expected_players = timing.get('player_ids', sorted(per_player))
    if len(expected_players) != 2 or set(expected_players) != set(per_player):
        errors.append('Expected both players to generate commands across the measurement window')
    expected_count = (end-start) * 60 / 1e9
    production_ok = all(abs(per_player[p] - expected_count) <= 2 for p in expected_players)
    if not timing.get('injected') and not gaps and not timing.get('gaps_100ms') and not production_ok:
        errors.append('Clean-run command production differs from 60 Hz by more than two boundary steps per player')
    recovery = None
    release = timing.get('release_ns',0)
    if release:
        resumes = {}
        for key, generated_at in generated.items():
            outcome = resolved.get(key)
            if generated_at >= release and outcome and outcome[1] == 'actual':
                resumes[key[0]] = min(resumes.get(key[0],outcome[0]),outcome[0])
        recovery_actual = len(resumes)==2 and max(resumes.values()) <= release + 1500000000
        recovery = {'post_release_first_actual_ns':resumes,'actual_within_1_5s':recovery_actual}
        if not recovery_actual:
            errors.append('Both players must execute a newly generated real command within 1.5 s of recovery')
        intervals = recovery_actual_intervals(resolutions, generated, expected_players, release)
        recovery['actual_intervals'] = intervals
        for player in expected_players:
            stabilized = any(interval['end_ns']-interval['start_ns'] >= 250_000_000 and
                             interval['start_ns'] <= release+1_500_000_000
                             for interval in intervals[player])
            if not stabilized:
                errors.append(f'Player {player}: current-epoch actual execution and backlog did not stabilize for 250 ms after recovery by 1.5 s')
        if not timing.get('recovery_client_passed'):
            errors.append('Client prediction/presentation recovery did not pass')
    send95, actual50, actual95 = nearest_rank(send_ms, .95), nearest_rank(actual_ms, .5), nearest_rank(actual_ms, .95)
    ratio = actual/n if n else 0
    if not n:
        errors.append('No generated measurement commands')
    if any(value < 0 for value in send_ms + actual_ms + host_ms + accepted_execution_ms):
        errors.append('Negative command stage time; clocks or event identity invalid')
    if enforce:
        if send95 > 22: errors.append('Generated to first send P95 exceeds 22 ms')
        if actual50 > 50 or actual95 > 66.7: errors.append('Generated to actual execution exceeds 50/66.7 ms')
        if ratio < .99: errors.append('Original command actual execution rate below 99%')
    remote_stale_frames = 0
    frames_file=directory/'frames.csv'
    if frames_file.exists():
        with frames_file.open() as frames:
            for frame in csv.DictReader(frames):
                if max(float(frame['remote_age_a']),float(frame['remote_age_b']))>=.1:
                    remote_stale_frames+=1
    disturbed = bool(remote_stale_frames or gaps or receipt_gaps or timing.get('gaps_100ms') or timing.get('injected') or
        timing.get('history_overflows') or transport_max_age >= .1)
    reset_causes = []
    for reset in resets:
        causes = [item for item in interference if item['player_id'] in (0, reset['player_id']) and
                  item['start_ns'] < reset['time_ns'] <= item['end_ns']+1_500_000_000]
        reset_causes.append({'player_id':reset['player_id'], 'epoch':reset['epoch'],
                             'time_ns':reset['time_ns'], 'preceding_interference':causes})
        if not causes:
            errors.append(f'Unexplained epoch reset for player {reset["player_id"]}: no preceding substantive interference')
    if not disturbed and queue_max >= 105:
        errors.append('Clean-run 30-tick queued-command sum reaches the backlog reset threshold')
    result = {'passed': not errors, 'latency_thresholds_enforced':enforce, 'errors': errors, 'window_ns': [start, end],
        'commands': n, 'commands_per_player':dict(per_player), 'expected_per_player':expected_count,
        'production_60hz_passed':production_ok, 'seeded_per_player':dict(seeded), 'recovery':recovery, 'actual': actual, 'substituted': substitute, 'unresolved': missing,
        'actual_fraction': ratio, 'first_send_p95_ms': finite(send95),
        'first_send_timestamp':'successful send-call end; conservative latency upper bound (schema 1: legacy point timestamp)',
        'actual_p50_ms': finite(actual50), 'actual_p95_ms': finite(actual95),
        'host_accept_p50_ms': finite(nearest_rank(host_ms,.5)),
        'host_accept_p95_ms': finite(nearest_rank(host_ms,.95)),
        'host_to_execution_p50_ms': finite(nearest_rank(accepted_execution_ms,.5)),
        'execution_sources': dict(sources), 'queue_30_tick_sum_max': queue_max,
        'queue_30_tick_sum_tail': {str(k):v for k,v in queue_tail.items()},
        'resets': len(resets), 'reset_causality':reset_causes, 'disturbed': disturbed, 'runtime_gap_events': gaps, 'snapshot_receipt_gaps':receipt_gaps, 'remote_stale_frames':remote_stale_frames,
        'transport_event_counts':transport_count, 'transport_max_age_seconds':transport_max_age,
        'trace_files': trace_files, 'trace_events': dict(event_counts),
        'quantile': 'nearest-rank; missing/substituted actual executions are positive infinity (JSON null)',
        'clock_scope':'same-host steady clock; not input-to-photon'}
    (directory/'command-evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    return result

if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path)
    parser.add_argument('--report-only',action='store_true');args=parser.parse_args()
    result=analyze_commands(args.directory,enforce=not args.report_only)
    print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 1)
