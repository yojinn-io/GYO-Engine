"""Regressions for long-run PvP latency evidence and censored event handling."""

import csv
import json
import math
from pathlib import Path
import tempfile
import unittest

import presentation_evidence as evidence
import start_phase_evidence


class LatencyEvidenceTests(unittest.TestCase):
    def scenario(self, delay, missing=(), ambiguous=False, malformed=False,
                 presented_gaps=(), wrong_observer=False, observer_id=2, changed_generation=False,
                 short=False, analyze_as_long=False, start_phase=None, report_extra=None, join_epoch=None, files=None):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-latency-evidence-")
        self.addCleanup(temporary.cleanup)
        directory = self.last_directory = Path(temporary.name)
        count, duration, period = (20, 16, .8) if short else (200, 120, .6)
        with (directory / "latency-plan.csv").open("w", newline="") as target:
            writer = csv.writer(target)
            writer.writerow(["event_id", "scheduled_host_seconds", "end_host_seconds", "sign",
                             "threshold_units", "duration_seconds", "event_count"])
            for event in range(count):
                writer.writerow([event, 102 + event * period, 102 + (event + 1) * period,
                                 1 if event % 2 == 0 else -1, .25, duration, count])
        with (directory / "latency-events.csv").open("w", newline="") as target:
            writer = csv.writer(target)
            writer.writerow(["event_id", "actual_start_host_seconds", "local_id", "movement_epoch",
                             "after_sequence", "origin_x", "origin_z", "direction_x", "direction_z"])
            for event in range(count):
                if event not in missing:
                    writer.writerow([event, 102 + event * period, 1, 1, int(120 + event * period * 60),
                                     0, 0 if event % 2 == 0 else .9, 0, 1 if event % 2 == 0 else -1])

        def position(time):
            elapsed = time - 102
            if elapsed <= 0 or elapsed >= duration:
                return 0
            leg = int(elapsed / period)
            phase = elapsed - leg * period
            return (0 if leg % 2 == 0 else .9) + (1 if leg % 2 == 0 else -1) * min(phase, .3) * 3

        fields = ["host_steady_seconds", "local_id", "local_x", "local_z", "remote_id", "remote_x", "remote_z",
                  "frame_id", "local_epoch", "remote_epoch", "skipped_frames", "local_previous", "local_current",
                  "local_alpha", "remote_lower_tick", "remote_lower_command", "remote_upper_command", "remote_alpha",
                  "connection_generation"]
        for role in ("create", "join"):
            (directory / f"{role}-report.txt").write_text(
                f"player_id={1 if role == 'create' else observer_id}\nmode={'latency-short' if short else 'latency'}\ncapture=none\nnominal_fps=60\nskipped_frames=0\n"
                + (report_extra or {}).get(role, ""), encoding="utf-8")
            if start_phase and role in start_phase:
                (directory / f"{role}-start-phase.json").write_text(
                    start_phase[role] if isinstance(start_phase[role], str) else json.dumps(start_phase[role]), encoding="utf-8")
            with (directory / f"{role}-presentation.csv").open("w", newline="") as target:
                writer = csv.DictWriter(target, fieldnames=fields)
                writer.writeheader()
                for frame in range(7500):
                    time = 100 + frame / 60
                    event = math.floor((time - 102) / period)
                    if role == "join" and event in presented_gaps and .06 < (time - 102) % period < .25:
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
                    if join_epoch and role == "join":
                        row["local_epoch"] = join_epoch(time)  # The observer's own epoch; never a latency input.
                    if wrong_observer and role == "join":
                        row["local_id"] = 1
                    if changed_generation and role == "join" and frame >= 600:
                        row["connection_generation"] = 2
                    writer.writerow(row)
        for name, text in (files or {}).items():
            (directory / name).write_text(text, encoding="utf-8")
        return (evidence.analyze_short_latency(directory) if short and not analyze_as_long
                else evidence.analyze_latency(directory))

    def test_explicit_short_keeps_same_latency_thresholds(self):
        result = self.scenario(.04, short=True)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["matched_event_count"], 20)
        self.assertFalse(result["long_run_certification"])
        self.assertEqual(result["criteria"]["maximum_p50_seconds"], .05)
        self.assertEqual(result["criteria"]["maximum_p95_seconds"], .08)
        self.assertFalse(self.scenario(.1, short=True)["passed"])

    def test_short_evidence_never_passes_long_run_gate(self):
        result = self.scenario(.04, short=True, analyze_as_long=True)
        self.assertFalse(result["passed"])
        self.assertEqual(result["criteria"]["minimum_seconds"], 120)
        self.assertEqual(result["criteria"]["minimum_events"], 200)

    def test_short_unmatched_event_stays_in_denominator(self):
        result = self.scenario(.04, short=True, missing=(5,))
        self.assertFalse(result["passed"])
        self.assertEqual(result["planned_event_count"], 20)
        self.assertEqual(result["infinite_event_count"], 1)

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

    @staticmethod
    def phase_epoch(status, error=None, epoch=1, set_at=101, player=1, samples=None):
        """A recorder epoch without its state history (history() adds it); error is the first decision's phase error."""
        stamp = int(set_at * 1e9)
        samples = (0 if status == "host_samples_absent" else 900) if samples is None else samples
        decided = error is not None
        return {"player_id": player, "movement_epoch": epoch, "life_generation": 1, "status": status,
                "first_observed_frame": 1, "first_observed_steady_ns": stamp, "host_samples": samples,
                "host_late_samples": 0, "host_slack_min_micros": 1200 if samples else None,
                "host_slack_max_micros": 3214 if samples else None, "connection_quality_failures_max": 0,
                "first_decision_frame": 3 if decided else None,
                "first_decision_steady_ns": stamp + 16_000_000 if decided else None, "first_error_seconds": error,
                "corrections": int(decided), "late_corrections": 0, "last_error_seconds": error,
                "last_correction_seconds": error}

    @staticmethod
    def window_report(placement, position, occluded_during=0, focus_lost_during=0, moved_during=0, resized_during=0,
                      start="occluded:0,hidden:0,minimized:0,input_focus:1", final=None, sync="succeeded"):
        return (f"window_placement={placement}\nwindow_usable_bounds=0,25,1536,875\nwindow_requested_position={position}\n"
                f"window_sync={sync}\nwindow_position_after_sync={position}\n"
                f"window_final_position={final or position}\nwindow_size=1280,720\nwindow_borders=28,0,0,0\n"
                f"window_flags_at_measurement_start={start}\nwindow_flags_at_end=occluded:0,hidden:0,minimized:0,input_focus:1\n"
                f"window_os_events=occluded:{occluded_during},exposed:1,hidden:0,minimized:0,focus_gained:1,focus_lost:{focus_lost_during},"
                f"moved:{moved_during + 1},resized:{resized_during}\n"
                f"window_os_events_during_measurement=occluded:{occluded_during},exposed:0,hidden:0,minimized:0,focus_gained:0,"
                f"focus_lost:{focus_lost_during},moved:{moved_during},resized:{resized_during}\n"
                f"window_synthetic_events=20\n")

    def test_start_phase_values_reach_short_evidence_per_role(self):
        records = {"create": {"supported": True, "player_id": 1, "dropped_observations": 0, "epochs": [
                       self.phase_epoch("tracking", .0012),
                       self.phase_epoch("host_samples_absent", epoch=2, set_at=110)]},
                   "join": {"supported": True, "player_id": 2, "dropped_observations": 0, "epochs": [
                       self.phase_epoch("acquiring", player=2, samples=25)]}}
        result = self.scenario(.04, short=True, start_phase=records)
        self.assertTrue(result["passed"], result["errors"])
        create, join = result["start_phase"]["create"], result["start_phase"]["join"]
        self.assertEqual((create["status"], create["first_epoch_status"], create["epoch_count"]), ("recorded", "tracking", 2))
        self.assertEqual(create["first_epoch"]["host_slack_max_micros"], 3214)
        self.assertEqual(create["first_epoch"]["first_error_seconds"], .0012)
        self.assertAlmostEqual(create["first_epoch"]["decided_seconds_before_measurement"], .984)
        self.assertEqual(create["status_counts"], {"tracking": 1, "host_samples_absent": 1})
        self.assertIsNone(create["epochs"][1]["decided_seconds_before_measurement"])
        self.assertIn("a stall reseed returns it to acquiring", create["status_scope"])
        # create measures the epoch its emitted events moved in; join the epoch it was in at begin.
        self.assertEqual((create["measured_epoch"], create["measured_epoch_status"]), (1, "tracking"))
        self.assertIn("latency-events.csv", create["measured_epoch_source"])
        self.assertEqual(join["first_epoch_status"], "acquiring")
        self.assertEqual(join["first_epoch"]["host_samples"], 25)
        self.assertIsNone(join["first_epoch"]["first_decision_frame"])
        self.assertIsNone(join["first_epoch"]["decided_seconds_before_measurement"])
        self.assertEqual((join["measured_epoch"], join["measured_epoch_record"]["player_id"]), (1, 2))
        self.assertEqual((join["dropped_observations"], join["measured_epoch_record_count"]), (0, 1))
        saved = json.loads((self.last_directory / "presentation-short-latency.json").read_text())
        self.assertEqual(saved["start_phase"]["join"]["first_epoch"]["host_samples"], 25)

    def test_join_measured_epoch_is_the_one_active_at_measurement_begin(self):
        records = {role: {"supported": True, "epochs": [self.phase_epoch("tracking", .003),
                                                        self.phase_epoch("acquiring", epoch=3, set_at=101.5)]}
                   for role in ("create", "join")}
        result = self.scenario(.04, short=True, start_phase=records, join_epoch=lambda time: 1 if time < 101.4 else 3)
        self.assertTrue(result["passed"], result["errors"])
        join = result["start_phase"]["join"]
        self.assertEqual((join["measured_epoch"], join["measured_epoch_status"]), (3, "acquiring"))
        self.assertEqual(join["first_epoch"]["movement_epoch"], 1)
        missing = self.scenario(.04, short=True, start_phase={"join": {"supported": True, "epochs": [self.phase_epoch("tracking", .003)]}},
                                join_epoch=lambda time: 5)
        self.assertIsNone(missing["start_phase"]["join"]["measured_epoch_record"])
        self.assertIn("no record for measured epoch 5", missing["start_phase"]["join"]["measured_epoch_reason"])

    def test_absent_or_invalid_start_phase_is_explicit_and_not_a_latency_verdict(self):
        result = self.scenario(.04, short=True, start_phase={"join": "{not json"})
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["start_phase"]["create"]["status"], "absent")
        self.assertIn("create-start-phase.json missing", result["start_phase"]["create"]["reason"])
        self.assertEqual(result["start_phase"]["create"]["measured_epoch"], 1)
        self.assertEqual(result["start_phase"]["join"]["status"], "invalid")
        unsupported = self.scenario(.04, short=True, start_phase={
            "create": {"supported": False, "epochs": []}, "join": {"supported": True, "epochs": []}})
        self.assertEqual(unsupported["start_phase"]["create"]["status"], "unsupported")
        self.assertEqual(unsupported["start_phase"]["join"]["status"], "no_epoch_observed")
        # A former A1 start-phase status is not a phase-tracking status.
        legacy = self.scenario(.04, short=True, start_phase={"create": {"supported": True, "epochs": [self.phase_epoch("shift_armed", .001)]}})
        self.assertEqual(legacy["start_phase"]["create"]["status"], "invalid")

    def test_non_numeric_steady_ns_marks_only_that_role_invalid(self):
        broken = self.phase_epoch("tracking", .003)
        broken["first_decision_steady_ns"] = "101.0s"
        result = self.scenario(.04, short=True, start_phase={
            "create": {"supported": True, "epochs": [broken]},
            "join": {"supported": True, "epochs": [self.phase_epoch("tracking", .003, player=2)]}})
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["start_phase"]["create"]["status"], "invalid")
        self.assertIn("first_decision_steady_ns must be an integer", result["start_phase"]["create"]["reason"])
        self.assertEqual(result["start_phase"]["create"]["measured_epoch"], 1)
        self.assertEqual(result["start_phase"]["join"]["status"], "recorded")
        # Malformed even without a plan (the gameplay reader): never silently accepted.
        self.assertEqual(evidence.summarize_start_phase({"epochs": [broken]}, "x.json")["status"], "invalid")

    def test_start_phase_without_a_plan_keeps_every_key_explicit(self):
        summary = evidence.summarize_start_phase({"supported": True, "dropped_observations": 2, "epochs": [
            self.phase_epoch("tracking", .003)]}, "action-client.json")
        epoch = summary["first_epoch"]
        self.assertIsNone(epoch["decided_seconds_before_measurement"])
        self.assertIsNone(epoch["measurement_window"])
        self.assertIsNone(epoch["stall_reseeds"])
        self.assertIsNone(summary["measured_epoch"])
        self.assertEqual(summary["measured_epoch_reason"], "no latency measurement plan")
        window_keys = ("measured_epoch_window_status", "measured_epoch_tracking_seconds", "measured_epoch_acquiring_seconds",
                       "measured_epoch_corrections_in_window")
        self.assertEqual({key: summary[key] for key in window_keys}, dict.fromkeys(window_keys))
        self.assertEqual(summary["dropped_observations"], 2)
        absent = evidence.summarize_start_phase(None, "missing.json")
        self.assertIsNone(absent["measured_epoch"])

    def test_window_positions_and_clean_run_are_recorded(self):
        result = self.scenario(.04, short=True, report_extra={
            "create": self.window_report("usable_bounds_top_left", "0,53"),
            "join": self.window_report("usable_bounds_bottom_right", "256,180", sync="timed_out")})
        self.assertTrue(result["passed"], result["errors"])
        self.assertIs(result["window_disturbed"], False)
        self.assertEqual(result["window"]["join"]["final_position"], [256, 180])
        self.assertEqual(result["window"]["join"]["position_after_sync"], [256, 180])
        self.assertEqual(result["window"]["join"]["sync"], "timed_out")
        # A MOVED before measurement only; the placement's own MOVED precedes the watcher and is never counted.
        self.assertEqual(result["window"]["create"]["os_events"]["moved"], 1)
        self.assertEqual(result["window"]["create"]["synthetic_events"], 20)
        overlap = result["window_overlap"]
        self.assertEqual(overlap["status"], "recorded")
        self.assertGreater(overlap["join_visible_fraction_with_create_on_top"], 0)
        self.assertLess(overlap["join_visible_fraction_with_create_on_top"], 1)

    def test_disturbed_window_events_are_flagged_without_changing_latency_verdict(self):
        result = self.scenario(.04, short=True, report_extra={
            "create": self.window_report("usable_bounds_top_left", "0,53", focus_lost_during=1, moved_during=1),
            "join": self.window_report("usable_bounds_bottom_right", "256,180", occluded_during=2, resized_during=1,
                                       start="occluded:1,hidden:0,minimized:0,input_focus:0")})
        self.assertTrue(result["passed"], result["errors"])
        self.assertIs(result["window_disturbed"], True)
        self.assertIn("join: 2 OS occluded event(s) during measurement", result["window_disturbances"])
        self.assertIn("join: window occluded at measurement start", result["window_disturbances"])
        self.assertIn("create: 1 OS focus_lost event(s) during measurement", result["window_disturbances"])
        self.assertIn("create: 1 OS moved event(s) during measurement", result["window_disturbances"])
        self.assertIn("join: 1 OS resized event(s) during measurement", result["window_disturbances"])

    def test_missing_window_evidence_is_unknown_not_clean(self):
        result = self.scenario(.04, short=True, report_extra={
            "create": self.window_report("usable_bounds_top_left", "0,53")})
        self.assertIsNone(result["window_disturbed"])
        self.assertEqual(result["window"]["join"]["status"], "absent")
        self.assertIn("join: window evidence absent", result["window_disturbances"][0])
        coincident = self.scenario(.04, short=True, report_extra={
            role: self.window_report("platform_default", "128,120") for role in ("create", "join")})
        self.assertEqual(coincident["window_overlap"]["join_visible_fraction_with_create_on_top"], 0)

    def test_truncated_window_position_marks_only_that_role_invalid(self):
        result = self.scenario(.04, short=True, report_extra={
            "create": self.window_report("usable_bounds_top_left", "0,53", final="0"),
            "join": self.window_report("usable_bounds_bottom_right", "256,180")})
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["window"]["create"]["status"], "invalid")
        self.assertIn("window_final_position must hold 2 integers", result["window"]["create"]["reason"])
        self.assertEqual(result["window"]["join"]["status"], "recorded")
        self.assertIsNone(result["window_disturbed"])
        self.assertNotIn("window_overlap", result)
        older = self.scenario(.04, short=True, report_extra={
            role: self.window_report("platform_default", "128,120").replace(",moved:0,resized:0", "").replace(",moved:1,resized:0", "")
            for role in ("create", "join")})
        self.assertEqual(older["window"]["create"]["status"], "invalid")
        self.assertIsNone(older["window_disturbed"])

    @staticmethod
    def changes(*entries):
        return [{"state": state, "frame": frame, "steady_ns": int(seconds * 1e9)} for state, frame, seconds in entries]

    def history(self, epoch, changes, last_seconds, **values):
        epoch.update(state_changes=changes, state_changes_dropped=0, last_state=changes[-1]["state"],
                     last_frame=changes[-1]["frame"] + 1, last_steady_ns=int(last_seconds * 1e9), **values)
        return epoch

    def test_measured_epoch_corrections_during_measurement_are_a_window_state(self):
        # Plan: 102..118 s. create settles twice inside the window (105..107 s
        # and 110..110.5 s); join settled only before measurement began.
        create = self.history(self.phase_epoch("tracking", .009), self.changes(
            ("acquiring", 1, 101), ("tracking", 3, 101.016), ("settling", 240, 105), ("tracking", 360, 107),
            ("settling", 540, 110), ("tracking", 570, 110.5)), 119, corrections=3)
        join = self.history(self.phase_epoch("tracking", .009, player=2), self.changes(
            ("acquiring", 1, 101), ("tracking", 3, 101.016), ("settling", 30, 101.5), ("tracking", 48, 101.8)), 119)
        result = self.scenario(.04, short=True, start_phase={"create": {"supported": True, "epochs": [create]},
                                                             "join": {"supported": True, "epochs": [join]}})
        self.assertTrue(result["passed"], result["errors"])
        measured = result["start_phase"]["create"]
        window = measured["measured_epoch_record"]["measurement_window"]
        self.assertEqual((window["status"], measured["measured_epoch_window_status"]), ("tracking", "tracking"))
        self.assertEqual(window["states"], ["tracking", "settling"])
        self.assertAlmostEqual(window["state_seconds"]["settling"], 2.5)
        self.assertAlmostEqual(window["state_seconds"]["tracking"], 13.5)
        self.assertEqual((window["corrections_in_window"], window["reacquisitions_in_window"]), (2, 0))
        # Settling is tracked time: the whole window counts as tracking.
        self.assertAlmostEqual(measured["measured_epoch_tracking_seconds"], 16)
        self.assertEqual(measured["measured_epoch_acquiring_seconds"], 0)
        self.assertAlmostEqual(window["window_seconds"], 16)
        self.assertAlmostEqual(window["observed_seconds"], 16)
        before = result["start_phase"]["join"]
        self.assertEqual(before["measured_epoch_status"], "tracking")
        self.assertEqual(before["measured_epoch_record"]["measurement_window"]["states"], ["tracking"])
        self.assertEqual(before["measured_epoch_corrections_in_window"], 0)
        saved = json.loads((self.last_directory / "presentation-short-latency.json").read_text())
        self.assertEqual(saved["start_phase"]["create"]["measured_epoch_corrections_in_window"], 2)

    def test_window_state_without_plan_history_or_full_history_is_explicit(self):
        acquiring = self.history(self.phase_epoch("acquiring"), self.changes(("acquiring", 1, 101)), 101.1)
        summary = evidence.summarize_start_phase({"supported": True, "epochs": [acquiring]}, "x.json", 102,
                                                 {"epoch": 1, "source": "test", "reason": None}, 118)
        self.assertEqual(summary["measured_epoch_status"], "acquiring")
        self.assertEqual(summary["measured_epoch_window_status"], "client_not_observed_in_window")
        no_plan = evidence.summarize_start_phase({"supported": True, "epochs": [acquiring]}, "x.json")
        self.assertIsNone(no_plan["first_epoch"]["measurement_window"])
        older = evidence.summarize_start_phase({"supported": True, "epochs": [self.phase_epoch("tracking", .003)]},
                                               "x.json", 102, {"epoch": 1, "source": "test", "reason": None}, 118)
        self.assertEqual(older["measured_epoch_window_status"], "not_recorded")
        self.assertIsNone(older["measured_epoch_tracking_seconds"])
        truncated = dict(acquiring, state_changes_dropped=3)
        self.assertEqual(start_phase_evidence.measurement_window(truncated, 102, 118)["status"], "state_history_truncated")
        unobserved = dict(self.phase_epoch("host_samples_absent"), state_changes=[], state_changes_dropped=0,
                          last_state=None, last_frame=None, last_steady_ns=None)
        self.assertEqual(start_phase_evidence.measurement_window(unobserved, 102, 118)["status"], "client_not_observed")
        late = self.history(self.phase_epoch("tracking", .009), self.changes(("acquiring", 1, 101), ("tracking", 60, 110)), 119)
        window = start_phase_evidence.measurement_window(late, 102, 118)
        self.assertEqual((window["status"], window["states"]), ("acquiring_during_measurement", ["acquiring", "tracking"]))
        self.assertAlmostEqual(window["acquiring_seconds"], 8)
        self.assertAlmostEqual(window["tracking_seconds"], 8)
        self.assertEqual(window["reacquisitions_in_window"], 0)  # The first state is not a reacquisition.
        unordered = self.history(self.phase_epoch("tracking", .003),
                                 self.changes(("acquiring", 1, 103), ("tracking", 3, 102)), 119)
        self.assertEqual(evidence.summarize_start_phase({"epochs": [unordered]}, "x.json")["status"], "invalid")

    def test_non_finite_start_phase_numbers_mark_only_that_role_invalid(self):
        valid = {"supported": True, "epochs": [self.phase_epoch("tracking", .003, player=2)]}
        for literal in ("NaN", "1e999"):
            with self.subTest(literal=literal):
                text = json.dumps({"supported": True, "epochs": [self.phase_epoch("tracking", .003)]})
                text = text.replace('"first_error_seconds": 0.003', f'"first_error_seconds": {literal}')
                self.assertIn(literal, text)
                result = self.scenario(.04, short=True, start_phase={"create": text, "join": valid})
                self.assertTrue(result["passed"], result["errors"])
                self.assertEqual(result["start_phase"]["create"]["status"], "invalid")
                self.assertIn("non-finite JSON number", result["start_phase"]["create"]["reason"])
                self.assertEqual(result["start_phase"]["join"]["status"], "recorded")
                # The analysis completed and wrote its evidence without NaN.
                saved = (self.last_directory / "presentation-short-latency.json").read_text()
                self.assertEqual(json.loads(saved)["start_phase"]["create"]["status"], "invalid")
        with tempfile.TemporaryDirectory(prefix="pvp-start-phase-") as temporary:
            path = Path(temporary) / "create-start-phase.json"
            path.write_text('{"supported": true, "dropped_observations": -Infinity, "epochs": []}', encoding="utf-8")
            self.assertEqual(evidence._start_phase_file(Path(temporary), "create")["status"], "invalid")
        # Already-parsed values (the gameplay reader) are checked as well.
        inf = self.phase_epoch("tracking", float("inf"))
        self.assertIn("first_error_seconds", evidence.summarize_start_phase({"epochs": [inf]}, "x.json")["reason"])
        extra = evidence.summarize_start_phase({"epochs": [], "future": [float("nan")]}, "x.json")
        self.assertEqual(extra["status"], "invalid")

    @staticmethod
    def linux_window_report(occluded_during=0):
        # The non-macOS probe keeps the platform-default placement: no usable
        # bounds, no requested position and no sync; X11 may report borders.
        return ("window_placement=platform_default\nwindow_usable_bounds=unavailable\nwindow_requested_position=none\n"
                "window_sync=not_requested\nwindow_position_after_sync=none\nwindow_final_position=320,180\n"
                "window_size=1280,720\nwindow_borders=37,1,1,1\n"
                "window_flags_at_measurement_start=occluded:0,hidden:0,minimized:0,input_focus:1\n"
                "window_flags_at_end=occluded:0,hidden:0,minimized:0,input_focus:0\n"
                f"window_os_events=occluded:{occluded_during},exposed:1,hidden:0,minimized:0,focus_gained:1,focus_lost:0,moved:0,resized:0\n"
                f"window_os_events_during_measurement=occluded:{occluded_during},exposed:0,hidden:0,minimized:0,focus_gained:0,"
                "focus_lost:0,moved:0,resized:0\nwindow_synthetic_events=20\n")

    def window_directory(self, reports):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-window-evidence-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        for role, text in reports.items():
            (directory / f"{role}-report.txt").write_text("player_id=1\n" + text, encoding="utf-8")
        return directory

    def test_linux_shaped_window_reports_are_recorded_and_may_overlap(self):
        result = evidence._windows(self.window_directory({role: self.linux_window_report() for role in ("create", "join")}))
        self.assertIs(result["window_disturbed"], False)
        create = result["window"]["create"]
        self.assertEqual((create["placement"], create["sync"]), ("platform_default", "not_requested"))
        self.assertIsNone(create["usable_bounds"])
        self.assertIsNone(create["requested_position"])
        self.assertIsNone(create["position_after_sync"])
        self.assertEqual(create["borders_top_left_bottom_right"], [37, 1, 1, 1])
        # Same default spot: each window is fully covered by the other.
        self.assertEqual(result["window_overlap"]["create_visible_fraction_with_join_on_top"], 0)
        # A Wayland compositor may report the covered window as occluded.
        wayland = evidence._windows(self.window_directory({"create": self.linux_window_report(),
                                                           "join": self.linux_window_report(occluded_during=1)}))
        self.assertIs(wayland["window_disturbed"], True)
        self.assertEqual(wayland["window_disturbances"], ["join: 1 OS occluded event(s) during measurement"])

    def test_duplicate_window_key_marks_only_that_role_invalid(self):
        duplicated = self.window_report("usable_bounds_top_left", "0,53") + "window_final_position=900,900\n"
        result = evidence._windows(self.window_directory({"create": duplicated,
                                                          "join": self.window_report("usable_bounds_bottom_right", "256,180")}))
        self.assertEqual(result["window"]["create"]["status"], "invalid")
        self.assertIn("duplicate window_* key(s): window_final_position", result["window"]["create"]["reason"])
        self.assertEqual(result["window"]["join"]["status"], "recorded")
        self.assertIsNone(result["window_disturbed"])

    def test_window_overlap_never_raises_on_malformed_geometry(self):
        overlap = evidence._window_overlap({"final_position": [0], "size": [1, 1], "borders_top_left_bottom_right": None},
                                           {"final_position": [0, 0], "size": [1, 1], "borders_top_left_bottom_right": None})
        self.assertEqual(overlap["status"], "invalid")

    @staticmethod
    def client_trace(*generated, life=1):
        """A complete schema-2 Client trace of generated events: (seconds, sequence, seeded, player, epoch)."""
        lines = []
        for seconds, sequence, seeded, player, epoch in generated:
            record = {"schema_version": 2, "kind": "generated", "time_ns": int(seconds * 1e9), "player_id": player,
                      "life_generation": life, "epoch": epoch, "sequence": sequence, "authority_tick": 0, "source": "none",
                      "queued": 0, "pending": 2, "seeded_neutral": seeded, "dropped_seconds": 0, "frame_seconds": 0,
                      "count": 0, "age_seconds": 0, "reset_reason": "", "started_ns": 0}
            lines.append(json.dumps(record))
        lines.append(json.dumps({"schema_version": 2, "kind": "trace_end", "events": len(generated), "dropped": 0}))
        return "\n".join(lines) + "\n"

    def test_stall_reseed_is_a_reacquisition_and_the_client_trace_counts_it_per_record(self):
        # Plan: 102..118 s. create tracks from 101.016 s; a stall reseed at 105 s
        # (neutral sequences 42-43) returns it to acquiring until 105.5 s.
        create = self.history(self.phase_epoch("tracking", .009), self.changes(
            ("acquiring", 1, 101), ("tracking", 3, 101.016), ("acquiring", 240, 105), ("tracking", 270, 105.5)), 119)
        # join never decided: reseeds while acquiring add no state change and are only in its trace.
        join = self.history(self.phase_epoch("acquiring", player=2), self.changes(("acquiring", 1, 101)), 119)
        trace = self.client_trace((101.0, 1, True, 1, 1), (101.0, 2, True, 1, 1), (101.016, 3, False, 1, 1),
                                  (105.0, 42, True, 1, 1), (105.0, 43, True, 1, 1), (105.016, 44, False, 1, 1),
                                  (106.0, 102, True, 1, 2))  # Another epoch: never this record's.
        join_trace = self.client_trace((101.0, 1, True, 2, 1), (101.0, 2, True, 2, 1), (101.5, 7, True, 2, 1),
                                       (101.5, 8, True, 2, 1), (103.0, 60, True, 2, 1), (103.0, 61, True, 2, 1))
        records = {"create": {"supported": True, "player_id": 1, "epochs": [create]},
                   "join": {"supported": True, "player_id": 2, "epochs": [join]}}
        result = self.scenario(.04, short=True, start_phase=records,
                               files={"create-commands.jsonl": trace, "join-commands.jsonl": join_trace})
        self.assertTrue(result["passed"], result["errors"])
        phase = result["start_phase"]["create"]
        entry = phase["measured_epoch_record"]
        # A reseed never rewrites the recorded status; it is counted beside it.
        self.assertEqual((entry["status"], entry["stall_reseeds"]), ("tracking", 1))
        window = entry["measurement_window"]
        self.assertEqual((window["status"], window["states"]), ("acquiring_during_measurement", ["tracking", "acquiring"]))
        self.assertEqual((window["reacquisitions_in_window"], window["corrections_in_window"]), (1, 0))
        self.assertAlmostEqual(window["tracking_seconds"], 15.5)
        self.assertAlmostEqual(window["acquiring_seconds"], .5)
        self.assertAlmostEqual(window["observed_seconds"], 16)
        self.assertAlmostEqual(phase["measured_epoch_acquiring_seconds"], .5)
        self.assertEqual(phase["status_counts"], {"tracking": 1})
        self.assertEqual(phase["reseed_detection"], {"status": "recorded", "source": "create-commands.jsonl", "reason": None})
        # The recorded record is untouched; only the summary's view carries the trace count.
        saved = json.loads((self.last_directory / "create-start-phase.json").read_text())
        self.assertNotIn("stall_reseeds", saved["epochs"][0])
        join_entry = result["start_phase"]["join"]["measured_epoch_record"]
        self.assertEqual((join_entry["status"], join_entry["stall_reseeds"]), ("acquiring", 2))
        join_window = join_entry["measurement_window"]
        self.assertEqual((join_window["status"], join_window["reacquisitions_in_window"]), ("acquiring_during_measurement", 0))
        self.assertAlmostEqual(join_window["acquiring_seconds"], 16)
        self.assertEqual(join_window["tracking_seconds"], 0)

    def test_reseed_detection_is_explicit_when_the_client_trace_cannot_count_reseeds(self):
        tracking = self.history(self.phase_epoch("tracking", .009), self.changes(
            ("acquiring", 1, 101), ("tracking", 3, 101.016)), 119)
        reacquired = self.history(self.phase_epoch("tracking", .009, player=2), self.changes(
            ("acquiring", 1, 101), ("tracking", 3, 101.016), ("acquiring", 300, 106), ("tracking", 330, 106.5)), 119.5)
        # join's trace is read even though its record already shows the reacquisition.
        reseeded = self.client_trace((101.0, 1, True, 2, 1), (101.0, 2, True, 2, 1), (106.0, 9, True, 2, 1))
        result = self.scenario(.04, short=True, files={"join-commands.jsonl": reseeded}, start_phase={
            "create": {"supported": True, "epochs": [tracking]},
            "join": {"supported": True, "player_id": 2, "epochs": [reacquired]}})
        create, join = result["start_phase"]["create"], result["start_phase"]["join"]
        self.assertEqual(create["measured_epoch_status"], "tracking")
        self.assertEqual(create["reseed_detection"]["status"], "absent")
        self.assertIn("create-commands.jsonl missing", create["reseed_detection"]["reason"])
        # Not counted is null, never zero; the window state does not depend on the trace.
        self.assertIsNone(create["measured_epoch_record"]["stall_reseeds"])
        self.assertAlmostEqual(create["measured_epoch_tracking_seconds"], 16)
        self.assertEqual(create["measured_epoch_acquiring_seconds"], 0)
        self.assertEqual(join["reseed_detection"]["status"], "recorded")
        entry = join["measured_epoch_record"]
        self.assertEqual((entry["status"], entry["stall_reseeds"]), ("tracking", 1))
        self.assertEqual(entry["measurement_window"]["reacquisitions_in_window"], 1)
        self.assertAlmostEqual(entry["measurement_window"]["tracking_seconds"], 15.5)
        self.assertAlmostEqual(entry["measurement_window"]["acquiring_seconds"], .5)
        with tempfile.TemporaryDirectory(prefix="pvp-start-phase-") as temporary:
            (Path(temporary) / "create-start-phase.json").write_text(json.dumps({"supported": True, "epochs": [tracking]}))
            (Path(temporary) / "create-commands.jsonl").write_text('{"kind": "generated"}\n')
            corrupt = evidence._start_phase_file(Path(temporary), "create", 102, {"epoch": 1, "source": "t", "reason": None}, 118)
        self.assertEqual(corrupt["reseed_detection"]["status"], "invalid")
        self.assertIn("corrupt diagnostic record", corrupt["reseed_detection"]["reason"])
        self.assertEqual(corrupt["measured_epoch_status"], "tracking")
        self.assertIsNone(corrupt["measured_epoch_record"]["stall_reseeds"])
        lifeless = start_phase_evidence.reseed_evidence([{"kind": "generated", "player_id": 1, "epoch": 1, "sequence": 9,
                                                          "seeded_neutral": True, "time_ns": 1}], "old.jsonl")
        self.assertEqual(lifeless["status"], "unsupported")
        unattributable = evidence.summarize_start_phase({"supported": True, "epochs": [tracking]}, "x.json", reseeds=lifeless)
        self.assertEqual(unattributable["reseed_detection"]["status"], "unsupported")
        self.assertIsNone(unattributable["first_epoch"]["stall_reseeds"])
        not_given = evidence.summarize_start_phase({"supported": True, "epochs": [tracking]}, "x.json")
        self.assertEqual(not_given["reseed_detection"]["status"], "not_checked")

    def test_record_without_state_history_counts_reseeds_but_names_no_window_state(self):
        older = self.phase_epoch("tracking", .002606)

        def generated(sequence, seeded, seconds, life=1):
            return {"kind": "generated", "player_id": 1, "epoch": 1, "life_generation": life, "sequence": sequence,
                    "seeded_neutral": seeded, "time_ns": int(seconds * 1e9)}
        reseeds = start_phase_evidence.reseed_evidence([generated(42, True, 101.5)], "create-commands.jsonl")
        summary = evidence.summarize_start_phase({"supported": True, "epochs": [older]}, "x.json", 102,
                                                 {"epoch": 1, "source": "test", "reason": None}, 118, reseeds)
        entry = summary["measured_epoch_record"]
        self.assertEqual((entry["status"], entry["stall_reseeds"]), ("tracking", 1))
        self.assertEqual(entry["measurement_window"]["status"], "not_recorded")
        self.assertIn("no state history", entry["measurement_window"]["reason"])
        self.assertIsNone(summary["measured_epoch_tracking_seconds"])
        # The epoch-start seed (sequences 1..2) is not a reseed, one reseed's consecutive
        # neutral sequences count once, and another life's reseed is never this record's.
        events = [generated(1, True, 101), generated(2, True, 101), generated(3, False, 101.016),
                  generated(9, True, 103), generated(10, True, 103), generated(11, False, 103.016),
                  generated(12, True, 104), generated(30, True, 105, life=2)]
        for trace, count in ((events, 2), (events[:3], 0)):
            with self.subTest(count=count):
                counted = evidence.summarize_start_phase({"supported": True, "epochs": [older]}, "x.json", reseeds=
                                                         start_phase_evidence.reseed_evidence(trace, "create-commands.jsonl"))
                self.assertEqual(counted["first_epoch"]["stall_reseeds"], count)

    def test_unattributed_player_zero_records_are_reported_apart(self):
        # An older gui probe keyed its first active frame by player 0.
        phantom = self.history(self.phase_epoch("host_samples_absent", player=0), self.changes(("acquiring", 0, 100.9)), 100.9)
        create = self.phase_epoch("tracking", .003)
        result = self.scenario(.04, short=True, start_phase={
            "create": {"supported": True, "player_id": 1, "unattributed_client_frames": 0, "epochs": [phantom, create]},
            "join": {"supported": True, "player_id": 2, "epochs": [phantom]}})
        self.assertTrue(result["passed"], result["errors"])
        phase = result["start_phase"]["create"]
        self.assertEqual((phase["epoch_count"], phase["first_epoch_status"]), (1, "tracking"))
        self.assertEqual(phase["first_epoch"]["player_id"], 1)
        self.assertEqual([item["player_id"] for item in phase["unattributed_epochs"]], [0])
        self.assertEqual(phase["status_counts"], {"tracking": 1})
        self.assertEqual(phase["unattributed_client_frames"], 0)
        only = result["start_phase"]["join"]
        self.assertEqual(only["status"], "no_epoch_observed")
        self.assertIn("1 record(s) without a player id", only["reason"])

    def test_gui_probe_pairs_the_client_observation_with_the_state_after_update(self):
        # The frame-start connection state may predate the Update that joins and
        # activates the prediction; observing with it keyed the first active
        # frame by player 0. The recorder test covers the player-0 guard itself.
        source = (Path(__file__).resolve().parent / "gui_main.cpp").read_text(encoding="utf-8")
        body = source[source.index("void RunLatency("):]
        body = body[:body.index("\nvoid ", 1)]
        update = body.index("application.Update(frame)")
        reread = body.index("const auto updated = connection.State();", update)
        observe = body.index("startPhase.Observe(", reread)
        self.assertEqual(body.count("startPhase.Observe("), 1)
        call = body[observe:body.index(";", observe)]
        self.assertIn("updated.playerId, FindSelf(updated), application.LocalMovement()", call)
        self.assertNotIn("state.playerId", call)

    def test_connection_generations_cannot_be_stitched_into_one_run(self):
        result = self.scenario(.04, changed_generation=True)
        self.assertFalse(result["passed"])
        self.assertIn("connection generation", result["errors"][0])


if __name__ == "__main__":
    unittest.main()
