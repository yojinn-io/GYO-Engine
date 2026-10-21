import json
import tempfile
import unittest
from pathlib import Path

import network_resets


def reset(player, epoch, reason="starvation", time_ns=1):
    return {"schema_version": 2, "kind": "reset", "time_ns": time_ns, "player_id": player, "epoch": epoch,
            "authority_tick": 1, "reset_reason": reason}


class NetworkResetsTests(unittest.TestCase):
    def run_dir(self, root, events, stages=1, dropped=0, end=True):
        root = Path(root)
        rows = [{"schema_version": 2, "kind": "resolved", "time_ns": 0}] + events
        if end:
            rows.append({"schema_version": 2, "kind": "trace_end", "events": len(rows), "dropped": dropped})
        (root / "match-commands.jsonl").write_text("".join(json.dumps(r) + "\n" for r in rows))
        for rtt in (0, 20, 40)[:stages - 1]:
            (root / f"network-rtt-{rtt}.log").write_text("")
        return root

    def test_one_starvation_per_stage_is_expected(self):
        with tempfile.TemporaryDirectory() as tmp:
            found = network_resets.classify(self.run_dir(tmp, [reset(2, 2), reset(12, 2), reset(19, 2),
                                                              reset(26, 2), reset(3, 1, "life_respawn")], stages=4))
        self.assertEqual(len(found["expected"]), 4)
        self.assertEqual(found["candidates"], [])
        self.assertEqual(found["tool_problems"], [])

    def test_other_resets_are_candidates(self):
        with tempfile.TemporaryDirectory() as tmp:
            found = network_resets.classify(self.run_dir(tmp, [reset(2, 2), reset(1, 2, "backlog"), reset(2, 3),
                                                              reset(4, 2)]))
        whys = sorted(c["why"] for c in found["candidates"])
        self.assertEqual(whys, ["more starvation resets than the 1 stage(s)", "reason other than starvation",
                                "starvation at epoch 3, not the life's first reset"])

    def test_a_player_reset_twice_at_epoch_two_is_a_candidate(self):
        with tempfile.TemporaryDirectory() as tmp:
            found = network_resets.classify(self.run_dir(tmp, [reset(2, 2), reset(2, 2)], stages=2))
        self.assertEqual([c["why"] for c in found["candidates"]], ["the player's second reset"])
        self.assertEqual(found["tool_problems"], ["1 expected stop/resume reset(s) for 2 stage(s)"])

    def test_tool_problems(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(network_resets.classify(tmp)["tool_problems"], ["match-commands.jsonl missing"])
        with tempfile.TemporaryDirectory() as tmp:
            found = network_resets.classify(self.run_dir(tmp, [reset(2, 2)], dropped=3))
            self.assertIn("match-commands.jsonl dropped 3 event(s)", found["tool_problems"])
        with tempfile.TemporaryDirectory() as tmp:
            found = network_resets.classify(self.run_dir(tmp, [reset(2, 2)], end=False))
            self.assertIn("match-commands.jsonl does not end with one trace_end", found["tool_problems"])
        with tempfile.TemporaryDirectory() as tmp:
            found = network_resets.classify(self.run_dir(tmp, []))
            self.assertIn("0 expected stop/resume reset(s) for 1 stage(s)", found["tool_problems"])


if __name__ == "__main__":
    unittest.main()
