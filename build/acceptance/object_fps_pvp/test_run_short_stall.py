"""IPC input-delay runner tests (D48 step 2, D50 b1).

run_case, the sleeper, the repository state and the clock are replaced, so the session tests start no services. The
relay test uses loopback sockets only (a fake Gateway and a fake Match around the real IpcDelay).
"""
from contextlib import redirect_stderr, redirect_stdout
import io
from itertools import chain, repeat
import json
from pathlib import Path
import socket
import tempfile
import time
import unittest
from unittest import mock

import action_probe
import backpressure_probe
from acceptance_util import PROTOCOL_VERSION
import run_short_stall as runner

FAULT = {'mechanism': 'ipc-input-delay', 'direction': 'gateway->match', 'delay_ms': 50, 'window_ms': 100,
         'delayed_chunks': 3, 'start_ns': 10, 'window_end_ns': 100_000_010, 'release_ns': 150_000_000}


def outcome(fault=FAULT, passed=True, artifacts=True, errors=()):
    """What a fake run_case writes to result.json and returns (run_case does both)."""
    result = {'passed': passed, 'errors': list(errors)}
    if fault is not None:
        result['fault'] = dict(fault)
    if artifacts:
        result['artifacts'] = {}
    return result


class ShortStallRunnerTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='pvp-ipc-delay-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.argv = ['--match', 'm', '--gateway', 'g', '--probe', 'p', '--arena', 'a',
                     '--output', str(self.root/'run'), '--delay-ms', '50', '--window-ms', '100', '--fault-at', '2.0']
        self.state = {'head': 'abc', 'tracked_changes': ''}

    def execute(self, extra, results, sample=None, repository=None, **clock):
        calls = []

        def case(options, mode, milliseconds):
            calls.append({'options': options, 'mode': mode, 'milliseconds': milliseconds,
                          'relay': action_probe.IpcPause})
            result = results[len(calls)-1]
            case_dir = options.output/options.case_name
            case_dir.mkdir(parents=True)
            (case_dir/'result.json').write_text(json.dumps(result))
            return result
        with redirect_stdout(io.StringIO()):
            summary = runner.execute(runner.parse(self.argv+extra), case, sample=sample,
                                     repository=repository or (lambda: dict(self.state)), **clock)
        return summary, calls

    def logged(self):
        return [json.loads(line) for line in (self.root/'run/results.jsonl').read_text().splitlines()]

    def test_rounds_alternate_fault_at_and_run_case_sees_the_delaying_relay_only_during_the_call(self):
        summary, calls = self.execute(['--fault-at', '11.5', '--rounds', '3', '--fps', '30'], [outcome()]*3)
        self.assertTrue(summary['completed'])
        self.assertEqual([c['options'].case_name for c in calls],
                         ['ipcdelay50-window100-at2.0-r1', 'ipcdelay50-window100-at11.5-r2',
                          'ipcdelay50-window100-at2.0-r3'])
        self.assertEqual([c['options'].fault_at for c in calls], [2.0, 11.5, 2.0])
        for call in calls:
            self.assertEqual((call['mode'], call['milliseconds']), ('host-ipc', 100))
            self.assertTrue(issubclass(call['relay'], runner.IpcDelay))
            self.assertIsNot(call['relay'], backpressure_probe.IpcPause)
            self.assertEqual(call['relay'].DELAY_MS, 50)
            self.assertEqual((call['options'].fps, call['options'].gameplay_v5), (30, True))
        self.assertIs(action_probe.IpcPause, backpressure_probe.IpcPause)
        plan = json.loads((self.root/'run/plan.json').read_text())
        self.assertEqual((plan['delay_ms'], plan['window_ms'], plan['fault_at']), (50, 100, [2.0, 11.5]))
        self.assertEqual(plan['frozen'], runner.FROZEN)
        self.assertEqual(plan['tools']['action_probe.py'], runner.FROZEN['action_probe.py'])
        self.assertIn('Gateway->Match', plan['delayed_traffic'])
        self.assertEqual([r['status'] for r in self.logged()], ['completed']*3)

    def test_run_case_verdict_is_recorded_only_and_a_runner_error_stops_the_session(self):
        failed = outcome(passed=False, errors=['Movement recovery exceeded 1.5 s'])
        broken = outcome(fault=None, passed=False, artifacts=False, errors=['Probe did not join both real Sessions'])
        summary, calls = self.execute(['--rounds', '4'], [failed, broken])
        self.assertEqual(len(calls), 2)
        self.assertEqual([r['status'] for r in summary['rounds']],
                         ['completed', 'runner_error', 'not_run', 'not_run'])
        self.assertEqual(summary['rounds'][0]['run_case'],
                         {'passed': False, 'errors': ['Movement recovery exceeded 1.5 s'], 'recorded_only': True})
        self.assertIn('runner error in round 2', summary['stopped'])
        self.assertFalse(summary['completed'])
        self.assertEqual([r['status'] for r in self.logged()], ['completed', 'runner_error'])
        self.assertTrue((self.root/'run/ipcdelay50-window100-at2.0-r1/result.json').exists())

    def test_missing_delay_fields_mean_the_replacement_did_not_take_effect_and_stop_the_session(self):
        paused = {'frames': 300, 'start_ns': 10, 'release_ns': 20, 'relay_error': None}  # IpcPause's own stats
        wrong = dict(FAULT, delay_ms=80)
        idle = dict(FAULT, delayed_chunks=0)
        window = dict(FAULT, window_ms=150)
        unreleased = dict(FAULT, release_ns=5)
        for fault, expected in ((paused, 'mechanism'), (wrong, 'delay_ms/window_ms'), (idle, 'delayed_chunks'),
                                (window, 'delay_ms/window_ms'), (unreleased, 'start_ns/release_ns')):
            with self.subTest(expected):
                for child in sorted(self.root.rglob('*'), reverse=True):
                    child.unlink() if child.is_file() else child.rmdir()
                summary, calls = self.execute(['--rounds', '2'], [outcome(fault=fault)])
                self.assertEqual(len(calls), 1)
                self.assertEqual([r['status'] for r in summary['rounds']], ['injection_unproven', 'not_run'])
                self.assertIn('D50 condition 3', summary['stopped'])
                self.assertIn(expected, summary['stopped'])

    def test_frozen_hash_mismatch_is_refused_before_anything_is_written(self):
        with mock.patch.dict(runner.FROZEN, {'backpressure_probe.py': '0'*64}):
            with self.assertRaisesRegex(runner.Refused, 'backpressure_probe.py: sha256 .* differs from frozen 0{64}'):
                self.execute([], [outcome()])
        self.assertFalse((self.root/'run').exists())
        self.assertEqual(runner.frozen_problems(), [])

    def test_existing_evidence_and_out_of_range_parameters_are_refused(self):
        (self.root/'run').mkdir()
        (self.root/'run/keep.txt').write_text('keep')
        with self.assertRaisesRegex(runner.Refused, 'never overwritten'):
            self.execute([], [])
        self.assertEqual((self.root/'run/keep.txt').read_text(), 'keep')
        for extra in (['--delay-ms', '200'], ['--window-ms', '20'], ['--fault-at', '0.5'], ['--rounds', '0'],
                      ['--idle-gate']):
            with self.subTest(extra), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                runner.parse(self.argv+extra)

    def test_a_repository_change_stops_before_the_next_round(self):
        states = iter([{'head': 'abc', 'tracked_changes': ''}]*2+[{'head': 'def', 'tracked_changes': ''}]*3)
        summary, calls = self.execute(['--rounds', '3'], [outcome()]*3, repository=lambda: next(states))
        self.assertEqual(len(calls), 1)
        self.assertEqual([r['status'] for r in summary['rounds']], ['completed', 'not_run', 'not_run'])
        self.assertIn('repository changed before round 2', summary['stopped'])

    def test_a_tracked_file_change_alone_stops_before_the_next_round(self):
        states = iter([{'head': 'abc', 'tracked_changes': ''}]*2+[{'head': 'abc', 'tracked_changes': ' M x'}]*3)
        summary, calls = self.execute(['--rounds', '2'], [outcome()]*2, repository=lambda: next(states))
        self.assertEqual(len(calls), 1)
        self.assertIn('repository changed before round 2', summary['stopped'])

    def test_idle_gate_retries_every_30_s_and_stops_after_30_minutes(self):
        sleeper = self.root/'sleeper.py'
        sleeper.write_text('# fake\n')
        now = [0.0]
        busy = lambda label: {'when': label, 'max_ms': 12.0}  # noqa: E731
        summary, calls = self.execute(['--sleeper', str(sleeper), '--idle-gate', '--rounds', '2'], [],
                                      sample=busy, sleep=lambda s: now.__setitem__(0, now[0]+s), clock=lambda: now[0])
        self.assertEqual(calls, [])
        self.assertEqual([r['status'] for r in summary['rounds']], ['not_run', 'not_run'])
        self.assertEqual(summary['stopped'], 'idle gate did not pass within 30 minutes')
        attempts = (self.root/'run/idle_gate.jsonl').read_text().splitlines()
        self.assertEqual(len(attempts), 60)
        self.assertFalse(any(json.loads(a)['passed'] for a in attempts))

    def test_idle_gate_pass_then_sleepers_around_every_round(self):
        sleeper = self.root/'sleeper.py'
        sleeper.write_text('# fake\n')
        maxima = chain([12.0], repeat(3.0))
        sample = lambda label: {'when': label, 'max_ms': next(maxima)}  # noqa: E731
        now = [0.0]
        summary, calls = self.execute(['--sleeper', str(sleeper), '--idle-gate'], [outcome()], sample=sample,
                                      sleep=lambda s: now.__setitem__(0, now[0]+s), clock=lambda: now[0])
        self.assertTrue(summary['completed'])
        self.assertEqual([a['passed'] for a in summary['idle_gate']], [False, True])
        self.assertEqual([s['when'] for s in summary['rounds'][0]['sleeper']], ['before', 'after'])
        self.assertEqual(json.loads((self.root/'run/plan.json').read_text())['sleeper']['seconds'], 5)

    def test_a_sleeper_failure_after_a_round_keeps_it_and_stops_the_session(self):
        sleeper = self.root/'sleeper.py'
        sleeper.write_text('# fake\n')
        answers = iter([{'when': 'before', 'max_ms': 3.0}, ValueError('sleeper printed no JSON')])

        def sample(_label):
            answer = next(answers)
            if isinstance(answer, Exception):
                raise answer
            return answer
        summary, calls = self.execute(['--sleeper', str(sleeper), '--rounds', '2'], [outcome()]*2, sample=sample)
        self.assertEqual(len(calls), 1)
        self.assertEqual([r['status'] for r in summary['rounds']], ['completed', 'not_run'])
        self.assertIn('sleeper printed no JSON', summary['stopped'])


