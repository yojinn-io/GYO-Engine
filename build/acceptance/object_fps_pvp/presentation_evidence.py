"""Measure same-host cross-window movement delay from the real GUI probes."""

import argparse
import csv
import json
import math
from pathlib import Path
import platform
import statistics

from start_phase_evidence import finite_json_float, read_reseed_evidence, reject_json_constant, summarize_start_phase


THRESHOLDS = (0.25, 0.5, 0.75, 1.0, 1.25)
MINIMUM_MATCHES = 3
MINIMUM_DELAY_SECONDS = -0.02  # Sampling/interpolation tolerance, not clock adjustment.
MAXIMUM_MEDIAN_SECONDS = 0.15
MAXIMUM_CROSSING_BRACKET_SECONDS = .1  # Existing runtime/presentation disturbance boundary.
FIELDS = {"host_steady_seconds", "local_id", "local_x", "local_z",
          "remote_id", "remote_x", "remote_z"}
WINDOW_EVENT_KINDS = ("occluded", "exposed", "hidden", "minimized", "focus_gained", "focus_lost", "moved", "resized")
WINDOW_FLAGS = ("occluded", "hidden", "minimized", "input_focus")
WINDOW_SYNC_STATES = ("succeeded", "timed_out", "not_requested")
# Exposed is informational. OS focus changes during measurement mean outside
# interaction; the probe's own synthetic focus is counted separately. The
# product releases its pointer on moved/resized (PvpApplication), so those
# during measurement are interference too.
DISTURBING_WINDOW_EVENTS = ("occluded", "hidden", "minimized", "focus_gained", "focus_lost", "moved", "resized")
DISTURBING_WINDOW_FLAGS = ("occluded", "hidden", "minimized")
DIAGNOSTIC_ERRORS = (KeyError, ValueError, TypeError, IndexError, AttributeError, OverflowError, ZeroDivisionError)


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



def _diagnostic_error(error):
    return f"missing {error}" if isinstance(error, KeyError) else str(error)


def _start_phase_file(directory, role, measurement_start_seconds=None, measured=None, measurement_end_seconds=None):
    path = directory / f"{role}-start-phase.json"
    try:
        # NaN, Infinity and overflowing literals make only this role invalid;
        # they never reach the evidence file, which is written without NaN.
        record = json.loads(path.read_text(encoding="utf-8"),
                            parse_constant=reject_json_constant, parse_float=finite_json_float)
    except FileNotFoundError:
        record = None
    except (OSError, ValueError) as error:
        summary = summarize_start_phase({}, path.name, measurement_start_seconds, measured)  # Keeps the explicit keys.
        summary.update(status="invalid", reason=f"{path.name}: {error}")
        return summary
    # The role's own Client trace counts stall reseeds per record (informational).
    reseeds = None if not isinstance(record, dict) else read_reseed_evidence(directory / f"{role}-commands.jsonl")
    return summarize_start_phase(record, path.name, measurement_start_seconds, measured, measurement_end_seconds, reseeds)


def _measured_epoch(directory, role, begin):
    """create: the epoch its emitted events moved in; join: its own epoch when measurement began."""
    if role == "create":
        source = "latency-events.csv movement_epoch of the first emitted event"
        try:
            with (directory / "latency-events.csv").open(newline="", encoding="utf-8") as stream:
                rows = sorted(csv.DictReader(stream), key=lambda row: int(row["event_id"]))
            if not rows:
                return {"epoch": None, "source": source, "reason": "no movement event was emitted"}
            epochs = sorted({int(row["movement_epoch"]) for row in rows})
            return {"epoch": int(rows[0]["movement_epoch"]), "source": source, "reason": None, "event_epochs": epochs}
        except (OSError, *DIAGNOSTIC_ERRORS) as error:
            return {"epoch": None, "source": source, "reason": f"latency-events.csv: {_diagnostic_error(error)}"}
    source = "join-presentation.csv local_epoch of the last frame at or before measurement begin"
    if begin is None:
        return {"epoch": None, "source": source, "reason": "no latency measurement plan"}
    try:
        epoch = None
        with (directory / "join-presentation.csv").open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                if float(row["host_steady_seconds"]) > begin:
                    break
                epoch = int(row["local_epoch"])
        if not epoch:
            return {"epoch": None, "source": source, "reason": "join had no movement epoch at measurement begin"}
        return {"epoch": epoch, "source": source, "reason": None}
    except (OSError, *DIAGNOSTIC_ERRORS) as error:
        return {"epoch": None, "source": source, "reason": f"join-presentation.csv: {_diagnostic_error(error)}"}


