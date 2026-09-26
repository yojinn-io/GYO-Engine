"""Measure same-host cross-window movement delay from the real GUI probes."""

import argparse
import csv
import json
import math
from pathlib import Path
import platform
import statistics


THRESHOLDS = (0.25, 0.5, 0.75, 1.0, 1.25)
MINIMUM_MATCHES = 3
MINIMUM_DELAY_SECONDS = -0.02  # Sampling/interpolation tolerance, not clock adjustment.
MAXIMUM_MEDIAN_SECONDS = 0.15
MAXIMUM_CROSSING_BRACKET_SECONDS = .1  # Existing runtime/presentation disturbance boundary.
FIELDS = {"host_steady_seconds", "local_id", "local_x", "local_z",
          "remote_id", "remote_x", "remote_z"}


def _mover_id(report):
    values = [line.partition("=")[2] for line in report.read_text(encoding="utf-8").splitlines()
              if line.partition("=")[0] == "player_id"]
    if len(values) != 1 or int(values[0]) <= 0:
        raise ValueError(f"{report.name} must identify exactly one positive player_id")
    return int(values[0])


def _samples(path, mover, remote):
    samples = []
    previous_time = None
    with path.open(newline="", encoding="utf-8") as source:
        rows = csv.DictReader(source)
        if not FIELDS.issubset(rows.fieldnames or ()):
            raise ValueError(f"{path.name}: missing presentation trace columns")
        for line, row in enumerate(rows, 2):
            timestamp = float(row["host_steady_seconds"])
            if not math.isfinite(timestamp) or timestamp <= 0 or (
                    previous_time is not None and timestamp <= previous_time):
                raise ValueError(f"{path.name}:{line}: timestamps must be finite, positive and strictly increasing")
            previous_time = timestamp
            local_id, remote_id = int(row["local_id"]), int(row["remote_id"])
            identity = remote_id if remote else local_id
            if identity == 0:
                continue  # Lobby or a frame before the other player joined.
            if identity != mover or (remote and (local_id <= 0 or local_id == mover)):
                raise ValueError(f"{path.name}:{line}: pose does not identify the expected mover")
            prefix = "remote" if remote else "local"
            x, z = float(row[prefix + "_x"]), float(row[prefix + "_z"])
            if not math.isfinite(x) or not math.isfinite(z):
                raise ValueError(f"{path.name}:{line}: non-finite movement pose")
            samples.append((timestamp, x, z))
    if len(samples) < 2:
        raise ValueError(f"{path.name}: fewer than two matching movement samples")
    return samples


def _progress(samples, origin):
    return [(timestamp, math.hypot(x - origin[0], z - origin[1]))
            for timestamp, x, z in samples]


def _crossing(samples, threshold):
    # Require an observed bracket, rather than inventing a crossing before the
    # first frame. Straight probe movement can still contain small corrections.
    for (before_time, before), (after_time, after) in zip(samples, samples[1:]):
        if before < threshold <= after:
            fraction = (threshold - before) / (after - before)
            return before_time + fraction * (after_time - before_time)
    return None


def _maximum_backstep(samples):
    return max((max(0.0, before[1] - after[1])
                for before, after in zip(samples, samples[1:])), default=0.0)


