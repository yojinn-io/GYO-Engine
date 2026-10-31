"""The scaled two-tree comparison (compare_authority_trees.py --scaled/--added, v8-product-hit-rules-permille P1).

Pure checks on dump lines in the runner's --dump format; no runner is built.
"""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_authority_trees import parse_scaled, scaled_difference  # noqa: E402

SCALED = {'hp': 1000, 'damage': 1000}
ADDED = {'damageCount', 'hitPart'}
BASE = ['#0 kind=snapshot tick=3 combat=1 hp=100 combatLife=1',
        '#1 kind=decision actionId=7 damage=25 decisionLife=1']
BRANCH = ['#0 kind=snapshot tick=3 combat=1 hp=100000 combatLife=1 damageCount=0',
          '#1 kind=decision actionId=7 damage=25000 decisionLife=1 hitPart=1']


class ScaledDifferenceTests(unittest.TestCase):
    def test_exact_scaling_with_added_fields_is_same(self):
        difference, seen = scaled_difference(BASE, BRANCH, SCALED, ADDED)
        self.assertIsNone(difference)
        self.assertEqual(seen, {'damageCount': 1, 'hitPart': 1})

    def test_unscaled_value_differs(self):
        branch = [BRANCH[0].replace('hp=100000', 'hp=100'), BRANCH[1]]
        difference, _ = scaled_difference(BASE, branch, SCALED, ADDED)
        self.assertEqual((difference['record'], difference['field'], difference['expected']), (0, 'hp', '100000'))

    def test_off_by_one_scaling_differs(self):
        branch = [BRANCH[0], BRANCH[1].replace('damage=25000', 'damage=25001')]
        difference, _ = scaled_difference(BASE, branch, SCALED, ADDED)
        self.assertEqual(difference['field'], 'damage')

    def test_other_field_change_differs(self):
        branch = [BRANCH[0].replace('tick=3', 'tick=4'), BRANCH[1]]
        difference, _ = scaled_difference(BASE, branch, SCALED, ADDED)
        self.assertEqual(difference['field'], 'tick')

    def test_unexpected_new_field_differs(self):
        branch = [BRANCH[0] + ' extra=1', BRANCH[1]]
        difference, _ = scaled_difference(BASE, branch, SCALED, ADDED)
        self.assertEqual(difference['field'], '<record length>')

    def test_record_count_differs(self):
        difference, _ = scaled_difference(BASE, BRANCH[:1], SCALED, ADDED)
        self.assertEqual(difference['field'], '<record count>')

    def test_reordered_field_differs(self):
        branch = ['#0 kind=snapshot combat=1 tick=3 hp=100000 combatLife=1 damageCount=0', BRANCH[1]]
        difference, _ = scaled_difference(BASE, branch, SCALED, ADDED)
        self.assertEqual(difference['field'], '<field name>')

    def test_absent_added_field_is_counted_zero(self):
        branch = [BRANCH[0].replace(' damageCount=0', ''), BRANCH[1]]
        difference, seen = scaled_difference(BASE, branch, SCALED, ADDED)
        self.assertIsNone(difference)
        self.assertEqual(seen['damageCount'], 0)


class ParseScaledTests(unittest.TestCase):
    def test_names_and_factor(self):
        self.assertEqual(parse_scaled('hp,damage=1000'), SCALED)

    def test_rejects_missing_or_trivial_factor(self):
        for text in ('hp', 'hp=', 'hp=1', '=1000', 'hp=x'):
            with self.assertRaises(ValueError, msg=text):
                parse_scaled(text)


if __name__ == '__main__':
    unittest.main()
