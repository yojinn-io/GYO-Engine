"""Bounded PvP v5 GUI combat evidence; successful Render return is not scanout.

The creator follows a predeclared 17-slot SDL schedule (combat_latency.hpp): four
hits kill the joiner, corpse shots land through the 180-tick death wait, four
more hits kill the respawned life, the twelfth round is spent, an empty-magazine
click and a reloading click stay local, and R reloads. Every action keeps its
place in the denominator; HP, ammo and reload state are replayed per life.
"""

import argparse
import bisect
from collections import Counter, defaultdict
import json
import math
from pathlib import Path

from gameplay_evidence import combat_expected

CYCLE = (["hit"] * 4 + ["dead_target_shot"] * 3 + ["idle"] + ["hit"] * 4 +
         ["dead_target_shot", "empty_click", "reload", "reloading_click", "idle"])
SLOT_START, SLOT_PERIOD = .2, .8
SUBMITTING = {"hit": 0, "dead_target_shot": 0, "reload": 1}  # step -> action kind
LOCAL_ONLY = {"empty_click", "reloading_click"}
RULES = {"maximum_hp": 100, "damage_per_hit": 25, "magazine_capacity": 12, "cooldown_ticks": 10,
         "reload_ticks": 90, "respawn_ticks": 180}
COMBAT_FIELDS = ("hp", "ammo", "reload_action_id", "reload_start_tick", "reload_end_tick", "last_shot_id", "last_shot_tick")
PLAYER_HIT = 2


def _reject_constant(value):
    raise ValueError(f"non-finite JSON constant: {value}")


def _json(text):
    return json.loads(text, parse_constant=_reject_constant)


def _integer(value, name, minimum=0):
    if type(value) is not int or value < minimum:
        raise ValueError(f"{name}: expected integer >= {minimum}")
    return value


def _number(value, name):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError(f"{name}: expected finite number")
    return value


def _records(value, name):
    if not isinstance(value, list) or any(not isinstance(row, dict) for row in value):
        raise ValueError(f"{name}: expected record array")
    return value


def _rank(values, quantile):
    if not values:
        return None
    result = sorted(values)[math.ceil(len(values) * quantile) - 1]
    return result if math.isfinite(result) else None


def schedule(duration):
    """The predeclared slots, rebuilt independently of the probe's own report."""
    slots, slot = [], 0
    while SLOT_START + slot * SLOT_PERIOD < duration:
        cycle, index = divmod(slot, len(CYCLE))
        slots.append({"slot": slot, "cycle": cycle, "cycle_slot": index, "step": CYCLE[index],
                      "due_seconds": SLOT_START + slot * SLOT_PERIOD})
        slot += 1
    return slots


def expected_target_life(entry):
    """Two lethal series per cycle: life 1+2c before slot 8, 2+2c from it."""
    return 1 + 2 * entry["cycle"] + (entry["cycle_slot"] >= 8)


def life_timeline(rows_by_role, players):
    """Per player: life -> first/last sample and death; from both Clients' authoritative snapshots."""
    lives = {player: {} for player in players}
    for rows in rows_by_role:
        for row in rows:
            tick = row["snapshot_tick"]
            for state in _records(row["players"], "players"):
                player, life = state["player_id"], _integer(state["life_generation"], "life_generation", 1)
                if player not in lives:
                    raise ValueError("unknown player in life history")
                entry = lives[player].setdefault(life, {"first_tick": tick, "first": state, "last_tick": tick, "dead": None,
                                                        "death_ticks": set(), "waits": set(), "epochs": set()})
                if tick < entry["first_tick"]:
                    entry.update(first_tick=tick, first=state)
                entry["last_tick"] = max(entry["last_tick"], tick)
                entry["epochs"].add(state["epoch"])
                if state["life_state"] == 1:
                    entry["death_ticks"].add(state["life_state_tick"])
                    entry["waits"].add(state["respawn_tick"] - state["life_state_tick"])
                    if entry["dead"] is None or tick < entry["dead"][0]:
                        entry["dead"] = tick, state
    return lives


def life_at(lives, tick):
    """The life whose start (respawn tick, or 0 for life 1) is the latest at or before tick."""
    current = None
    for life, entry in sorted(lives.items()):
        start = 0 if life == 1 else entry["first"]["life_state_tick"]
        if start <= tick:
            current = life
    return current


