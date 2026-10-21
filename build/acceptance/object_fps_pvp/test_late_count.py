import json
import tempfile
import unittest
from pathlib import Path

import late_count


def frame(a, b):
    return {"phase_late_corrections_a": a, "phase_late_corrections_b": b, "time_ns": 0}


class LateCountTests(unittest.TestCase):
    lost = 0

    def write(self, root, frames=None, phase=None, ids=(1, 2)):
        root = Path(root)
        if frames is not None:
            end = {"kind": "evidence_end", "lost": self.lost, "records": len(frames)}
            (root / "action-frames.jsonl").write_text("".join(json.dumps(f) + "\n" for f in frames + [end]))
        if phase is not None:
            (root / "result.json").write_text(json.dumps({"start_phase": phase}))
        (root / "action-client.json").write_text(json.dumps({"player_ids": list(ids)}))
        return root

    def test_increments_count_resets_from_zero(self):
        self.assertEqual(late_count.increments([0, 1, 1, 3, 0, 2, 2]), 5)
        self.assertEqual(late_count.increments([]), 0)
        # the maximum (the old reading) would say 3
        self.assertEqual(max([0, 1, 1, 3, 0, 2, 2]), 3)

    def test_both_sources_agree(self):
        with tempfile.TemporaryDirectory() as tmp:
            phase = {"1": {"epochs": [{"late_corrections": 3}, {"late_corrections": 2}], "dropped_observations": 0},
                     "2": {"epochs": [{"late_corrections": 1}], "dropped_observations": 0}}
            frames = [frame(0, 0), frame(2, 0), frame(3, 1), frame(0, 1), frame(2, 1)]
            found = late_count.count(self.write(tmp, frames, phase))
        self.assertEqual(found["late_corrections"], 6)
        self.assertEqual(found["frames_by_player"], {1: 5, 2: 1})
        self.assertEqual(found["tool_problems"], [])

    def test_disagreement_is_a_tool_problem(self):
        with tempfile.TemporaryDirectory() as tmp:
            phase = {"1": {"epochs": [{"late_corrections": 3}], "dropped_observations": 0}}
            found = late_count.count(self.write(tmp, [frame(0, 0), frame(2, 0)], phase))
        self.assertIsNone(found["late_corrections"])
        self.assertIn("start_phase 3 != frames 2", found["tool_problems"])

    def test_dropped_observations_is_a_tool_problem(self):
        with tempfile.TemporaryDirectory() as tmp:
            phase = {"1": {"epochs": [{"late_corrections": 1}], "dropped_observations": 4}}
            found = late_count.count(self.write(tmp, None, phase))
        self.assertIsNone(found["late_corrections"])
        self.assertTrue(any("dropped_observations 4" in p for p in found["tool_problems"]))

    def test_lost_frames_are_a_tool_problem(self):
        self.lost = 2
        with tempfile.TemporaryDirectory() as tmp:
            found = late_count.count(self.write(tmp, [frame(0, 0), frame(1, 0)], None))
        self.assertIsNone(found["late_corrections"])
        self.assertTrue(any("lost 2 frame(s)" in p for p in found["tool_problems"]))

    def test_one_source_alone(self):
        with tempfile.TemporaryDirectory() as tmp:
            found = late_count.count(self.write(tmp, [frame(0, 0), frame(1, 2)], None))
        self.assertEqual(found["late_corrections"], 3)
        self.assertIsNone(found["start_phase"])

    def test_frames_without_counters_are_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            found = late_count.count(self.write(tmp, [{"time_ns": 0}], None))
        self.assertIsNone(found["frames"])
        self.assertIsNone(found["late_corrections"])


if __name__ == "__main__":
    unittest.main()
