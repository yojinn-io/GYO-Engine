"""Product-owned check of the DropReason golden (pv7 contract §4).

tests/object_fps_pvp/fixtures/drop_reasons.json binds every DropReason value to
its proto name and its ingress drop_ key. Go (TestDropReasonsMatchTheGoldenAndTheKeys)
and C++ ("PvP IPC drop reasons match the shared golden") read it too; this side
reads the proto source, so the check needs no toolchain.
"""
from pathlib import Path
import json
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
GOLDEN = REPOSITORY / 'tests/object_fps_pvp/fixtures/drop_reasons.json'
PROTO = REPOSITORY / 'apps/object_fps_pvp/protocol/runtime_v7.proto'


def proto_enum():
    body = re.search(r'enum DropReason \{(.*?)\}', PROTO.read_text(encoding='utf-8'), re.S).group(1)
    return {int(value): name for name, value in re.findall(r'^\s*(DROP_[A-Z_]+)\s*=\s*(\d+);', body, re.M)}


class DropReasonsTest(unittest.TestCase):
    def test_golden_matches_the_proto(self):
        golden = json.loads(GOLDEN.read_text(encoding='utf-8'))
        enum = proto_enum()
        self.assertEqual(enum.pop(0), 'DROP_REASON_UNSPECIFIED')
        self.assertEqual({entry['value']: entry['name'] for entry in golden}, enum)

    def test_each_name_is_its_key_in_upper_case_and_in_its_scope(self):
        for entry in json.loads(GOLDEN.read_text(encoding='utf-8')):
            with self.subTest(name=entry['name']):
                self.assertEqual(entry['name'], entry['key'].upper())
                self.assertTrue(entry['key'].startswith('drop_'))
                player = 1 <= entry['value'] <= 34
                self.assertEqual(entry['scope'], 'player' if player else 'gateway')
                self.assertTrue(player or 40 <= entry['value'] <= 45)


if __name__ == '__main__':
    unittest.main()
