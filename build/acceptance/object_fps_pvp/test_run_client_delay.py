"""Client->Gateway pure-delay runner tests (v7 batch 08c L2 preparation, L2P-injector).

Deterministic: the delay line and the relay run on a fake clock; the relay's sockets and threads are never created
(only _ImpairedGateway.__init__ is replaced; ActionRelay.__init__ runs unchanged) and its socket is a recorder.
run_case and the repository state are replaced, so the session tests start no services.
"""
import ast
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest import mock

import action_probe
import impaired_network
import run_client_delay as runner

CLIENTS = {1: ('127.0.0.1', 40001), 2: ('127.0.0.1', 40002)}
GATEWAY = ('127.0.0.1', 50000)


def packet(kind, session, sequence, body=b''):
    return (bytes(6)+kind.to_bytes(2, 'big')+session.to_bytes(8, 'big')+sequence.to_bytes(4, 'big') +
            len(body).to_bytes(2, 'big')+bytes(2)+body)


class Clock:
    def __init__(self, now=100.0):
        self.now = now

    def __call__(self):
        return self.now


def drive(line, profile, arm_at, arrivals, end, step=.0005):
    """Feeds (time, session) Client datagrams to a DelayLine on a fine grid; returns [(sequence, received, sent)]."""
    pending = sorted(arrivals)
    sent, now, sequence = [], arrivals[0][0]-.1, 0
    armed = False
    while now <= end:
        if not armed and now >= arm_at:
            line.arm(now)
            armed = True
        for (number, _), session, received, due, delay in line.due(now):
            line.sent('upstream', session, received, now, due, delay)
            sent.append((number, received, now))
        while pending and pending[0][0] <= now:
            at, session = pending.pop(0)
            sequence += 1
            line.receive('upstream', session, at)
            if line.offer((sequence, session), session, at) is None:
                line.sent('upstream', session, at, now)
                sent.append((sequence, at, now))
        line.maybe_released(now)
        now = round(now+step, 6)
    return sent


def sixty_hz(start, end, sessions=(1, 2)):
    count = round((end-start)*60)
    return [(round(start+i/60, 6), s) for i in range(count) for s in sessions]


class ProfileTests(unittest.TestCase):
    def test_x1_is_constant_inside_the_window_and_ends_after_it(self):
        profile = runner.Profile('x1', delay_ms=200, window_ms=1000)
        self.assertEqual([profile.delay_seconds(t) for t in (-.1, 0, .5, .999)], [0.0, .2, .2, .2])
        self.assertIsNone(profile.delay_seconds(1.0))
        self.assertEqual((profile.duration_ms, profile.maximum_delay_ms, profile.falling_slope), (1000, 200, -1.0))

    def test_ramp_rises_and_falls_linearly(self):
        profile = runner.Profile('ramp', peak_ms=250, rise_ms=1000, fall_ms=500)
        for elapsed, expected in ((0, 0), (.5, .125), (1.0, .25), (1.25, .125), (1.4999, .00005)):
            self.assertAlmostEqual(profile.delay_seconds(elapsed), expected, places=6)
        self.assertIsNone(profile.delay_seconds(1.5))
        self.assertEqual((profile.duration_ms, profile.falling_slope), (1500, -.5))

    def test_a_falling_slope_below_minus_one_is_refused(self):
        steep = runner.Profile('ramp', peak_ms=250, rise_ms=1000, fall_ms=249)
        self.assertTrue(any('below -1' in p for p in runner.profile_problems(steep)))
        self.assertEqual(runner.profile_problems(runner.Profile('ramp', peak_ms=250, rise_ms=1000, fall_ms=250)), [])

    def test_the_queue_bound_is_maximum_delay_times_packet_rate_below_256(self):
        profile = runner.Profile('x1', delay_ms=500, window_ms=1000)
        self.assertTrue(any('queue bound 256' in p for p in runner.profile_problems(profile, 512)))
        self.assertEqual(runner.profile_problems(profile, 511), [])
        # The default rate (two Sessions x 120) keeps 1000 ms under the bound: 240 < 256.
        self.assertEqual(runner.profile_problems(runner.Profile('x1', delay_ms=1000, window_ms=1000)), [])

    def test_bounds_and_the_end_before_the_probe_tail(self):
        cases = ((runner.Profile('x1', delay_ms=0, window_ms=1000), 'delay_ms 0'),
                 (runner.Profile('x1', delay_ms=200, window_ms=50), 'window_ms 50'),
                 (runner.Profile('ramp', peak_ms=1200, rise_ms=1000, fall_ms=1200), 'peak_ms 1200'))
        for profile, expected in cases:
            with self.subTest(expected):
                self.assertTrue(any(expected in p for p in runner.profile_problems(profile)))
        profile = runner.Profile('x1', delay_ms=250, window_ms=3000)
        self.assertEqual(runner.profile_problems(profile, fault_at=[9.75]), [])
        self.assertTrue(any('later than 13' in p for p in runner.profile_problems(profile, fault_at=[9.8])))
        self.assertTrue(any('fault_at 0.5' in p for p in runner.profile_problems(profile, fault_at=[0.5])))


