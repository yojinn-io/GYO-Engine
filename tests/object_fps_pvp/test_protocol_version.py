"""Product-owned check that every protocol version definition agrees (pv6 contract §1).

The product C++ constant, the product Go adapter constant and the acceptance
C++ and Python expected values are read as source text, so the check needs no
Go toolchain. Each owner keeps exactly one definition.
"""
from pathlib import Path
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
DEFINITIONS = {
    'product C++': (REPOSITORY / 'apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp',
                    r'^inline constexpr std::uint16_t ProtocolVersion = (\d+);$'),
    'product Go': (REPOSITORY / 'apps/object_fps_pvp/gateway/adapter/adapter.go',
                   r'^const ProtocolVersion = (\d+)$'),
    'acceptance C++': (REPOSITORY / 'build/acceptance/object_fps_pvp/acceptance_protocol.hpp',
                       r'^inline constexpr unsigned AcceptanceProtocolVersion = (\d+);$'),
    'acceptance Python': (REPOSITORY / 'build/acceptance/object_fps_pvp/acceptance_util.py',
                          r'^PROTOCOL_VERSION = (\d+)$'),
}


def version(path, pattern):
    found = re.findall(pattern, path.read_text(encoding='utf-8'), re.MULTILINE)
    if len(found) != 1:
        raise AssertionError(f'{path}: expected one definition, found {len(found)}')
    return int(found[0])


class ProtocolVersionTests(unittest.TestCase):
    def test_every_definition_equals_the_product_constant(self):
        values = {owner: version(*definition) for owner, definition in DEFINITIONS.items()}
        self.assertEqual(set(values.values()), {values['product C++']}, values)

    def test_the_check_finds_a_mismatch(self):
        # Guard against a pattern that matches nothing or everything.
        self.assertEqual(version(DEFINITIONS['product C++'][0], DEFINITIONS['product C++'][1]),
                         version(DEFINITIONS['product Go'][0], DEFINITIONS['product Go'][1]))
        self.assertRaises(AssertionError, version, DEFINITIONS['product Go'][0], r'^const NoSuchVersion = (\d+)$')


if __name__ == '__main__':
    unittest.main()
