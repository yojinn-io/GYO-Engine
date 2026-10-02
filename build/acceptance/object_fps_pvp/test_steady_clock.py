"""Python stamps compared with C++ steady_clock traces must share its clock domain."""
import ast
from pathlib import Path
import threading
import time
import unittest
from unittest.mock import patch

import run_network

ROOT = Path(__file__).resolve().parent
# Every cross-domain stamp, per function: (kind, target) -> occurrences. "assign"
# is an assignment target, "key" a dict-literal key, "compare" a comparison whose
# other operand contains the target. Each occurrence must read
# run_network.steady_clock_ns, so reverting any single one fails.
SITES = {
    'action_probe.py': {
        'ActionRelay.arm': {('assign', "self.fault['start_ns']"): 1},
        'ActionRelay._send': {('key', 'time_ns'): 1},
        'ActionRelay._release': {('assign', "self.fault['release_ns']"): 3},
        'ActionRelay._receive': {('assign', 'now'): 1},
        'run_case': {('compare', "ready['start_ns']"): 1, ('assign', "fault['start_ns']"): 1,
                     ('assign', "fault['release_ns']"): 1},
    },
    'backpressure_probe.py': {
        'IpcPause._accept': {('assign', "self.stats['start_ns']"): 1, ('assign', "self.stats['release_ns']"): 1},
        'DownstreamPause._receive': {('assign', "self.fault['start_ns']"): 1, ('assign', "self.fault['release_ns']"): 1},
        'run_case': {('key', 'start_ns'): 1, ('assign', "fault['release_ns']"): 1},
    },
    'run_player_short.py': {
        'SnapshotHold._receive': {('assign', 'now'): 1},
    },
}
# Functions whose whole body compares with C++ host_steady_seconds.
NO_PYTHON_MONOTONIC = {('run_player_short.py', 'SnapshotHold._receive')}


def _functions(tree):
    found = {}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            found[node.name] = node
        elif isinstance(node, ast.ClassDef):
            for item in node.body:
                if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    found[f'{node.name}.{item.name}'] = item
    return found


def _calls(node, text):
    return [call for call in ast.walk(node) if isinstance(call, ast.Call) and ast.unparse(call.func) == text]


def _site_values(function, site):
    """Value expressions at every occurrence of ``site`` in ``function``."""
    kind, target = site
    values = []
    for node in ast.walk(function):
        if kind == 'assign' and isinstance(node, ast.Assign):
            values += [node.value for item in node.targets if ast.unparse(item) == target]
        elif kind == 'key' and isinstance(node, ast.Dict):
            values += [value for key, value in zip(node.keys, node.values)
                       if isinstance(key, ast.Constant) and key.value == target]
        elif kind == 'compare' and isinstance(node, ast.Compare):
            operands = [node.left, *node.comparators]
            if any(target in ast.unparse(operand) for operand in operands):
                values += [operand for operand in operands if target not in ast.unparse(operand)]
    return values


def _reads_steady_clock(value):
    return bool(_calls(value, 'steady_clock_ns')) and not (_calls(value, 'time.monotonic') or _calls(value, 'time.monotonic_ns'))


