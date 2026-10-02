"""Unit tests for the SDL-injected action runner's verdict; no GUI or services are started."""
import json
from pathlib import Path
import tempfile
import unittest

import run_action_short as runner


class ActionSummaryTests(unittest.TestCase):
    def directory(self, reports):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-action-summary-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        for role, report in reports.items():
            text = report if isinstance(report, str) else json.dumps(report)
            (directory / f"{role}-action.json").write_text(text, encoding="utf-8")
        return directory

    @staticmethod
    def report(role, captures=0, **changes):
        report = {"passed": True, "checks": {name: True for name in runner.REQUIRED_CHECKS[role]},
                  "captures": [{"file": f"{role}-{index}.bmp"} for index in range(captures)],
                  "platform": {"os": "macOS", "gpu_driver": "metal", "input": "sdl_injected"},
                  "remote": {"replays": 0}}
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
        full = {role: self.report(role, captures=runner.CAPTURES_PER_ROLE) for role in ("create", "join")}
        self.assertTrue(runner.summarize(self.directory(full), True)["passed"])
        short = {"create": self.report("create", captures=3), "join": self.report("join", captures=4)}
        summary = runner.summarize(self.directory(short), True)
        self.assertIn("create: expected 4 GPU captures, found 3", summary["errors"])

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


if __name__ == "__main__":
    unittest.main()