def _named_counts(value, names):
    if value == "unobserved":
        return None
    counts = {}
    for item in value.split(","):
        name, separator, number = item.partition(":")
        if not separator or name in counts:
            raise ValueError(f"malformed count list {value!r}")
        counts[name] = int(number)
    if tuple(counts) != names:
        raise ValueError(f"expected {', '.join(names)} in {value!r}")
    return counts


def _integers(report, key, count, absent=None):
    value = report[key]
    if absent is not None and value == absent:
        return None
    parts = value.split(",")
    if len(parts) != count:
        raise ValueError(f"{key} must hold {count} integers, got {value!r}")
    return [int(part) for part in parts]


def _window_record(report):
    result = {"status": "recorded", "placement": report["window_placement"],
              "usable_bounds": _integers(report, "window_usable_bounds", 4, "unavailable"),
              "requested_position": _integers(report, "window_requested_position", 2, "none"),
              "sync": report["window_sync"],
              "position_after_sync": _integers(report, "window_position_after_sync", 2, "none"),
              "final_position": _integers(report, "window_final_position", 2),
              "size": _integers(report, "window_size", 2),
              "borders_top_left_bottom_right": _integers(report, "window_borders", 4),
              "flags_at_measurement_start": _named_counts(report["window_flags_at_measurement_start"], WINDOW_FLAGS),
              "flags_at_end": _named_counts(report["window_flags_at_end"], WINDOW_FLAGS),
              "os_events": _named_counts(report["window_os_events"], WINDOW_EVENT_KINDS),
              "os_events_during_measurement": _named_counts(report["window_os_events_during_measurement"], WINDOW_EVENT_KINDS),
              "synthetic_events": int(report["window_synthetic_events"])}
    if "window_placement_error" in report:
        result["placement_error"] = report["window_placement_error"]
    if result["sync"] not in WINDOW_SYNC_STATES:
        raise ValueError(f"window_sync must be one of {', '.join(WINDOW_SYNC_STATES)}, got {result['sync']!r}")
    if min(result["size"]) <= 0:
        raise ValueError(f"window_size must be positive, got {result['size']}")
    if result["os_events"] is None or result["os_events_during_measurement"] is None:
        raise ValueError("window event counts are unobserved")
    return result


def window_evidence(path, role):
    """Window placement and OS window events of one latency probe report."""
    try:
        report = _read_report(path, unique_prefix="window_")
    except OSError as error:
        return {"status": "absent", "reason": f"{path.name}: {error}"}
    except ValueError as error:
        return {"status": "invalid", "reason": f"{path.name}: {error}"}
    if "window_os_events" not in report:
        return {"status": "absent", "reason": f"{path.name} has no window evidence (probe predates it)"}
    try:
        result = _window_record(report)
        start, during = result["flags_at_measurement_start"], result["os_events_during_measurement"]
        disturbances = [f"{role}: window state at measurement start was not observed"] if start is None else [
            f"{role}: window {flag} at measurement start" for flag in DISTURBING_WINDOW_FLAGS if start[flag]]
        disturbances += [f"{role}: {during[kind]} OS {kind} event(s) during measurement"
                         for kind in DISTURBING_WINDOW_EVENTS if during[kind]]
    except DIAGNOSTIC_ERRORS as error:
        return {"status": "invalid", "reason": f"{path.name}: {_diagnostic_error(error)}"}
    result["disturbances"] = disturbances
    return result


def _window_overlap(create, join):
    """Share of each window's client area left visible with the other on top.

    SDL positions and sizes name the client area. The top border is added only
    when the platform reports one: macOS reports zero borders, so there the
    title bar above each window is not part of this geometry.
    """
    try:
        def rectangle(window):
            (x, y), (width, height) = window["final_position"], window["size"]
            top = (window["borders_top_left_bottom_right"] or [0])[0]
            return x, y - top, x + width, y + height
        first, second = rectangle(create), rectangle(join)
        shared = max(0, min(first[2], second[2]) - max(first[0], second[0])) * \
            max(0, min(first[3], second[3]) - max(first[1], second[1]))
        def visible(box):
            area = (box[2] - box[0]) * (box[3] - box[1])
            return None if area <= 0 else 1 - shared / area
        return {"status": "recorded", "create_visible_fraction_with_join_on_top": visible(first),
                "join_visible_fraction_with_create_on_top": visible(second)}
    except DIAGNOSTIC_ERRORS as error:
        return {"status": "invalid", "reason": f"window overlap: {_diagnostic_error(error)}"}


