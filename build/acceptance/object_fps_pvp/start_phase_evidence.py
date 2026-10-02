"""Read the probes' per-epoch phase-tracking records (start_phase_record.hpp).

Shared by the GUI latency and gameplay evidence readers, which otherwise do not
depend on each other. Diagnostic only: nothing here changes a latency or
gameplay verdict. Absence and malformed input (including non-finite numbers)
are reported as an explicit status with a reason, never raised and never
promoted to a value.
"""
from collections import Counter
import math
from pathlib import Path

from command_evidence import read_trace_events

PHASE_STATUSES = ("tracking", "acquiring", "host_samples_absent")
# Per-frame Client tracking states in a record's state_changes.
CLIENT_STATES = ("acquiring", "settling", "tracking")
# LocalPlayerPrediction's InitialCommandLead: an epoch-start seed (from
# lastResolvedCommand 0) publishes neutral sequences 1..2, while a stall reseed
# seeds from a resolved command beyond them.
INITIAL_COMMAND_LEAD = 2
STATUS_SCOPE = ("tracking means the Client decided its fixed-step phase at least once in this epoch and life from Host "
                "movement slack samples; it keeps correcting while it tracks (settling while a correction slews) and "
                "a stall reseed returns it to acquiring inside the same record. acquiring means Host samples arrived "
                "but no decision was observed; host_samples_absent means no Host slack sample reached the probe")
WINDOW_SCOPE = ("Client tracking state over the planned measurement window from the record's state changes; a state "
                "holds from the frame it was first seen until the next change, the last one until the last observed "
                "frame. corrections_in_window counts entries into settling inside the window and reacquisitions "
                "entries into acquiring after the first state (stall reseeds); observed_seconds is shorter than "
                "window_seconds when the Client was not observed for part of the window")
RESEED_SCOPE = ("Stall reseeds per record from the Client trace: a run of seeded_neutral generated events seeded beyond "
                f"the epoch-start lead (sequence > {INITIAL_COMMAND_LEAD}) for the record's player, epoch and life. "
                "Informational; the record's own state changes already show the reacquisition")
NO_MEASUREMENT = {"epoch": None, "source": None, "reason": "no latency measurement plan"}
_INTEGER_FIELDS = ("player_id", "movement_epoch", "life_generation", "first_observed_frame", "first_observed_steady_ns",
                   "host_samples", "host_late_samples", "host_slack_min_micros", "host_slack_max_micros",
                   "connection_quality_failures_max", "first_decision_frame", "first_decision_steady_ns",
                   "corrections", "late_corrections", "last_frame", "last_steady_ns", "state_changes_dropped")
_SECONDS_FIELDS = ("first_error_seconds", "last_error_seconds", "last_correction_seconds")


def reject_json_constant(name):
    """json.loads parse_constant hook: NaN and Infinity are invalid evidence, not values."""
    raise ValueError(f"non-finite JSON number {name}")


def finite_json_float(text):
    """json.loads parse_float hook: an overflowing literal such as 1e999 is invalid, not infinity."""
    value = float(text)
    if not math.isfinite(value):
        raise ValueError(f"non-finite JSON number {text}")
    return value


def _require_finite(value, path):
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError(f"{path} is not finite")
    if isinstance(value, dict):
        for key, item in value.items():
            _require_finite(item, f"{path}.{key}")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            _require_finite(item, f"{path}[{index}]")


def _integer(value, name):
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{name} must be an integer, got {value!r}")
    return value


def _check_epoch(epoch):
    if not isinstance(epoch, dict) or epoch.get("status") not in PHASE_STATUSES:
        raise ValueError(f"epochs must be records with a known status ({', '.join(PHASE_STATUSES)})")
    for name in _INTEGER_FIELDS:
        if epoch.get(name) is not None:
            _integer(epoch[name], name)
    for name in _SECONDS_FIELDS:
        value = epoch.get(name)
        if value is not None and (isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value)):
            raise ValueError(f"{name} must be a finite number, got {value!r}")
    if epoch.get("last_state") not in (None, *CLIENT_STATES):
        raise ValueError(f"last_state must be one of {CLIENT_STATES}, got {epoch.get('last_state')!r}")
    changes = epoch.get("state_changes")
    if changes is None:
        return
    if not isinstance(changes, list):
        raise ValueError("state_changes must be a list")
    previous = None
    for change in changes:
        if not isinstance(change, dict) or change.get("state") not in CLIENT_STATES:
            raise ValueError(f"state_changes entries need a known state ({', '.join(CLIENT_STATES)})")
        _integer(change.get("frame"), "state_changes frame")
        stamp = _integer(change.get("steady_ns"), "state_changes steady_ns")
        if previous is not None and stamp < previous:
            raise ValueError("state_changes must be in steady-clock order")
        previous = stamp


