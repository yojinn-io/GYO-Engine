"""Unit tests for the SDL-injected action runner's verdict; no GUI or services are started."""
import json
from pathlib import Path
import tempfile
import threading
import unittest

import run_action_short as runner


class ActionSummaryTests(unittest.TestCase):
    def directory(self, reports, frames=None):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-action-summary-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        for role, report in reports.items():
            text = report if isinstance(report, str) else json.dumps(report)
            (directory / f"{role}-action.json").write_text(text, encoding="utf-8")
            role_frames = (frames or {}).get(role, self.frames())
            (directory / f"{role}-action-frames.jsonl").write_text(
                "".join(json.dumps(frame) + "\n" for frame in role_frames), encoding="utf-8")
        return directory

    @staticmethod
    def frames(shots=2, dead_meshes=0):
        frames = [{"frame_id": 1, "presented": True,
                   "presented_weapon": {"active": True, "dead": False, "submitted_meshes": 3}}]
        frames += [{"frame_id": 2 + shot, "presented": True, "submission_to_presented_seconds": .02,
                    "presented_weapon": {"active": True, "dead": False, "submitted_meshes": 3}}
                   for shot in range(shots)]
        frames.append({"frame_id": 10, "presented": True,
                       "presented_weapon": {"active": True, "dead": True, "submitted_meshes": dead_meshes}})
        return frames

    @staticmethod
    def report(role, captures=0, **changes):
        report = {"passed": True, "checks": {name: True for name in runner.REQUIRED_CHECKS[role]},
                  "captures": [{"file": f"{role}-{index}.bmp"} for index in range(captures)],
                  "platform": {"os": "macOS", "gpu_driver": "metal", "input": "sdl_injected"},
                  "remote": {"replays": 0}, "injected_sdl_shots": 2}
        report.update(changes)
        return report

    def test_both_roles_with_every_check_pass_and_keep_their_platform(self):
        summary = runner.summarize(self.directory({role: self.report(role) for role in ("create", "join")}), False)
        self.assertTrue(summary["passed"])
        self.assertEqual(summary["errors"], [])
        self.assertEqual(summary["platform"]["join"]["gpu_driver"], "metal")

    def test_missing_check_failed_probe_and_absent_role_are_each_named(self):
        create = self.report("create")
        del create["checks"]["move_while_reloading"]
        join = self.report("join", passed=False, error="Target death, suppression or respawn coverage incomplete")
        summary = runner.summarize(self.directory({"create": create, "join": join}), False)
        self.assertFalse(summary["passed"])
        self.assertIn("create: missing checks move_while_reloading", summary["errors"])
        self.assertIn("join: Target death, suppression or respawn coverage incomplete", summary["errors"])
        absent = runner.summarize(self.directory({"create": self.report("create")}), False)
        self.assertFalse(absent["passed"])
        self.assertTrue(any(error.startswith("join-action.json:") for error in absent["errors"]))

    def test_a_check_must_be_true_not_merely_present(self):
        create = self.report("create")
        create["checks"]["held_fire_one_shot"] = "yes"
        summary = runner.summarize(self.directory({"create": create, "join": self.report("join")}), False)
        self.assertIn("create: missing checks held_fire_one_shot", summary["errors"])

    def test_capture_case_requires_every_gpu_capture(self):
        full = {role: self.report(role, captures=runner.CAPTURES_PER_ROLE[role]) for role in ("create", "join")}
        self.assertTrue(runner.summarize(self.directory(full), True)["passed"])
        short = {"create": self.report("create", captures=4), "join": self.report("join", captures=4)}
        summary = runner.summarize(self.directory(short), True)
        self.assertIn("create: expected 9 GPU captures, found 4", summary["errors"])

    def test_dead_local_player_presenting_weapon_meshes_fails(self):
        reports = {role: self.report(role) for role in ("create", "join")}
        clean = runner.summarize(self.directory(reports), False)
        self.assertTrue(clean["passed"], clean["errors"])
        mutated = runner.summarize(self.directory(reports, {"join": self.frames(dead_meshes=2)}), False)
        self.assertFalse(mutated["passed"])
        self.assertIn("join: frame 10 presented first-person weapon meshes while dead", mutated["errors"])

    def test_every_injected_shot_needs_one_valid_first_presented_duration(self):
        reports = {role: self.report(role) for role in ("create", "join")}
        missing = runner.summarize(self.directory(reports, {"create": self.frames(shots=1)}), False)
        self.assertIn("create: 1 shot(s) reached a successful presentation, 2 were injected", missing["errors"])
        negative = self.frames()
        negative[1]["submission_to_presented_seconds"] = -.001
        invalid = runner.summarize(self.directory(reports, {"create": negative}), False)
        self.assertIn("create: invalid submitted-shot-to-Presented duration", invalid["errors"])

    def test_missing_frame_trace_is_named(self):
        directory = self.directory({role: self.report(role) for role in ("create", "join")})
        (directory / "join-action-frames.jsonl").unlink()
        summary = runner.summarize(directory, False)
        self.assertIn("join-action-frames.jsonl: missing or malformed", summary["errors"])

    def test_external_input_is_named_as_disturbance_not_a_product_verdict(self):
        create = self.report("create", passed=False, disturbed=True, unscheduled_yaw_frames=12,
                             error="Remote target death, held death pose and new life were not all presented")
        summary = runner.summarize(self.directory({"create": create, "join": self.report("join")}), False)
        self.assertFalse(summary["passed"])
        self.assertEqual(summary["disturbed"], ["create"])
        self.assertIn("create: disturbed by external input (12 frame(s) of unscheduled mouse motion); "
                      "not a product verdict", summary["errors"])

    def test_malformed_report_is_an_error_not_a_crash(self):
        summary = runner.summarize(self.directory({"create": "{not json", "join": "[]"}), False)
        self.assertFalse(summary["passed"])
        self.assertEqual(len(summary["errors"]), 2)


