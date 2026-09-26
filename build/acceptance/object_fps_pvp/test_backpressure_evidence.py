"""Recovery must prove uninterrupted fresh Actual executions, not occasional pulses."""
import csv
import json
from pathlib import Path
import tempfile
import unittest

from backpressure_probe import analyze


def event(kind, timestamp, player=0, epoch=1, sequence=0, tick=0, source='none', queued=0):
    return {'kind': kind, 'time_ns': timestamp, 'player_id': player, 'epoch': epoch,
            'sequence': sequence, 'authority_tick': tick, 'source': source, 'queued': queued,
            'pending': 0, 'seeded_neutral': False, 'dropped_seconds': 0,
            'frame_seconds': 0, 'count': 0, 'age_seconds': 0, 'reset_reason': 'none'}


class BackpressureEvidence(unittest.TestCase):
    def fixture(self, directory, mode='actual'):
        start, release = 1_000_000_000, 1_250_000_000
        match, clients, frames = [], [], []
        for tick in range(1, 181):
            timestamp = start + tick * 16_666_667
            epoch, sequence = 1, tick
            if mode == 'rotating_epoch' and tick >= 16:
                epoch = 2 + (tick-16)//10
                sequence = 1 + (tick-16)%10
            for player in (1, 2):
                source = 'held' if mode == 'all_held' or (mode == 'one_actual_four_held' and tick % 5) else 'actual'
                generated_epoch = 1 if mode == 'old_epoch' else epoch
                resolved_epoch = 2 if mode == 'old_epoch' else epoch
                generated_at = release-1 if mode == 'pre_release_generated' else (
                    timestamp+1_000_000 if mode == 'future_generated' else timestamp-5_000_000)
                clients.append(event('generated', generated_at, player, generated_epoch, sequence))
                if mode != 'missing_steps' or tick % 5:
                    match.append(event('resolved', timestamp, player, resolved_epoch, sequence, tick, source,
                                       33 if mode == 'overflow_queue' and tick == 170 else 2))
            match.append(event('snapshot_produced', timestamp, tick=tick))
            frames.append({'time_ns': timestamp+1_000, 'frame_seconds': 1/60,
                           'pending_a': 13 if mode == 'overflow_pending' and tick == 170 else 2, 'pending_b': 2,
                           'queued_a': 2, 'queued_b': 2, 'epoch_a': epoch, 'epoch_b': epoch,
                           'remote_age_a': .001, 'remote_age_b': .001,
                           'resolved_a': sequence, 'resolved_b': sequence,
                           'authority_x_a': 1, 'authority_z_a': 1,
                           'authority_x_b': 2, 'authority_z_b': 2})
        for name, events in (('match-commands.jsonl', match), ('clients-commands.jsonl', clients)):
            events.sort(key=lambda item: item['time_ns'])
            (directory/name).write_text(''.join(json.dumps(item)+'\n' for item in events) +
                                       json.dumps({'kind': 'trace_end', 'events': len(events), 'dropped': 0})+'\n')
        with (directory/'frames.csv').open('w') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(frames[0]))
            writer.writeheader(); writer.writerows(frames)
        (directory/'timing.json').write_text(json.dumps({'player_ids': [1, 2], 'start_ns': start,
            'end_ns': start+3_000_000_000, 'release_ns': release, 'injected': True,
            'recovery_client_passed': True}))
        return {'start_ns': start, 'release_ns': release}

    def test_fresh_actuals_remain_stable(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            result = analyze(directory, self.fixture(directory))
            self.assertTrue(result['passed'])

    def test_rejects_intermittent_actual_and_invalid_identity(self):
        for mode in ('one_actual_four_held', 'all_held', 'old_epoch', 'pre_release_generated',
                     'rotating_epoch', 'missing_steps', 'future_generated', 'overflow_queue', 'overflow_pending'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                fault = self.fixture(directory, mode)
                with self.assertRaises((AssertionError, ValueError)):
                    analyze(directory, fault)


if __name__ == '__main__':
    unittest.main()
