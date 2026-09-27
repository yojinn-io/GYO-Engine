"""Evidence must reject changed decisions, duplicated effects and rate bursts."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from action_probe import analyze, encoded, fields, maximum_window
from action_evidence import records, analyze_legal


class ActionEvidence(unittest.TestCase):
    def fixture(self, output):
        clients = [{'player_id': p, 'decisions': 1, 'maximum_hp': 100,
                    'combat': [{'player_id': 1, 'hp': 75}, {'player_id': 2, 'hp': 75}]} for p in (1, 2)]
        (output/'action-client.json').write_text(json.dumps({'passed': True, 'clients': clients,
            'maximum_retained': [2, 2], 'maximum_unconsumed': [1, 1], 'player_ids': [1, 2]}))
        events, wire = [], []
        for player in (1, 2):
            d = {'action_id': 1, 'resolved_tick': 2, 'accepted': True, 'rejection': 0,
                 'hit_kind': 2, 'target_id': 3-player, 'damage': 25}
            events.append({'kind': 'decision', 'player_id': player, 'decision': d})
            wire.append({'event': 'received', 'upstream': False, 'kind': 7, 'session': player,
                         'bytes': 64, 'time_ns': 40_000_000, 'decisions': [d]})
            for kind, count, spacing in ((1, 6, 1_000_000_000), (3, 300, 16_666_667), (6, 150, 33_333_334)):
                for i in range(count):
                    packet = {'event': 'received', 'upstream': True, 'kind': kind, 'session': player,
                              'bytes': 64, 'time_ns': 1+i*spacing}
                    wire.extend((packet, dict(packet, event='forwarded')))
        (output/'actions.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in events))
        (output/'action-frames.json').write_text(json.dumps([
            {**{'snapshot_tick_'+s: tick for s in ('a', 'b')},
             **{'combat_'+s: [{'player_id': p, 'hp': hp} for p in (1, 2)] for s in ('a', 'b')}}
            for tick, hp in ((1, 100), (2, 75), (3, 75))]))
        return {'relay_error': None, 'events': wire, 'players': {1: 1, 2: 2}}

    def test_complete_evidence_passes(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            self.assertTrue(analyze(output, self.fixture(output), {'start_ns': 0, 'release_ns': 0}, 'unit-fixture')['passed'])

    def test_rejects_duplicate_effect_even_when_final_hp_matches(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            relay = self.fixture(output)
            frames = json.loads((output/'action-frames.json').read_text())
            frames[1]['combat_a'][0]['hp'] = 50
            (output/'action-frames.json').write_text(json.dumps(frames))
            result = analyze(output, relay, {'start_ns': 0, 'release_ns': 0}, 'unit-fixture')
            self.assertFalse(result['passed'])
            self.assertTrue(any('Snapshot combat effect' in error for error in result['errors']))

    def test_rejects_mutated_original_decision(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            relay = self.fixture(output)
            duplicate = dict(relay['events'][0])
            duplicate['decisions'] = [dict(duplicate['decisions'][0], resolved_tick=3)]
            relay['events'].append(duplicate)
            result = analyze(output, relay, {'start_ns': 0, 'release_ns': 0}, 'unit-fixture')
            self.assertFalse(result['passed'])
            self.assertTrue(any('Repeated result mutated' in error for error in result['errors']))

    def test_rate_counts_adversarial_packets_and_boundary(self):
        self.assertEqual(maximum_window([0, 999_999_999, 1_000_000_000]), 2)
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            relay = self.fixture(output)
            relay['events'] += [{'event': 'malformed_action', 'upstream': True, 'kind': 6,
                                 'session': 1, 'bytes': 27, 'time_ns': 1} for _ in range(121)]
            result = analyze(output, relay, {'start_ns': 0, 'release_ns': 0}, 'unit-fixture')
            self.assertFalse(result['passed'])
            self.assertTrue(any('exceeds120' in error for error in result['errors']))

    def test_raw_inspector_rejects_truncated_fields(self):
        for payload in (b'\x08\x80', b'\x0a\x04\x08', b'\x1d\x00\x00', b'\x00'):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                fields(payload)
        sample = [(1, 0, 2**64-1), (2, 2, b'\x08\x01'), (3, 5, b'\x00\x00\x80\x3f')]
        self.assertEqual(fields(encoded(sample)), sample)

    def test_gateway_pause_never_exempts_the_cap_or_required_fault_evidence(self):
        for mode, maximum, rejected, expected in (
            ('unit-fixture', 91, 1, 'rate limiter discarded'),
            ('gateway', 121, 118, '120'),
            ('gateway', 120, 118, 'Requested fault was not applied'),
        ):
            with self.subTest(mode=mode, maximum=maximum), tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary)
                relay = self.fixture(output)
                relay['gateway_counters'] = {'rate_accepted_packets': 911,
                    'rate_limited_packets': rejected, 'max_session_window_packets': maximum}
                result = analyze(output, relay, {'start_ns': 0, 'release_ns': 0}, mode)
                self.assertFalse(result['passed'])
                self.assertTrue(any(expected in error for error in result['errors']), result['errors'])

    def test_streamed_evidence_rejects_truncation_loss_and_extra_records(self):
        for tail in ('', '{"kind":"evidence_end","records":2,"lost":0}\n',
                     '{"kind":"evidence_end","records":1,"lost":1}\n',
                     '{"kind":"evidence_end","records":1,"lost":0}\n{}\n'):
            with self.subTest(tail=tail), tempfile.TemporaryDirectory() as temporary:
                path = Path(temporary)/'events.jsonl'
                path.write_text('{"kind":"submitted"}\n'+tail)
                with self.assertRaises(ValueError):
                    list(records(path))

    def legal_fixture(self, output, *, rejected=False, missing=False, match_delay=20, delivery_delay=40):
        start = 3_000_000_000
        count = 13
        final_hp = 100 if rejected or missing else 0
        clients = [{'player_id': p, 'maximum_hp': 100, 'shot_damage': 25, 'maximum_batch_shots': 1,
                    'maximum_datagram_bytes': 64, 'submitted': count, 'decisions': 0 if missing else count,
                    'retained': 0, 'retired_through': count,
                    'combat': [{'player_id': target, 'hp': final_hp} for target in (1, 2)]} for p in (1, 2)]
        (output/'action-client.json').write_text(json.dumps({'legal_shots': True, 'passed': True,
            'measurement_start_ns': start, 'measurement_end_ns': start+5_000_000_000, 'duration': 5,
            'start_ns': start, 'end_ns': start+7_000_000_000,
            'player_ids': [1, 2], 'clients': clients, 'fps': 60, 'blocked_submissions': [0, 0]}))
        events, ticks = [], []
        for index in range(count):
            generated = start+index*400_000_000
            resolved_tick = index+2
            ticks.append({'kind': 'snapshot_produced', 'authority_tick': resolved_tick,
                          'time_ns': generated+match_delay*1_000_000})
            for player in (1, 2):
                events.append({'kind': 'submitted', 'player_id': player, 'time_ns': generated,
                               'request': {'action_id': index+1}})
                if not missing:
                    events.append({'kind': 'decision', 'player_id': player, 'time_ns': generated+delivery_delay*1_000_000,
                        'decision': {'action_id': index+1, 'resolved_tick': resolved_tick,
                                     'accepted': not rejected, 'rejection': 3 if rejected else 0,
                                     'damage': 0 if rejected or index >= 4 else 25,
                                     'target_id': 3-player, 'hit_kind': 2}})
        frame = {'time_ns': start, 'frame_seconds': 1/60}
        for suffix in ('a', 'b'):
            frame.update({'frozen_'+suffix: False, 'pending_'+suffix: 3, 'retained_'+suffix: 0,
                          'snapshot_tick_'+suffix: 1, 'combat_'+suffix: [{'player_id': p, 'hp': 100} for p in (1, 2)]})
        frame_values = []
        for index in range(351):
            stamp = start+index*20_000_000
            completed = sum(tick['time_ns'] <= stamp for tick in ticks)
            value = dict(frame, time_ns=stamp, frame_seconds=.02)
            for suffix in ('a', 'b'):
                value['snapshot_tick_'+suffix] = completed+1
                value['combat_'+suffix] = [{'player_id': p,
                    'hp': 100 if rejected or missing else 100-min(4, completed)*25} for p in (1, 2)]
            frame_values.append(value)
        for name, values in (('actions.jsonl', events), ('action-frames.jsonl', frame_values)):
            (output/name).write_text(''.join(json.dumps(value)+'\n' for value in values)+
                json.dumps({'kind': 'evidence_end', 'records': len(values), 'lost': 0})+'\n')
        movement = {'passed': True, 'errors': [], 'disturbed': False, 'resets': 0, 'production_60hz_passed': True}
        return movement, ticks

    def test_legal_evidence_requires_continuing_shots_and_complete_hp(self):
        for fault, expected in (
            ('stopped_shots', 'stopped before measurement ended'),
            ('skipped_shot', 'skipped its next eligible frame'),
            ('missing_hp', 'Frame combat state'),
            ('duplicate_hp', 'Frame combat state'),
            ('regressed_tick', 'Snapshot Tick'),
            ('wrong_terminal_hp', 'Terminal HP'),
        ):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary)
                movement, ticks = self.legal_fixture(output)
                if fault in ('stopped_shots', 'skipped_shot'):
                    path = output/'actions.jsonl'
                    values = list(records(path))
                    values = [value for value in values if
                              ((value.get('request') or value.get('decision'))['action_id'] <= 4
                               if fault == 'stopped_shots' else
                               (value.get('request') or value.get('decision'))['action_id'] != 8)]
                    path.write_text(''.join(json.dumps(value)+'\n' for value in values)+
                        json.dumps({'kind': 'evidence_end', 'records': len(values), 'lost': 0})+'\n')
                elif fault == 'wrong_terminal_hp':
                    path = output/'action-client.json'
                    value = json.loads(path.read_text())
                    value['clients'][0]['combat'][0]['hp'] = 25
                    path.write_text(json.dumps(value))
                else:
                    path = output/'action-frames.jsonl'
                    values = list(records(path))
                    if fault == 'missing_hp': values[-1]['combat_a'] = []
                    elif fault == 'duplicate_hp': values[-1]['combat_a'][1] = values[-1]['combat_a'][0]
                    else: values[-1]['snapshot_tick_a'] = 1
                    path.write_text(''.join(json.dumps(value)+'\n' for value in values)+
                        json.dumps({'kind': 'evidence_end', 'records': len(values), 'lost': 0})+'\n')
                with patch('action_evidence.analyze_commands', return_value=movement), \
                        patch('action_evidence.read_trace_events', return_value=ticks):
                    result = analyze_legal(output)
                self.assertFalse(result['passed'])
                self.assertTrue(any(expected in error for error in result['errors']), result['errors'])

    def test_legal_short_does_not_qualify_as_long_and_enforces_both_latency_gates(self):
        for changes, expected in (({}, True), ({'rejected': True}, False), ({'missing': True}, False),
                                  ({'match_delay': 101, 'delivery_delay': 130}, False),
                                  ({'delivery_delay': 151}, False)):
            with self.subTest(changes=changes), tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary)
                movement, ticks = self.legal_fixture(output, **changes)
                with patch('action_evidence.analyze_commands', return_value=movement), \
                        patch('action_evidence.read_trace_events', return_value=ticks):
                    result = analyze_legal(output)
                self.assertEqual(result['passed'], expected, result['errors'])
                self.assertFalse(result['full_30_minute_qualified'])

    def test_clean_legal_run_rejects_unexplained_reset_or_disturbance(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            movement, ticks = self.legal_fixture(output)
            movement['resets'] = 1
            with patch('action_evidence.analyze_commands', return_value=movement), \
                    patch('action_evidence.read_trace_events', return_value=ticks):
                self.assertFalse(analyze_legal(output)['passed'])

    def test_declared_duration_cannot_promote_short_trace_to_long(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            movement, ticks = self.legal_fixture(output)
            path = output/'action-client.json'
            client = json.loads(path.read_text())
            client['duration'] = 1800
            client['measurement_end_ns'] = client['measurement_start_ns']+1_800_000_000_000
            path.write_text(json.dumps(client))
            with patch('action_evidence.analyze_commands', return_value=movement), \
                    patch('action_evidence.read_trace_events', return_value=ticks):
                result = analyze_legal(output)
            self.assertFalse(result['passed'])
            self.assertFalse(result['full_30_minute_qualified'])
            self.assertTrue(any('coverage' in error or 'Actual probe clock' in error for error in result['errors']))

    def test_rejected_action_cannot_hide_damage_in_fault_mode(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            movement, ticks = self.legal_fixture(output)
            path = output/'actions.jsonl'
            values = [json.loads(line) for line in path.read_text().splitlines()]
            values[1]['decision'].update(accepted=False, rejection=3)  # Keep the illegal25damage.
            path.write_text(''.join(json.dumps(value)+'\n' for value in values))
            with patch('action_evidence.analyze_commands', return_value=movement), \
                    patch('action_evidence.read_trace_events', return_value=ticks):
                result = analyze_legal(output, injected=True)
            self.assertFalse(result['passed'])
            self.assertIn('Rejected action produced damage', result['errors'])


if __name__ == '__main__':
    unittest.main()
