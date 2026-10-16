"""Product-owned source guard for D32 (pv7 contract §2): diagnostics never feed a decision.

The clock estimator and the other pv7 diagnostics live only in the I/O layer
(IpcHost, ClientConnection; the Gateway has its own Go guard,
TestClockStaysOutOfDecisions). The files that decide (the Match, the host, the
Client's prediction, timeline and shot gate) must name none of their symbols,
and MatchRuntimeHost's interface takes no diagnostic value. Each later pv7
batch adds the symbols of its diagnostics here.
"""
from pathlib import Path
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
SOURCE = REPOSITORY / 'apps/object_fps_pvp/src/Pvp'
INCLUDE = REPOSITORY / 'apps/object_fps_pvp/include/RetroFPS/Pvp'
DECIDING = [
    SOURCE / 'PvpMatch.cpp',
    SOURCE / 'LocalPlayerPrediction.cpp',
    SOURCE / 'MatchRuntimeHost.cpp',
    INCLUDE / 'MatchRuntimeHost.hpp',
    INCLUDE / 'SnapshotTimeline.hpp',
    SOURCE / 'PvpApplication.cpp',
    SOURCE / 'FireGate.cpp',  # until a1-shot-step retires it
]
DIAGNOSTIC_SYMBOLS = [
    'ClockSync', 'ClockSample', 'ClockStamp', 'ClockWindow', 'has_clock', 'mutable_clock', '.clock()',
]


class DiagnosticIsolationTest(unittest.TestCase):
    def test_deciding_files_name_no_diagnostic_symbol(self):
        for path in DECIDING:
            text = path.read_text(encoding='utf-8')
            for symbol in DIAGNOSTIC_SYMBOLS:
                with self.subTest(file=path.name, symbol=symbol):
                    self.assertNotIn(symbol, text, f'{path.relative_to(REPOSITORY)} names {symbol!r} (D32)')

    def test_the_estimator_stays_in_the_io_layer(self):
        users = sorted(path.name for path in [*SOURCE.glob('*.cpp'), *INCLUDE.glob('*.hpp')]
                       if 'ClockSync' in path.read_text(encoding='utf-8'))
        self.assertEqual(users, ['ClockSync.cpp', 'ClockSync.hpp', 'IpcHost.cpp'])


if __name__ == '__main__':
    unittest.main()
