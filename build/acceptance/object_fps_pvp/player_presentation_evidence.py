"""Successful two-GUI character submissions and distance-driven phase evidence."""
import argparse
import json
import math
from pathlib import Path

from run_weapon_short import rank


def validate_samples(samples):
    """Validate observed geometry/phase, independently of the scripted inputs."""
    errors, pairs, holds, backward, resets = [], 0, 0, 0, 0
    previous = None
    anchors = {}
    for sample in samples:
        remote = sample.get("remote")
        if not remote:
            previous = None
            continue
        c = remote["character"]
        identity = remote["player_id"]
        position = remote["position"]
        if c["player_id"] != identity or not c["ready"] or c["prepared_instances"] < 1:
            errors.append("Character identity/readiness does not match successful remote submission")
        if min(c["body_meshes"], c["hair_meshes"], c["weapon_meshes"], c["upper_body_mask_count"]) <= 0:
            errors.append("Successful remote pose lacks body, buns, hand weapon or upper-body mask")
        numbers = position + c["foot_anchor"] + c["weapon_world_position"] + [
            c[name] for name in ("scale", "cycle_distance", "jog_weight", "phase_cycles", "unwrapped_phase_cycles",
                                "signed_distance", "total_distance", "distance_delta", "speed", "playback_rate")]
        if (not all(math.isfinite(value) for value in numbers) or c["scale"] <= 0 or
                c["cycle_distance"] <= 0 or not 0 <= c["jog_weight"] <= 1):
            errors.append("Nonfinite or invalid character geometry/phase")
            previous = None
            continue
        anchor = c["foot_anchor"] + [c["scale"]]
        if identity in anchors and math.dist(anchor, anchors[identity]) > 1e-6:
            errors.append("Reference-pose foot anchor/scale changed with animation")
        anchors[identity] = anchor
        if c["phase_reset"]:
            resets += 1
            if abs(c["distance_delta"]) > 1e-6 or not c["reset_reason"]:
                errors.append("Phase reset consumed a discontinuous displacement or omitted its reason")
            if any(abs(c[name]) > 1e-6 for name in ("phase_cycles", "unwrapped_phase_cycles", "signed_distance", "total_distance")):
                errors.append("Phase reset retained old phase or accumulated path distance")
        if previous and previous["remote"]["player_id"] == identity:
            old = previous["remote"]["character"]
            if c["reset_count"] == old["reset_count"] and not c["phase_reset"]:
                phase_delta = c["unwrapped_phase_cycles"] - old["unwrapped_phase_cycles"]
                signed_delta = c["signed_distance"] - old["signed_distance"]
                distance_delta = c["total_distance"] - old["total_distance"]
                # The gait phase advances by the frame's own blended cycle distance.
                if abs(phase_delta - signed_delta / c["cycle_distance"]) > 2e-5:
                    errors.append("Gait phase is not proportional to actual signed displacement")
                if distance_delta < -1e-6 or abs(signed_delta) > distance_delta + 2e-5:
                    errors.append("Accumulated locomotion distance is inconsistent")
                geometric_delta = math.dist(position, previous["remote"]["position"])
                consecutive = sample["frame_id"] == previous["frame_id"] + 1
                if consecutive and geometric_delta < 1e-7 and abs(phase_delta) > 2e-5:
                    errors.append("Stationary or pure-turn pose continued advancing gait phase")
                if c["holding"] and old["holding"]:
                    holds += 1
                    if abs(phase_delta) > 2e-5 or geometric_delta > 2e-5:
                        errors.append("Timeline hold moved the character or walked in place")
                if consecutive and c["backward"] and c["distance_delta"] > 1e-5:
                    backward += 1
                    if phase_delta >= 0:
                        errors.append("Backward displacement did not reverse the existing gait phase")
                # Consecutive successful renders must agree with the transform
                # actually submitted. Across a skipped render, use accumulated
                # distance instead of treating the last per-attempt delta as all.
                if consecutive and abs(c["distance_delta"] - geometric_delta) > 2e-4:
                    errors.append("Locomotion used a distance different from the submitted position")
                pairs += 1
        previous = sample
    return {"errors": list(dict.fromkeys(errors)), "validated_pairs": pairs,
            "held_pairs": holds, "backward_pairs": backward, "phase_reset_frames": resets}


