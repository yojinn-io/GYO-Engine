"""Product-owned source guard for D32 (pv7 contract §2): diagnostics never feed a decision.

The clock estimator and the other pv7 diagnostics live only in the I/O layer
(IpcHost, ClientConnection; the Gateway has its own Go guard,
TestClockStaysOutOfDecisions). The files that decide (the Match, the host, the
Client's prediction, timeline and shot gate) must name none of their symbols,
and MatchRuntimeHost's interface takes no diagnostic value. Each later pv7
batch adds the symbols of its diagnostics here.
"""
from pathlib import Path
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
SOURCE = REPOSITORY / 'apps/object_fps_pvp/src/Pvp'
INCLUDE = REPOSITORY / 'apps/object_fps_pvp/include/RetroFPS/Pvp'
DECIDING = [
    SOURCE / 'PvpMatch.cpp',
    SOURCE / 'LocalPlayerPrediction.cpp',
    SOURCE / 'MatchRuntimeHost.cpp',
    INCLUDE / 'MatchRuntimeHost.hpp',
    INCLUDE / 'MatchSubstitutionRecords.hpp',
    INCLUDE / 'SnapshotTimeline.hpp',
    # v7.1 outage-resync client-interrupted: the Client's interruption and timeout
    # decide from arrival times only (network-architecture §4).
    INCLUDE / 'ConnectionLiveness.hpp',
    SOURCE / 'PvpApplication.cpp',
    SOURCE / 'ShotGate.cpp',
]
DIAGNOSTIC_SYMBOLS = [
    'ClockSync', 'ClockSample', 'ClockStamp', 'ClockWindow', 'has_clock', 'mutable_clock', '.clock()',
    'GatewayDropReport', 'DropReason', 'DropReports', 'drop_report',
    # cause-records (pv7 §5): dwell, the Client's sequence, both holds, the
    # refusal reply and the Leave reason.
    'gateway_dwell_us', 'client_transport_sequence', 'match_hold_us', 'gateway_hold_us', 'refused_batches',
    'last_refusal', 'ActionRefusal', 'LeaveReason', 'CauseRecords',
]


