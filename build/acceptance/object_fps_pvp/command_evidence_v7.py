"""Analyzer v7: the command rules whose meaning changed when command generation
moved to the Client's simulation role (v7 batch 04).

The frozen analyzer (command_evidence.py) is not modified and still runs on
every round; its verdict on these rules is listed, never decisive. This file
re-evaluates only the rules below, under ANALYZER_ID, from the same traces, and
writes command-evidence-v7.json beside the frozen command-evidence.json.

v6 meaning -> v7 meaning:
- runtime_gap. v6: a Client runtime_gap came from the frame (the main thread's
  frame interval, or time a frame dropped). v7: only the simulation role emits
  it, for its own wake interval (frame_seconds) of 100 ms or more, or for time it
  dropped or blocked. A main-thread stall no longer produces one; it shows only
  as a presentation interval (GUI) or the probe's frame gap (timing.json).
- Seed clamp. The first step after a seed keeps at most one tick, so it drops
  frame_seconds - one tick (frozen formula, unchanged). v6: frame_seconds was a
  display frame, exempt below 100 ms. v7: it is the simulation's own interval,
  at most about 1.33 ticks plus its wake lateness, so a clamp is exempt only
  below two ticks; a longer one means the simulation itself woke late and stays
  a simulation gap.
- 60 Hz production (+-2 boundary steps, +3 per LifeRespawn). v6: skipped after
  any runtime gap, frame gap of 100 ms or injected stall, since a stalled frame
  stopped generation. v7: the simulation generates at 60 Hz whatever the main
  thread does, so only a simulation gap skips the check; main-thread stalls,
  injected or observed, do not.
- STALL_RULE. See STALL_RULE_V7.
- Main-thread stall (new). A round with an injected main-thread stall must show
  no substituted (held or neutral) resolution from the stall's start until
  STALL_RECOVERY_SECONDS after its release, and no epoch reset other than
  LifeRespawn in the measurement window.
"""
import argparse
from collections import Counter
import json
from pathlib import Path

import command_evidence as frozen

ANALYZER_ID = "pvp-v7-commands-1"
TICK_SECONDS = frozen.TICK_SECONDS
SEED_CLAMP_MAXIMUM_FRAME_SECONDS = 2 * TICK_SECONDS
STALL_RECOVERY_SECONDS = 1.5
STALL_RULE_V7 = ("a simulation step interval of 100 ms or more, or time the simulation dropped or blocked; "
                 "the role catches up at most five fixed steps per run, so a wake gap of about 83 ms already "
                 "drops time. Main-thread frames never drop simulation time")


def is_match_trace(path):
    return Path(path).name.startswith("match")


def stall_length_ns(directory, timing):
    """The injected main-thread stall's length, from the run manifest."""
    manifest = Path(directory) / "run-manifest.json"
    if manifest.exists():
        value = json.loads(manifest.read_text(encoding="utf-8")).get("stall_ms")
        if isinstance(value, (int, float)) and not isinstance(value, bool) and value > 0:
            return int(value * 1_000_000)
    value = timing.get("stall_ms")
    if isinstance(value, (int, float)) and not isinstance(value, bool) and value > 0:
        return int(value * 1_000_000)
    return None


def split_seed_clamps(gaps, life_resets):
    """The frozen clamp matching, offered only gaps shorter than two ticks."""
    eligible = [gap for gap in gaps if gap["frame_seconds"] < SEED_CLAMP_MAXIMUM_FRAME_SECONDS]
    late = [gap for gap in gaps if gap["frame_seconds"] >= SEED_CLAMP_MAXIMUM_FRAME_SECONDS]
    clamps, unmatched = frozen.match_life_seed_clamps(eligible, life_resets)
    return clamps, sorted(unmatched + late, key=lambda gap: gap["time_ns"])


