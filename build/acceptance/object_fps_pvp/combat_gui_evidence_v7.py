"""Analyzer v7 (D83): the GUI combat HUD rule whose meaning changed when Reconcile moved to the Client's
simulation role (v7 batch 04).

The frozen analyzer (combat_gui_evidence.py) is not modified. It runs unchanged on a private copy of the
four combat inputs, so the evidence directory is never rewritten; every one of its errors except its HUD
mismatch messages still decides. Only the HUD rule is re-evaluated, under ANALYZER_ID, from the same files.

v6 meaning -> v7 meaning:
- hud_tick. v6: the probe's hud_tick (the presented local movement's authorityTick) came from the
  snapshot the main thread had just reconciled, the same snapshot the HUD's HP/ammo/life came from. v7: the
  movement observation, and so hud_tick, comes from the snapshot the simulation role last reconciled on its
  own wake, while the HUD still reads the main thread's newest snapshot. The HUD's source tick is therefore
  in the interval below (see Bounds), no longer equal to hud_tick.
- HUD rule. v6: the HUD equals the per-life replay of the local player at hud_tick, with
  life_at(hud_tick) == hud_life and hud_tick <= snapshot_tick. v7: hud_tick <= snapshot_tick, and some
  tick t in [lower, snapshot_tick], lower = max(hud_tick, the previous row's snapshot_tick), has
  life_at(t) == hud_life and the replay of (local, hud_life) at t equals hud_hp and hud_ammo, with hud_dead
  exactly when that hp is 0; and each role's HUD source tick can be chosen non-decreasing.

Bounds. Upper: the HUD reads the main thread's snapshot before the probe records snapshot_tick after render.
Lower (by program order): snapshots only advance and this frame's refresh follows the previous row's record, so
the source is not older than the previous row's snapshot_tick. hud_tick as a lower bound is empirical only: the
simulation role keeps its own snapshot history, and a snapshot arriving between the main thread's refresh and
PresentAt can make hud_tick newer than the HUD's source; such a frame fails (a false failure, never a false
pass) and is reported as suspected_lower_bound_false_failure when every failing frame matches in
[previous snapshot_tick, hud_tick). A HUD value newer than snapshot_tick always fails.

The rule is looser than v6 by the interval width (snapshot_tick - lower), reported per run; a HUD lagging the
main thread's snapshot by no more than that is not detected. An interval-only match wider than
WIDE_INTERVAL_TICKS is a user flag.

Consistency: the replay is rebuilt from the same decisions as the frozen analyzer; its v6 verdict on every
presented frame must reproduce the frozen HUD messages exactly, otherwise the run fails.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import shutil
import tempfile

import combat_gui_evidence as frozen

ANALYZER_ID = "pvp-v7-combat-hud-2"
ROLES = ("create", "join")
INPUTS = tuple(f"{role}-combat{suffix}" for role in ROLES for suffix in (".json", "-hp.jsonl"))
HUD_ERROR = re.compile(r"^(create|join)-combat-hp\.jsonl: displayed HUD HP/ammo/life mismatch at frame (\d+)$")
RULE_V7 = ("hud_tick <= snapshot_tick, and some t in [max(hud_tick, previous row's snapshot_tick), snapshot_tick] has "
           "life_at(t) == hud_life and the per-life replay of the local player at t equals hud_hp and hud_ammo, hud_dead "
           "exactly when that hp is 0; the matched ticks of a role's presented frames admit a non-decreasing choice")
# User flag (not an error) for an interval-only match wider than this: presentation_evidence.py:17
# MAXIMUM_CROSSING_BRACKET_SECONDS = .1 (the runtime/presentation disturbance boundary) at the 60 Hz tick.
WIDE_INTERVAL_TICKS = 6


def frozen_result(directory, life_respawn_resets):
    """The frozen analyzer's verdict on a private copy of the four inputs (the evidence stays untouched)."""
    with tempfile.TemporaryDirectory(prefix="pvp-combat-gui-v7-") as temporary:
        copy = Path(temporary) / "run"
        copy.mkdir()
        for name in INPUTS:
            if (directory / name).is_file():
                shutil.copyfile(directory / name, copy / name)
        return frozen.analyze_combat_gui(copy, life_respawn_resets)


def read_inputs(directory):
    reports = {role: frozen._json((directory / f"{role}-combat.json").read_text(encoding="utf-8")) for role in ROLES}
    rows = {}
    for role in ROLES:
        with (directory / f"{role}-combat-hp.jsonl").open(encoding="utf-8") as source:
            rows[role] = [frozen._json(text) for text in source]
    return reports, rows