PLATFORM_FIELDS = ("os", "architecture", "video_driver", "gpu_driver", "refresh_hz", "usable_bounds", "input")
TIMER_FIELDS = ("late_p50_ms", "late_p99_ms", "late_max_ms", "interval_p50_ms", "interval_p99_ms", "interval_max_ms",
                "interval_over_slow_fraction")


def timer_evidence(report):
    """The probe's empty-loop late-wake distribution: interpretation only, never part of a verdict."""
    if "platform_timer_samples" not in report:
        return None
    try:
        timer = {"use": "interpretation only; never a verdict, threshold or denominator",
                 "sleeper": report["platform_timer_sleeper"], "schedule": report.get("platform_timer_schedule"),
                 "samples": int(report["platform_timer_samples"])}
        for field in TIMER_FIELDS:
            timer[field] = float(report[f"platform_timer_{field}"])
        return timer
    except (KeyError, ValueError) as error:
        return {"status": "invalid", "reason": f"timer baseline: {error}"}


def platform_evidence(path):
    """Platform fingerprint of one probe report; absent for a probe that predates it."""
    try:
        report = _read_report(path, unique_prefix="platform_")
    except OSError as error:
        return {"status": "absent", "reason": f"{path.name}: {error}"}
    except ValueError as error:
        return {"status": "invalid", "reason": f"{path.name}: {error}"}
    if "platform_os" not in report:
        return {"status": "absent", "reason": f"{path.name} has no platform fingerprint (probe predates it)"}
    try:
        result = {"status": "recorded"}
        for field in PLATFORM_FIELDS:
            result[field] = report[f"platform_{field}"]
        result["refresh_hz"] = float(result["refresh_hz"])
        result["usable_bounds"] = _integers(report, "platform_usable_bounds", 4, "unavailable")
    except DIAGNOSTIC_ERRORS as error:
        return {"status": "invalid", "reason": f"{path.name}: {_diagnostic_error(error)}"}
    result["timer"] = timer_evidence(report)
    return result


def _windows(directory):
    windows = {}
    for role in ("create", "join"):
        try:
            windows[role] = window_evidence(directory / f"{role}-report.txt", role)
        except DIAGNOSTIC_ERRORS as error:  # Defensive: one role never aborts the other or the verdict.
            windows[role] = {"status": "invalid", "reason": f"{role}-report.txt: {_diagnostic_error(error)}"}
    result = {"window": windows}
    if all(window["status"] == "recorded" for window in windows.values()):
        result["window_disturbances"] = [item for window in windows.values() for item in window["disturbances"]]
        result["window_disturbed"] = bool(result["window_disturbances"])
        result["window_overlap"] = _window_overlap(windows["create"], windows["join"])
    else:
        # Unknown is reported as such; it is never promoted to clean.
        result["window_disturbed"] = None
        result["window_disturbances"] = [f"{role}: window evidence {window['status']}: {window['reason']}"
                                         for role, window in windows.items() if window["status"] != "recorded"]
    return result


def _rank(values, quantile):
    if not values:
        return None
    value = sorted(values)[max(0, math.ceil(quantile * len(values)) - 1)]
    return value if math.isfinite(value) else None


def _read_report(path, unique_prefix=None):
    """key=value lines; a repeated key with ``unique_prefix`` is invalid rather than last-wins."""
    pairs = [line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines() if "=" in line]
    if unique_prefix is not None:
        seen, repeated = set(), []
        for key, _ in pairs:
            if key.startswith(unique_prefix) and key in seen and key not in repeated:
                repeated.append(key)
            seen.add(key)
        if repeated:
            raise ValueError(f"duplicate {unique_prefix}* key(s): {', '.join(repeated)}")
    return dict(pairs)


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
    return _analyze_latency(directory, short=False)