def analyze_v7(directory):
    directory = Path(directory)
    start, end, timing = frozen.measurement_window(directory)
    errors = []
    simulation_gaps, host_gaps, resets, all_life_resets = [], [], [], []
    presentation_stalls = 0
    generated = Counter()
    substituted = []
    files = sorted(directory.glob("*commands.jsonl"))
    for path in files:
        match = is_match_trace(path)
        try:
            for event in frozen.read_trace_events(path):
                kind, timestamp = event["kind"], event["time_ns"]
                measured = start <= timestamp < end
                if kind == "runtime_gap" and measured and (event["frame_seconds"] >= .1 or event["dropped_seconds"] > 0):
                    (host_gaps if match else simulation_gaps).append(event)
                elif kind == "generated" and measured and not event["seeded_neutral"]:
                    generated[event["player_id"]] += 1
                elif kind == "reset":
                    if measured:
                        resets.append(event)
                    if event.get("reset_reason") == "life_respawn":
                        all_life_resets.append(event)
                elif kind == "resolved" and event["source"] in ("held", "neutral"):
                    substituted.append(event)
                elif kind == "presentation" and measured and event["frame_seconds"] >= .1:
                    presentation_stalls += 1
        except ValueError as exc:
            errors.append(str(exc))
    if not any(is_match_trace(path) for path in files) or not any(not is_match_trace(path) for path in files):
        errors.append("Missing Match or Client trace")
    seed_clamps, simulation_gaps = split_seed_clamps(simulation_gaps, all_life_resets)
    life_resets = [reset for reset in resets if reset.get("reset_reason") == "life_respawn"]
    other_resets = [reset for reset in resets if reset.get("reset_reason") != "life_respawn"]

    expected_players = timing.get("player_ids", sorted(generated))
    expected_count = (end - start) * 60 / 1e9
    respawns = Counter(reset["player_id"] for reset in life_resets)
    allowance = {player: 2 + frozen.LIFE_RESPAWN_PRODUCTION_STEPS * respawns[player] for player in expected_players}
    production_applies = not simulation_gaps
    production_ok = bool(expected_players) and all(
        abs(generated[player] - expected_count) <= allowance[player] for player in expected_players)
    if production_applies and not production_ok:
        errors.append("Command production differs from 60 Hz by more than its allowance without a simulation gap")

    stall = None
    if timing.get("injected"):
        release = timing.get("release_ns", 0)
        length = stall_length_ns(directory, timing)
        if not release or length is None:
            errors.append("Injected main-thread stall without its release time and length")
        else:
            begin, until = release - length, release + int(STALL_RECOVERY_SECONDS * 1e9)
            inside = [event for event in substituted if begin <= event["time_ns"] <= until]
            stall = {"start_ns": begin, "release_ns": release, "until_ns": until,
                     "substituted": len(inside), "substituted_sources": dict(Counter(e["source"] for e in inside)),
                     "resets_other_than_life_respawn": len(other_resets)}
            if inside:
                errors.append(f"A main-thread stall substituted {len(inside)} command(s) (held or neutral)")
            if other_resets:
                errors.append(f"A main-thread stall run reset {len(other_resets)} epoch(s) other than LifeRespawn")
            stall["passed"] = not inside and not other_resets

    result = {
        "analyzer_id": ANALYZER_ID, "passed": not errors, "errors": errors, "window_ns": [start, end],
        "rules": {
            "runtime_gap": "Client runtime_gap events come only from the simulation role",
            "seed_clamp": f"dropped == frame - one tick, frame below {SEED_CLAMP_MAXIMUM_FRAME_SECONDS:.6f} s",
            "production": "60 Hz +-2 boundary steps (+3 per LifeRespawn); skipped only after a simulation gap",
            "stall_rule": STALL_RULE_V7,
            "main_thread_stall": f"no held/neutral resolution from the stall start to {STALL_RECOVERY_SECONDS} s "
                                 "after release; no non-LifeRespawn epoch reset"},
        "simulation_gaps": simulation_gaps, "host_gaps": host_gaps, "seed_clamps": seed_clamps,
        "disturbed_simulation": bool(simulation_gaps),
        "main_thread_frame_gaps": {"probe_gaps_100ms": timing.get("gaps_100ms", 0),
                                   "presentation_intervals_100ms": presentation_stalls},
        "production": {"applies": production_applies, "passed": production_ok,
                       "commands_per_player": {str(k): v for k, v in generated.items()},
                       "expected_per_player": expected_count,
                       "allowance_steps": {str(k): v for k, v in allowance.items()}},
        "main_thread_stall": stall,
    }
    (directory / "command-evidence-v7.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", type=Path)
    arguments = parser.parse_args()
    outcome = analyze_v7(arguments.directory)
    print(json.dumps(outcome, indent=2))
    raise SystemExit(0 if outcome["passed"] else 1)
