"""Outage runner tests (v7.1 outage-resync resync-l2-l3).

Deterministic: the relay runs on a fake clock; its sockets and threads are never created (only
_ImpairedGateway.__init__ is replaced; ActionRelay.__init__ runs unchanged) and its sockets are recorders.
"""
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest import mock

import action_probe
import impaired_network
import run_outage as runner

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


class Recorder:
    def __init__(self, log, port):
        self.log, self.port = log, port

    def sendto(self, payload, destination):
        self.log.append((self.port, payload, destination))

    def getsockname(self):
        return ('127.0.0.1', self.port)

    def close(self):
        pass


class ProfileTests(unittest.TestCase):
    def test_each_kind_cuts_only_its_directions(self):
        cuts = {kind: (runner.Profile(kind, 1000).cuts('upstream'), runner.Profile(kind, 1000).cuts('downstream'))
                for kind in runner.KINDS}
        self.assertEqual(cuts, {'both': (True, True), 'uplink': (True, False), 'downlink': (False, True),
                                'partial': (False, False), 'port': (True, True), 'none': (False, False)})

    def test_arguments_are_checked_before_anything_runs(self):
        self.assertEqual(runner.profile_problems(runner.Profile('both', 1000)), [])
        self.assertTrue(runner.profile_problems(runner.Profile('both', 0)))
        self.assertTrue(runner.profile_problems(runner.Profile('partial', 1000)))
        self.assertTrue(runner.profile_problems(runner.Profile('uplink', 1000, period=3)))
        self.assertEqual(runner.profile_problems(runner.Profile('partial', 1000, period=3, late_ms=3000)), [])