def stall_reseeds(events):
    """The first event of each stall reseed per (player_id, epoch, life_generation), in trace order."""
    reseeds, previous = {}, {}
    for event in events:
        if event.get("kind") != "generated":
            continue
        key = (event["player_id"], event["epoch"], event["life_generation"])
        seeded, sequence = bool(event.get("seeded_neutral")), event["sequence"]
        last = previous.get(key)
        previous[key] = (seeded, sequence)
        # One reseed publishes consecutive neutral sequences; the epoch-start seed is not a reseed.
        if not seeded or sequence <= INITIAL_COMMAND_LEAD or (last is not None and last[0] and last[1] + 1 == sequence):
            continue
        reseeds.setdefault(key, []).append({"steady_ns": event["time_ns"], "sequence": sequence})
    return reseeds


def reseed_evidence(events, source):
    """Stall reseeds of one Client trace's events, or an explicit status when they cannot be attributed."""
    events = list(events)
    for event in events:
        if event.get("kind") == "generated":
            life = event.get("life_generation")
            if isinstance(life, bool) or not isinstance(life, int) or life < 0:
                return {"status": "unsupported", "source": source,
                        "reason": f"{source}: generated events carry no valid life_generation ({life!r})"}
    return {"status": "recorded", "source": source, "reason": None, "reseeds": stall_reseeds(events)}


def read_reseed_evidence(path):
    """A Client trace's stall reseeds; absence and corruption are explicit statuses, never raised."""
    path = Path(path)
    try:
        return reseed_evidence(read_trace_events(path), path.name)
    except FileNotFoundError:
        return {"status": "absent", "source": path.name, "reason": f"{path.name} missing"}
    except (OSError, ValueError, KeyError, TypeError) as error:
        return {"status": "invalid", "source": path.name, "reason": str(error)}


def measurement_window(epoch, begin, finish):
    """The Client's tracking state during [begin, finish] (seconds); None when no plan names the window."""
    if begin is None or finish is None:
        return None
    changes = epoch.get("state_changes")
    if changes is None:
        return {"status": "not_recorded", "reason": "record carries no state history"}
    if epoch.get("state_changes_dropped"):
        return {"status": "state_history_truncated",
                "reason": f"{epoch['state_changes_dropped']} later state change(s) were counted but not stored"}
    last = epoch.get("last_steady_ns")
    if not changes or last is None:
        return {"status": "client_not_observed", "reason": "no active Client frame was recorded for this epoch"}
    seconds, states, corrections, reacquisitions = {}, [], 0, 0
    for index, change in enumerate(changes):
        start_ns = change["steady_ns"]
        end_ns = changes[index + 1]["steady_ns"] if index + 1 < len(changes) else last
        start, end = start_ns / 1e9, end_ns / 1e9
        if begin <= start <= finish:
            corrections += change["state"] == "settling"
            reacquisitions += change["state"] == "acquiring" and index > 0
        # A state that ended exactly at begin did not hold inside the window.
        if start > finish or end < begin or (end == begin and start < end):
            continue
        seconds[change["state"]] = seconds.get(change["state"], 0.0) + max(0.0, min(end, finish) - max(start, begin))
        if change["state"] not in states:
            states.append(change["state"])
    tracked = seconds.get("tracking", 0.0) + seconds.get("settling", 0.0)
    acquiring = seconds.get("acquiring", 0.0)
    status = ("client_not_observed_in_window" if not states else
              "acquiring_during_measurement" if "acquiring" in states else "tracking")
    return {"status": status, "states": states, "state_seconds": seconds, "tracking_seconds": tracked,
            "acquiring_seconds": acquiring, "observed_seconds": tracked + acquiring, "window_seconds": finish - begin,
            "corrections_in_window": corrections, "reacquisitions_in_window": reacquisitions, "scope": WINDOW_SCOPE}


def _reseed_detection(reseeds):
    if reseeds is None:
        return {"status": "not_checked", "source": None, "reason": "no Client trace was given"}
    return {key: reseeds.get(key) for key in ("status", "source", "reason")}