class DiagnosticIsolationTest(unittest.TestCase):
    def test_deciding_files_name_no_diagnostic_symbol(self):
        for path in DECIDING:
            text = path.read_text(encoding='utf-8')
            for symbol in DIAGNOSTIC_SYMBOLS:
                with self.subTest(file=path.name, symbol=symbol):
                    self.assertNotIn(symbol, text, f'{path.relative_to(REPOSITORY)} names {symbol!r} (D32)')

    def test_phase_tracking_learns_of_a_resume_only_by_the_drain_flag(self):
        # network-architecture §7 (連線中斷與重新對時): the interruption state
        # belongs to the network worker; phase tracking reads neither it nor its
        # duration, and decides no gap of its own.
        for path in [SOURCE / 'LocalPlayerPrediction.cpp', INCLUDE / 'LocalPlayerPrediction.hpp',
                     SOURCE / 'ClientSimulation.cpp', INCLUDE / 'ClientSimulation.hpp', SOURCE / 'ClientSimulationRole.cpp']:
            text = path.read_text(encoding='utf-8')
            for symbol in ('.interrupted', 'interruptedSince', 'ConnectionLiveness', 'ConnectionInterruptedAfter',
                           'ConnectionTimeoutAfter'):
                with self.subTest(file=path.name, symbol=symbol):
                    self.assertNotIn(symbol, text, f'{path.relative_to(REPOSITORY)} names {symbol!r}')

    def test_the_resume_flag_is_raised_only_with_its_snapshot_under_one_lock(self):
        # §4 recovery step 3: no simulation batch holds the resuming snapshot
        # without the flag. ClientSimulationHistory raises the flag only in
        # Push, with the snapshot; the worker pushes once, passing the liveness
        # arrival's resume, inside the lock it took for that snapshot with no
        # other lock section in between.
        history = (INCLUDE / 'ClientSimulationHistory.hpp').read_text(encoding='utf-8')
        self.assertEqual(history.count('resumed_ = resumed_ || resumes;'), 1)
        self.assertEqual(len(re.findall(r'resumed_ = (?!resumed_ \|\| resumes;)', history)), 1)  # Clear only
        text = (SOURCE / 'ClientConnection.cpp').read_text(encoding='utf-8')
        self.assertEqual(text.count('simulationHistory.Push('), 1)
        push = text.index('simulationHistory.Push({snapshot,arrivedAt},arrival.resumed)')
        arrival = text.index('const auto arrival=liveness.Snapshot(arrivedAt);')
        lock = text.rindex('std::scoped_lock lock(mutex);', 0, arrival)
        self.assertNotIn('scoped_lock', text[lock + len('std::scoped_lock'):push])

    def test_liveness_reads_only_arrival_times(self):
        # §7 (連線中斷與重新對時): the worker passes ConnectionLiveness the
        # steady_clock it read when a datagram arrived, or now; never a time
        # derived from the clock echo. Every call's argument is one of these.
        text = (SOURCE / 'ClientConnection.cpp').read_text(encoding='utf-8')
        calls = []
        for match in re.finditer(r'\bliveness\.(\w+)\(', text):
            depth, end = 1, match.end()
            while depth:
                depth += {'(': 1, ')': -1}.get(text[end], 0)
                end += 1
            calls.append((match.group(1), text[match.end():end - 1]))
        # 11 since v7.1 D188: the Hello interval no longer reads Interrupted() (D189).
        self.assertEqual(len(calls), 11)
        allowed = {'Begin': {'Clock::now()'}, 'Welcome': {'arrivedAt'}, 'Snapshot': {'arrivedAt'}, 'Poll': {'polledAt'},
                   'Interrupted': {''}, 'Playing': {''}, 'Since': {''}, 'NextDeadline': {''}}
        for method, argument in calls:
            with self.subTest(method=method, argument=argument):
                self.assertIn(method, allowed)
                self.assertIn(argument, allowed[method])
        self.assertIn('const auto polledAt=Clock::now();\n        const auto event=liveness.Poll(polledAt);', text)
        for arrival in re.findall(r'const auto (arrivedAt)=([^;]*);', text):
            self.assertEqual(arrival[1], 'Clock::now()')

    def test_every_socket_error_is_classified_before_it_fails_the_session(self):
        # network-architecture §4 (D172 D-7): at both places where a socket
        # error ends the session, the transient check comes first.
        text = (SOURCE / 'ClientConnection.cpp').read_text(encoding='utf-8')
        throws = [m.start() for m in re.finditer(r'throw std::runtime_error\(error\.message\(\)\)', text)]
        self.assertEqual(len(throws), 2)
        for at in throws:
            before = text[text.rindex('\n', 0, text.rindex('\n', 0, at)):at]
            self.assertIn('TransientSocketError(error)', before)

    def test_no_decision_reads_the_view_tick(self):
        # pv7 §7: the view tick is request content the host only records
        # (MatchShotTrace.hpp). E and S became decision inputs in a1-shot-step
        # (pv7 §6, A1), so they left this list.
        for path in [SOURCE / 'PvpMatch.cpp', SOURCE / 'MatchRuntimeHost.cpp', INCLUDE / 'MatchRuntimeHost.hpp',
                     SOURCE / 'ShotGate.cpp', SOURCE / 'LocalPlayerPrediction.cpp', INCLUDE / 'SnapshotTimeline.hpp']:
            text = path.read_text(encoding='utf-8')
            for symbol in ('viewTickQ16', 'view_tick'):
                with self.subTest(file=path.name, symbol=symbol):
                    self.assertNotIn(symbol, text, f'{path.relative_to(REPOSITORY)} names {symbol!r} (D-2)')

    def test_the_estimator_stays_in_the_io_layer(self):
        users = sorted(path.name for path in [*SOURCE.glob('*.cpp'), *INCLUDE.glob('*.hpp')]
                       if 'ClockSync' in path.read_text(encoding='utf-8'))
        self.assertEqual(users, ['ClientConnection.cpp', 'ClockSync.hpp', 'IpcHost.cpp'])

    def test_the_host_only_records_its_lock_waits(self):
        # edge-input-passthrough: the host times its own mutex for the L2 load
        # comparison. Like its tick wake statistics, it only records them for
        # TakeStatistics; nothing it decides reads them.
        lines = [line.strip() for line in (SOURCE / 'MatchRuntimeHost.cpp').read_text(encoding='utf-8').splitlines()
                 if 'lockWaits' in line]
        self.assertEqual(len(lines), 2)
        for line in lines:
            with self.subTest(line=line):
                self.assertRegex(line, r'^statistics_\.lockWaits\.(input|step)\.Record\(')


    def test_the_substitution_records_read_no_ledger(self):
        # v7.1 outage-resync (network-architecture §7 "被替代序號的單一紀錄"): the
        # records decide slack samples; the ingress ledger only reads them, never
        # the other way. The records' header names nothing of the ledger or of the
        # ingress statistics, and does not include them.
        text = (INCLUDE / 'MatchSubstitutionRecords.hpp').read_text(encoding='utf-8')
        for pattern in (r'MatchIngressLedger', r'IngressStatistics\.hpp', r'IngressCommandClass', r'MatchIngressCounts',
                        r'IngressName\b', r'IngressSubstitution(?!Close)', r'Classify\b', r'annex'):
            with self.subTest(pattern=pattern):
                self.assertIsNone(re.search(pattern, text), f'MatchSubstitutionRecords.hpp names {pattern!r}')

if __name__ == '__main__':
    unittest.main()
