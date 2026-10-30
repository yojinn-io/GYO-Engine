"""Guard of the wire counter exhaustion sites (protocol-v8 section 2a, pv8-seq-ceiling).

Every counter that crosses the wire ends at WireCounterMax (the uint32 maximum)
although C++ keeps it in std::uint64_t. The boundary of the decision itself is
tested in WireTests.cpp ("PvP wire counters end at the uint32 maximum"); the
Match and the Client action ids cannot reach 2^32 in a test, so this guard
checks that each listed site decides with WireCounter.hpp and that no site
went back to the uint64 limit. A new site changes the expected count here.
"""
from pathlib import Path
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
PRODUCT = REPOSITORY / 'apps/object_fps_pvp'

# file: {call: count}. The table in protocol-v8 section 2a names each site.
SITES = {
    'src/Pvp/PvpMatch.cpp': {'WireCounterExhausted(': 7, 'WireCounterAdd(': 1},
    'src/Pvp/LocalPlayerPrediction.cpp': {'WireCounterExhausted(': 3},
    'src/Pvp/ClientConnection.cpp': {'WireCounterExhausted(': 1},
    'gateway/server.go': {'s.nextPlayer >= math.MaxUint32': 1},
}
FORBIDDEN = ('numeric_limits<std::uint64_t>', 'numeric_limits<ActionId>', 'numeric_limits<PlayerId>',
             '^uint64(0)', 'math.MaxUint64', 'UINT64_MAX')


class WireCounterSitesTest(unittest.TestCase):
    def test_each_site_decides_with_the_wire_counter(self):
        for name, calls in SITES.items():
            text = (PRODUCT / name).read_text(encoding='utf-8')
            for call, count in calls.items():
                with self.subTest(file=name, call=call):
                    self.assertEqual(text.count(call), count)

    def test_no_site_uses_the_uint64_limit(self):
        for name in SITES:
            text = (PRODUCT / name).read_text(encoding='utf-8')
            for limit in FORBIDDEN:
                with self.subTest(file=name, limit=limit):
                    self.assertNotIn(limit, text)


if __name__ == '__main__':
    unittest.main()