class DelayLineTests(unittest.TestCase):
    def test_x1_delays_every_datagram_in_the_window_and_drains_without_overtaking_or_loss(self):
        profile = runner.Profile('x1', delay_ms=200, window_ms=1000)
        line = runner.DelayLine(profile)
        arrivals = sixty_hz(9.5, 12.0)
        sent = drive(line, profile, 10.0, arrivals, 13.0)
        self.assertEqual([s[0] for s in sent], list(range(1, len(arrivals)+1)))  # receive order kept
        self.assertTrue(all(b[2] >= a[2] for a, b in zip(sent, sent[1:])))
        for _, received, at in sent:
            if received < 10.0 or received >= 11.2:
                self.assertAlmostEqual(at, received, delta=.0006)
            elif received < 11.0:
                self.assertAlmostEqual(at-received, .2, delta=.0006)
            else:  # the step down: held behind the last delayed datagram (slope -1 drain)
                self.assertLess(at-received, .2+.0006)
                self.assertGreaterEqual(at, 11.0+.2-1/60-.0006)
        evidence = line.evidence()
        self.assertEqual(evidence['received_equals_forwarded'], {'before': True, 'after': True})
        self.assertEqual(evidence['counts_after']['upstream'], {'received': len(arrivals), 'forwarded': len(arrivals)})
        # 1 s window at 60 Hz, two Sessions, plus what arrives in the drain before the last delayed datagram is due.
        self.assertEqual(evidence['delayed'], 2*60+evidence['held_behind'])
        self.assertIn(evidence['held_behind'], (2*11, 2*12))
        self.assertEqual(evidence['scheduled_delay_ms']['max'], 200.0)
        self.assertLess(evidence['lateness_ms']['max'], .6)
        self.assertLessEqual(evidence['maximum_queue'], 2*13)  # 0.2 s x 120 datagrams/s, plus the grid
        self.assertFalse(evidence['queue_near_reached'])
        self.assertAlmostEqual(line.release_at, 11.2-1/60, delta=.001)

    def test_ramp_follows_the_profile_and_keeps_order_down_to_slope_minus_one(self):
        for fall in (500, 250):
            with self.subTest(fall=fall):
                profile = runner.Profile('ramp', peak_ms=250, rise_ms=1000, fall_ms=fall)
                line = runner.DelayLine(profile)
                arrivals = sixty_hz(9.5, 12.5)
                sent = drive(line, profile, 10.0, arrivals, 13.5)
                self.assertEqual([s[0] for s in sent], list(range(1, len(arrivals)+1)))
                evidence = line.evidence()
                self.assertEqual(evidence['received_equals_forwarded'], {'before': True, 'after': True})
                self.assertLessEqual(abs(evidence['profile_excess_ms']['max']), .0015)  # due == received + profile (rounding)
                self.assertEqual(evidence['held_behind'], 0)
                self.assertAlmostEqual(evidence['scheduled_delay_ms']['max'], 250, delta=250/60+.01)
                self.assertTrue(all(b[2] >= a[2] for a, b in zip(sent, sent[1:])))

    def test_a_queue_near_the_bound_is_recorded_and_nothing_is_dropped(self):
        profile = runner.Profile('x1', delay_ms=1000, window_ms=1000)
        line = runner.DelayLine(profile)
        arrivals = [(10.0+i*.004, 1) for i in range(220)]  # 250 datagrams/s, one Session
        sent = drive(line, profile, 10.0, arrivals, 12.5)
        evidence = line.evidence()
        self.assertTrue(evidence['queue_near_reached'])
        self.assertGreaterEqual(evidence['maximum_queue'], runner.QUEUE_NEAR)
        self.assertEqual(len(sent), 220)
        self.assertEqual(evidence['received_equals_forwarded'], {'before': True, 'after': True})


