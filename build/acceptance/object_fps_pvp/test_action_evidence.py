"""Evidence must reject changed decisions, duplicated effects and rate bursts."""
import json
from pathlib import Path
import tempfile
import unittest

from action_probe import analyze, encoded, fields, maximum_window
from action_evidence import records


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


if __name__ == '__main__':
    unittest.main()
