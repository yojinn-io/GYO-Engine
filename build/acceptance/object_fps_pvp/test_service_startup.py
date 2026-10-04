"""Match/Gateway startup ordering: the Gateway starts only after the Match listens.

No sockets, services or real clocks: the Match is a fake process and its log a
temporary file; runner usage is checked on the runners' own source.
"""
import ast
from contextlib import nullcontext, redirect_stdout
import io
from pathlib import Path
import socket
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import run_network
import run_timing

ROOT = Path(__file__).resolve().parent
REPOSITORY = ROOT.parents[2]
# Every owner runner that starts a Match and a Gateway.
RUNNERS = ('action_probe.py', 'backpressure_probe.py', 'recovery_probe.py', 'run_action_short.py',
           'run_gameplay_gui.py', 'run_gameplay_soak.py',
           'run_native_window.py', 'run_network.py', 'run_player_short.py', 'run_timing.py')
LISTEN = '127.0.0.1:5000'


class FakeMatch:
    def __init__(self, exit_after=None, code=1):
        self.polls, self.exit_after, self.code, self.returncode = 0, exit_after, code, None

    def poll(self):
        self.polls += 1
        if self.exit_after is not None and self.polls > self.exit_after:
            self.returncode = self.code
        return self.returncode


class FakeClock:
    def __init__(self):
        self.now, self.sleeps = 100., []

    def __call__(self):
        return self.now

    def sleep(self, seconds):
        self.sleeps.append(seconds)
        self.now += seconds


def no_socket(*args, **kwargs):
    raise AssertionError('readiness must not connect to the Match')


class MatchReadyTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='pvp-match-ready-')
        self.addCleanup(temporary.cleanup)
        self.log = Path(temporary.name)/'match.log'
        self.clock = FakeClock()
        for name in ('socket', 'create_connection'):
            patcher = mock.patch.object(socket, name, side_effect=no_socket)
            patcher.start()
            self.addCleanup(patcher.stop)

    def wait(self, process, timeout=1., **kwargs):
        return run_network.wait_for_match_ready(process, self.log, LISTEN, timeout, clock=self.clock,
                                                sleep=kwargs.pop('sleep', self.clock.sleep), poll_seconds=.02)

    def test_returns_once_the_match_logs_its_listen_address(self):
        def sleep(seconds):
            self.clock.sleep(seconds)
            if len(self.clock.sleeps) == 3:
                self.log.write_text(f'Object_FPS_PVP Match ready: {LISTEN} arena=pvp authority=60Hz\n')
        # Absent at first, then empty, then ready: each is polled, never connected.
        self.assertIsNone(self.wait(FakeMatch(), sleep=sleep))
        self.assertEqual(len(self.clock.sleeps), 3)

    def test_only_the_exact_listen_address_counts(self):
        self.log.write_text('starting\nObject_FPS_PVP Match ready: 127.0.0.1:50001 arena=pvp\n'
                            'Object_FPS_PVP Match ready: 127.0.0.1:500 arena=pvp\n')
        with self.assertRaisesRegex(RuntimeError, f'did not report listening on {LISTEN} within 1 s'):
            self.wait(FakeMatch())
        self.assertTrue(run_network.match_ready(f'x\nObject_FPS_PVP Match ready: {LISTEN}\n', LISTEN))
        self.assertFalse(run_network.match_ready(f'Error: Object_FPS_PVP Match ready: {LISTEN} arena=a', LISTEN))

    def test_a_match_that_exits_first_fails_without_waiting_out_the_timeout(self):
        self.log.write_text('IPC port must be a decimal integer in [1, 65535]\n')
        process = FakeMatch(exit_after=2, code=1)
        with self.assertRaisesRegex(RuntimeError, 'Match exited with code 1 before it listened on 127.0.0.1:5000; inspect match.log'):
            self.wait(process, timeout=60)
        self.assertEqual(len(self.clock.sleeps), 2)

    def test_timeout_is_bounded(self):
        with self.assertRaisesRegex(RuntimeError, 'within 0.5 s'):
            self.wait(FakeMatch(), timeout=.5)
        self.assertLessEqual(sum(self.clock.sleeps), .5 + .02 + 1e-9)
        self.assertEqual(run_network.MATCH_READY_TIMEOUT_SECONDS, 15.)

    def test_ready_line_is_the_one_the_match_prints(self):
        source = (REPOSITORY/'apps/object_fps_pvp/match_main.cpp').read_text(encoding='utf-8')
        printed = source.index(f'"{run_network.MATCH_READY_PREFIX}"<<listen<<" arena="')
        # Printed only after the IPC listener started successfully.
        self.assertLess(source.index('ipc.Start(listen,error)'), printed)


