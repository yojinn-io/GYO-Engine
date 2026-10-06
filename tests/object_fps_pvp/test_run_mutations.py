"""Checks of the mutation runner itself and of its catalog; no build, no mutant is run."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_mutations  # noqa: E402

TOOLS = run_mutations.Tools('unused', go='go')


def python_check(code, timeout=None):
    check = {'command': ['{python}', '-c', code]}
    if timeout:
        check['timeout'] = timeout
    return check


class RunMutantTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.source = self.root / 'source.txt'
        self.source.write_text('value = 1\n', encoding='utf-8')
        # The check fails with "caught mutant" when the source no longer says 1.
        self.reads = f"import sys; t=open(r'{self.source}').read(); " \
                     "sys.exit(0) if 'value = 1' in t else sys.exit('caught mutant')"

    def tearDown(self):
        self.directory.cleanup()

    def mutant(self, **fields):
        base = {'id': 'm', 'file': 'source.txt', 'find': 'value = 1', 'replace': 'value = 2', 'check': 'c',
                'expect': 'caught mutant'}
        base.update(fields)
        return base

    def run_one(self, check, **fields):
        result = run_mutations.run_mutant(self.mutant(**fields), {'c': check}, TOOLS, root=self.root)
        self.assertEqual(self.source.read_text(encoding='utf-8'), 'value = 1\n', 'source not restored')
        return result['status']

    def test_killed_only_with_the_expected_message(self):
        self.assertEqual(self.run_one(python_check(self.reads)), 'killed')
        self.assertEqual(self.run_one(python_check(self.reads), expect='another message'), 'wrong_failure')

    def test_a_passing_check_is_a_survivor(self):
        self.assertEqual(self.run_one(python_check('pass')), 'survived')

    def test_text_not_found_once_is_stale(self):
        self.assertEqual(self.run_one(python_check(self.reads), find='value = 9'), 'stale')
        self.source.write_text('value = 1\nvalue = 1\n', encoding='utf-8')
        status = run_mutations.run_mutant(self.mutant(), {'c': python_check(self.reads)}, TOOLS, root=self.root)
        self.assertEqual(status['status'], 'stale')
        self.source.write_text('value = 1\n', encoding='utf-8')

    def test_a_missing_tool_is_never_a_kill(self):
        check = {'command': ['gyo-no-such-tool-for-mutation-tests']}
        self.assertEqual(self.run_one(check, expect='.*'), 'error')

    def test_a_hanging_check_times_out(self):
        self.assertEqual(self.run_one(python_check('import time; time.sleep(30)', timeout=1), expect='.*'), 'timeout')


class CatalogTests(unittest.TestCase):
    def test_catalog_is_current(self):
        catalog = json.loads(run_mutations.CATALOG.read_text(encoding='utf-8'))
        ids = [m['id'] for m in catalog['mutants']]
        self.assertEqual(len(ids), len(set(ids)))
        for mutant in catalog['mutants']:
            with self.subTest(mutant=mutant['id']):
                self.assertIn(mutant['check'], catalog['checks'])
                self.assertNotEqual(mutant['find'], mutant['replace'])
                text = (run_mutations.REPOSITORY / mutant['file']).read_text(encoding='utf-8')
                self.assertEqual(text.count(mutant['find']), 1, 'stale mutant: update mutations.json')


if __name__ == '__main__':
    unittest.main()
