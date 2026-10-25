"""compare_reports.py, the reader of the Match's gw_drop report lines (log-contract): every Gateway drop report
and the Match's copy of it, keyed by (player, report_sequence), must be equal one for one; violations and streams
without a final are reported (v7.1 contract-readers, N-compare-reports-untracked)."""
import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import compare_reports

GATEWAY = ("2026/10/16 15:04:58.150603 drop report player=0 report_sequence=1 final=0 samples=3 samples_omitted=0 "
           "merged_streams=0 drop_coalesced_snapshots=3\n"
           "2026/10/16 15:04:59.150603 drop report player=0 report_sequence=2 final=1 samples=0 samples_omitted=0 "
           "merged_streams=0\n"
           "2026/10/16 15:04:58.150747 drop report player=1 report_sequence=1 final=0 samples=2 samples_omitted=0 "
           "merged_streams=0 drop_replaced_snapshot_packets=2\n")
MATCH = ("[ObjectFPS/PvP Match] gw_drop report player=0 report_sequence=1 final=0 samples=3 samples_omitted=0 "
         "merged_streams=0 drop_coalesced_snapshots=3\n"
         "[ObjectFPS/PvP Match] gw_drop report player=0 report_sequence=2 final=1 samples=0 samples_omitted=0 "
         "merged_streams=0\n"
         "[ObjectFPS/PvP Match] gw_drop report player=1 report_sequence=1 final=0 samples=2 samples_omitted=0 "
         "merged_streams=0 drop_replaced_snapshot_packets=2\n")


def run_main(roots):
    with contextlib.redirect_stdout(io.StringIO()):
        return compare_reports.main(roots)


class CompareReportsTests(unittest.TestCase):
    def run_dir(self, root, name, gateway, match):
        run = Path(root)/name
        run.mkdir()
        (run/'gateway.log').write_text(gateway)
        (run/'match.log').write_text(match)
        return run

    def test_equal_reports_pass_and_name_the_stream_without_a_final(self):
        with tempfile.TemporaryDirectory() as root:
            result = compare_reports.compare(self.run_dir(root, 'r', GATEWAY, MATCH))
            self.assertTrue(result['equal'])
            self.assertEqual((result['gateway_reports'], result['match_reports']), (3, 3))
            self.assertEqual(result['streams'], [0, 1])
            self.assertEqual(result['streams_without_final'], [1])
            self.assertEqual(run_main([root]), 0)

    def test_a_report_the_match_did_not_read_fails(self):
        with tempfile.TemporaryDirectory() as root:
            match = ''.join(line + '\n' for line in MATCH.splitlines() if 'player=1 ' not in line)
            result = compare_reports.compare(self.run_dir(root, 'r', GATEWAY, match))
            self.assertFalse(result['equal'])
            self.assertEqual(result['only_gateway'], [(1, 1)])
            self.assertEqual(run_main([root]), 1)

    def test_a_report_read_with_other_content_fails(self):
        with tempfile.TemporaryDirectory() as root:
            match = MATCH.replace('samples=2 ', 'samples=1 ')
            result = compare_reports.compare(self.run_dir(root, 'r', GATEWAY, match))
            self.assertFalse(result['equal'])
            self.assertEqual(result['different'], [(1, 1)])
            self.assertEqual(run_main([root]), 1)

    def test_a_violation_fails_although_the_reports_are_equal(self):
        with tempfile.TemporaryDirectory() as root:
            match = MATCH + '[ObjectFPS/PvP Match] gw_drop violation player=1 reason=sequence_gap\n'
            result = compare_reports.compare(self.run_dir(root, 'r', GATEWAY, match))
            self.assertTrue(result['equal'])
            self.assertEqual(result['violations'], ['player=1 reason=sequence_gap'])
            self.assertEqual(run_main([root]), 1)

    def test_every_run_below_the_roots_is_compared(self):
        with tempfile.TemporaryDirectory() as root:
            self.run_dir(root, 'a', GATEWAY, MATCH)
            self.run_dir(root, 'b', GATEWAY, MATCH.replace('samples=3 ', 'samples=4 '))
            (Path(root)/'c').mkdir()
            (Path(root)/'c'/'gateway.log').write_text(GATEWAY)  # no match.log: not a run
            self.assertEqual(run_main([root]), 1)


if __name__ == '__main__':
    unittest.main()
