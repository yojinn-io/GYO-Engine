"""Small synthetic regressions for the GUI combat evidence gate."""

import json
import math
from pathlib import Path
import tempfile
import unittest

from combat_gui_evidence import analyze_combat_gui


class CombatGuiEvidenceTests(unittest.TestCase):
    def fixture(self, duration=16):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-combat-gui-evidence-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        count = math.ceil((duration - .2) / .8)
        records = []
        for index in range(count):
            frame = 12 + 48 * index
            generated = 100 + frame / 60
            records.append({"action_id": index + 1, "generated_seconds": generated,
                            "submission_frame_id": frame})
        decisions = [{"action_id": row["action_id"], "resolved_tick": row["submission_frame_id"] + 2,
                      "accepted": True, "rejection": 0, "hit_kind": 2, "target_id": 2,
                      "damage": 25 if row["action_id"] <= 4 else 0,
                      "received_seconds": row["generated_seconds"] + .05} for row in records]
        feedback = [{"action_id": row["action_id"], "submission_frame_id": row["submission_frame_id"],
                     "frame_id": row["submission_frame_id"], "submitted_seconds": row["generated_seconds"],
                     "presented_seconds": row["generated_seconds"] + .002,
                     "shooting": True, "presented_action_id": row["action_id"]} for row in records]
        frame_count = int((duration + 2) * 60)
        for role, local in (("create", 1), ("join", 2)):
            expected = count if local == 1 else 0
            report = {"schema_version": 1, "enabled": True, "role": role,
                      "duration_seconds": duration, "local_id": local, "expected_target_id": 3 - local,
                      "maximum_hp": 100, "damage_per_hit": 25, "planned_shots": expected,
                      "dispatched_shots": expected, "submitted_shots": expected,
                      "decision_count": expected, "accepted": expected, "rejected": 0,
                      "animations": expected, "hp_at_end": 100 if local == 1 else 0,
                      "skipped_presented": 0, "errors": [], "hp_frame_count": frame_count,
                      "submissions": records if local == 1 else [],
                      "decisions": decisions if local == 1 else [],
                      "first_feedback": feedback if local == 1 else [],
                      "last_action_transport": {"allocated": expected, "acknowledged": expected,
                          "retired": expected, "pending_requests": 0, "retained_decisions": 0,
                          "unconsumed": 0, "protocol_errors": 0,
                          "max_datagram_bytes": 160 if expected else 0, "max_batch_shots": 1 if expected else 0}}
            (directory / f"{role}-combat.json").write_text(json.dumps(report), encoding="utf-8")
            with (directory / f"{role}-combat-hp.jsonl").open("w", encoding="utf-8") as target:
                for frame in range(frame_count):
                    tick = frame + 1
                    hp = 100 - sum(row["damage"] for row in decisions if row["resolved_tick"] <= tick)
                    sample = {"frame_id": frame, "host_seconds": 100 + frame / 60 + .003,
                              "snapshot_tick": tick, "local_id": local, "presented": True,
                              "hud_tick": tick, "hud_hp": 100 if local == 1 else hp,
                              "combat": [{"player_id": 1, "hp": 100}, {"player_id": 2, "hp": hp}]}
                    target.write(json.dumps(sample) + "\n")
        return directory

    @staticmethod
    def edit_report(directory, edit, role="create"):
        path = directory / f"{role}-combat.json"
        value = json.loads(path.read_text())
        edit(value)
        path.write_text(json.dumps(value))

    @staticmethod
    def edit_hp(directory, edit, role="join"):
        path = directory / f"{role}-combat-hp.jsonl"
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        edit(rows)
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))

    def test_clean_short_and_integration_keep_scope(self):
        for duration in (16, 120):
            result = analyze_combat_gui(self.fixture(duration))
            self.assertTrue(result["passed"], result["errors"])
            self.assertEqual(result["planned_shots"], 20 if duration == 16 else 150)
            self.assertEqual(result["integration_120_seconds"], duration == 120)
            self.assertFalse(result["full_acceptance"])

    def test_missing_outcome_keeps_planned_denominator(self):
        directory = self.fixture()
        self.edit_report(directory, lambda report: report["decisions"].pop())
        result = analyze_combat_gui(directory)
        self.assertFalse(result["passed"])
        self.assertEqual(result["roles"]["create"]["planned_shots"], 20)
        self.assertEqual(result["roles"]["create"]["infinite_arrival_count"], 1)

    def test_duplicate_decision_cannot_hide_behind_summary(self):
        directory = self.fixture()
        self.edit_report(directory, lambda report: report["decisions"].append(report["decisions"][0]))
        result = analyze_combat_gui(directory)
        self.assertFalse(result["passed"])
        self.assertTrue(any("exactly once" in error for error in result["errors"]))

    def test_next_successful_present_is_required_but_skips_are_valid(self):
        directory = self.fixture()
        def postpone(report):
            report["first_feedback"][0]["frame_id"] += 1
            report["first_feedback"][0]["presented_seconds"] += 1 / 60
        self.edit_report(directory, postpone)
        self.assertFalse(analyze_combat_gui(directory)["passed"])
        self.edit_hp(directory, lambda rows: rows[12].update(presented=False, hud_tick=0, hud_hp=0), "create")
        self.edit_report(directory, lambda report: report.update(skipped_presented=1))
        result = analyze_combat_gui(directory)
        self.assertTrue(result["passed"], result["errors"])

    def test_hp_oracle_distinguishes_new_snapshot_from_presented_hud(self):
        directory = self.fixture()
        # Tick 14 contains the first damage; the displayed frame may still use tick 13.
        self.edit_hp(directory, lambda rows: rows[13].update(hud_tick=13, hud_hp=100))
        self.assertTrue(analyze_combat_gui(directory)["passed"])
        self.edit_hp(directory, lambda rows: rows[13]["combat"][1].update(hp=50))
        result = analyze_combat_gui(directory)
        self.assertFalse(result["passed"])
        self.assertTrue(any("snapshot HP mismatch" in error for error in result["errors"]))

    def test_truncated_or_corrupt_hp_fails(self):
        directory = self.fixture()
        self.edit_hp(directory, lambda rows: rows.pop())
        self.assertFalse(analyze_combat_gui(directory)["passed"])
        path = directory / "join-combat-hp.jsonl"
        path.write_text(path.read_text() + '{"frame_id":')
        self.assertFalse(analyze_combat_gui(directory)["passed"])

    def test_slow_decisions_and_unsettled_transport_fail(self):
        directory = self.fixture()
        def slow(report):
            for row in report["decisions"]:
                row["received_seconds"] += .2
            report["last_action_transport"]["retired"] -= 1
        self.edit_report(directory, slow)
        result = analyze_combat_gui(directory)
        self.assertFalse(result["passed"])
        self.assertGreater(result["roles"]["create"]["decision_arrival_p95_seconds"], .150)
        self.assertTrue(any("retired" in error for error in result["errors"]))

    def test_nonfinite_data_and_changed_identity_fail(self):
        directory = self.fixture()
        self.edit_hp(directory, lambda rows: rows[10].update(local_id=99))
        self.assertFalse(analyze_combat_gui(directory)["passed"])
        self.edit_report(directory, lambda report: report["decisions"][0].update(received_seconds=float("nan")))
        self.assertFalse(analyze_combat_gui(directory)["passed"])


if __name__ == "__main__":
    unittest.main()