def analyze_presentation(directory):
    """Write evidence even on validation failure; the caller checks ``passed``."""
    directory = Path(directory)
    evidence = {
        "passed": False,
        "method": "First positive displacement crossing, linearly interpolated between submitted camera/remote draw positions timestamped after Renderer::Render returns; remote crossing minus local crossing. Not monitor scanout or input-to-photon timing.",
        "platform": platform.system(),
        "scope": "Two GUI processes on the same host using the same steady clock, with localhost TCP/UDP/HTTP. The current Linux run compares a host-wide monotonic clock; no network clock synchronization is implied. Not physical LAN or cross-host latency evidence.",
        "origin": "First create-process local render position; planar distance in arena world units.",
        "sources": ["create-report.txt", "create-presentation.csv", "join-presentation.csv"],
        "criteria": {"minimum_matched_thresholds": MINIMUM_MATCHES,
                     "minimum_delay_seconds": MINIMUM_DELAY_SECONDS,
                     "maximum_median_delay_seconds": MAXIMUM_MEDIAN_SECONDS},
        "errors": [],
    }
    try:
        mover = _mover_id(directory / "create-report.txt")
        local = _samples(directory / "create-presentation.csv", mover, remote=False)
        remote = _samples(directory / "join-presentation.csv", mover, remote=True)
        origin = local[0][1:]
        local_progress, remote_progress = _progress(local, origin), _progress(remote, origin)
        evidence.update({"mover_player_id": mover, "origin_x": origin[0], "origin_z": origin[1],
                         "local_samples": len(local), "remote_samples": len(remote),
                         "maximum_local_backstep_units": _maximum_backstep(local_progress),
                         "maximum_remote_backstep_units": _maximum_backstep(remote_progress)})
        if remote_progress[0][1] > 0.02:
            evidence["errors"].append("Remote trace did not observe the mover at the starting position")
        thresholds, delays = [], []
        previous_local = previous_remote = None
        for threshold in THRESHOLDS:
            local_time = _crossing(local_progress, threshold)
            remote_time = _crossing(remote_progress, threshold)
            delay = None
            if local_time is not None and remote_time is not None:
                if ((previous_local is not None and local_time <= previous_local) or
                        (previous_remote is not None and remote_time <= previous_remote)):
                    evidence["errors"].append("Positive displacement crossings were not strictly ordered")
                previous_local, previous_remote = local_time, remote_time
                delay = remote_time - local_time
                delays.append(delay)
                if delay < MINIMUM_DELAY_SECONDS:
                    evidence["errors"].append(f"Remote crossed {threshold} units more than 20 ms before local")
            thresholds.append({"displacement_units": threshold,
                               "local_crossing_host_seconds": local_time,
                               "remote_crossing_host_seconds": remote_time,
                               "delay_seconds": delay})
        median = statistics.median(delays) if delays else None
        evidence.update({"thresholds": thresholds, "matched_threshold_count": len(delays),
                         "mean_delay_seconds": statistics.mean(delays) if delays else None,
                         "median_delay_seconds": median,
                         "maximum_delay_seconds": max(delays) if delays else None})
        if len(delays) < MINIMUM_MATCHES:
            evidence["errors"].append(f"Only {len(delays)} matched displacement thresholds; need {MINIMUM_MATCHES}")
        if median is not None and median > MAXIMUM_MEDIAN_SECONDS:
            evidence["errors"].append(f"Median cross-window delay {median:.6f} s exceeds 0.150 s")
    except (OSError, ValueError, TypeError, KeyError) as error:
        evidence["errors"].append(str(error))
    evidence["passed"] = not evidence["errors"]
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "presentation-latency.json").write_text(
        json.dumps(evidence, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return evidence



def _rank(values, quantile):
    if not values:
        return None
    value = sorted(values)[max(0, math.ceil(quantile * len(values)) - 1)]
    return value if math.isfinite(value) else None


def _read_report(path):
    return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines() if "=" in line)


def _latency_samples(path, mover, remote, observer=None):
    result, previous_time, previous_frame, previous_generation = [], None, None, None
    with path.open(newline="", encoding="utf-8") as source:
        rows = csv.DictReader(source)
        required = FIELDS | {"frame_id", "local_epoch", "remote_epoch", "skipped_frames"}
        if not required.issubset(rows.fieldnames or ()):
            raise ValueError(f"{path.name}: missing successful-Present trace fields")
        for line, row in enumerate(rows, 2):
            stamp, frame = float(row["host_steady_seconds"]), int(row["frame_id"])
            if not math.isfinite(stamp) or stamp <= 0 or (previous_time is not None and stamp <= previous_time):
                raise ValueError(f"{path.name}:{line}: invalid or unordered steady timestamps")
            if previous_frame is not None and frame <= previous_frame:
                raise ValueError(f"{path.name}:{line}: repeated or unordered presented frame")
            previous_time, previous_frame = stamp, frame
            expected_local = observer if remote else mover
            if int(row["local_id"]) != expected_local:
                raise ValueError(f"{path.name}:{line}: local player does not match the reported observer identity")
            if "connection_generation" in row:
                generation = int(row["connection_generation"])
                if generation < 0 or (previous_generation is not None and generation != previous_generation):
                    raise ValueError(f"{path.name}:{line}: connection generation changed during latency evidence")
                previous_generation = generation
            prefix = "remote" if remote else "local"
            identity = int(row[prefix + "_id"])
            if not identity:
                continue
            if identity != mover:
                raise ValueError(f"{path.name}:{line}: unexpected mover identity")
            x, z = float(row[prefix + "_x"]), float(row[prefix + "_z"])
            if not math.isfinite(x) or not math.isfinite(z):
                raise ValueError(f"{path.name}:{line}: nonfinite position")
            cursor = None
            if remote and int(row.get("remote_lower_tick", 0)):
                a, b = int(row["remote_lower_command"]), int(row["remote_upper_command"])
                alpha = float(row["remote_alpha"])
                cursor = a + (b - a) * alpha
            elif not remote and int(row.get("local_current", 0)):
                a, b = int(row["local_previous"]), int(row["local_current"])
                alpha = float(row["local_alpha"])
                cursor = a + (b - a) * alpha
            if cursor is not None and (not math.isfinite(cursor) or cursor < 0 or not 0 <= alpha <= 1):
                raise ValueError(f"{path.name}:{line}: invalid interpolation command cursor")
            result.append({"time": stamp, "x": x, "z": z, "epoch": int(row[prefix + "_epoch"]),
                           "cursor": cursor, "skipped": int(row["skipped_frames"]), "raw": row})
    if len(result) < 2:
        raise ValueError(f"{path.name}: fewer than two mover samples")
    return result


def _event_crossings(samples, event, next_event, fallback_end):
    crossings = []
    for before, after in zip(samples, samples[1:]):
        if after["time"] < event["actual"] or before["epoch"] != event["epoch"] or after["epoch"] != event["epoch"]:
            continue
        a = (before["x"] - event["x"]) * event["dx"] + (before["z"] - event["z"]) * event["dz"]
        b = (after["x"] - event["x"]) * event["dx"] + (after["z"] - event["z"]) * event["dz"]
        if not a < event["threshold"] <= b:
            continue
        if after["time"] - before["time"] >= MAXIMUM_CROSSING_BRACKET_SECONDS - 1e-9:
            # Do not invent a submitted pose during an unobserved runtime gap.
            # Use the command/time interval only to determine whether this
            # unknown crossing could belong to the event. Keep a sentinel even
            # if another well-observed crossing exists, so ambiguity cannot be
            # turned into a successful first match. This is a bracket limit,
            # not a latency timeout: a well-observed one-second delay is valid.
            if before["cursor"] is not None and after["cursor"] is not None:
                low, high = sorted((before["cursor"], after["cursor"]))
                if high <= event["after_sequence"]:
                    continue
                if next_event and next_event["epoch"] == event["epoch"] and low > next_event["after_sequence"]:
                    continue
            elif before["time"] >= fallback_end:
                continue
            crossings.append(math.inf)
            continue
        fraction = (event["threshold"] - a) / (b - a)
        stamp = before["time"] + fraction * (after["time"] - before["time"])
        if stamp < event["actual"] - .02:
            continue
        if before["cursor"] is not None and after["cursor"] is not None:
            cursor = before["cursor"] + fraction * (after["cursor"] - before["cursor"])
            if cursor <= event["after_sequence"]:
                continue
            if next_event and next_event["epoch"] == event["epoch"] and cursor > next_event["after_sequence"]:
                continue
        elif stamp >= fallback_end:
            # The saved v2 baseline has no interpolation command cursors. Its
            # geometric fallback conservatively marks late/ambiguous events as
            # misses; it never removes them from the acceptance denominator.
            continue
        crossings.append(stamp)
    return crossings


def analyze_latency(directory):
    """One long run; every predeclared event remains in the denominator."""
    directory = Path(directory)
    evidence = {
        "passed": False,
        "method": "One directed 0.25-unit displacement crossing per predeclared event; remote successful-submission crossing minus local crossing. Current traces identify events by epoch and interpolated command range. Unmatched, ambiguous or unobserved crossings spanning a 100 ms Presented gap count as +infinity for nearest-rank quantiles; this is not a timeout on well-observed slow events.",
        "scope": "Same-host monotonic timestamps after successful renderer submission; not monitor scanout, input-to-photon or cross-host clock synchronization.",
        "criteria": {"minimum_seconds": 120, "minimum_events": 200, "minimum_matched_rate": .99,
                     "maximum_p50_seconds": .05, "maximum_p95_seconds": .08, "minimum_nominal_fps": 60,
                     "unknown_crossing_bracket_seconds": MAXIMUM_CROSSING_BRACKET_SECONDS},
        "errors": [],
    }
    try:
        mover = _mover_id(directory / "create-report.txt")
        observer = _mover_id(directory / "join-report.txt")
        if observer == mover:
            raise ValueError("Observer player_id must be distinct from the mover")
        reports = {role: _read_report(directory / f"{role}-report.txt") for role in ("create", "join")}
        for role, report in reports.items():
            if report.get("mode") != "latency" or report.get("capture") != "none":
                raise ValueError(f"{role}: long-run evidence requires latency mode without capture")
            if float(report["nominal_fps"]) < 60:
                evidence["errors"].append(f"{role}: nominal FPS below 60")
        local = _latency_samples(directory / "create-presentation.csv", mover, False)
        remote = _latency_samples(directory / "join-presentation.csv", mover, True, observer)
        with (directory / "latency-plan.csv").open(newline="", encoding="utf-8") as source:
            plan = list(csv.DictReader(source))
        count = len(plan)
        if count < 200:
            raise ValueError("Fewer than 200 predeclared movement events")
        previous_end = None
        for index, row in enumerate(plan):
            start, end = float(row["scheduled_host_seconds"]), float(row["end_host_seconds"])
            if (int(row["event_id"]) != index or int(row["event_count"]) != count or
                    not all(math.isfinite(value) for value in (start, end)) or end - start < .599999 or
                    (previous_end is not None and abs(start - previous_end) > .00001) or
                    int(row["sign"]) != (-1 if index % 2 else 1) or float(row["threshold_units"]) != .25 or
                    float(row["duration_seconds"]) < 120):
                raise ValueError("Invalid predeclared movement event schedule")
            previous_end = end
        begin, finish = float(plan[0]["scheduled_host_seconds"]), float(plan[-1]["end_host_seconds"])
        if finish - begin < 119.999:
            raise ValueError("Measurement duration below 120 seconds")
        events = {}
        with (directory / "latency-events.csv").open(newline="", encoding="utf-8") as source:
            for row in csv.DictReader(source):
                index = int(row["event_id"])
                if index in events or not 0 <= index < count or int(row["local_id"]) != mover:
                    raise ValueError("Duplicate or invalid emitted movement event")
                event = {"actual": float(row["actual_start_host_seconds"]), "epoch": int(row["movement_epoch"]),
                         "after_sequence": int(row["after_sequence"]), "x": float(row["origin_x"]), "z": float(row["origin_z"]),
                         "dx": float(row["direction_x"]), "dz": float(row["direction_z"]), "threshold": .25}
                if (not all(math.isfinite(value) for value in event.values()) or event["epoch"] <= 0 or
                        not float(plan[index]["scheduled_host_seconds"]) - .001 <= event["actual"] < float(plan[index]["end_host_seconds"]) or
                        abs(math.hypot(event["dx"], event["dz"]) - 1) > .001):
                    raise ValueError(f"Invalid emitted event {index}")
                events[index] = event
        delays, matches = [], []
        for index, planned in enumerate(plan):
            event = events.get(index)
            match = {"event_id": index, "delay_seconds": None, "status": "not_dispatched"}
            delay = math.inf
            if event:
                next_event = next((events[later] for later in range(index + 1, count) if later in events), None)
                fallback_end = float(planned["end_host_seconds"]) if index + 1 < count else finish + 2
                source_crossings = _event_crossings(local, event, next_event, fallback_end)
                remote_crossings = _event_crossings(remote, event, next_event, fallback_end)
                match.update({"local_candidates": len(source_crossings), "remote_candidates": len(remote_crossings)})
                unknown_gap = any(not math.isfinite(stamp) for stamp in source_crossings + remote_crossings)
                if unknown_gap:
                    match["status"] = "unknown_presented_gap"
                elif len(source_crossings) == 1 and len(remote_crossings) == 1:
                    delay = remote_crossings[0] - source_crossings[0]
                    match.update({"status": "matched", "delay_seconds": delay,
                                  "local_crossing_host_seconds": source_crossings[0], "remote_crossing_host_seconds": remote_crossings[0]})
                    if delay < -.02:
                        evidence["errors"].append(f"Event {index}: remote movement preceded local by more than 20 ms")
                else:
                    match["status"] = "ambiguous" if len(source_crossings) > 1 or len(remote_crossings) > 1 else "missing_crossing"
            delays.append(delay)
            matches.append(match)
        finite = [delay for delay in delays if math.isfinite(delay)]
        rate = len(finite) / count
        p50, p95 = _rank(delays, .5), _rank(delays, .95)
        if rate < .99:
            evidence["errors"].append(f"Matched event rate {rate:.4f} is below 0.99")
        if p50 is None or p50 > .05:
            evidence["errors"].append("P50 exceeds 50 ms (unmatched events count as infinity)")
        if p95 is None or p95 > .08:
            evidence["errors"].append("P95 exceeds 80 ms (unmatched events count as infinity)")
        cadence = {}
        for role, samples in (("create", local), ("join", remote)):
            during = [sample for sample in samples if begin <= sample["time"] <= finish]
            gaps = [after["time"] - before["time"] for before, after in zip(during, during[1:])]
            cadence[role] = {"nominal_fps": float(reports[role]["nominal_fps"]), "submitted_frames": len(during),
                             "measured_mean_fps": (len(during) - 1) / (during[-1]["time"] - during[0]["time"]) if len(during) > 1 else None,
                             "frame_interval_p50_seconds": _rank(gaps, .5), "frame_interval_p95_seconds": _rank(gaps, .95),
                             "maximum_frame_interval_seconds": max(gaps, default=None),
                             "skipped_frames": int(reports[role]["skipped_frames"])}
            if samples[0]["time"] > begin or samples[-1]["time"] < finish + 1.9:
                evidence["errors"].append(f"{role}: trace does not cover the planned measurement and trailing drain")
        command_attribution = all(sample["cursor"] is not None for sample in local + remote)
        evidence.update({"mover_player_id": mover, "planned_event_count": count, "emitted_event_count": len(events),
                         "matched_event_count": len(finite), "matched_event_rate": rate, "infinite_event_count": count - len(finite),
                         "unknown_gap_event_count": sum(match["status"] == "unknown_presented_gap" for match in matches),
                         "duration_seconds": finish - begin, "p50_seconds": p50, "p95_seconds": p95,
                         "median_delay_seconds": p50, "matched_only_median_seconds": statistics.median(finite) if finite else None,
                         "matched_only_maximum_seconds": max(finite, default=None), "events": matches, "cadence": cadence,
                         "command_attribution": command_attribution,
                         "fallback": None if command_attribution else "Saved baseline: conservative predeclared geometric event windows; unmatched remain infinity."})
    except (OSError, ValueError, TypeError, KeyError, IndexError, ZeroDivisionError) as error:
        evidence["errors"].append(str(error))
    evidence["passed"] = not evidence["errors"]
    (directory / "presentation-latency.json").write_text(json.dumps(evidence, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return evidence

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Directory containing both GUI presentation traces")
    directory = parser.parse_args().directory
    result = analyze_latency(directory) if (directory / "latency-plan.csv").exists() else analyze_presentation(directory)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["passed"] else 1)
