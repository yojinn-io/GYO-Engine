"""Self-test of analyzer v7 (command_evidence_v7.py) on synthetic traces."""

import json
from pathlib import Path
import tempfile
import unittest

import command_evidence as frozen
import command_evidence_v7 as v7

START = 1_000_000_000
SECOND = 1_000_000_000
DURATION = 4
TICK_NS = SECOND // 60


def event(kind, timestamp, player=1, sequence=1, epoch=1, **values):
    result = {"kind": kind, "time_ns": int(timestamp), "player_id": player,
              "epoch": epoch, "sequence": sequence, "authority_tick": sequence,
              "source": "none", "queued": 2, "pending": 3, "seeded_neutral": False,
              "frame_seconds": 0, "dropped_seconds": 0, "count": 0, "age_seconds": 0}
    result.update(values)
    return result


class AnalyzerV7Tests(unittest.TestCase):
    def run_directory(self, *, missing=(), release=None, stall_ms=None, gaps_100ms=0,
                      client_extra=(), match_extra=(), source=None):
        """Two players generating at 60 Hz for DURATION seconds, all resolved Actual."""
        temporary = tempfile.TemporaryDirectory(prefix="pvp-command-evidence-v7-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        timing = {"start_ns": START, "end_ns": START + DURATION * SECOND, "duration": DURATION, "fps": 30,
                  "player_ids": [1, 2], "gaps_100ms": gaps_100ms, "history_overflows": 0,
                  "injected": release is not None, "release_ns": release or 0, "recovery_client_passed": True}
        client, match = list(client_extra), list(match_extra)
        for player in (1, 2):
            for index in range(DURATION * 60):
                generated = START + index * SECOND // 60 + 1_000_000
                if (player, index) in missing:
                    continue
                sequence = index + 1
                client.append(event("generated", generated, player, sequence))
                client.append(event("sent", generated + 1_000_000, player, sequence))
                match.append(event("host_accepted", generated + 5_000_000, player, sequence))
                resolved = generated + 30_000_000
                match.append(event("resolved", resolved, player, sequence,
                                   source=source(resolved) if source else "actual"))
        (directory / "timing.json").write_text(json.dumps(timing), encoding="utf-8")
        if stall_ms is not None:
            (directory / "run-manifest.json").write_text(json.dumps({"stall_ms": stall_ms}), encoding="utf-8")
        for name, records in (("clients-commands.jsonl", client), ("match-commands.jsonl", match)):
            ordered = sorted(records, key=lambda record: record["time_ns"])
            with (directory / name).open("w", encoding="utf-8") as output:
                for record in ordered:
                    output.write(json.dumps(record) + "\n")
                output.write(json.dumps({"kind": "trace_end", "events": len(ordered), "dropped": 0}) + "\n")
        return directory

    def test_clean_round_passes_with_the_analyzer_id(self):
        result = v7.analyze_v7(self.run_directory())
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["analyzer_id"], v7.ANALYZER_ID)
        self.assertTrue(result["production"]["applies"])
        self.assertTrue(result["production"]["passed"])

    def test_main_thread_frame_gaps_no_longer_skip_the_production_check(self):
        missing = {(1, index) for index in range(100, 110)}
        directory = self.run_directory(missing=missing, gaps_100ms=1)
        result = v7.analyze_v7(directory)
        self.assertTrue(result["production"]["applies"])
        self.assertFalse(result["passed"])
        self.assertIn("Command production differs from 60 Hz", " ".join(result["errors"]))
        # The frozen analyzer skips its check after a frame gap.
        self.assertNotIn("Clean-run command production", " ".join(frozen.analyze_commands(directory, enforce=False)["errors"]))

    def test_a_simulation_gap_skips_the_production_check(self):
        missing = {(1, index) for index in range(100, 110)}
        gap = event("runtime_gap", START + 2 * SECOND, 1, 100, frame_seconds=.12, dropped_seconds=.037)
        result = v7.analyze_v7(self.run_directory(missing=missing, client_extra=[gap]))
        self.assertFalse(result["production"]["applies"])
        self.assertTrue(result["disturbed_simulation"])
        self.assertEqual(len(result["simulation_gaps"]), 1)
        self.assertTrue(result["passed"], result["errors"])

    def test_a_host_gap_is_not_a_simulation_gap(self):
        host = event("runtime_gap", START + 2 * SECOND, 0, 0, frame_seconds=.12)
        result = v7.analyze_v7(self.run_directory(match_extra=[host]))
        self.assertEqual(len(result["host_gaps"]), 1)
        self.assertFalse(result["disturbed_simulation"])
        self.assertTrue(result["production"]["applies"])

    def test_a_seed_clamp_of_two_ticks_or_more_is_listed_as_a_late_wake(self):
        tick = 1 / 60
        for frame, late in ((tick + .0005, False), (2 * tick - .0001, False), (2 * tick, True), (.05, True)):
            with self.subTest(frame=frame):
                # The session's first seed: sequence and pending are the lead plus one step.
                clamp = event("runtime_gap", START + SECOND, 1, 3, pending=3, life_generation=1,
                              frame_seconds=frame, dropped_seconds=frame - tick)
                result = v7.analyze_v7(self.run_directory(client_extra=[clamp]))
                # The dropped time is discarded on purpose: exempt as in the frozen rule.
                self.assertEqual(len(result["seed_clamps"]), 1)
                self.assertEqual(len(result["simulation_gaps"]), 0)
                self.assertTrue(result["production"]["applies"])
                self.assertEqual(len(result["late_seed_clamps"]), 1 if late else 0)
        # A clamp past 100 ms is no clamp at all.
        long = event("runtime_gap", START + SECOND, 1, 3, pending=3, life_generation=1,
                     frame_seconds=.12, dropped_seconds=.12 - tick)
        self.assertEqual(len(v7.analyze_v7(self.run_directory(client_extra=[long]))["simulation_gaps"]), 1)

    def test_an_injected_main_thread_stall_must_not_substitute_or_reset(self):
        release = START + 2 * SECOND
        clean = v7.analyze_v7(self.run_directory(release=release, stall_ms=250))
        self.assertTrue(clean["passed"], clean["errors"])
        self.assertTrue(clean["main_thread_stall"]["passed"])
        inside = release + SECOND
        held = v7.analyze_v7(self.run_directory(
            release=release, stall_ms=250,
            source=lambda resolved: "held" if abs(resolved - inside) < TICK_NS // 2 else "actual"))
        self.assertFalse(held["passed"])
        # One resolution of each player inside the window.
        self.assertEqual(held["main_thread_stall"]["substituted"], 2)
        self.assertEqual(held["main_thread_stall"]["substituted_sources"], {"held": 2})
        outside = release + 2 * SECOND
        late = v7.analyze_v7(self.run_directory(
            release=release, stall_ms=250,
            source=lambda resolved: "neutral" if abs(resolved - outside) < TICK_NS // 2 else "actual"))
        self.assertEqual(late["main_thread_stall"]["substituted"], 0)
        reset = event("reset", START + 3 * SECOND, 1, 0, 2, reset_reason="starvation")
        reset_run = v7.analyze_v7(self.run_directory(release=release, stall_ms=250, match_extra=[reset]))
        self.assertFalse(reset_run["passed"])
        self.assertEqual(reset_run["main_thread_stall"]["resets_other_than_life_respawn"], 1)

    def test_an_injected_stall_needs_its_length(self):
        result = v7.analyze_v7(self.run_directory(release=START + 2 * SECOND))
        self.assertFalse(result["passed"])
        self.assertIn("without its release time and length", " ".join(result["errors"]))

    def gameplay_round(self, mode, client_extra=()):
        """A gameplay matrix round: no timing.json; action-client.json bounds the run."""
        directory = self.run_directory(client_extra=client_extra)
        (directory / "timing.json").unlink()
        (directory / "action-client.json").write_text(json.dumps(
            {"start_ns": START, "end_ns": START + DURATION * SECOND, "player_ids": [1, 2], "fps": 30}), encoding="utf-8")
        (directory / "result.json").write_text(json.dumps({"mode": mode}), encoding="utf-8")
        return directory

    def test_a_gameplay_round_checks_simulation_gaps_only_when_clean(self):
        tick = 1 / 60
        late = event("runtime_gap", START + 2 * SECOND, 1, 50, frame_seconds=.12, dropped_seconds=.12 - 5 * tick)
        clean = v7.analyze_v7(self.gameplay_round("baseline", [late]))
        self.assertEqual(clean["round"], "gameplay")
        self.assertFalse(clean["production"]["applies"])
        self.assertFalse(clean["passed"])
        self.assertIn("Clean gameplay round", " ".join(clean["errors"]))
        faulted = v7.analyze_v7(self.gameplay_round("drain-stall", [late]))
        self.assertTrue(faulted["passed"], faulted["errors"])
        self.assertEqual(len(faulted["simulation_gaps"]), 1)
        punctual = event("runtime_gap", START + SECOND, 1, 3, pending=3, life_generation=1,
                         frame_seconds=tick + .0004, dropped_seconds=.0004)
        self.assertTrue(v7.analyze_v7(self.gameplay_round("baseline", [punctual]))["passed"])

    def test_the_result_is_written_beside_the_frozen_evidence(self):
        directory = self.run_directory()
        v7.analyze_v7(directory)
        written = json.loads((directory / "command-evidence-v7.json").read_text(encoding="utf-8"))
        self.assertEqual(written["analyzer_id"], v7.ANALYZER_ID)


if __name__ == "__main__":
    unittest.main()
