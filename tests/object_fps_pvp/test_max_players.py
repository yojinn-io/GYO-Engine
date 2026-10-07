"""Product-owned check that every room capacity definition agrees (pv6 contract §1, 2026-10-07 revision).

The product C++ constant, the product Go adapter constant and the acceptance
C++ and Python expected values are read as source text, so the check needs no
Go toolchain. Each owner keeps exactly one definition; product code refers to it
and writes no capacity literal.
"""
from pathlib import Path
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
DEFINITIONS = {
    'product C++': (REPOSITORY / 'apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp',
                    r'^inline constexpr std::uint32_t MaxPlayers = (\d+);$'),
    'product Go': (REPOSITORY / 'apps/object_fps_pvp/gateway/adapter/adapter.go',
                   r'^const MaxPlayers = (\d+)$'),
    'acceptance C++': (REPOSITORY / 'build/acceptance/object_fps_pvp/acceptance_capacity.hpp',
                       r'^inline constexpr unsigned AcceptanceMaxPlayers = (\d+);$'),
    'acceptance Python': (REPOSITORY / 'build/acceptance/object_fps_pvp/acceptance_capacity.py',
                          r'^MAX_PLAYERS = (\d+)$'),
}


def capacity(path, pattern):
    found = re.findall(pattern, path.read_text(encoding='utf-8'), re.MULTILINE)
    if len(found) != 1:
        raise AssertionError(f'{path}: expected one definition, found {len(found)}')
    return int(found[0])


class MaxPlayersTests(unittest.TestCase):
    def test_every_definition_equals_the_product_constant(self):
        values = {owner: capacity(*definition) for owner, definition in DEFINITIONS.items()}
        self.assertEqual(set(values.values()), {values['product C++']}, values)

    def test_product_code_has_no_capacity_literal(self):
        # The former two-player literals: the Match join limit, Ready and the Client's bound.
        sources = REPOSITORY / 'apps/object_fps_pvp/src/Pvp'
        for name, literal in (('PvpMatch.cpp', 'players_.size() >= 2'), ('IpcHost.cpp', 'set_max_players(2)'),
                              ('ClientConnection.cpp', 'players_size()>2')):
            self.assertNotIn(literal, (sources / name).read_text(encoding='utf-8'), name)

    def test_the_check_finds_a_mismatch(self):
        # Guard against a pattern that matches nothing or everything.
        self.assertEqual(capacity(DEFINITIONS['product C++'][0], DEFINITIONS['product C++'][1]),
                         capacity(DEFINITIONS['product Go'][0], DEFINITIONS['product Go'][1]))
        self.assertRaises(AssertionError, capacity, DEFINITIONS['product Go'][0], r'^const NoSuchCapacity = (\d+)$')


if __name__ == '__main__':
    unittest.main()
