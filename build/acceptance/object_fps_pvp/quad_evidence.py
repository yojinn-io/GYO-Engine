"""Room-capacity (N-Client) acceptance evidence; the two-Client analyzers are unchanged.

Command stage: ``command_evidence.analyze_commands`` with ``players=None`` (every declared player; v7.1
analyzer-converge, D192, replaced this module's former N-player copy of it). ``room_shape`` keeps the keys and
order this module has always written.

Functional: room capacity, the refused extra join, one decision per action, the
allowed rejections, respawns at free spawns, identical snapshots across Clients and
the IPC transport statistics of the Match trace.
"""
import argparse
from collections import Counter, defaultdict
import json
import math
from pathlib import Path

from acceptance_capacity import MAX_PLAYERS
from command_evidence import analyze_commands, read_trace_events

# Racing outcomes of a bot's shot: its shooter or its life died before the authority resolved it.
REJECTION_NAMES = ('None', 'InvalidReference', 'Expired', 'Cooldown', 'Dead', 'StaleLife', 'InvalidLife',
                   'Reloading', 'EmptyMagazine', 'MagazineFull')
ALLOWED_REJECTIONS = frozenset(('Dead', 'StaleLife'))
LIFE_DEAD = 1
SPAWN_TOLERANCE = 1e-4
# The keys of the command stage this module writes, in its order; "players" is the room's declared size.
ROOM_KEYS = (
    'passed', 'latency_thresholds_enforced', 'errors', 'window_ns', 'players', 'commands', 'commands_per_player',
    'expected_per_player', 'production_60hz_passed', 'production_allowance_steps', 'seeded_per_player', 'recovery',
    'actual', 'substituted', 'unresolved', 'actual_fraction', 'first_send_p95_ms', 'actual_p50_ms', 'actual_p95_ms',
    'host_accept_p50_ms', 'host_accept_p95_ms', 'host_to_execution_p50_ms', 'execution_sources',
    'queue_30_tick_sum_max', 'queue_30_tick_sum_tail', 'resets', 'unexpected_resets', 'life_respawn_resets',
    'life_seed_clamps', 'lifecycle_cancelled', 'reset_reasons', 'reset_causality', 'disturbed', 'runtime_gap_events',
    'snapshot_receipt_gaps', 'remote_stale_frames', 'transport_event_counts', 'transport_max_age_seconds',
    'trace_files', 'trace_events', 'presentation_frame_intervals')


def room_shape(commands):
    """analyze_commands(players=None) in the keys and order this module writes. Only a projection: the production
    allowance has one entry per declared player, which is the room's size."""
    shaped = dict(commands, players=len(commands['production_allowance_steps']))
    return {key: shaped[key] for key in ROOM_KEYS}


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
    commands = room_shape(analyze_commands(directory, enforce=judged, players=None, write=False))
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
              'timer': client.get('timer'), 'timer_after': client.get('timer_after'), 'gui': gui_summary, 'command_evidence': commands}
    (directory / 'quad-evidence.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('directory', type=Path, nargs='+')
    parser.add_argument('--record-only', action='store_true', help='report without the latency thresholds (clean-30)')
    args = parser.parse_args()
    results = [analyze_quad(directory, judged=not args.record_only) for directory in args.directory]
    print(json.dumps([{'directory': str(d), 'passed': r['passed'], 'errors': r['errors']}
                      for d, r in zip(args.directory, results)], indent=2))
    return 0 if all(r['passed'] for r in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
