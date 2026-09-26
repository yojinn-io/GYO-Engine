"""Regressions for long-run PvP latency evidence and censored event handling."""

import csv
import math
from pathlib import Path
import tempfile
import unittest

import presentation_evidence as evidence


class LatencyEvidenceTests(unittest.TestCase):
    def scenario(self, delay, missing=(), ambiguous=False, malformed=False,
                 presented_gaps=(), wrong_observer=False, observer_id=2, changed_generation=False):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-latency-evidence-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        with (directory / "latency-plan.csv").open("w", newline="") as target:
            writer = csv.writer(target)
            writer.writerow(["event_id", "scheduled_host_seconds", "end_host_seconds", "sign",
                             "threshold_units", "duration_seconds", "event_count"])
            for event in range(200):
                writer.writerow([event, 102 + event * .6, 102 + (event + 1) * .6,
                                 1 if event % 2 == 0 else -1, .25, 120, 200])
        with (directory / "latency-events.csv").open("w", newline="") as target:
            writer = csv.writer(target)
            writer.writerow(["event_id", "actual_start_host_seconds", "local_id", "movement_epoch",
                             "after_sequence", "origin_x", "origin_z", "direction_x", "direction_z"])
            for event in range(200):
                if event not in missing:
                    writer.writerow([event, 102 + event * .6, 1, 1, 120 + event * 36,
                                     0, 0 if event % 2 == 0 else .9, 0, 1 if event % 2 == 0 else -1])

        def position(time):
            elapsed = time - 102
            if elapsed <= 0 or elapsed >= 120:
                return 0
            leg = int(elapsed / .6)
            phase = elapsed - leg * .6
            return (0 if leg % 2 == 0 else .9) + (1 if leg % 2 == 0 else -1) * min(phase, .3) * 3

        fields = ["host_steady_seconds", "local_id", "local_x", "local_z", "remote_id", "remote_x", "remote_z",
                  "frame_id", "local_epoch", "remote_epoch", "skipped_frames", "local_previous", "local_current",
                  "local_alpha", "remote_lower_tick", "remote_lower_command", "remote_upper_command", "remote_alpha",
                  "connection_generation"]
        for role in ("create", "join"):
            (directory / f"{role}-report.txt").write_text(
                f"player_id={1 if role == 'create' else observer_id}\nmode=latency\ncapture=none\nnominal_fps=60\nskipped_frames=0\n",
                encoding="utf-8")
            with (directory / f"{role}-presentation.csv").open("w", newline="") as target:
                writer = csv.DictWriter(target, fieldnames=fields)
                writer.writeheader()
                for frame in range(7500):
                    time = 100 + frame / 60
                    event = math.floor((time - 102) / .6)
                    if role == "join" and event in presented_gaps and .06 < (time - 102) % .6 < .25:
                        continue
                    command = (time - 100) * 60
                    remote_command = max(0, (time - delay - 100) * 60)
                    row = dict.fromkeys(fields, 0)
                    row.update(host_steady_seconds=time, local_id=1 if role == "create" else 2,
                               local_z=position(time) if role == "create" else 0,
                               remote_id=2 if role == "create" else 1,
                               remote_z=position(time - delay) if role == "join" else 0,
                               frame_id=frame, local_epoch=1, remote_epoch=1, connection_generation=1,
                               local_previous=math.floor(command), local_current=math.floor(command) + 1,
                               local_alpha=command - math.floor(command), remote_lower_tick=max(1, math.floor(remote_command)),
                               remote_lower_command=math.floor(remote_command), remote_upper_command=math.floor(remote_command) + 1,
                               remote_alpha=remote_command - math.floor(remote_command))
                    if ambiguous and role == "join" and 102.145 < time < 102.16:
                        row["remote_z"] = .2
                    if malformed and frame == 600:
                        row["frame_id"] = 599
                    if wrong_observer and role == "join":
                        row["local_id"] = 1
                    if changed_generation and role == "join" and frame >= 600:
                        row["connection_generation"] = 2
                    writer.writerow(row)
        return evidence.analyze_latency(directory)

    def test_clean_40_ms_passes(self):
        result = self.scenario(.04)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["matched_event_count"], 200)
        self.assertAlmostEqual(result["p50_seconds"], .04)

    def test_slow_100_ms_is_retained_and_fails(self):
        result = self.scenario(.1)
        self.assertFalse(result["passed"])
        self.assertEqual(result["matched_event_count"], 200)
        self.assertAlmostEqual(result["p95_seconds"], .1)

    def test_one_missing_stays_in_denominator(self):
        result = self.scenario(.04, missing=(19,))
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["planned_event_count"], 200)
        self.assertEqual(result["infinite_event_count"], 1)
        self.assertEqual(result["matched_event_rate"], .995)

    def test_more_than_one_percent_missing_fails(self):
        result = self.scenario(.04, missing=(18, 19, 20))
        self.assertFalse(result["passed"])
        self.assertGreaterEqual(result["infinite_event_count"], 3)

    def test_one_second_late_events_are_not_discarded_or_aliased(self):
        result = self.scenario(1)
        self.assertFalse(result["passed"])
        self.assertEqual(result["matched_event_count"], 200)
        self.assertAlmostEqual(result["p95_seconds"], 1)

    def test_multiple_crossings_are_ambiguous_not_first_match_wins(self):
        result = self.scenario(.04, ambiguous=True)
        self.assertEqual(result["infinite_event_count"], 1)
        self.assertEqual(result["events"][0]["status"], "ambiguous")

    def test_repeated_frame_is_corrupt_evidence(self):
        result = self.scenario(.04, malformed=True)
        self.assertFalse(result["passed"])
        self.assertIn("repeated", result["errors"][0])

    def test_long_presented_gaps_cannot_fake_fast_crossings(self):
        result = self.scenario(.04, presented_gaps=range(200))
        self.assertFalse(result["passed"])
        self.assertEqual(result["planned_event_count"], 200)
        self.assertEqual(result["infinite_event_count"], 200)
        self.assertEqual(result["unknown_gap_event_count"], 200)
        self.assertIsNone(result["p50_seconds"])
        self.assertGreater(result["cadence"]["join"]["maximum_frame_interval_seconds"], .2)

    def test_one_unobserved_crossing_remains_in_all_event_denominator(self):
        result = self.scenario(.04, presented_gaps=(19,))
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["planned_event_count"], 200)
        self.assertEqual(result["matched_event_count"], 199)
        self.assertEqual(result["infinite_event_count"], 1)
        self.assertEqual(result["events"][19]["status"], "unknown_presented_gap")

    def test_unknown_crossing_is_not_removed_when_another_candidate_exists(self):
        event = {"actual": 100, "epoch": 1, "after_sequence": 10, "x": 0, "z": 0,
                 "dx": 0, "dz": 1, "threshold": .25}
        samples = [{"time": time, "epoch": 1, "cursor": cursor, "x": 0, "z": z}
                   for time, cursor, z in ((100, 10, 0), (100.2, 22, .3),
                                           (100.216, 23, .2), (100.232, 24, .3))]
        crossings = evidence._event_crossings(samples, event, {"epoch": 1, "after_sequence": 46}, 100.6)
        self.assertEqual(len(crossings), 2)
        self.assertTrue(math.isinf(crossings[0]))
        self.assertAlmostEqual(crossings[1], 100.224)

    def test_observer_trace_must_match_distinct_reported_player(self):
        result = self.scenario(.04, wrong_observer=True)
        self.assertFalse(result["passed"])
        self.assertIn("observer identity", result["errors"][0])

    def test_observer_report_cannot_name_the_mover(self):
        result = self.scenario(.04, observer_id=1)
        self.assertFalse(result["passed"])
        self.assertIn("distinct", result["errors"][0])

    def test_connection_generations_cannot_be_stitched_into_one_run(self):
        result = self.scenario(.04, changed_generation=True)
        self.assertFalse(result["passed"])
        self.assertIn("connection generation", result["errors"][0])


if __name__ == "__main__":
    unittest.main()
