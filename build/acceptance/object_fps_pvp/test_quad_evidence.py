"""Room-capacity evidence: value-by-value cross-validation with command_evidence and the quad verdicts.

The two-player corpora come from test_command_evidence's own builders, so both analyzers read
exactly the traces that analyzer's tests already pin down.
"""
import copy
import csv
import json
import math
import unittest

import quad_evidence as quad
import test_command_evidence as reference
from acceptance_capacity import MAX_PLAYERS

START, SECOND, DURATION = reference.START, reference.SECOND, reference.DURATION
event = reference.event


class Corpora(unittest.TestCase):
    """Borrows the two-player trace builders without collecting their tests."""
    make_run = reference.CommandEvidenceTests.make_run
    write = staticmethod(reference.CommandEvidenceTests.write)
    life_respawn = staticmethod(reference.CommandEvidenceTests.life_respawn)
    with_send_intervals = staticmethod(reference.CommandEvidenceTests.with_send_intervals)

    def corpus(self, change=None, **options):
        directory, client, match, timing = self.make_run(**options)
        if change:
            change(directory, client, match, timing)
        self.write(directory, client, match, timing)
        return directory


def held_every_tenth(generated, resolved, epoch, index):
    return 'held' if index % 10 == 0 else 'actual'


def slow_tail(generated, resolved, epoch, index):
    return 'neutral' if index % 40 == 0 else 'actual'


def life_respawn(directory, client, match, timing):
    Corpora.life_respawn(match, client)


def life_respawn_without_new_life(directory, client, match, timing):
    Corpora.life_respawn(match, client, reset_life=1)


def life_respawn_uncancelled(directory, client, match, timing):
    Corpora.life_respawn(match, client, cancel=False)


def unexplained_reasoned_resets(directory, client, match, timing):
    match.append(event('reset', START + SECOND, player=2, epoch=2, sequence=0, reset_reason='starvation'))
    match.append(event('reset', START + 3 * SECOND, epoch=2, sequence=0))
    client.append(event('runtime_gap', START + 2 * SECOND, player=2, frame_seconds=1.5, dropped_seconds=1.4))
    client.append(event('runtime_gap', START + 3 * SECOND + 100_000_000, player=1, frame_seconds=1.5, dropped_seconds=1.4))


def presentation_role(directory, client, match, timing):
    presented, stamp = [], START + 5_000_000
    for seconds in [1 / 60] * 56 + [.019, .02, .03, .04]:
        stamp += int(seconds * SECOND)
        presented.append(event('presentation', stamp, frame_seconds=seconds))
    with (directory / 'create-commands.jsonl').open('w', encoding='utf-8') as output:
        for record in presented:
            output.write(json.dumps(record) + '\n')
        output.write(json.dumps({'kind': 'trace_end', 'events': len(presented), 'dropped': 0}) + '\n')


def receipt_gap(directory, client, match, timing):
    client[:] = [record for record in client if not (record['kind'] == 'snapshot_received' and record['player_id'] == 1
                 and START + SECOND < record['time_ns'] < START + SECOND + 180_000_000)]


def remote_age_spike(directory, client, match, timing):
    with (directory / 'frames.csv').open('w', newline='') as output:
        writer = csv.writer(output)
        writer.writerow(['time_ns', 'remote_age_a', 'remote_age_b'])
        writer.writerow([START + SECOND, .02, .03])
        writer.writerow([START + SECOND + 16_666_667, .101, .03])


def history_overflow(directory, client, match, timing):
    timing['history_overflows'] = 1


