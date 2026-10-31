"""Guard of the wire counter exhaustion sites (protocol-v8 section 2a, pv8-seq-ceiling).

Every counter that crosses the wire ends at WireCounterMax (the uint32 maximum)
although C++ keeps it in std::uint64_t. The boundary of the decision itself is
tested in WireTests.cpp ("PvP wire counters end at the uint32 maximum"); the
Match and the Client action ids cannot reach 2^32 in a test, so this guard
checks that each listed site decides with WireCounter.hpp and that no site
went back to the uint64 limit. pv8-seq-narrow: every setter of a narrowed
field at the two encoding boundaries goes through wire::Counter, whose Assert
WireTests.cpp tests. A new site changes the expected count here.
"""
from pathlib import Path
import re
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


# pv8-seq-narrow: the setters of the narrowed fields (protocol-v8 section 2a)
# at the two encoding boundaries, and how many each file has.
NARROWED_FIELDS = ('acknowledged_through', 'action_id', 'command_sequence', 'cooldown_ticks', 'last_attacker_id',
                   'last_damage_tick', 'last_resolved_command', 'last_shot_action_id', 'last_shot_tick',
                   'life_generation', 'life_state_tick', 'movement_epoch', 'movement_slack_sequence',
                   'next_allowed_shot_tick', 'observed_authority_tick', 'player_id', 'reload_action_id',
                   'reload_end_tick', 'reload_start_tick', 'reload_ticks', 'resolved_tick', 'respawn_tick',
                   'respawn_ticks', 'retired_through', 'sequence', 'target_id', 'target_life_generation', 'tick',
                   'jump_phase_tick')  # pv9 §4.3: PlayerAnimation's player_id, life_generation and jump_phase_tick
ENCODERS = {'src/Pvp/IpcHost.cpp': 33, 'src/Pvp/ClientConnection.cpp': 10}
SETTER = re.compile(r'\bset_(%s)\(' % '|'.join(NARROWED_FIELDS))


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

    def test_each_narrowed_setter_goes_through_wire_counter(self):
        for name, count in ENCODERS.items():
            text = (PRODUCT / name).read_text(encoding='utf-8')
            setters = list(SETTER.finditer(text))
            with self.subTest(file=name):
                self.assertEqual(len(setters), count)
                for setter in setters:
                    line = text.count('\n', 0, setter.start()) + 1
                    self.assertTrue(text.startswith('wire::Counter(', setter.end()), f'{name}:{line} {setter.group(0)}')


if __name__ == '__main__':
    unittest.main()