class RapidRecordTests(unittest.TestCase):
    """The rapid-fire cases are record-only: tallies come from the actor's per-click record, nothing is judged."""
    def directory(self, reports, clicks):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-rapid-summary-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        for role, report in reports.items():
            (directory / f"{role}-rapid.json").write_text(json.dumps(report), encoding="utf-8")
        if clicks is not None:
            (directory / "create-rapid-clicks.jsonl").write_text(
                "".join(json.dumps(click) + "\n" for click in clicks), encoding="utf-8")
        return directory

    @staticmethod
    def click(index, outcome, segment_click, previous=None, arrived=None, verdict=None):
        return {"index": index, "segment": 0, "segment_click": segment_click, "outcome": outcome,
                "previous_sent_action": previous, "previous_verdict_arrived": arrived,
                "action_id": index + 1 if outcome == "sent" else None, "verdict": verdict}

    def clicks(self):
        accepted = {"state": "accepted", "rejection": 0, "exact": True}
        return [self.click(0, "sent", 0, verdict=accepted),
                self.click(1, "local_block", 1, previous=1, arrived=False),
                self.click(2, "sent", 2, previous=1, arrived=True, verdict={"state": "rejected", "rejection": 3,
                                                                            "exact": True}),
                self.click(3, "sent", 3, previous=3, arrived=False, verdict={"state": "ambiguous", "exact": False}),
                self.click(4, "sent", 4, previous=4, arrived=True, verdict=None)]

    @staticmethod
    def reports(**changes):
        create = {"passed": True, "disturbed": False, "platform": {"os": "macOS"},
                  "counts": {"authority_cooldown_rejections": 1, "local_cooldown_blocks": 1, "player_hits": 0,
                             "attribution_mismatches": 0}}
        create.update(changes)
        return {"create": create, "join": {"passed": True, "disturbed": False, "platform": {"os": "macOS"}}}

    def test_cases_cover_every_scenario_at_30_60_144_and_the_default_matrix_is_unchanged(self):
        self.assertEqual(set(runner.CASES), {"action30", "action60", "action144", "capture"})
        self.assertEqual(len(runner.RAPID_CASES), len(runner.RAPID_SCENARIOS) * 3)
        self.assertEqual(runner.RAPID_CASES["rapid-interval12-30"], ("interval12", 30))
        self.assertEqual(runner.RAPID_CASES["rapid-edge-144"], ("edge", 144))
        self.assertFalse(set(runner.CASES) & set(runner.RAPID_CASES))

    def test_tallies_split_outcomes_verdicts_and_previous_decision_arrival(self):
        counts = runner.rapid_counts(self.clicks())
        self.assertEqual((counts["clicks"], counts["sent"], counts["local_blocks"], counts["neither"]), (5, 4, 1, 0))
        self.assertEqual((counts["accepted"], counts["rejected"], counts["ambiguous"], counts["missing"]), (1, 1, 1, 1))
        self.assertEqual(counts["cooldown_rejections_attributed"], 1)
        self.assertEqual(counts["previous_verdict_arrived"], {"eligible": 4, "arrived": 2})
        self.assertEqual(counts["previous_verdict_arrived_sent"], {"eligible": 3, "arrived": 2})

    def test_completed_record_reports_the_product_cooldown_count_without_judging_it(self):
        summary = runner.summarize_rapid(self.directory(self.reports(), self.clicks()), "edge")
        self.assertTrue(summary["passed"], summary["errors"])
        self.assertTrue(summary["record_only"])
        self.assertTrue(summary["clean"])
        self.assertEqual(summary["excluded_injections"], [])
        self.assertEqual(summary["counts"]["authority_cooldown_rejections"], 1)
        self.assertFalse(runner.summarize_rapid(self.directory(self.reports(), self.clicks()), "pause")["clean"])

    def test_disturbance_failure_missing_clicks_and_counter_mismatch_are_named(self):
        disturbed = runner.summarize_rapid(self.directory(self.reports(disturbed=True, unscheduled_yaw_frames=3),
                                                          self.clicks()), "interval12")
        self.assertFalse(disturbed["passed"])
        self.assertEqual(disturbed["disturbed"], ["create"])
        failed = runner.summarize_rapid(self.directory(self.reports(passed=False, error="Rapid probe exceeded bounded "
                                                                    "deadline"), self.clicks()), "interval12")
        self.assertIn("create: Rapid probe exceeded bounded deadline", failed["errors"])
        missing = runner.summarize_rapid(self.directory(self.reports(), None), "interval12")
        self.assertIn("create-rapid-clicks.jsonl: missing or malformed", missing["errors"])
        reports = self.reports()
        reports["create"]["counts"]["local_cooldown_blocks"] = 2
        mismatch = runner.summarize_rapid(self.directory(reports, self.clicks()), "interval12")
        self.assertIn("create: per-click local blocks do not equal the product counter", mismatch["errors"])