def _summary(record, source, measurement_start_seconds, measurement_end_seconds, measured, reseeds):
    if not isinstance(record, dict):
        raise ValueError("record must be a JSON object")
    _require_finite(record, "record")
    epochs = record["epochs"]
    if not isinstance(epochs, list):
        raise ValueError("epochs must be a list")
    for epoch in epochs:
        _check_epoch(epoch)
    detection = _reseed_detection(reseeds)
    if record.get("supported") is False:
        return {"status": "unsupported", "reason": f"{source}: probe was built against a product without phase-tracking fields",
                "reseed_detection": detection}
    # Player id 0 is never a joined player: such a record was observed before
    # the probe knew its id (older gui probe) and is reported apart.
    unattributed = [epoch for epoch in epochs if epoch.get("player_id") == 0]
    epochs = [epoch for epoch in epochs if epoch.get("player_id") != 0]
    if not epochs:
        detail = f" ({len(unattributed)} record(s) without a player id)" if unattributed else ""
        return {"status": "no_epoch_observed", "unattributed_epochs": unattributed, "reseed_detection": detection,
                "reason": f"{source}: the probe never observed an active movement epoch{detail}"}
    found = (reseeds or {}).get("reseeds") if (reseeds or {}).get("status") == "recorded" else None
    annotated = []
    for epoch in epochs:
        item = dict(epoch)
        stamp = item.get("first_decision_steady_ns")
        item["decided_seconds_before_measurement"] = (None if measurement_start_seconds is None or stamp is None
                                                      else measurement_start_seconds - stamp / 1e9)
        item["stall_reseeds"] = None if found is None else len(
            found.get((item.get("player_id"), item.get("movement_epoch"), item.get("life_generation"))) or [])
        item["measurement_window"] = measurement_window(item, measurement_start_seconds, measurement_end_seconds)
        annotated.append(item)
    dropped = record.get("dropped_observations")
    if dropped is not None:
        _integer(dropped, "dropped_observations")
    unattributed_frames = record.get("unattributed_client_frames")
    if unattributed_frames is not None:
        _integer(unattributed_frames, "unattributed_client_frames")
    player = record.get("player_id")
    matches = [] if measured.get("epoch") is None else [
        item for item in annotated if item.get("movement_epoch") == measured["epoch"] and
        (player is None or item.get("player_id") in (None, player))]
    entry = matches[0] if matches else None
    reason = measured.get("reason")
    if entry is None and reason is None:
        reason = f"{source}: no record for measured epoch {measured.get('epoch')}"
    window = (entry or {}).get("measurement_window") or {}
    return {"status": "recorded", "reason": None, "epoch_count": len(annotated),
            "first_epoch_status": annotated[0]["status"], "first_epoch": annotated[0],
            "measured_epoch_status": entry["status"] if entry else None, "measured_epoch_record": entry,
            "measured_epoch_reason": reason, "measured_epoch_record_count": len(matches),
            "measured_epoch_window_status": window.get("status"),
            "measured_epoch_tracking_seconds": window.get("tracking_seconds"),
            "measured_epoch_acquiring_seconds": window.get("acquiring_seconds"),
            "measured_epoch_corrections_in_window": window.get("corrections_in_window"),
            "status_counts": dict(Counter(item["status"] for item in annotated)),
            "dropped_observations": dropped, "reseed_detection": detection, "unattributed_epochs": unattributed,
            "unattributed_client_frames": unattributed_frames, "epochs": annotated}


def summarize_start_phase(record, source, measurement_start_seconds=None, measured=None, measurement_end_seconds=None,
                          reseeds=None):
    """One probe's per-epoch record; ``measured`` names the epoch under measurement, if any.

    ``measurement_start_seconds`` and ``measurement_end_seconds`` are the plan's
    window in the probes' steady clock; without both, window states are null.
    ``reseeds`` is that probe's Client trace (read_reseed_evidence/reseed_evidence),
    reported per record as stall_reseeds.
    """
    measured = dict(measured or NO_MEASUREMENT)
    base = {"source": source, "epochs": [], "status_scope": STATUS_SCOPE, "reseed_scope": RESEED_SCOPE,
            "measured_epoch": measured.get("epoch"), "measured_epoch_source": measured.get("source"),
            "measured_epoch_reason": measured.get("reason")}
    if record is None:
        return {**base, "status": "absent",
                "reason": f"{source} missing: probe predates phase-tracking recording or failed before writing it"}
    try:
        return {**base, **_summary(record, source, measurement_start_seconds, measurement_end_seconds, measured, reseeds)}
    except (KeyError, TypeError, ValueError, AttributeError, IndexError, OverflowError, RecursionError) as error:
        detail = f"missing {error}" if isinstance(error, KeyError) else str(error)
        return {**base, "status": "invalid", "reason": f"{source}: {detail}"}
