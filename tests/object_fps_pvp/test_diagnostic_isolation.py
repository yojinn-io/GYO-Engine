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

    def test_the_resume_flag_is_set_with_its_snapshot_under_one_lock(self):
        # §4 recovery step 3: no simulation batch holds the resuming snapshot
        # without the flag. The flag is set after the lock that also pushes the
        # snapshot into the simulation history was taken, with no other lock
        # section in between.
        text = (SOURCE / 'ClientConnection.cpp').read_text(encoding='utf-8')
        self.assertEqual(text.count('simulationResumed=true'), 1)
        flag = text.index('simulationResumed=true')
        push = text.index('simulationSnapshots.push_back')
        lock = text.rindex('std::scoped_lock lock(mutex);', 0, flag)
        self.assertLess(flag, push)
        self.assertNotIn('scoped_lock', text[lock + len('std::scoped_lock'):push])

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