class TimerBaselineIsNeverAVerdict(unittest.TestCase):
    """The empty-loop timer baseline is interpretation only: the analyzer may pass it through, never read it."""
    def test_services_stop_after_the_clients_and_the_match_after_the_gateway_link_closed(self):
        events = []

        class Fake:
            def __init__(self, name, on_terminate=None):
                self.name, self.alive, self.on_terminate = name, True, on_terminate

            def poll(self):
                return None if self.alive else 0

            def terminate(self):
                events.append(self.name)
                self.alive = False
                if self.on_terminate:
                    self.on_terminate()

            def wait(self, timeout=None):
                return 0

            def kill(self):
                self.alive = False

        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'match.log'
            log.write_text('[ObjectFPS/PvP Match] join player=1 accepted\n')
            # The Match reads the Gateway's close a little after the Gateway exited.
            def close_later():
                events.append('eof')
                log.write_text(log.read_text() + '[ObjectFPS/PvP Match] ipc closed reason=eof\n')
            match = Fake('match')
            gateway = Fake('gateway', lambda: threading.Timer(0.1, close_later).start())
            runner.stop_in_order([match, gateway, Fake('create'), Fake('join')], gateway, match, log)
            self.assertEqual(events, ['join', 'create', 'gateway', 'eof', 'match'])
            # Without that line the Match is still stopped, after the bounded wait.
            events.clear()
            log.write_text('')
            match, gateway = Fake('match'), Fake('gateway')
            runner.stop_in_order([match, gateway], gateway, match, log, close_wait=0.05)
            self.assertEqual(events, ['gateway', 'match'])

    def test_source_only_passes_the_timer_baseline_through(self):
        source = (Path(__file__).resolve().parent / 'run_action_short.py').read_text(encoding="utf-8")
        lines = [line.strip() for line in source.splitlines() if "timer" in line.lower()]
        self.assertEqual(lines, [], "a check may have started reading the timer baseline")


if __name__ == "__main__":
    unittest.main()