class IpcDelayRelayTests(unittest.TestCase):
    """Loopback check of the injection itself: Gateway->Match delayed inside the window, Match->Gateway never."""

    def setUp(self):
        self.match_listener = socket.socket()
        self.match_listener.bind(('127.0.0.1', 0))
        self.match_listener.listen(1)
        self.addCleanup(self.match_listener.close)
        self.relay = runner.delaying_relay(50)(self.match_listener.getsockname()[1])
        self.addCleanup(self.relay.close)
        self.gateway = socket.create_connection(('127.0.0.1', self.relay.port), timeout=2)
        self.addCleanup(self.gateway.close)
        self.match_listener.settimeout(2)
        self.match, _ = self.match_listener.accept()
        self.match.settimeout(2)
        self.addCleanup(self.match.close)

    @staticmethod
    def receive(conn, size):
        data = b''
        while len(data) < size:
            data += conn.recv(size-len(data))
        return data, time.monotonic()

    def test_gateway_to_match_is_delayed_in_order_and_match_to_gateway_is_not(self):
        self.gateway.sendall(b'A')
        self.assertEqual(self.receive(self.match, 1)[0], b'A')
        armed = time.monotonic()
        self.relay.arm(0.1)
        sent = time.monotonic()
        self.gateway.sendall(b'B')
        payload = bytes((0x08, PROTOCOL_VERSION))+b'snapshot'
        frame = len(payload).to_bytes(4, 'big')+payload
        frame_sent = time.monotonic()
        self.match.sendall(frame)
        got, frame_received = self.receive(self.gateway, len(frame))
        self.assertEqual(got, frame)
        self.assertLess(frame_received-frame_sent, 0.04, 'Match->Gateway must not wait for the delay')
        got, received = self.receive(self.match, 1)
        self.assertEqual(got, b'B')
        self.assertGreaterEqual(received-sent, 0.049)
        time.sleep(max(0.0, armed+0.12-time.monotonic()))
        c_sent = time.monotonic()
        self.gateway.sendall(b'C')
        got, c_received = self.receive(self.match, 1)
        self.assertEqual(got, b'C')
        self.assertLess(c_received-c_sent, 0.04, 'bytes read after the drained window are forwarded at once')
        self.assertTrue(self.relay.released.wait(1))
        stats = self.relay.evidence()
        self.assertEqual(runner.delay_problems(stats, 50, 100), [])
        self.assertEqual((stats['delayed_chunks'], stats['delayed_bytes'], stats['partial_header_bytes']), (1, 1, 0))
        self.assertEqual(stats['window_end_ns']-stats['start_ns'], 100_000_000)
        self.assertGreater(stats['last_delayed_sent_ns'], stats['start_ns'])
        self.assertGreater(stats['release_ns'], stats['last_delayed_sent_ns'])
        self.assertIsNone(stats['relay_error'])

    def test_release_waits_for_every_delayed_chunk(self):
        armed = time.monotonic()
        self.relay.arm(0.1)
        time.sleep(max(0.0, armed+0.07-time.monotonic()))
        self.gateway.sendall(b'B')
        time.sleep(0.015)
        self.gateway.sendall(b'D')
        self.assertEqual(self.receive(self.match, 2)[0], b'BD')
        self.gateway.sendall(b'C')
        self.assertEqual(self.receive(self.match, 1)[0], b'C')
        self.assertTrue(self.relay.released.wait(1))
        stats = self.relay.evidence()
        self.assertEqual(stats['delayed_chunks'], 2)
        self.assertGreater(stats['release_ns'], stats['last_delayed_sent_ns'])


if __name__ == '__main__':
    unittest.main()