def replay_groups(reports, shooter):
    """The frozen analyzer's per-(player, life) decision groups, rebuilt from the same reports."""
    duration = frozen._number(reports["create"]["duration_seconds"], "duration_seconds")
    actions = [entry for entry in frozen.schedule(duration) if entry["step"] in frozen.SUBMITTING]
    submissions = {row["action_id"]: row for row in frozen._records(reports["create"]["submissions"], "submissions")}
    decisions = {row["action_id"]: row for row in frozen._records(reports["create"]["decisions"], "decisions")}
    groups = defaultdict(list)
    for action_id, entry in enumerate(actions, 1):
        decision = decisions.get(action_id)
        if submissions.get(action_id) is None or decision is None:
            continue
        hit = entry["step"] == "hit"
        replayed = {"action_id": action_id, "resolved_tick": frozen._integer(decision["resolved_tick"], "resolved_tick", 1),
                    "accepted": decision["accepted"], "action_kind": decision["action_kind"], "life_generation": 1,
                    "damage": frozen._integer(decision["damage"], "damage"), "target_id": decision["target_id"],
                    "target_life_generation": frozen.expected_target_life(entry) if hit else 0}
        groups[shooter, 1].append((shooter, replayed))
        if replayed["target_id"]:
            groups[replayed["target_id"], replayed["target_life_generation"]].append((shooter, replayed))
    return groups


def hud_frames(reports, rows):
    """Per presented frame: the v6 verdict, the ticks of [lower, snapshot_tick] the HUD matches, the width."""
    ids = {role: frozen._integer(reports[role]["local_id"], f"{role}.local_id", 1) for role in ROLES}
    groups = replay_groups(reports, ids["create"])
    lives = frozen.life_timeline(rows.values(), tuple(ids.values()))
    memo = {}

    def expected(player, life, tick):
        key = player, life, tick
        if key not in memo:
            memo[key] = frozen.combat_expected(player, life, tick, groups.get((player, life), []), frozen.RULES["maximum_hp"],
                                               frozen.RULES["magazine_capacity"], frozen.RULES["reload_ticks"])
        return memo[key]

    frames = []
    for role in ROLES:
        local, previous = ids[role], None
        for row in rows[role]:
            tick = frozen._integer(row["snapshot_tick"], f"{role}.snapshot_tick", 1)
            if row.get("presented") is not True:
                previous = tick
                continue
            frame = frozen._integer(row["frame_id"], f"{role}.frame_id")
            hud_tick = frozen._integer(row["hud_tick"], f"{role}.hud_tick", 1)
            hud_life = frozen._integer(row["hud_life"], f"{role}.hud_life", 1)

            def matches(t):
                want = expected(local, hud_life, t)
                return (frozen.life_at(lives[local], t) == hud_life and row["hud_hp"] == want["hp"] and
                        row["hud_ammo"] == want["ammo"] and row["hud_dead"] is (want["hp"] == 0))

            # The HUD's source is the main thread's snapshot of this frame: not newer than snapshot_tick, and not
            # older than the snapshot the previous row recorded after its render (snapshots only advance).
            lower = hud_tick if previous is None else max(hud_tick, previous)
            interval = range(lower, tick + 1)
            matched = [t for t in interval if matches(t)] if hud_tick <= tick else []
            states = {(expected(local, hud_life, t)["hp"], expected(local, hud_life, t)["ammo"]) for t in interval}
            passed = hud_tick <= tick and bool(matched)
            below = ([t for t in range(previous, hud_tick) if matches(t)]
                     if not passed and previous is not None and previous < hud_tick else [])
            frames.append({"role": role, "frame": frame, "snapshot_tick": tick, "hud_tick": hud_tick, "hud_life": hud_life,
                           "previous_snapshot_tick": previous, "lower_tick": lower, "hud_lag_ticks": tick - hud_tick,
                           "width_ticks": tick - lower, "v6_passed": hud_tick <= tick and matches(hud_tick),
                           "matched_ticks": matched, "matched_offsets": [t - hud_tick for t in matched],
                           "v7_passed": passed, "interval_states": len(states), "below_hud_tick_matches": below})
            previous = tick
    return frames


def source_order_violations(frames):
    """Frames where no matched tick keeps each role's HUD source tick non-decreasing (greedy: the smallest one that does)."""
    floor, violations = {}, []
    for f in frames:
        if not f["matched_ticks"]:
            continue
        candidates = [t for t in f["matched_ticks"] if t >= floor.get(f["role"], 0)]
        if not candidates:
            violations.append(f)
            continue
        floor[f["role"]] = min(candidates)
    return violations


def guarded_output(directory):
    """combat-gui-evidence-v7.json; refused inside a session whose files.sha256 would no longer match."""
    for parent in [directory, *directory.resolve().parents][:5]:
        if (parent / "files.sha256").exists():
            raise ValueError(f"refusing to write into {directory}: {parent / 'files.sha256'} lists this evidence")
    return directory / "combat-gui-evidence-v7.json"


