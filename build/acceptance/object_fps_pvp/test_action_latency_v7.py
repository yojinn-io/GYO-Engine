"""Synthetic gameplay records for action_latency_v7.py: two accepted actions, one rejected, one without a worker stamp."""
import json
from pathlib import Path
import tempfile
import unittest

import action_latency_v7 as v7

TICK = int(1e9 / 60)


def write_case(directory, with_received=True):
    base = 10 * TICK
    actions = [
        {"kind": "submitted", "time_ns": base, "player_id": 1, "request": {"action_id": 1}},
        {"kind": "submitted", "time_ns": base + 2 * TICK, "player_id": 1, "request": {"action_id": 2}},
        {"kind": "submitted", "time_ns": base + 3 * TICK, "player_id": 1, "request": {"action_id": 3}},
        {"kind": "decision", "time_ns": base + 4 * TICK, "player_id": 1, **({"received_ns": base + 3 * TICK} if with_received else {}),
         "decision": {"action_id": 1, "rejection": 0, "resolved_tick": 11}},
        {"kind": "decision", "time_ns": base + 6 * TICK, "player_id": 1, "received_ns": base + 5 * TICK,
         "decision": {"action_id": 2, "rejection": 0, "resolved_tick": 13}},
        {"kind": "decision", "time_ns": base + 6 * TICK, "player_id": 1, "decision": {"action_id": 3, "rejection": 3, "resolved_tick": 14}},
    ]
    (directory / "actions.jsonl").write_text("\n".join(json.dumps(e) for e in actions) + "\n", encoding="utf-8")
    match = [{"kind": "snapshot_produced", "authority_tick": t, "time_ns": t * TICK} for t in range(9, 20)]
    (directory / "match-commands.jsonl").write_text("\n".join(json.dumps(e) for e in match) + "\n", encoding="utf-8")
    relay = {"players": {"77": 1}, "events": [
        {"event": "received", "upstream": False, "kind": 7, "session": 77, "time_ns": base + 2 * TICK, "decisions": [{"action_id": 1}]},
        {"event": "received", "upstream": False, "kind": 7, "session": 77, "time_ns": base + 3 * TICK, "decisions": [{"action_id": 1}]},
        {"event": "received", "upstream": False, "kind": 7, "session": 77, "time_ns": base + 4 * TICK + TICK // 2, "decisions": [{"action_id": 2}]},
    ]}
    (directory / "relay.json").write_text(json.dumps(relay), encoding="utf-8")


class ActionLatencyV7Tests(unittest.TestCase):
    def case(self, **options):
        directory = Path(self.enterContext(tempfile.TemporaryDirectory()))
        write_case(directory, **options)
        return v7.analyze(directory)

    def test_accepted_actions_are_timed_against_tick_relay_worker_and_frame(self):
        summary = self.case()
        self.assertEqual(summary["actions"], 2)  # the rejected action is left out
        ms = TICK / 1e6
        self.assertAlmostEqual(summary["to_tick_ms"]["p50"], 1 * ms, places=6)
        self.assertAlmostEqual(summary["to_tick_ms"]["p95"], 1 * ms, places=6)
        # The first relay sighting counts, not the resend.
        self.assertAlmostEqual(summary["to_relay_down_ms"]["p50"], 2 * ms, places=6)
        self.assertAlmostEqual(summary["to_relay_down_ms"]["p95"], 2.5 * ms, places=6)
        self.assertAlmostEqual(summary["to_worker_ms"]["p95"], 3 * ms, places=6)
        self.assertAlmostEqual(summary["to_frame_ms"]["p95"], 4 * ms, places=6)
        self.assertAlmostEqual(summary["frame_phase_vs_tick_ms"]["p50"], 0, places=3)

    def test_records_without_worker_stamps_leave_that_measure_out(self):
        summary = self.case(with_received=False)
        self.assertEqual(summary["to_worker_ms"]["count"], 1)
        self.assertEqual(summary["to_frame_ms"]["count"], 2)


if __name__ == "__main__":
    unittest.main()