def analyze(directory):
    directory = Path(directory)
    result = {"passed": False, "scope": "12-second same-host two-GUI character regression; v4 gameplay, not v5 certification.",
              "method": "Successful Presented positions and owning character pose/phase. Gait cycles = actual signed distance / the blended walk/jog cycle distance of each frame.",
              "direction_limit": "Sideways uses the forward walk/jog approximation; backwards samples it in reverse. No dedicated direction clips or IK.",
              "clients": {}, "errors": []}
    try:
        reports = {role: json.loads((directory / f"{role}-player.json").read_text()) for role in ("create", "join")}
        traces = {role: [json.loads(line) for line in (directory / f"{role}-player-frames.jsonl").read_text().splitlines()]
                  for role in reports}
        for role, frames in traces.items():
            report = reports[role]
            if not report["passed"]:
                result["errors"].append(f"{role}: {report.get('error', 'probe failed')}")
            shown = [frame for frame in frames if frame["presented"]]
            stamps = [frame["presented_seconds"] for frame in shown]
            intervals = [b-a for a, b in zip(stamps, stamps[1:])]
            if not shown or any(not math.isfinite(stamp) or stamp <= 0 for stamp in stamps) or any(gap <= 0 for gap in intervals):
                raise ValueError(f"{role}: missing or nonmonotonic successful presentations")
            checks = validate_samples(shown)
            result["errors"].extend(f"{role}: {error}" for error in checks["errors"])
            stable = [frame for frame in shown if 0 <= frame["seconds"] <= 9]
            fps = ((len(stable)-1) / (stable[-1]["presented_seconds"]-stable[0]["presented_seconds"])) if len(stable)>1 else 0
            if report["capture"] == "none" and fps < report["nominal_fps"] * .85:
                result["errors"].append(f"{role}: measured FPS {fps:.2f} below existing short-probe 85% cadence gate")
            first_seen = {}
            for frame in shown:
                if frame.get("remote"):
                    first_seen.setdefault(str(frame["remote"]["player_id"]), frame)
            costs = {}
            for key in ("update_ms", "render_ms", "prepare_world_ms", "remote_submit_ms"):
                values = [frame[key] for frame in shown]
                costs[key] = {"p50": rank(values, .5), "p95": rank(values, .95), "maximum": max(values)}
            if report["capture"] == "none" and any(frame["update_ms"] + frame["render_ms"] >= 100 for frame in first_seen.values()):
                result["errors"].append(f"{role}: first visible character crossed existing 100ms disturbance boundary")
            result["clients"][role] = {**checks, "nominal_fps": report["nominal_fps"], "measured_mean_fps_0_to_9s": fps,
                "full_run_successful_frames": len(shown), "all_frame_intervals_seconds": intervals,
                "frame_interval_p50_seconds": rank(intervals,.5), "frame_interval_p95_seconds": rank(intervals,.95),
                "maximum_frame_interval_seconds": max(intervals, default=None), "costs_ms": costs,
                "first_seen_characters": {key: {name: frame[name] for name in
                    ("frame_id", "seconds", "update_ms", "render_ms", "prepare_world_ms", "remote_submit_ms", "remote")}
                    for key, frame in first_seen.items()}, "initialize_graphics_ms": report["initialize_graphics_ms"]}
        observer = [frame for frame in traces["join"] if frame["presented"]]
        segments = {"forward": (.95,1.5,1), "backward": (2.2,2.8,-1), "strafe": (3.3,3.62,1),
                    "diagonal": (4.1,4.42,1), "pure_turn": (4.85,5.05,0), "wall_stop": (7.5,7.78,0)}
        result["segments"] = {}
        for name,(begin,end,sign) in segments.items():
            samples = [frame for frame in observer if begin <= frame["seconds"] <= end and frame.get("remote")]
            if len(samples) < 2:
                result["errors"].append(f"{name}: too few successful poses")
                continue
            a,b = samples[0]["remote"]["character"],samples[-1]["remote"]["character"]
            delta = b["unwrapped_phase_cycles"]-a["unwrapped_phase_cycles"]
            distance = b["total_distance"]-a["total_distance"]
            result["segments"][name] = {"samples":len(samples),"distance":distance,"phase_delta_cycles":delta,
                "cycles_per_meter": abs(delta)/distance if distance>1e-6 else None,
                "cycle_distance": b["cycle_distance"], "jog_weight": b["jog_weight"]}
            # Like the cadence gate and path crossings, segment phase is timing
            # evidence: GPU readback frames reanchor the gait in capture runs.
            if reports["join"]["capture"] == "none" and (
                    (sign == 0 and (abs(delta)>2e-4 or distance>2e-4)) or (sign and sign*delta <= .015)):
                result["errors"].append(f"{name}: displacement-driven stop/direction evidence failed")
        # Compare identical observed path lengths at different presentation FPS,
        # rather than equating the duration of a scripted key hold with distance.
        forward = [frame["remote"]["character"] for frame in observer if .65 <= frame["seconds"] <= 1.85 and frame.get("remote")]
        crossings = {}
        if forward:
            origin = forward[0]
            for distance in (.5,1.0,1.5):
                for a,b in zip(forward,forward[1:]):
                    if a["reset_count"] != origin["reset_count"] or b["reset_count"] != origin["reset_count"]:
                        continue
                    before,after=a["total_distance"]-origin["total_distance"],b["total_distance"]-origin["total_distance"]
                    if before < distance <= after:
                        fraction=(distance-before)/(after-before)
                        phase=a["unwrapped_phase_cycles"]+fraction*(b["unwrapped_phase_cycles"]-a["unwrapped_phase_cycles"])-origin["unwrapped_phase_cycles"]
                        crossings[str(distance)]={"cycles":phase,"cycle_distance":b["cycle_distance"],"jog_weight":b["jog_weight"]}
                        break
        result["equal_distance_forward_phase"] = crossings
        if reports["join"]["capture"] == "none" and len(crossings)!=3:
            result["errors"].append("Forward segment did not observe all predeclared 0.5/1.0/1.5-unit path crossings")
        held = [frame for frame in observer if 8.2 <= frame["seconds"] <= 8.65 and
                frame.get("remote") and frame["remote"]["character"]["holding"]]
        if len(held) < 2:
            result["errors"].append("Controlled snapshot hold did not produce at least two successful held poses")
        resumed = [frame for frame in observer if 8.65 <= frame["seconds"] < 9.15 and frame.get("remote") and
                   not frame["remote"]["character"]["holding"]]
        if not resumed:
            result["errors"].append("No successful resumed pose after controlled snapshot hold")
        result["controlled_hold"] = {"held_frames":len(held),"resumed_frames":len(resumed)}
        old_id,new_id = reports["create"]["initial_player_id"],reports["create"].get("rejoined_player_id")
        absence = [frame for frame in observer if 9.2 <= frame["seconds"] <= 10.2 and frame.get("remote") is None]
        new_samples = [frame for frame in observer if frame.get("remote") and frame["remote"]["player_id"] == new_id]
        if not new_id or new_id == old_id or not absence or not new_samples:
            result["errors"].append("Leave/rejoin did not remove the old model and introduce a distinct player")
        elif abs(new_samples[0]["remote"]["character"]["total_distance"]) > 1e-5:
            result["errors"].append("Rejoined player's first visible pose retained old displacement phase")
        result["lifecycle"] = {"old_player_id":old_id,"new_player_id":new_id,"absent_presented_frames":len(absence)}
        if reports["join"]["mode"] == "player-capture":
            result["captures"] = reports["join"]["captures"]
    except (OSError,ValueError,KeyError,TypeError,IndexError,ZeroDivisionError) as error:
        result["errors"].append(str(error))
    result["passed"] = not result["errors"]
    (directory/"player-presentation-evidence.json").write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory",type=Path)
    result = analyze(parser.parse_args().directory)
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if result["passed"] else 1)