def analyze_combat_gui_v7(directory, life_respawn_resets=None, write=False):
    """Frozen verdict minus its HUD messages, plus the v7 HUD rule; writes combat-gui-evidence-v7.json only if write."""
    directory = Path(directory)
    original = frozen_result(directory, life_respawn_resets)
    hud_errors = [error for error in original["errors"] if HUD_ERROR.match(error)]
    other_errors = [error for error in original["errors"] if not HUD_ERROR.match(error)]
    errors = list(other_errors)
    frames, hud, flags = [], {}, []
    try:
        reports, rows = read_inputs(directory)
        frames = hud_frames(reports, rows)
    except (OSError, ValueError, TypeError, KeyError, OverflowError) as error:
        errors.append(f"v7 HUD rule could not be evaluated: {error}")
    suspected = False
    if frames:
        flagged = sorted((m.group(1), int(m.group(2))) for m in map(HUD_ERROR.match, hud_errors))
        recomputed = sorted((f["role"], f["frame"]) for f in frames if not f["v6_passed"])
        if flagged != recomputed:
            errors.append("v7 replay does not reproduce the frozen analyzer's HUD verdict "
                          f"(frozen {len(flagged)} frame(s), recomputed {len(recomputed)})")
        consistent = not errors
        failed = [f for f in frames if not f["v7_passed"]]
        for f in failed:
            if f["hud_lag_ticks"] < 0:
                errors.append(f"{f['role']}-combat-hp.jsonl: hud_tick after snapshot_tick at frame {f['frame']}")
            else:
                errors.append(f"{f['role']}-combat-hp.jsonl: displayed HUD HP/ammo/life matches no tick of "
                              f"[max(hud_tick, previous snapshot_tick), snapshot_tick] at frame {f['frame']}")
        backwards = source_order_violations(frames)
        for f in backwards:
            errors.append(f"{f['role']}-combat-hp.jsonl: HUD source tick goes backwards at frame {f['frame']}")
        # Reported only (U6): every failing frame matches in [previous snapshot_tick, hud_tick), nothing else failed.
        suspected = bool(failed) and consistent and not backwards and all(f["below_hud_tick_matches"] for f in failed)
        only = [f for f in frames if f["v7_passed"] and not f["v6_passed"]]
        wide = [f for f in only if f["width_ticks"] > WIDE_INTERVAL_TICKS]
        flags = [f"{f['role']}-combat-hp.jsonl: HUD matches only inside a {f['width_ticks']}-tick interval "
                 f"(> {WIDE_INTERVAL_TICKS}) at frame {f['frame']}" for f in wide]
        widths, lags = Counter(f["width_ticks"] for f in frames), Counter(f["hud_lag_ticks"] for f in frames)
        hud = {"presented_frames": len(frames),
               "v6_mismatch_frames": len(recomputed),
               "interval_only_frames": only,
               "wide_interval_only_frames": wide,
               "failed_frames": failed[:20],
               "source_order_violations": len(backwards),
               "hud_tick_below_previous_snapshot": sum(f["previous_snapshot_tick"] is not None and
                                                       f["hud_tick"] < f["previous_snapshot_tick"] for f in frames),
               "width_ticks_distribution": {str(k): v for k, v in sorted(widths.items())},
               "hud_lag_ticks_distribution": {str(k): v for k, v in sorted(lags.items())},
               # Where the v7 rule is looser than v6: the interval holds more than one replayed HP/ammo state.
               "frames_with_state_change_in_interval": sum(f["interval_states"] > 1 for f in frames),
               "max_width_ticks": max(widths)}
    elif not errors:
        errors.append("no presented HUD frame")
    result = {"analyzer_id": ANALYZER_ID, "passed": not errors, "errors": errors, "flags": flags,
              "suspected_lower_bound_false_failure": suspected,
              "rules": {"hud": RULE_V7, "other": "every frozen combat_gui_evidence error except its HUD mismatch messages",
                        "flag": f"an interval-only match wider than {WIDE_INTERVAL_TICKS} ticks (user flag, not an error)"},
              "frozen": {"passed": original["passed"], "errors": original["errors"]},
              "frozen_hud_errors": hud_errors, "other_errors": other_errors, "hud": hud,
              "life_respawns_verified": original.get("life_respawns_verified")}
    if write:
        guarded_output(directory).write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return result


def round_resets(directory):
    """The LifeRespawn resets run_timing.py passed to the frozen analyzer, from round.json when present."""
    path = Path(directory) / "round.json"
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8")).get("life_respawn_resets")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", type=Path, help="a GUI combat round (round-N); its round.json supplies the resets")
    parser.add_argument("--write", action="store_true",
                        help="write combat-gui-evidence-v7.json (refused inside a session listed by files.sha256)")
    arguments = parser.parse_args()
    outcome = analyze_combat_gui_v7(arguments.directory, round_resets(arguments.directory), arguments.write)
    print(json.dumps(outcome, indent=2, allow_nan=False))
    raise SystemExit(0 if outcome["passed"] else 1)