class SteadyClockTests(unittest.TestCase):
    def test_darwin_reads_clock_monotonic_raw_like_apple_libcxx(self):
        with patch.object(time, 'CLOCK_MONOTONIC_RAW', 4, create=True), \
                patch.object(time, 'clock_gettime_ns', return_value=123, create=True) as raw, \
                patch.object(time, 'monotonic_ns', side_effect=AssertionError('macOS monotonic excludes sleep')):
            self.assertEqual(run_network.steady_clock_source('darwin')(), 123)
        raw.assert_called_once_with(4)

    def test_linux_and_other_platforms_keep_python_monotonic(self):
        # Linux libstdc++ steady_clock and Python monotonic both read CLOCK_MONOTONIC.
        for system in ('linux', 'win32'):
            with self.subTest(system=system):
                self.assertIs(run_network.steady_clock_source(system), time.monotonic_ns)

    def test_module_clock_is_this_platform_selection(self):
        selected = run_network.steady_clock_source()
        first, second, third = run_network.steady_clock_ns(), selected(), run_network.steady_clock_ns()
        self.assertLessEqual(first, second)
        self.assertLessEqual(second, third)

    def test_every_cross_domain_site_reads_the_steady_clock(self):
        for name, functions in SITES.items():
            tree = ast.parse((ROOT/name).read_text(encoding='utf-8'), name)
            self.assertFalse(_calls(tree, 'time.monotonic_ns'), f'{name}: time.monotonic_ns is not the C++ steady clock')
            defined = _functions(tree)
            for qualified, expected in functions.items():
                function = defined.get(qualified)
                self.assertIsNotNone(function, f'{name}: {qualified} not found')
                for site, count in expected.items():
                    with self.subTest(module=name, function=qualified, site=site):
                        values = _site_values(function, site)
                        self.assertEqual(len(values), count, f'expected {count} occurrence(s)')
                        for value in values:
                            self.assertTrue(_reads_steady_clock(value), ast.unparse(value))
                if (name, qualified) in NO_PYTHON_MONOTONIC:
                    self.assertFalse(_calls(function, 'time.monotonic'), f'{qualified} must not read time.monotonic()')

    def test_the_guard_detects_any_single_reverted_site(self):
        for name, functions in SITES.items():
            source = (ROOT/name).read_text(encoding='utf-8')
            tree = ast.parse(source, name)
            defined = _functions(tree)
            for qualified, expected in functions.items():
                for site in expected:
                    for value in _site_values(defined[qualified], site):
                        call = _calls(value, 'steady_clock_ns')[0]
                        with self.subTest(module=name, function=qualified, site=site, line=call.lineno):
                            # Revert exactly this call, by position, to Python's monotonic clock.
                            lines = source.splitlines(keepends=True)
                            line = lines[call.lineno-1]
                            lines[call.lineno-1] = (line[:call.col_offset] + 'time.monotonic_ns()' +
                                                    line[call.end_col_offset:])
                            reverted = _functions(ast.parse(''.join(lines)))[qualified]
                            self.assertFalse(all(_reads_steady_clock(item) for item in _site_values(reverted, site)))

    def test_harness_modules_use_the_run_network_clock(self):
        import action_probe
        import backpressure_probe
        import run_player_short
        for module in (action_probe, backpressure_probe, run_player_short):
            with self.subTest(module=module.__name__):
                self.assertIs(module.steady_clock_ns, run_network.steady_clock_ns)

    def test_snapshot_hold_window_is_judged_on_the_steady_clock(self):
        import run_player_short

        class Socket:
            def __init__(self):
                self.sent = []

            def sendto(self, payload, destination):
                self.sent.append(destination)
        hold = run_player_short.SnapshotHold.__new__(run_player_short.SnapshotHold)
        hold.lock, hold.udp, hold.upstream_udp = threading.Lock(), Socket(), ('127.0.0.1', 1)
        hold.clients, hold.session_order, hold.dropped = {7: ('127.0.0.1', 2), 8: ('127.0.0.1', 3)}, [7, 8], []
        hold.stats, hold.forwarded_snapshots = {'unroutable': 0}, 0
        hold.hold_start, hold.hold_end = 100.0, 100.3  # C++ host_steady_seconds of the GUI probe.
        snapshot = bytes(6) + (4).to_bytes(2, 'big') + (8).to_bytes(8, 'big') + bytes(8)
        with patch.object(run_player_short, 'steady_clock_ns', return_value=100_100_000_000), \
                patch.object(time, 'monotonic', side_effect=AssertionError('Python monotonic is another clock domain')):
            hold._receive(snapshot, hold.upstream_udp)
        self.assertEqual(hold.dropped, [{'steady_seconds': 100.1, 'bytes': len(snapshot)}])
        with patch.object(run_player_short, 'steady_clock_ns', return_value=100_400_000_000):
            hold._receive(snapshot, hold.upstream_udp)
        self.assertEqual((len(hold.dropped), hold.udp.sent), (1, [('127.0.0.1', 3)]))

    def test_action_relay_event_stamps_use_the_steady_clock(self):
        import action_probe

        class Socket:
            def sendto(self, payload, destination):
                pass
        relay = action_probe.ActionRelay.__new__(action_probe.ActionRelay)
        relay.mode, relay.udp, relay.events = 'clean', Socket(), []
        with patch.object(action_probe, 'steady_clock_ns', return_value=42), \
                patch.object(time, 'monotonic_ns', side_effect=AssertionError('Python monotonic is another clock domain')):
            relay._send(bytes(24), ('127.0.0.1', 1), False)
        self.assertEqual(relay.events[0]['time_ns'], 42)


if __name__ == '__main__':
    unittest.main()