class RelayTests(unittest.TestCase):
    """DelayRelay on ActionRelay's real __init__; only the socket/thread base is replaced."""

    def make(self, profile):
        socket_log = []

        class Udp:
            def sendto(self, payload, destination):
                socket_log.append((payload, destination))

        def base(relay, http, udp, rtt):
            relay.upstream_udp, relay.lock, relay.clients = GATEWAY, threading.Lock(), {}
            relay.error, relay.udp, relay.stop = None, Udp(), threading.Event()
        clock = Clock()
        cls = type('Relay', (runner.delaying_relay(profile),), {'CLOCK': staticmethod(clock)})
        with mock.patch.object(impaired_network._ImpairedGateway, '__init__', base):
            relay = cls(1, 2, runner.DISPATCH_MODE)
        return relay, clock, socket_log

    def tick(self, relay, clock, until, step=.001):
        while clock.now < until:
            clock.now = round(clock.now+step, 6)
            with relay.lock:
                relay._release()

    def test_client_datagrams_are_delayed_gateway_datagrams_pass_and_both_directions_are_conserved(self):
        relay, clock, socket_log = self.make(runner.Profile('x1', delay_ms=100, window_ms=200))
        relay._receive(packet(2, 1, 1), CLIENTS[1])
        relay._receive(packet(4, 1, 1), GATEWAY)
        self.assertEqual([d for _, d in socket_log], [GATEWAY, CLIENTS[1]])
        relay.arm('upstream', .2)
        armed = clock.now
        relay._receive(packet(3, 1, 2), CLIENTS[1])
        relay._receive(packet(4, 1, 2), GATEWAY)  # downstream is never held
        self.assertEqual(len(socket_log), 3)
        self.tick(relay, clock, armed+.099)
        self.assertEqual(len(socket_log), 3)
        self.tick(relay, clock, armed+.1005)
        self.assertEqual(socket_log[-1], (packet(3, 1, 2), GATEWAY))
        self.tick(relay, clock, armed+.21)
        self.assertGreater(relay.fault['release_ns'], relay.fault['start_ns'])
        evidence = relay.evidence()
        delay = evidence['client_delay']
        self.assertEqual(delay['received_equals_forwarded'], {'before': True, 'after': True})
        self.assertEqual(delay['counts_after']['upstream'], {'received': 2, 'forwarded': 2})
        self.assertEqual(delay['counts_after']['downstream'], {'received': 2, 'forwarded': 2})
        self.assertEqual((delay['delayed'], delay['arm_duration_ms']), (1, 200))
        self.assertEqual(evidence['fault']['mechanism'], runner.MECHANISM)
        self.assertEqual(evidence['fault']['delay_ms'], 100)
        labels = [e['event'] for e in evidence['events']]
        self.assertEqual(labels.count('received'), 4)
        self.assertEqual(labels.count('forwarded'), 4)
        self.assertFalse([label for label in labels if label not in ('received', 'forwarded')])
        self.assertEqual(evidence['network']['random_dropped'], 0)
        self.assertEqual(evidence['fault']['maximum_held_datagrams'], 0)

    def test_none_of_the_frozen_drop_paths_is_reachable(self):
        # network mode's 5 %, burst2, loss-*, keep-latest and cross-life all live in ActionRelay._receive or in
        # _ImpairedGateway._receive; DelayRelay replaces the method, whatever mode run_case passes.
        self.assertIsNot(runner.DelayRelay._receive, action_probe.ActionRelay._receive)
        self.assertIsNot(runner.DelayRelay._release, action_probe.ActionRelay._release)
        self.assertIsNot(runner.DelayRelay.arm, action_probe.ActionRelay.arm)
        source = ast.parse(Path(runner.__file__).read_text())
        method = next(n for n in ast.walk(source) if isinstance(n, ast.FunctionDef) and n.name == '_receive')
        self.assertFalse([n for n in ast.walk(method) if isinstance(n, ast.Call) and 'random' in ast.unparse(n)])
        self.assertFalse([n for n in ast.walk(method) if isinstance(n, ast.Attribute) and n.attr == 'held'])
        for mode in ('network40', 'burst2', 'loss-shot', 'upstream'):
            with self.subTest(mode):
                relay, clock, socket_log = self.make(runner.Profile('x1', delay_ms=50, window_ms=200))
                relay.mode = mode
                relay.arm('upstream', .2)
                for sequence in range(1, 41):
                    relay._receive(packet(6 if sequence % 3 == 0 else 3, 1, sequence), CLIENTS[1])
                    relay._receive(packet(4, 1, sequence), GATEWAY)
                    self.tick(relay, clock, clock.now+1/60)
                self.tick(relay, clock, clock.now+.3)
                delay = relay.evidence()['client_delay']
                self.assertEqual(delay['counts_after']['upstream'], {'received': 40, 'forwarded': 40})
                self.assertEqual(delay['counts_after']['downstream'], {'received': 40, 'forwarded': 40})

    def test_a_gateway_datagram_for_an_unknown_session_is_counted_not_hidden(self):
        relay, _, socket_log = self.make(runner.Profile('x1', delay_ms=100, window_ms=200))
        relay.arm('upstream', .2)
        relay._receive(packet(4, 9, 1), GATEWAY)
        delay = relay.evidence()['client_delay']
        self.assertEqual(delay['counts_after']['unroutable_downstream'], 1)
        self.assertEqual(delay['received_equals_forwarded']['after'], False)
        self.assertEqual(socket_log, [])