def _passes_runtime(node):
    return any(isinstance(item, ast.Constant) and item.value == '--runtime' for item in ast.walk(node))


def _starts(function, label):
    """Each runner's local start(label, command) launches and logs one service as <label>.log."""
    return [node for node in ast.walk(function) if isinstance(node, ast.Call) and ast.unparse(node.func) == 'start' and
            node.args and isinstance(node.args[0], ast.Constant) and label(str(node.args[0].value))]


def _gateway_starts(function):
    return _starts(function, lambda value: value.startswith('gateway'))


def _match_starts(function):
    return _starts(function, lambda value: value == 'match')


class RunnerUsageTests(unittest.TestCase):
    def test_every_match_and_gateway_runner_waits_for_the_match_first(self):
        found, launchers = set(), set()
        for path in sorted(ROOT.glob('*.py')):
            if path.name.startswith('test_'):
                continue
            tree = ast.parse(path.read_text(encoding='utf-8'))
            if _passes_runtime(tree):
                launchers.add(path.name)  # Any file that builds a Gateway command must be covered below.
            for function in (node for node in ast.walk(tree) if isinstance(node, ast.FunctionDef)):
                gateways = _gateway_starts(function)
                if not gateways:
                    continue
                found.add(path.name)
                with self.subTest(runner=path.name, function=function.name):
                    matches = _match_starts(function)
                    self.assertEqual(len(matches), 1, 'one Match start per Gateway-starting function')
                    waits = [node for node in ast.walk(function) if isinstance(node, ast.Call) and
                             ast.unparse(node.func) == 'wait_for_match_ready']
                    self.assertEqual(len(waits), 1)
                    wait, match = waits[0], matches[0]
                    first_gateway = min(node.lineno for node in gateways)
                    self.assertLess(match.lineno, wait.lineno)
                    self.assertLess(wait.lineno, first_gateway)
                    # The waited-for process is the started Match, its log and its own listen address.
                    assigned = [node for node in ast.walk(function) if isinstance(node, ast.Assign) and node.value is match]
                    self.assertEqual(ast.unparse(wait.args[0]), ast.unparse(assigned[0].targets[0]))
                    self.assertIn("'match.log'", ast.unparse(wait.args[1]).replace('"', "'"))
                    listen = ast.unparse(match.args[1])
                    self.assertIn(ast.unparse(wait.args[2]).replace('"', "'"), listen.replace('"', "'"))
        self.assertEqual(found, set(RUNNERS))
        self.assertEqual(launchers, set(RUNNERS))

    def test_run_round_never_starts_the_gateway_when_the_match_is_not_ready(self):
        temporary = tempfile.TemporaryDirectory(prefix='pvp-startup-order-')
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        for name in ('match', 'gateway', 'probe', 'arena'):
            (root/name).write_text(name)
        launched, stopped = [], []

        class Process:
            def __init__(self, command, stdout=None, stderr=None):
                self.command, self.returncode = [str(part) for part in command], None
                launched.append(self.command[0])

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                return self.returncode

            def terminate(self):
                stopped.append(self.command[0])
                self.returncode = 0

            def kill(self):
                self.returncode = -9

        args = SimpleNamespace(output=root/'run', rounds=1, fps=60, duration=16., events=20, gui=True, combat=False,
                               short=True, soak=False, report_only=True, stall_ms=0,
                               match=root/'match', gateway=root/'gateway', probe=root/'probe', arena=root/'arena')
        refused = RuntimeError('Match exited with code 1 before it listened on 127.0.0.1:5000; inspect match.log')
        with mock.patch.object(run_timing.subprocess, 'Popen', Process), \
                mock.patch.object(run_timing, 'free_port', return_value=5000), \
                mock.patch.object(run_timing, 'wait_for_match_ready', side_effect=refused) as ready, \
                mock.patch.object(run_timing.urllib.request, 'urlopen', return_value=nullcontext()), \
                redirect_stdout(io.StringIO()):
            self.assertEqual(run_timing.execute_rounds(args), 1)
        self.assertEqual(launched, [str(root/'match')])
        self.assertEqual(stopped, [str(root/'match')])
        process, log, listen = ready.call_args.args
        self.assertEqual((process.command[0], log, listen), (str(root/'match'), root/'run/round-1/match.log', LISTEN))
        self.assertIn('before it listened', (root/'run/round-1/runner-failure.txt').read_text())


if __name__ == '__main__':
    unittest.main()
