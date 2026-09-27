"""Bounded PvP GUI combat evidence; successful Render return is not scanout."""

import argparse
import bisect
import json
import math
from pathlib import Path


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


def analyze_combat_gui(directory):
    """Keep every predeclared shot in the denominator, and always save a result."""
    directory = Path(directory)
    result = {
        "schema_version": 1, "passed": False, "full_acceptance": False,
        "scope": "Same-host real SDL input and successful Presented frames; timestamps end at Render return, not monitor scanout or input-to-photon. This evidence alone is not complete v4 acceptance.",
        "criteria": {"maximum_decision_arrival_p95_seconds": .150,
                     "required_legal_acceptance_rate": 1.0,
                     "feedback": "next successful Presented frame",
                     "authoritative_execution_time": "not measured by this GUI trace; separate action probe required"},
        "errors": [], "roles": {},
    }
    errors = result["errors"]
    try:
        reports = {role: _json((directory / f"{role}-combat.json").read_text(encoding="utf-8"))
                   for role in ("create", "join")}
        ids = {role: _integer(report["local_id"], f"{role}.local_id", 1)
               for role, report in reports.items()}
        if ids["create"] == ids["join"]:
            raise ValueError("both roles have the same player identity")
        duration = _number(reports["create"]["duration_seconds"], "duration_seconds")
        if duration not in (16, 120):
            raise ValueError("duration_seconds must be explicit 16-second short or 120-second integration mode")
        planned = math.ceil((duration - .2) / .8)
        result.update(duration_seconds=duration, planned_shots=planned,
                      integration_120_seconds=duration == 120,
                      sources=[f"{role}-combat{suffix}" for role in reports
                               for suffix in (".json", "-hp.jsonl")])
        all_decisions = []
        feedback_by_role = {}
        for role, report in reports.items():
            expected = planned if role == "create" else 0
            label = f"{role} combat"
            if report["schema_version"] != 1 or report["enabled"] is not True:
                raise ValueError(f"{label}: unsupported schema or disabled combat")
            if report["duration_seconds"] != duration:
                raise ValueError(f"{label}: inconsistent duration")
            if report["maximum_hp"] != 100 or report["damage_per_hit"] != 25:
                raise ValueError(f"{label}: observed rules do not match fixed v4 acceptance contract")
            if report["expected_target_id"] != ids["join" if role == "create" else "create"]:
                raise ValueError(f"{label}: unexpected target identity")
            if not isinstance(report["errors"], list) or report["errors"]:
                errors.append(f"{label}: probe errors {report['errors']!r}")
            for name in ("planned_shots", "dispatched_shots", "submitted_shots", "decision_count", "accepted", "animations"):
                if _integer(report[name], f"{label}.{name}") != expected:
                    errors.append(f"{label}: {name} must equal all {expected} predeclared shots")
            if _integer(report["rejected"], f"{label}.rejected") != 0:
                errors.append(f"{label}: legal shooting was rejected")
            _integer(report["skipped_presented"], f"{label}.skipped_presented")
            if report["hp_at_end"] != (100 if role == "create" else 0):
                errors.append(f"{label}: wrong final local HP")
            transport = report["last_action_transport"]
            for name in ("allocated", "acknowledged", "retired"):
                if _integer(transport[name], f"{label}.transport.{name}") != expected:
                    errors.append(f"{label}: transport {name} did not account for all planned actions")
            for name in ("pending_requests", "retained_decisions", "unconsumed", "protocol_errors"):
                if _integer(transport[name], f"{label}.transport.{name}") != 0:
                    errors.append(f"{label}: transport {name} did not settle to zero")
            if (_integer(transport["max_datagram_bytes"], f"{label}.max_datagram_bytes") > 1200 or
                    _integer(transport["max_batch_shots"], f"{label}.max_batch_shots") > 8):
                errors.append(f"{label}: action transport exceeded v4 datagram/batch limits")
            submissions = _records(report["submissions"], f"{label}.submissions")
            decisions = _records(report["decisions"], f"{label}.decisions")
            feedback = _records(report["first_feedback"], f"{label}.first_feedback")
            feedback_by_role[role] = feedback
            wanted = set(range(1, expected + 1))
            indexes = []
            for name, rows in (("submissions", submissions), ("decisions", decisions), ("first_feedback", feedback)):
                keys = [_integer(row["action_id"], f"{label}.{name}.action_id", 1) for row in rows]
                if len(keys) != len(set(keys)) or set(keys) != wanted:
                    errors.append(f"{label}: {name} must contain each planned ActionId exactly once")
                indexes.append({row["action_id"]: row for row in rows})
            submission_index, decision_index, _ = indexes
            previous_time = None
            for row in submissions:
                stamp = _number(row["generated_seconds"], f"{label}.generated_seconds")
                if previous_time is not None and stamp <= previous_time:
                    errors.append(f"{label}: submissions are not strictly time ordered")
                previous_time = stamp
            arrivals = []
            for action_id in range(1, expected + 1):
                request, decision = submission_index.get(action_id), decision_index.get(action_id)
                if request is None or decision is None:
                    arrivals.append(math.inf)
                    continue
                delay = (_number(decision["received_seconds"], f"{label}.received_seconds") -
                         _number(request["generated_seconds"], f"{label}.generated_seconds"))
                if delay < 0:
                    errors.append(f"{label}: decision predates action {action_id}")
                arrivals.append(delay)
                tick = _integer(decision["resolved_tick"], f"{label}.resolved_tick", 1)
                damage = _integer(decision["damage"], f"{label}.damage")
                if (decision["accepted"] is not True or decision["rejection"] != 0 or
                        decision["hit_kind"] != 2 or decision["target_id"] != ids["join"] or
                        damage != (25 if action_id <= 4 else 0)):
                    errors.append(f"{label}: action {action_id} is not the expected legal Player hit")
                all_decisions.append((tick, decision["target_id"], damage))
            p95 = _rank(arrivals, .95)
            if expected and (p95 is None or p95 > .150):
                errors.append(f"{label}: all-planned decision arrival P95 exceeds 150 ms")
            result["roles"][role] = {
                "planned_shots": expected, "submitted_shots": len(submissions),
                "unique_decisions": len(decision_index),
                "infinite_arrival_count": sum(not math.isfinite(x) for x in arrivals),
                "decision_arrival_p50_seconds": _rank(arrivals, .5),
                "decision_arrival_p95_seconds": p95,
                "skipped_presented": report["skipped_presented"],
            }

        def hp_at(player, tick):
            return 100 - sum(damage for resolved, target, damage in all_decisions
                             if target == player and resolved <= tick)

        result.update(positive_damage_decisions=sum(damage > 0 for _, _, damage in all_decisions),
                      total_damage=sum(damage for _, _, damage in all_decisions),
                      expected_final_target_hp=0)
        if result["positive_damage_decisions"] != 4 or result["total_damage"] != 100:
            errors.append("Expected exactly four unique positive-damage decisions totalling 100 HP")
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
            frames, previous_frame, previous_time, previous_tick = [], -1, -1., -1
            presented_rows = {}
            for row in rows:
                frame = _integer(row["frame_id"], f"{path.name}.frame_id")
                stamp = _number(row["host_seconds"], f"{path.name}.host_seconds")
                tick = _integer(row["snapshot_tick"], f"{path.name}.snapshot_tick", 1)
                if frame <= previous_frame or stamp <= previous_time or tick < previous_tick:
                    raise ValueError(f"{path.name}: unordered frame/time/snapshot history")
                previous_frame, previous_time, previous_tick = frame, stamp, tick
                if row["local_id"] != ids[role]:
                    raise ValueError(f"{path.name}: changed local identity")
                combat = _records(row["combat"], f"{path.name}.combat")
                player_ids = [_integer(state["player_id"], f"{path.name}.player_id", 1) for state in combat]
                if len(player_ids) != 2 or set(player_ids) != set(ids.values()):
                    raise ValueError(f"{path.name}: missing or duplicate player combat state")
                for state in combat:
                    if _integer(state["hp"], f"{path.name}.hp") != hp_at(state["player_id"], tick):
                        errors.append(f"{path.name}: snapshot HP mismatch at frame {frame}, tick {tick}")
                if type(row["presented"]) is not bool:
                    raise ValueError(f"{path.name}: invalid presented flag")
                if row["presented"]:
                    hud_tick = _integer(row["hud_tick"], f"{path.name}.hud_tick", 1)
                    if hud_tick > tick or _integer(row["hud_hp"], f"{path.name}.hud_hp") != hp_at(ids[role], hud_tick):
                        errors.append(f"{path.name}: displayed HUD HP mismatch at frame {frame}")
                    frames.append(frame)
                    presented_rows[frame] = row
            if rows[-1]["host_seconds"] - rows[0]["host_seconds"] < duration - .1:
                errors.append(f"{path.name}: HP history does not cover the declared measurement duration")
            feedback_delays = []
            submissions = {row["action_id"]: row for row in report["submissions"]}
            for item in feedback_by_role[role]:
                action_id = item["action_id"]
                submitted_frame = _integer(item["submission_frame_id"], f"{role}.submission_frame_id")
                frame = _integer(item["frame_id"], f"{role}.feedback.frame_id")
                index = bisect.bisect_left(frames, submitted_frame)
                if (index == len(frames) or frames[index] != frame or item["shooting"] is not True or
                        item["presented_action_id"] != action_id):
                    errors.append(f"{role}: action {action_id} lacks its animation on the next successful Presented frame")
                stamp = _number(item["presented_seconds"], f"{role}.presented_seconds")
                submitted = _number(item["submitted_seconds"], f"{role}.submitted_seconds")
                if (action_id not in submissions or
                        abs(submitted - submissions[action_id]["generated_seconds"]) > 1e-6 or
                        submitted_frame != submissions[action_id]["submission_frame_id"]):
                    errors.append(f"{role}: action {action_id} feedback does not match its submission")
                if stamp < submitted or frame not in presented_rows or stamp > presented_rows[frame]["host_seconds"] + 1e-6:
                    errors.append(f"{role}: action {action_id} has invalid presentation timestamps")
                feedback_delays.append(stamp - submitted)
            result["roles"][role].update(
                hp_samples=len(rows), presented_hp_samples=len(frames),
                feedback_samples=len(feedback_delays),
                feedback_p95_seconds=_rank(feedback_delays, .95),
                feedback_max_seconds=max(feedback_delays, default=None))
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