def good_relay(profile, limited=0):
    line = runner.DelayLine(profile)
    drive(line, profile, 10.0, sixty_hz(9.5, 12.5), 14.0)
    return {'client_delay': dict(line.evidence(), arm_duration_ms=profile.duration_ms),
            'fault': {'mechanism': runner.MECHANISM, 'start_ns': 10, 'release_ns': 20, **profile.fields()},
            'relay_error': None, 'gateway_counters': {'rate_limited_packets': limited}}


class RunnerTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='pvp-client-delay-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.frozen = self.root/'frozen.sha256'
        self.frozen.write_text(''.join(f'{runner.source_sha256(runner.HERE/name)}  build/acceptance/object_fps_pvp/{name}\n'
                                       for name in runner.FROZEN))
        self.argv = ['--match', 'm', '--gateway', 'g', '--probe', 'p', '--arena', 'a', '--output', str(self.root/'run'),
                     '--frozen-list', str(self.frozen), '--profile', 'x1', '--delay-ms', '200', '--window-ms', '2000',
                     '--fault-at', '4.0']

    def execute(self, extra, relays):
        calls = []

        def case(options, mode, milliseconds):
            calls.append({'options': options, 'mode': mode, 'milliseconds': milliseconds,
                          'relay': action_probe.ActionRelay})
            relay = relays[len(calls)-1]
            case_dir = options.output/options.case_name
            case_dir.mkdir(parents=True)
            if relay is not None:
                (case_dir/'relay.json').write_text(json.dumps(relay))
            return {'passed': False, 'errors': ['Unexpected verdict'], 'artifacts': {}}
        with redirect_stdout(io.StringIO()):
            summary = runner.execute(runner.parse(self.argv+extra), case,
                                     repository=lambda: {'head': 'abc', 'tracked_changes': ''})
        return summary, calls

    def test_each_round_runs_run_case_with_the_delaying_relay_and_restores_it(self):
        profile = runner.Profile('x1', delay_ms=200, window_ms=2000)
        summary, calls = self.execute(['--fault-at', '8.0', '--rounds', '2'], [good_relay(profile)]*2)
        self.assertTrue(summary['completed'], summary['stopped'])
        self.assertEqual([(c['mode'], c['milliseconds'], c['options'].fault_at) for c in calls],
                         [('upstream', 2000, 4.0), ('upstream', 2000, 8.0)])
        for call in calls:
            self.assertTrue(issubclass(call['relay'], runner.DelayRelay))
            self.assertEqual(call['relay'].PROFILE.fields(), profile.fields())
        self.assertIs(action_probe.ActionRelay, runner.FROZEN_ACTION_RELAY)
        self.assertEqual(calls[0]['options'].case_name, 'clientdelay-x1-200ms-2000ms-at4.0-r1')
        record = summary['rounds'][0]
        self.assertEqual(record['run_case'], {'passed': False, 'errors': ['Unexpected verdict'], 'recorded_only': True})
        self.assertNotIn('samples', record['client_delay'])
        plan = json.loads((self.root/'run/plan.json').read_text())
        self.assertEqual((plan['delay_ms'], plan['window_ms'], plan['packets_per_second']), (200, 2000, 240))
        self.assertEqual(plan['frozen_list']['sha256'], runner.sha256(self.frozen))

    def test_an_unproven_or_impure_injection_stops_the_run(self):
        profile = runner.Profile('x1', delay_ms=200, window_ms=2000)
        missing = {'fault': {'start_ns': 1, 'release_ns': 2}, 'relay_error': None}
        unequal = good_relay(profile)
        unequal['client_delay']['received_equals_forwarded']['after'] = False
        near = good_relay(profile)
        near['client_delay']['queue_near_reached'] = True
        limited = good_relay(profile, limited=3)
        other = good_relay(runner.Profile('x1', delay_ms=150, window_ms=2000))
        late = good_relay(profile)
        late['client_delay']['lateness_ms']['p99'] = 12.0
        for relay, expected in ((missing, 'no client_delay'), (unequal, 'received != forwarded'),
                                (near, 'near the bound'), (limited, 'Gateway rate rejections 3'),
                                (other, 'delay_ms 150/150 differs'), (late, 'lateness p99 12.0')):
            with self.subTest(expected):
                for child in sorted(self.root.rglob('*'), reverse=True):
                    if child != self.frozen:
                        child.unlink() if child.is_file() else child.rmdir()
                summary, calls = self.execute(['--rounds', '2'], [relay])
                self.assertEqual(len(calls), 1)
                self.assertEqual([r['status'] for r in summary['rounds']], ['injection_invalid', 'not_run'])
                self.assertIn(expected, summary['stopped'])

    def test_a_frozen_mismatch_or_an_unlisted_relay_file_is_refused_before_anything_is_written(self):
        lines = self.frozen.read_text().splitlines()
        for content, expected in ((lines[0].replace(lines[0][:64], '0'*64)+'\n'+lines[1]+'\n', 'differs from the list'),
                                  (lines[1]+'\n', 'not in the frozen list')):
            with self.subTest(expected):
                self.frozen.write_text(content)
                with self.assertRaisesRegex(runner.Refused, expected):
                    self.execute([], [None])
                self.assertFalse((self.root/'run').exists())
        with mock.patch.dict(runner.FROZEN, {'impaired_network.py': '0'*64}):
            self.assertTrue(any('differs from FROZEN' in p for p in runner.frozen_problems(self.frozen)))

    def test_parameters_are_checked_before_anything_runs(self):
        for extra in (['--peak-ms', '100'], ['--delay-ms', '1100'], ['--packets-per-second', '1300'],
                      ['--fault-at', '11.0'], ['--rounds', '0']):
            with self.subTest(extra), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                runner.parse(self.argv+extra)
        ramp = [a for a in self.argv if a not in ('--delay-ms', '200', '--window-ms', '2000')]
        ramp[ramp.index('x1')] = 'ramp'
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            runner.parse(ramp+['--peak-ms', '250', '--rise-ms', '2000', '--fall-ms', '200'])
        self.assertEqual(runner.parse(ramp+['--peak-ms', '250', '--rise-ms', '2000', '--fall-ms', '250']).delay.fields(),
                         {'profile': 'ramp', 'peak_ms': 250, 'rise_ms': 2000, 'fall_ms': 250})

    def test_the_runner_is_not_a_gateway_launcher_for_test_service_startup(self):
        # test_service_startup.py lists every script here that builds a Gateway command ('--runtime') or calls
        # start('gateway...'); this runner reaches the Gateway only through the unchanged run_case.
        tree = ast.parse(Path(runner.__file__).read_text(encoding='utf-8'))
        self.assertFalse([n for n in ast.walk(tree) if isinstance(n, ast.Constant) and n.value == '--runtime'])
        self.assertFalse([n for n in ast.walk(tree) if isinstance(n, ast.Call) and ast.unparse(n.func) == 'start'])


if __name__ == '__main__':
    unittest.main()