def analyze_combat_gui(directory, life_respawn_resets=None):
    """Keep every predeclared action in the denominator, and always save a result.

    ``life_respawn_resets`` is command_evidence's list of the run's LifeRespawn
    resets; when given, it must equal the respawns seen in the HP history."""
    directory = Path(directory)
    result = {
        "schema_version": 2, "protocol": 5, "passed": False, "full_acceptance": False,
        "scope": "Same-host real SDL input and successful Presented frames; timestamps end at Render return, not monitor scanout or input-to-photon. This evidence alone is not complete v5 acceptance.",
        "criteria": {"maximum_decision_arrival_p95_seconds": .150,
                     "verdicts": "every predeclared action has its exact v5 verdict; empty and reloading clicks are never submitted",
                     "feedback": "next successful Presented frame for every submitted shot",
                     "per_life": "snapshot and HUD HP/ammo/reload equal the per-life replay of unique decisions",
                     "authoritative_execution_time": "not measured by this GUI trace; separate action probe required"},
        "errors": [], "roles": {},
    }
    errors = result["errors"]
    def check(ok, message):
        if not ok and message not in errors:
            errors.append(message)
    try:
        reports = {role: _json((directory / f"{role}-combat.json").read_text(encoding="utf-8"))
                   for role in ("create", "join")}
        ids = {role: _integer(report["local_id"], f"{role}.local_id", 1) for role, report in reports.items()}
        if ids["create"] == ids["join"]:
            raise ValueError("both roles have the same player identity")
        shooter, target = ids["create"], ids["join"]
        duration = _number(reports["create"]["duration_seconds"], "duration_seconds")
        if duration not in (16, 120):
            raise ValueError("duration_seconds must be explicit 16-second short or 120-second integration mode")
        slots = schedule(duration)
        actions = [entry for entry in slots if entry["step"] in SUBMITTING]
        shots = [entry for entry in actions if SUBMITTING[entry["step"]] == 0]
        local_clicks = sum(entry["step"] in LOCAL_ONLY for entry in slots)
        lethal = [entry for entry in slots if entry["step"] == "hit" and entry["cycle_slot"] in (3, 11)]
        result.update(duration_seconds=duration, planned_slots=len(slots), planned_actions=len(actions),
                      planned_shots=len(shots), planned_local_clicks=local_clicks, planned_deaths=len(lethal),
                      integration_120_seconds=duration == 120,
                      sources=[f"{role}-combat{suffix}" for role in reports for suffix in (".json", "-hp.jsonl")])
        for role, report in reports.items():
            mover, label = role == "create", f"{role} combat"
            if report["schema_version"] != 2 or report.get("protocol") != 5 or report["enabled"] is not True:
                raise ValueError(f"{label}: unsupported schema or disabled combat")
            if report["duration_seconds"] != duration:
                raise ValueError(f"{label}: inconsistent duration")
            if any(report[name] != value for name, value in RULES.items()):
                raise ValueError(f"{label}: observed rules do not match the v5 contract")
            if report["expected_target_id"] != ids["join" if mover else "create"]:
                raise ValueError(f"{label}: unexpected target identity")
            if not isinstance(report["errors"], list) or report["errors"]:
                errors.append(f"{label}: probe errors {report['errors']!r}")
            if mover:
                declared = [(row["slot"], row["step"]) for row in _records(report["schedule"], f"{label}.schedule")]
                check(declared == [(entry["slot"], entry["step"]) for entry in slots],
                      f"{label}: probe schedule differs from the predeclared v5 schedule")
            expected = {"planned_slots": len(slots) if mover else 0, "dispatched_slots": len(slots) if mover else 0,
                        "planned_actions": len(actions) if mover else 0, "submitted_actions": len(actions) if mover else 0,
                        "decision_count": len(actions) if mover else 0, "accepted": len(actions) if mover else 0,
                        "rejected": 0, "submitted_shots": len(shots) if mover else 0, "animations": len(shots) if mover else 0,
                        "planned_local_clicks": local_clicks if mover else 0, "suppressed_clicks": local_clicks if mover else 0}
            for name, value in expected.items():
                if _integer(report[name], f"{label}.{name}") != value:
                    errors.append(f"{label}: {name} must equal {value}")
            _integer(report["skipped_presented"], f"{label}.skipped_presented")
            transport = report["last_action_transport"]
            for name in ("allocated", "acknowledged", "retired"):
                if _integer(transport[name], f"{label}.transport.{name}") != expected["planned_actions"]:
                    errors.append(f"{label}: transport {name} did not account for all planned actions")
            for name in ("pending_requests", "retained_decisions", "unconsumed", "protocol_errors"):
                if _integer(transport[name], f"{label}.transport.{name}") != 0:
                    errors.append(f"{label}: transport {name} did not settle to zero")
            if (_integer(transport["max_datagram_bytes"], f"{label}.max_datagram_bytes") > 1200 or
                    _integer(transport["max_batch_shots"], f"{label}.max_batch_shots") > 8):
                errors.append(f"{label}: action transport exceeded datagram/batch limits")

        # Submissions and decisions: action IDs are allocated in schedule order.
        report = reports["create"]
        submissions = _records(report["submissions"], "create.submissions")
        decisions = _records(report["decisions"], "create.decisions")
        feedback = _records(report["first_feedback"], "create.first_feedback")
        submission_index, decision_index = {}, {}
        for name, rows, index in (("submissions", submissions, submission_index), ("decisions", decisions, decision_index)):
            for row in rows:
                key = _integer(row["action_id"], f"create.{name}.action_id", 1)
                check(key not in index, f"create: {name} repeat an ActionId")
                index[key] = row
            check(set(index) == set(range(1, len(actions) + 1)), f"create: {name} must contain each planned ActionId exactly once")
        previous, arrivals, verdicts, replay = None, [], Counter(), []
        hits_by_tick = []
        for action_id, entry in enumerate(actions, 1):
            request, decision = submission_index.get(action_id), decision_index.get(action_id)
            if request is not None:
                stamp = _number(request["generated_seconds"], "create.generated_seconds")
                check(previous is None or stamp > previous, "create: submissions are not strictly time ordered")
                previous = stamp
                check(request["slot"] == entry["slot"] and request["action_kind"] == SUBMITTING[entry["step"]],
                      f"create: action {action_id} was not submitted by slot {entry['slot']} ({entry['step']})")
            if request is None or decision is None:
                arrivals.append(math.inf)
                continue
            delay = _number(decision["received_seconds"], "create.received_seconds") - request["generated_seconds"]
            check(delay >= 0, f"create: decision predates action {action_id}")
            arrivals.append(delay)
            tick = _integer(decision["resolved_tick"], "create.resolved_tick", 1)
            damage = _integer(decision["damage"], "create.damage")
            step = entry["step"]
            verdicts[f"{step}:{decision['rejection']}:{decision['hit_kind']}"] += 1
            common = decision["accepted"] is True and decision["rejection"] == 0 and decision["action_kind"] == SUBMITTING[step]
            if step == "hit":
                ok = common and decision["hit_kind"] == PLAYER_HIT and decision["target_id"] == target and damage == 25
                hits_by_tick.append((tick, action_id, expected_target_life(entry)))
            elif step == "dead_target_shot":
                ok = common and decision["hit_kind"] != PLAYER_HIT and decision["target_id"] == 0 and damage == 0
            else:
                ok = common and decision["target_id"] == 0 and damage == 0
            check(ok, f"create: action {action_id} ({step}, slot {entry['slot']}) has the wrong v5 verdict")
            replay.append((shooter, {"action_id": action_id, "resolved_tick": tick, "accepted": decision["accepted"],
                                     "action_kind": decision["action_kind"], "life_generation": 1, "damage": damage,
                                     "target_id": decision["target_id"],
                                     "target_life_generation": expected_target_life(entry) if step == "hit" else 0}))
        p95 = _rank(arrivals, .95)
        if p95 is None or p95 > .150:
            errors.append("create: all-planned decision arrival P95 exceeds 150 ms")
        result["verdicts"] = dict(verdicts)

        # HP histories: ordering, per-life replay of every snapshot and the displayed HUD.
        rows_by_role = {}
        for role, report in reports.items():
            path = directory / f"{role}-combat-hp.jsonl"
            rows = []
            with path.open(encoding="utf-8") as source:
                for line, text in enumerate(source, 1):
                    row = _json(text)
                    if not isinstance(row, dict):
                        raise ValueError(f"{path.name}:{line}: expected object")
                    rows.append(row)
            if len(rows) < 2:
                raise ValueError(f"{path.name}: empty or insufficient HP history")
            if len(rows) != _integer(report["hp_frame_count"], f"{path.name}.hp_frame_count"):
                raise ValueError(f"{path.name}: HP history length does not match the retained frame count")
            rows_by_role[role] = rows
        lives = life_timeline(rows_by_role.values(), (shooter, target))
        for tick, action_id, life in hits_by_tick:
            check(life_at(lives[target], tick) == life, f"create: hit {action_id} landed outside its predeclared target life")
        groups = defaultdict(list)
        for owner, decision in replay:
            groups[owner, decision["life_generation"]].append((owner, decision))
            if decision["target_id"]:
                groups[decision["target_id"], decision["target_life_generation"]].append((owner, decision))
        memo = {}
        def expected_state(player, life, tick):
            key = player, life, tick
            if key not in memo:
                memo[key] = combat_expected(player, life, tick, groups.get((player, life), []), RULES["maximum_hp"],
                                            RULES["magazine_capacity"], RULES["reload_ticks"])
            return memo[key]
        feedback_delays = []
        for role, rows in rows_by_role.items():
            name = f"{role}-combat-hp.jsonl"
            presented, previous_frame, previous_time, previous_tick = {}, -1, -1., -1
            for row in rows:
                frame = _integer(row["frame_id"], f"{name}.frame_id")
                stamp = _number(row["host_seconds"], f"{name}.host_seconds")
                tick = _integer(row["snapshot_tick"], f"{name}.snapshot_tick", 1)
                if frame <= previous_frame or stamp <= previous_time or tick < previous_tick:
                    raise ValueError(f"{name}: unordered frame/time/snapshot history")
                previous_frame, previous_time, previous_tick = frame, stamp, tick
                if row["local_id"] != ids[role]:
                    raise ValueError(f"{name}: changed local identity")
                combat = _records(row["combat"], f"{name}.combat")
                states = {state["player_id"]: state for state in _records(row["players"], f"{name}.players")}
                if len(combat) != 2 or {state["player_id"] for state in combat} != set(ids.values()) or set(states) != set(ids.values()):
                    raise ValueError(f"{name}: missing or duplicate player combat/life state")
                for state in combat:
                    player, life = state["player_id"], _integer(state["life_generation"], f"{name}.life_generation", 1)
                    check(life == states[player]["life_generation"], f"{name}: combat and player life differ")
                    want = expected_state(player, life, tick)
                    if any(state[field] != want[field] for field in COMBAT_FIELDS):
                        errors.append(f"{name}: snapshot combat state differs from the per-life replay at frame {frame}, tick {tick}")
                if type(row["presented"]) is not bool:
                    raise ValueError(f"{name}: invalid presented flag")
                if row["presented"]:
                    hud_tick = _integer(row["hud_tick"], f"{name}.hud_tick", 1)
                    hud_life = _integer(row["hud_life"], f"{name}.hud_life", 1)
                    want = expected_state(ids[role], hud_life, hud_tick)
                    if (hud_tick > tick or life_at(lives[ids[role]], hud_tick) != hud_life or
                            row["hud_hp"] != want["hp"] or row["hud_ammo"] != want["ammo"] or
                            row["hud_dead"] is not (want["hp"] == 0)):
                        errors.append(f"{name}: displayed HUD HP/ammo/life mismatch at frame {frame}")
                    presented[frame] = row
            if rows[-1]["host_seconds"] - rows[0]["host_seconds"] < duration - .1:
                errors.append(f"{name}: HP history does not cover the declared measurement duration")
            if role == "create":
                frames = sorted(presented)
                for item in feedback:
                    action_id = _integer(item["action_id"], "create.feedback.action_id", 1)
                    submitted_frame = _integer(item["submission_frame_id"], "create.submission_frame_id")
                    frame = _integer(item["frame_id"], "create.feedback.frame_id")
                    index = bisect.bisect_left(frames, submitted_frame)
                    if (index == len(frames) or frames[index] != frame or item["shooting"] is not True or
                            item["presented_action_id"] != action_id):
                        errors.append(f"create: action {action_id} lacks its animation on the next successful Presented frame")
                    request = submission_index.get(action_id)
                    stamp = _number(item["presented_seconds"], "create.presented_seconds")
                    submitted = _number(item["submitted_seconds"], "create.submitted_seconds")
                    if (request is None or request["action_kind"] != 0 or
                            abs(submitted - request["generated_seconds"]) > 1e-6 or
                            submitted_frame != request["submission_frame_id"]):
                        errors.append(f"create: action {action_id} feedback does not match a submitted shot")
                    if stamp < submitted or frame not in presented or stamp > presented[frame]["host_seconds"] + 1e-6:
                        errors.append(f"create: action {action_id} has invalid presentation timestamps")
                    feedback_delays.append(stamp - submitted)
                check(len(feedback) == len(shots) and len({item["action_id"] for item in feedback}) == len(shots),
                      "create: every submitted shot needs exactly one next-Presented feedback")
            result["roles"][role] = {"hp_samples": len(rows), "presented_hp_samples": len(presented)}
        result["roles"]["create"].update(
            submitted_actions=len(submissions), unique_decisions=len(decision_index),
            infinite_arrival_count=sum(not math.isfinite(x) for x in arrivals),
            decision_arrival_p50_seconds=_rank(arrivals, .5), decision_arrival_p95_seconds=p95,
            feedback_samples=len(feedback_delays), feedback_p95_seconds=_rank(feedback_delays, .95),
            feedback_max_seconds=max(feedback_delays, default=None))

        # Lives: the shooter never dies; the target dies once per lethal series and respawns 180 ticks later.
        check(set(lives[shooter]) == {1} and lives[shooter][1]["dead"] is None, "create: the shooter changed life or died")
        target_lives = lives[target]
        observed = sorted(target_lives)
        check(observed == list(range(1, len(observed) + 1)), "join: life generations are not contiguous from 1")
        deaths, respawns = [], []
        last_tick = max(row["snapshot_tick"] for rows in rows_by_role.values() for row in rows)
        for life in observed:
            entry = target_lives[life]
            if entry["dead"]:
                state = entry["dead"][1]
                deaths.append({"life": life, "tick": state["life_state_tick"], "respawn_tick": state["respawn_tick"]})
                check(entry["waits"] == {RULES["respawn_ticks"]}, "join: death wait is not 180 authority ticks")
                check(len(entry["death_ticks"]) == 1, "join: more than one death within a life")
            elif life != observed[-1]:
                errors.append(f"join: life {life} ended without an observed death")
            if life > 1:
                old = target_lives.get(life - 1, {})
                first = entry["first"]
                if old.get("dead"):
                    check(first["life_state_tick"] >= old["dead"][1]["respawn_tick"], "join: respawn occurred before deadline")
                    check(first["epoch"] > max(old["epochs"]), "join: respawn did not establish a new movement epoch")
                respawns.append({"player_id": target, "life_generation": life, "authority_tick": first["life_state_tick"]})
        unrespawned = [d for d in deaths if d["life"] == observed[-1]]
        check(len(deaths) == len(lethal), f"join: {len(deaths)} deaths differ from {len(lethal)} predeclared lethal series")
        check(not unrespawned or unrespawned[0]["respawn_tick"] > last_tick,
              "join: final death passed its respawn deadline without a new life")
        result.update(deaths=len(deaths), respawns=len(respawns), final_target_life=observed[-1],
                      final_target_dead_awaiting_respawn=bool(unrespawned))
        if life_respawn_resets is not None:
            reported = sorted((r["player_id"], r["life_generation"], r["authority_tick"]) for r in life_respawn_resets)
            seen = sorted((r["player_id"], r["life_generation"], r["authority_tick"]) for r in respawns)
            check(reported == seen, "Host LifeRespawn resets differ from the observed deaths and new lives")
            result["life_respawns_verified"] = reported == seen
    except (OSError, ValueError, TypeError, KeyError, OverflowError) as error:
        errors.append(str(error))
    result["passed"] = not errors
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "combat-gui-evidence.json").write_text(
        json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    arguments = parser.parse_args()
    evidence = analyze_combat_gui(arguments.directory)
    print(json.dumps(evidence, indent=2, allow_nan=False))
    raise SystemExit(0 if evidence["passed"] else 1)