def transport(directory, client, match, timing):
    for n, age in enumerate((0, .004, .12, .02), 1):
        match.append(event('transport', START + n * SECOND // 2, player=0, sequence=0, epoch=0, count=n, age_seconds=age))


def send_intervals(directory, client, match, timing):
    Corpora.with_send_intervals(client, match)


def duplicate_generated(directory, client, match, timing):
    client.append(copy.deepcopy(next(record for record in client if record['kind'] == 'generated')))


def queue_backlog(directory, client, match, timing):
    for record in match:
        if record['kind'] == 'resolved':
            record['queued'] = 4


TWO_PLAYER_CORPORA = {
    'clean': {},
    'half-rate': {'rate': 30},
    'one-producer': {'players': (1,)},
    'held': {'source': held_every_tenth},
    'neutral': {'source': slow_tail},
    'unexplained-epoch': {'epoch_change': START + 2 * SECOND},
    'life-respawn': {'change': life_respawn},
    'life-respawn-without-new-life': {'change': life_respawn_without_new_life},
    'life-respawn-uncancelled': {'change': life_respawn_uncancelled},
    'reasoned-resets': {'change': unexplained_reasoned_resets},
    'presentation-role': {'change': presentation_role},
    'receipt-gap': {'change': receipt_gap},
    'remote-age-spike': {'change': remote_age_spike},
    'history-overflow': {'change': history_overflow},
    'transport': {'change': transport},
    'send-intervals': {'change': send_intervals},
    'duplicate-generated': {'change': duplicate_generated},
    'queue-backlog': {'change': queue_backlog},
}


class CrossValidationTests(Corpora):
    def test_every_two_player_corpus_matches_command_evidence_value_by_value(self):
        for name, options in TWO_PLAYER_CORPORA.items():
            with self.subTest(corpus=name):
                directory = self.corpus(**options)
                self.assertEqual(quad.cross_validate(directory), {})

    def test_the_corpora_cover_passing_and_failing_runs(self):
        outcomes = {name: reference.evidence.analyze_commands(self.corpus(**options), enforce=False)['passed']
                    for name, options in TWO_PLAYER_CORPORA.items()}
        self.assertTrue(outcomes['clean'])
        self.assertFalse(outcomes['half-rate'])
        self.assertIn(True, outcomes.values())
        self.assertGreaterEqual(list(outcomes.values()).count(False), 5)

    def test_the_comparison_reports_a_difference(self):
        # Three producers: command_evidence names both players, the N-player form accepts them.
        directory = self.corpus(players=(1, 2, 3))
        timing = json.loads((directory / 'timing.json').read_text())
        timing['player_ids'] = [1, 2, 3]
        (directory / 'timing.json').write_text(json.dumps(timing))
        self.assertIn('errors', quad.cross_validate(directory))

    def test_a_recorded_gameplay_run_is_compared_on_a_copy_with_the_gameplay_window(self):
        directory = self.corpus()
        (directory / 'timing.json').unlink()
        # gameplay_evidence's fixed window [start + 1 s, start + 15 s); both analyzers judge the same window.
        (directory / 'action-client.json').write_text(json.dumps({'start_ns': START - SECOND, 'player_ids': [1, 2]}))
        self.assertEqual(quad.cross_validate_recorded(directory), {})
        self.assertEqual(sorted(path.name for path in directory.iterdir()),
                         ['action-client.json', 'clients-commands.jsonl', 'match-commands.jsonl'])

    def test_fault_recovery_is_left_to_the_two_client_analyzers(self):
        directory = self.corpus(release=START + SECOND)
        with self.assertRaises(ValueError):
            quad.command_metrics(directory)


class QuadCommandTests(Corpora):
    def four_players(self, **options):
        def every_player(directory, client, match, timing):
            timing['player_ids'] = list(options.get('players', range(1, MAX_PLAYERS + 1)))
        return self.corpus(every_player, players=tuple(range(1, MAX_PLAYERS + 1)),
                           **{key: value for key, value in options.items() if key != 'players'})

    def test_a_full_room_at_60_hz_passes(self):
        result = quad.command_metrics(self.four_players())
        self.assertTrue(result['passed'], result['errors'])
        self.assertEqual(result['players'], MAX_PLAYERS)
        self.assertEqual(result['commands'], MAX_PLAYERS * DURATION * 60)

    def test_a_silent_player_fails(self):
        result = quad.command_metrics(self.four_players(players=range(1, MAX_PLAYERS + 2)))
        self.assertIn(quad.EVERY_PLAYER_ERROR, result['errors'])

    def test_half_rate_production_fails(self):
        self.assertFalse(quad.command_metrics(self.four_players(rate=30), enforce=False)['passed'])


QUAD_SPAWNS = [[2.0, 0.0, 2.0], [8.0, 0.0, 8.0], [2.0, 0.0, 8.0], [8.0, 0.0, 2.0]]
RESPAWN_TICK = 230


def snapshot(tick, *, respawned=True, positions=None):
    players, combat = [], []
    for index, player_id in enumerate(range(1, MAX_PLAYERS + 1)):
        position = list((positions or {}).get(player_id, [2.0 + index, 0.0, 5.0]))
        life, state, state_tick = 1, 0, 0
        if player_id == 2 and tick >= 50:
            life, state, state_tick = 1, 1, 50
            if respawned and tick >= RESPAWN_TICK:
                life, state, state_tick = 2, 0, RESPAWN_TICK
                if tick == RESPAWN_TICK and player_id not in (positions or {}):
                    position = list(QUAD_SPAWNS[1])
        players.append({'player_id': player_id, 'life_generation': life, 'life_state': state, 'life_state_tick': state_tick,
                        'respawn_tick': 0, 'epoch': life, 'position': position, 'yaw': 0.0})
        combat.append({'player_id': player_id, 'life_generation': life, 'hp': 100, 'ammo': 12, 'reload_end_tick': 0,
                       'damage_count': 0, 'last_attacker_id': 0, 'last_damage_tick': 0})
    return {'tick': tick, 'players': players, 'combat': combat}


class QuadFunctionalTests(Corpora):
    def make_quad(self, *, change_client=None, change_snapshots=None, gui=None, slow=False):
        def every_player(directory, client, match, timing):
            timing['player_ids'] = list(range(1, MAX_PLAYERS + 1))
            if slow:
                for record in match:
                    if record['kind'] == 'resolved':
                        record['time_ns'] += 45_000_000
        directory = self.corpus(every_player, players=tuple(range(1, MAX_PLAYERS + 1)))
        frames = [START + n * SECOND // 60 for n in range(DURATION * 60)]
        clients = []
        for player_id in range(1, MAX_PLAYERS + 1):
            submitted = [{'action_id': n, 'action_kind': 0, 'life_generation': 1} for n in (1, 2)]
            decisions = [{'action_id': 1, 'accepted': True, 'rejection': 0},
                         {'action_id': 2, 'accepted': False, 'rejection': quad.REJECTION_NAMES.index('Dead')}]
            clients.append({'player_id': player_id, 'submitted': submitted, 'decisions': decisions, 'retained': 0,
                            'maximum_players_seen': MAX_PLAYERS})
        client = {'protocol': 6, 'capacity': MAX_PLAYERS, 'start_ns': START - SECOND, 'end_ns': START + DURATION * SECOND,
                  'player_ids': list(range(1, MAX_PLAYERS + 1)), 'fps': 60, 'duration': DURATION + 2, 'created_room': True,
                  'expect_players': MAX_PLAYERS,
                  'fifth': {'listed_players': MAX_PLAYERS, 'listed_capacity': MAX_PLAYERS, 'error': 'room_full', 'joined': False},
                  'frame_seconds': [1 / 60] * len(frames), 'frame_time_ns': frames, 'timer': None,
                  'arena': {'id': 'quad', 'radius': .35, 'spawns': QUAD_SPAWNS}, 'clients': clients}
        snapshots = [snapshot(tick) for tick in range(40, 260)]
        if change_client:
            change_client(client)
        lines = []
        for index in range(MAX_PLAYERS):
            for value in snapshots:
                line = copy.deepcopy(value)
                line['client'] = index
                lines.append(line)
        if change_snapshots:
            change_snapshots(lines)
        if gui is not None:
            client['player_ids'] = client['player_ids'][:-1]
            client['clients'] = client['clients'][:-1]
            (directory / 'gui-quad.json').write_text(json.dumps(gui))
        (directory / 'quad-client.json').write_text(json.dumps(client))
        (directory / 'quad-snapshots.jsonl').write_text(''.join(json.dumps(line) + '\n' for line in lines))
        return directory

    def errors(self, directory, **options):
        return quad.analyze_quad(directory, **options)['errors']

    def test_a_full_room_with_a_refused_fifth_and_a_free_respawn_passes(self):
        directory = self.make_quad()
        result = quad.analyze_quad(directory)
        self.assertTrue(result['passed'], result['errors'])
        self.assertEqual(result['rejections'], {'Dead': MAX_PLAYERS})
        self.assertEqual([(r['player_id'], r['life_generation'], r['at_spawn']) for r in result['respawns']], [(2, 2, True)])
        self.assertEqual(result['shared_snapshot_ticks'], 220)
        # The measurement window and the room come from the probe, not from an earlier timing file.
        timing = json.loads((directory / 'timing.json').read_text())
        self.assertEqual((timing['player_ids'], timing['start_ns'], timing['end_ns']),
                         (list(range(1, MAX_PLAYERS + 1)), START, START + DURATION * SECOND))

    def test_the_fifth_join_must_be_refused_as_room_full(self):
        for change in ({'error': 'match_full'}, {'joined': True}, {'listed_players': MAX_PLAYERS - 1}):
            with self.subTest(change=change):
                directory = self.make_quad(change_client=lambda c: c['fifth'].update(change))
                self.assertTrue(any('extra join' in e or 'full room' in e for e in self.errors(directory)))

    def test_every_action_has_exactly_one_decision(self):
        directory = self.make_quad(change_client=lambda c: c['clients'][0]['decisions'].pop())
        self.assertTrue(any('one to one' in error for error in self.errors(directory)))
        directory = self.make_quad(change_client=lambda c: c['clients'][1].update(retained=1))
        self.assertTrue(any('retained' in error for error in self.errors(directory)))

    def test_only_racing_rejections_are_allowed(self):
        cooldown = quad.REJECTION_NAMES.index('Cooldown')
        directory = self.make_quad(change_client=lambda c: c['clients'][2]['decisions'][1].update(rejection=cooldown))
        self.assertTrue(any('Cooldown' in error for error in self.errors(directory)))

    def test_clients_must_observe_identical_snapshots(self):
        def differ(lines):
            lines[MAX_PLAYERS * 0 + 10]['players'][0]['position'][0] += .5
        self.assertTrue(any('different payloads' in error for error in self.errors(self.make_quad(change_snapshots=differ))))

    def test_every_snapshot_holds_the_full_room(self):
        def drop(lines):
            for line in lines:
                if line['tick'] == 100:
                    line['players'].pop()
        self.assertTrue(any('without the full room' in error for error in self.errors(self.make_quad(change_snapshots=drop))))

    def test_a_respawn_lands_on_a_free_spawn(self):
        def off_spawn(lines):
            for line in lines:
                if line['tick'] == RESPAWN_TICK:
                    line['players'][1]['position'] = [5.0, 0.0, 5.0]
        self.assertTrue(any('free spawn' in error for error in self.errors(self.make_quad(change_snapshots=off_spawn))))

        def crowded(lines):
            for line in lines:
                if line['tick'] == RESPAWN_TICK:
                    line['players'][0]['position'] = [8.2, 0.0, 8.0]
        self.assertTrue(any('free spawn' in error for error in self.errors(self.make_quad(change_snapshots=crowded))))

    def test_a_run_without_a_respawn_fails(self):
        def never(lines):
            lines[:] = [line for line in lines if line['tick'] < RESPAWN_TICK]
        self.assertTrue(any('died and respawned' in error for error in self.errors(self.make_quad(change_snapshots=never))))

    def test_clean_30_records_latency_without_judging_it(self):
        directory = self.make_quad(slow=True)
        self.assertTrue(any('Command stage' in error for error in self.errors(directory)))
        result = quad.analyze_quad(directory, judged=False)
        self.assertTrue(result['passed'], result['errors'])
        self.assertFalse(result['command_evidence']['latency_thresholds_enforced'])

    def test_a_gui_participant_keeps_the_room_full(self):
        frames = [START + n * SECOND // 60 for n in range(121)]
        gui = {'player_id': MAX_PLAYERS, 'frames': 121, 'presented_frames': 120, 'skipped_presentation_frames': 0,
               'minimum_players': MAX_PLAYERS, 'maximum_players': MAX_PLAYERS, 'deaths': 0, 'life_generations': [1],
               'frame_time_ns': frames, 'duration': 2}
        result = quad.analyze_quad(self.make_quad(gui=gui))
        self.assertTrue(result['passed'], result['errors'])
        self.assertTrue(math.isclose(result['gui']['achieved_fps'], 60, rel_tol=1e-3))
        gui['minimum_players'] = MAX_PLAYERS - 1
        self.assertTrue(any('GUI Client' in error for error in self.errors(self.make_quad(gui=gui))))

    def test_transport_statistics_read_the_match_trace(self):
        def with_transport(directory, client, match, timing):
            transport(directory, client, match, timing)
        directory = self.corpus(with_transport)
        stats = quad.transport_statistics(directory / 'match-commands.jsonl')
        self.assertEqual((stats['events'], stats['coalesced_snapshots'], stats['aged_events']), (4, 4, 3))
        self.assertEqual((stats['age_p95_seconds'], stats['age_max_seconds']), (.12, .12))


if __name__ == '__main__':
    unittest.main()