class RelayTests(unittest.TestCase):
    def make(self, profile):
        log = []

        def base(relay, http, udp, rtt):
            relay.upstream_udp, relay.lock, relay.clients = GATEWAY, threading.Lock(), {}
            relay.error, relay.udp, relay.stop = None, Recorder(log, 30000), threading.Event()
        clock = Clock()
        ports = iter(range(31000, 32000))
        cls = type('Relay', (runner.outage_relay(profile),),
                   {'CLOCK': staticmethod(clock), 'MOVED_SOCKET': staticmethod(lambda: Recorder(log, next(ports)))})
        with mock.patch.object(impaired_network._ImpairedGateway, '__init__', base):
            relay = cls(1, 2, runner.DISPATCH_MODE)
        return relay, clock, log

    def step(self, relay, clock, until):
        while clock.now < until:
            clock.now = round(clock.now+.001, 6)
            with relay.lock:
                relay._release()

    def test_both_drops_everything_in_the_window_only_and_conserves(self):
        relay, clock, log = self.make(runner.Profile('both', 500))
        relay._receive(packet(2, 1, 1), CLIENTS[1])
        relay._receive(packet(4, 1, 1), GATEWAY)
        relay.arm('upstream', .5)
        relay._receive(packet(3, 1, 2), CLIENTS[1])
        relay._receive(packet(4, 1, 2), GATEWAY)
        self.assertEqual(len(log), 2)
        self.step(relay, clock, 100.501)
        relay._receive(packet(3, 1, 3), CLIENTS[1])
        relay._receive(packet(4, 1, 3), GATEWAY)
        self.assertEqual(len(log), 4)
        outage = relay.evidence()['outage']
        self.assertTrue(outage['conserved'])
        self.assertEqual(outage['counts']['upstream'], {'received': 3, 'forwarded': 2, 'dropped': 1, 'late': 0})
        self.assertEqual(outage['counts']['downstream'], {'received': 3, 'forwarded': 2, 'dropped': 1, 'late': 0})
        evidence = relay.evidence()
        self.assertEqual(runner.round_problems(evidence, runner.Profile('both', 500)), [])

    def test_uplink_and_downlink_cut_one_direction(self):
        for kind, kept in (('uplink', CLIENTS[1][1]), ('downlink', GATEWAY[1])):
            with self.subTest(kind=kind):
                relay, clock, log = self.make(runner.Profile(kind, 500))
                relay._receive(packet(2, 1, 1), CLIENTS[1])
                relay.arm('upstream', .5)
                relay._receive(packet(3, 1, 2), CLIENTS[1])
                relay._receive(packet(4, 1, 2), GATEWAY)
                self.assertEqual([destination[1] for _, _, destination in log[1:]], [kept])
                self.step(relay, clock, 100.501)
                self.assertEqual(runner.round_problems(relay.evidence(), runner.Profile(kind, 500)), [])

    def test_partial_passes_one_in_period_and_delivers_the_rest_late(self):
        profile = runner.Profile('partial', 500, period=3, late_ms=3000)
        relay, clock, log = self.make(profile)
        relay._receive(packet(2, 1, 1), CLIENTS[1])
        relay.arm('upstream', .5)
        for sequence in range(2, 8):
            relay._receive(packet(3, 1, sequence), CLIENTS[1])
        self.assertEqual([int.from_bytes(p[16:20], 'big') for _, p, _ in log], [1, 2, 5])
        self.step(relay, clock, 102.999)
        self.assertEqual(len(log), 3)
        self.step(relay, clock, 103.0)
        self.assertEqual(len(log), 7)
        self.assertEqual(runner.round_problems(relay.evidence(), profile), [])

    def test_port_moves_every_session_to_a_new_source_port_after_the_window(self):
        profile = runner.Profile('port', 600)
        relay, clock, log = self.make(profile)
        for session in (1, 2):
            relay._receive(packet(2, session, 1), CLIENTS[session])
            relay.players[session] = session
        relay.arm('upstream', .6)
        relay._receive(packet(2, 1, 2), CLIENTS[1])
        relay._receive(packet(5, 1, 1), GATEWAY)
        self.step(relay, clock, 100.601)
        relay._receive(packet(2, 1, 3), CLIENTS[1])
        relay._receive(packet(2, 2, 2), CLIENTS[2])
        relay._receive(packet(5, 1, 2), GATEWAY)
        ports = [port for port, _, _ in log]
        self.assertEqual(ports[:2], [30000, 30000])
        self.assertEqual(sorted(ports[2:4]), [31000, 31001])
        self.assertEqual(log[-1][2], CLIENTS[1])  # downstream still reaches the Client
        evidence = relay.evidence()
        self.assertEqual(set(evidence['outage']['moves']), {'1', '2'})
        self.assertEqual(runner.round_problems(evidence, profile), [])

    def test_round_problems_reject_a_missing_or_impure_injection(self):
        profile = runner.Profile('uplink', 500)
        self.assertTrue(runner.round_problems({}, profile))
        relay, clock, log = self.make(profile)
        relay._receive(packet(2, 1, 1), CLIENTS[1])
        relay.arm('upstream', .5)
        self.step(relay, clock, 100.501)
        problems = runner.round_problems(relay.evidence(), profile)
        self.assertTrue(any('no upstream datagram was cut' in p for p in problems))
        evidence = relay.evidence()
        evidence['outage']['counts']['downstream']['dropped'] = 1
        self.assertTrue(any('keeps them' in p for p in runner.round_problems(evidence, profile)))


class RunnerTests(unittest.TestCase):
    def test_the_relay_is_replaced_for_one_round_only(self):
        original = action_probe.ActionRelay
        with runner.replaced_action_relay(runner.Profile('both', 500)):
            self.assertTrue(issubclass(action_probe.ActionRelay, runner.OutageRelay))
        self.assertIs(action_probe.ActionRelay, original)

    def test_a_round_with_an_invalid_injection_stops_the_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            args = runner.parse(['--match', 'm', '--gateway', 'g', '--probe', 'p', '--arena', 'a', '--output',
                                 str(root/'out'), '--frozen-list', 'f', '--kind', 'both', '--duration-ms', '500',
                                 '--fault-at', '6', '--rounds', '2'])

            def case(options, mode, milliseconds):
                (root/'out'/options.case_name).mkdir(parents=True)
                (root/'out'/options.case_name/'relay.json').write_text(json.dumps({}))
                return {'passed': True, 'artifacts': {}}
            with mock.patch.object(runner.run_client_delay, 'frozen_problems', return_value=[]), \
                    mock.patch.object(runner.run_client_delay, 'sha256', return_value='0'), \
                    redirect_stdout(io.StringIO()):
                summary = runner.execute(args, case=case, repository=lambda: {'head': 'x'})
            self.assertEqual([r['status'] for r in summary['rounds']], ['injection_invalid', 'not_run'])
            self.assertFalse(summary['completed'])


if __name__ == '__main__':
    unittest.main()
