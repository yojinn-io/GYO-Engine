"""Bounded runner lifecycle tests; no sockets, GUI or real-clock measurement."""

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

import run_timing


class TimingRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='pvp-timing-runner-')
        self.addCleanup(self.temporary.cleanup)
        self.args = SimpleNamespace(output=Path(self.temporary.name)/'new-run', rounds=3,
                                    fps=60, duration=120., events=200, gui=True,
                                    combat=False, short=False, soak=False, report_only=False)

    def run_rounds(self, function):
        with redirect_stdout(io.StringIO()):
            return run_timing.execute_rounds(self.args, function)

    def results(self):
        return json.loads((self.args.output/'results.json').read_text())

    def test_all_rounds_pass_and_have_readable_summary(self):
        calls = []
        def measurement(args, output, fps):
            calls.append(output.name)
            return {'overall_passed': True, 'actual_fraction': 1., 'disturbed': False,
                    'actual_p50_ms': 45., 'actual_p95_ms': 50.,
                    'presentation': {'p50_seconds': .048, 'p95_seconds': .05,
                                     'matched_event_count': 200, 'planned_event_count': 200}}
        self.assertEqual(self.run_rounds(measurement), 0)
        self.assertEqual(calls, ['round-1', 'round-2', 'round-3'])
        self.assertTrue(self.results()['passed'])
        self.assertFalse(self.results()['full_product_acceptance'])
        self.assertIn('48.000 / 50.000', (self.args.output/'summary.md').read_text())

    def test_failed_round_keeps_evidence_and_marks_remaining_not_run(self):
        evidence = {'overall_passed': False, 'passed': True,
                    'presentation': {'passed': False, 'errors': ['P50 exceeds 50 ms']}}
        def measurement(args, output, fps):
            output.mkdir()
            (output/'round.json').write_text(json.dumps(evidence))
            (output/'create.log').write_text('original raw log\n')
            raise RuntimeError('fixed threshold failed')
        self.assertEqual(self.run_rounds(measurement), 1)
        result = self.results()
        self.assertFalse(result['passed'])
        self.assertEqual([item['status'] for item in result['rounds']], ['failed', 'not_run', 'not_run'])
        self.assertEqual(result['rounds'][0]['evidence'], evidence)
        self.assertIn('fixed threshold failed', (self.args.output/'round-1/runner-failure.txt').read_text())
        self.assertEqual((self.args.output/'round-1/create.log').read_text(), 'original raw log\n')
        self.assertFalse((self.args.output/'round-2').exists())

    def test_startup_failure_also_writes_terminal_summary(self):
        def measurement(args, output, fps):
            raise OSError('could not start service')
        self.assertEqual(self.run_rounds(measurement), 1)
        self.assertIn('could not start service', self.results()['rounds'][0]['error'])
        self.assertTrue((self.args.output/'summary.md').is_file())

    def test_interrupt_also_marks_remaining_not_run(self):
        def measurement(args, output, fps):
            raise KeyboardInterrupt()
        self.assertEqual(self.run_rounds(measurement), 1)
        self.assertIn('KeyboardInterrupt', self.results()['rounds'][0]['error'])

    def test_existing_nonempty_output_is_never_overwritten(self):
        self.args.output.mkdir()
        marker = self.args.output/'previous-evidence.txt'
        marker.write_text('keep')
        with self.assertRaisesRegex(ValueError, 'never overwritten'):
            self.run_rounds(lambda *args: self.fail('must not run'))
        self.assertEqual(marker.read_text(), 'keep')

    def test_returned_failure_also_stops_without_exception(self):
        self.assertEqual(self.run_rounds(lambda *args: {'overall_passed': False}), 1)
        self.assertEqual([item['status'] for item in self.results()['rounds']], ['failed', 'not_run', 'not_run'])

    def test_full_gui_requires_clean_round_even_when_latency_passes(self):
        for disturbed, resets, expected in ((False, 0, True), (True, 0, False),
                                            (False, 1, False), (None, 0, False),
                                            (False, None, False)):
            with self.subTest(disturbed=disturbed, resets=resets):
                evidence = {'overall_passed': True, 'passed': True, 'errors': [],
                            'disturbed': disturbed, 'resets': resets,
                            'presentation': {'passed': True}, 'combat': {'passed': True}}
                self.assertEqual(run_timing.require_clean_gui_round(evidence), expected)
                self.assertEqual(evidence['overall_passed'], expected)
                self.assertEqual(evidence['gui_clean_passed'], expected)
                if not expected:
                    self.assertIn('cannot qualify as clean', evidence['errors'][0])


if __name__ == '__main__':
    unittest.main()