def analyze_short_latency(directory):
    """Explicit bounded regression; never certifies the 120-second baseline."""
    return _analyze_latency(directory, short=True)


def _analyze_latency(directory, *, short):
    directory = Path(directory)
    minimum_seconds, minimum_events = (16, 20) if short else (120, 200)
    mode = "latency-short" if short else "latency"
    evidence = {
        "passed": False,
        "mode": mode,
        "long_run_certification": not short,
        "method": "One directed 0.25-unit displacement crossing per predeclared event; remote successful-submission crossing minus local crossing. Current traces identify events by epoch and interpolated command range. Unmatched, ambiguous or unobserved crossings spanning a 100 ms Presented gap count as +infinity for nearest-rank quantiles; this is not a timeout on well-observed slow events.",
        "scope": "Same-host monotonic timestamps after successful renderer submission; not monitor scanout, input-to-photon or cross-host clock synchronization.",
        "criteria": {"minimum_seconds": minimum_seconds, "minimum_events": minimum_events, "minimum_matched_rate": .99,
                     "maximum_p50_seconds": .05, "maximum_p95_seconds": .08, "minimum_nominal_fps": 60,
                     "unknown_crossing_bracket_seconds": MAXIMUM_CROSSING_BRACKET_SECONDS},
        "errors": [],
    }
    begin = finish = None
    try:
        mover = _mover_id(directory / "create-report.txt")
        observer = _mover_id(directory / "join-report.txt")
        if observer == mover:
            raise ValueError("Observer player_id must be distinct from the mover")
        reports = {role: _read_report(directory / f"{role}-report.txt") for role in ("create", "join")}
        for role, report in reports.items():
            if report.get("mode") != mode or report.get("capture") != "none":
                raise ValueError(f"{role}: {mode} evidence requires the matching explicit mode without capture")
            if float(report["nominal_fps"]) < 60:
                evidence["errors"].append(f"{role}: nominal FPS below 60")
        local = _latency_samples(directory / "create-presentation.csv", mover, False)
        remote = _latency_samples(directory / "join-presentation.csv", mover, True, observer)
        with (directory / "latency-plan.csv").open(newline="", encoding="utf-8") as source:
            plan = list(csv.DictReader(source))
        count = len(plan)
        if count < minimum_events:
            raise ValueError(f"Fewer than {minimum_events} predeclared movement events")
        previous_end = None
        for index, row in enumerate(plan):
            start, end = float(row["scheduled_host_seconds"]), float(row["end_host_seconds"])
            if (int(row["event_id"]) != index or int(row["event_count"]) != count or
                    not all(math.isfinite(value) for value in (start, end)) or end - start < .599999 or
                    (previous_end is not None and abs(start - previous_end) > .00001) or
                    int(row["sign"]) != (-1 if index % 2 else 1) or float(row["threshold_units"]) != .25 or
                    float(row["duration_seconds"]) < minimum_seconds):
                raise ValueError("Invalid predeclared movement event schedule")
            previous_end = end
        begin, finish = float(plan[0]["scheduled_host_seconds"]), float(plan[-1]["end_host_seconds"])
        if finish - begin < minimum_seconds - .001:
            raise ValueError(f"Measurement duration below {minimum_seconds} seconds")
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
    # Diagnostic records only: they never change the latency verdict above.
    measured = {role: _measured_epoch(directory, role, begin) for role in ("create", "join")}
    evidence["start_phase"] = {role: _start_phase_file(directory, role, begin, measured[role], finish) for role in ("create", "join")}
    evidence.update(_windows(directory))
    evidence["platform"] = {role: platform_evidence(directory / f"{role}-report.txt") for role in ("create", "join")}
    evidence["passed"] = not evidence["errors"]
    filename = "presentation-short-latency.json" if short else "presentation-latency.json"
    (directory / filename).write_text(json.dumps(evidence, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return evidence

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Directory containing both GUI presentation traces")
    parser.add_argument("--short", action="store_true", help="Explicit 16-second/20-event regression; not long-run certification")
    args = parser.parse_args()
    directory = args.directory
    result = (analyze_short_latency(directory) if args.short else
              analyze_latency(directory) if (directory / "latency-plan.csv").exists() else analyze_presentation(directory))
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["passed"] else 1)
