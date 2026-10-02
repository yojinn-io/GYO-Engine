"""Read the probes' per-epoch A1 start-phase records (start_phase_record.hpp).

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

START_PHASE_STATUSES = ("shift_armed", "shift_withdrawn_below_cut", "shift_armed_after_below_cut_decision",
                        "skipped_host_late", "skipped_frame_rate_below_tick", "cancelled_by_reseed",
                        "rejected_or_skipped_unknown", "pending_frame_window", "not_armed_or_invalidated",
                        "host_wait_absent")
# Per-frame Client states in a record's client_state_changes.
CLIENT_STATES = ("undecided", "shift_armed", "withdrawn_below_cut", "skipped_below_cut", "skipped_host_late",
                 "cancelled_by_reseed", "skipped_unknown")
SKIP_REASONS = (None, "host_late", "frame_rate_below_tick", "cancelled_by_reseed", "unrecognised")
# The only Client state in which the start-phase shift is applied; every other
# observed state (undecided, skipped, withdrawn, cancelled) is unaligned.
ALIGNED_STATE = "shift_armed"
# LocalPlayerPrediction's InitialCommandLead: an epoch-start seed (from
# lastResolvedCommand 0) publishes neutral sequences 1..2, while a stall reseed
# seeds from a resolved command beyond them.
INITIAL_COMMAND_LEAD = 2
SHIFT_ARMED_SCOPE = ("shift_armed means the Client armed a phase shift and never withdrew it while observed; a shift withdrawn "
                     "below 60 FPS reads shift_withdrawn_below_cut (withdrawn frames, withdrawals/restorations, last state) and "
                     "its measurement-window state says whether that happened during measurement; pending_frame_window means "
                     "the Client was still collecting frame intervals; cancelled_by_reseed means a stall reseed cancelled the "
                     "pending or decided start phase for the rest of that epoch and life, as reported by the product "
                     "(cancel_reason_supported) or, for a product without that reason, derived from the Client trace "
                     "(cancelled_by_reseed.source client_trace_fallback, recorded_status kept). Slew completion is not "
                     "observable from the recorder")
WINDOW_SCOPE = ("Client state over the planned measurement window from the record's state changes; a state holds from the "
                "frame it was first seen until the next change, the last one until the last observed frame, and a "
                "trace-derived reseed cancellation from the reseed until the last observed frame. shift_applied_seconds is "
                "the time in shift_armed and unaligned_seconds the time in every other observed state; together they are "
                "observed_seconds, shorter than window_seconds when the Client was not observed for part of the window")
TRACE_FALLBACK_SCOPE = ("Harness fallback for products whose skip reason cannot name a reseed cancellation: a stall reseed "
                        "is a run of seeded_neutral generated events seeded beyond the epoch-start lead (sequence > "
                        f"{INITIAL_COMMAND_LEAD}) for the record's player, epoch and life. Such a reseed clears the armed "
                        "shift and any pending start phase for the rest of that epoch and life while the observation keeps "
                        "its earlier values. A decided record is cancelled by the first reseed after its decision frame "
                        "began, an undecided one by its first reseed. runtime_gap events are not used on their own: a covered "
                        "frame gap discards time without reseeding and keeps the armed shift")
NO_MEASUREMENT = {"epoch": None, "source": None, "reason": "no latency measurement plan"}
_INTEGER_FIELDS = ("player_id", "movement_epoch", "life_generation", "first_observed_frame", "first_observed_steady_ns",
                   "host_wait_micros", "host_wait_first_frame", "host_wait_first_steady_ns", "host_wait_conflicting_frames",
                   "client_first_frame", "client_first_steady_ns", "client_conflicting_frames", "client_first_armed_frame",
                   "client_first_armed_steady_ns", "client_cancelled_frames", "client_cancelled_first_frame",
                   "client_cancelled_first_steady_ns", "last_client_frame", "last_client_steady_ns", "withdrawn_frames",
                   "withdrawn_first_frame", "withdrawn_first_steady_ns", "withdrawals", "restorations",
                   "host_wait_undecided_frames", "client_active_frames_at_last_undecided", "client_state_changes_dropped")
_SECONDS_FIELDS = ("client_wait_seconds", "client_shift_seconds", "client_armed_shift_seconds",
                   "last_client_wait_seconds", "last_client_shift_seconds")


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
    if not isinstance(epoch, dict) or epoch.get("status") not in START_PHASE_STATUSES:
        raise ValueError(f"epochs must be records with a known status ({', '.join(START_PHASE_STATUSES)})")
    for name in _INTEGER_FIELDS:
        if epoch.get(name) is not None:
            _integer(epoch[name], name)
    for name in _SECONDS_FIELDS:
        value = epoch.get(name)
        if value is not None and (isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value)):
            raise ValueError(f"{name} must be a finite number, got {value!r}")
    for name in ("client_skip_reason", "last_client_skip_reason"):
        if epoch.get(name) not in SKIP_REASONS:
            raise ValueError(f"{name} must be one of {SKIP_REASONS}, got {epoch.get(name)!r}")
    if epoch.get("last_client_state") not in (None, *CLIENT_STATES):
        raise ValueError(f"last_client_state must be one of {CLIENT_STATES}, got {epoch.get('last_client_state')!r}")
    changes = epoch.get("client_state_changes")
    if changes is None:
        return
    if not isinstance(changes, list):
        raise ValueError("client_state_changes must be a list")
    previous = None
    for change in changes:
        if not isinstance(change, dict) or change.get("state") not in CLIENT_STATES:
            raise ValueError(f"client_state_changes entries need a known state ({', '.join(CLIENT_STATES)})")
        _integer(change.get("frame"), "client_state_changes frame")
        stamp = _integer(change.get("steady_ns"), "client_state_changes steady_ns")
        if previous is not None and stamp < previous:
            raise ValueError("client_state_changes must be in steady-clock order")
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


def _seconds_before(measurement_start_seconds, epoch, side):
    stamp = epoch.get(side + "_first_steady_ns")
    if stamp is None:
        return None
    return None if measurement_start_seconds is None else measurement_start_seconds - stamp / 1e9


def _segments(changes, last, cancelled_ns):
    """(state, start_ns, end_ns) spans; a trace-derived cancellation replaces every state from the reseed on."""
    spans = [(change["state"], change["steady_ns"],
              changes[index + 1]["steady_ns"] if index + 1 < len(changes) else last)
             for index, change in enumerate(changes)]
    if cancelled_ns is None or cancelled_ns >= last:
        return spans
    kept = [(state, start, min(end, cancelled_ns)) for state, start, end in spans if start < cancelled_ns]
    return kept + [("cancelled_by_reseed", max(cancelled_ns, changes[0]["steady_ns"]), last)]


def measurement_window(epoch, begin, finish, cancelled_ns=None):
    """The Client's state during [begin, finish] (seconds); None when no plan names the window.

    ``cancelled_ns`` is a trace-derived reseed cancellation (steady-clock ns); a
    product-reported one is already part of the state changes.
    """
    if begin is None or finish is None:
        return None
    changes = epoch.get("client_state_changes")
    if changes is None:
        result = {"status": "not_recorded", "reason": "record carries no client state history (older recorder)"}
        if cancelled_ns is not None and cancelled_ns / 1e9 <= begin:
            # A cancellation is final for its epoch and life, so no shift was applied in it during the window.
            result["cancelled_before_window"] = True
        return result
    if epoch.get("client_state_changes_dropped"):
        return {"status": "state_history_truncated",
                "reason": f"{epoch['client_state_changes_dropped']} later state change(s) were counted but not stored"}
    last = epoch.get("last_client_steady_ns")
    if not changes or last is None:
        return {"status": "client_not_observed", "reason": "no active Client frame was recorded for this epoch"}
    seconds, states = {}, []
    for state, start_ns, end_ns in _segments(changes, last, cancelled_ns):
        start, end = start_ns / 1e9, end_ns / 1e9
        # A state that ended exactly at begin did not hold inside the window.
        if start > finish or end < begin or (end == begin and start < end):
            continue
        seconds[state] = seconds.get(state, 0.0) + max(0.0, min(end, finish) - max(start, begin))
        if state not in states:
            states.append(state)
    withdrawn, cancelled = "withdrawn_below_cut" in states, "cancelled_by_reseed" in states
    status = ("client_not_observed_in_window" if not states else "withdrawn_below_cut" if withdrawn else
              "cancelled_by_reseed" if cancelled else states[0] if len(states) == 1 else "mixed")
    unaligned = sum(value for state, value in seconds.items() if state != ALIGNED_STATE)
    applied = seconds.get(ALIGNED_STATE, 0.0)
    return {"status": status, "states": states, "state_seconds": seconds, "withdrawn_during_measurement": withdrawn,
            "cancelled_during_measurement": cancelled, "shift_applied_seconds": applied, "unaligned_seconds": unaligned,
            "observed_seconds": applied + unaligned, "window_seconds": finish - begin, "scope": WINDOW_SCOPE}


def _product_cancellation(item):
    if item["status"] != "cancelled_by_reseed":
        return None
    return {"source": "product", "steady_ns": item.get("client_cancelled_first_steady_ns"),
            "frame": item.get("client_cancelled_first_frame")}


def _trace_cancellation(item, reseeds, source):
    """The stall reseed that cancelled this record's start phase per the Client trace, or None."""
    found = reseeds.get((item.get("player_id"), item.get("movement_epoch"), item.get("life_generation"))) or []
    decided = item.get("client_wait_seconds") is not None and item.get("client_first_steady_ns") is not None
    decided_ns = item["client_first_steady_ns"] if decided else None
    after = [reseed for reseed in found if decided_ns is None or reseed["steady_ns"] > decided_ns]
    if not after:
        return None
    first = after[0]
    return {"source": "client_trace_fallback", "trace": source, "steady_ns": first["steady_ns"], "frame": None,
            "reseed_sequence": first["sequence"], "after_decision": decided,
            "seconds_after_decision": None if decided_ns is None else (first["steady_ns"] - decided_ns) / 1e9,
            "stall_reseeds_in_epoch": len(found)}


def _reseed_detection(record, reseeds):
    if record.get("cancel_reason_supported") is True:
        return {"status": "product", "source": "start-phase record", "reason": None}
    if reseeds is None:
        return {"status": "not_checked", "source": None, "reason": "no Client trace was given for the reseed fallback"}
    detection = {key: reseeds.get(key) for key in ("status", "source", "reason")}
    if reseeds.get("status") == "recorded":
        detection["stall_reseeds"] = {f"{player}/{epoch}/{life}": len(items)
                                      for (player, epoch, life), items in sorted(reseeds["reseeds"].items())}
    return detection


def _summary(record, source, measurement_start_seconds, measurement_end_seconds, measured, reseeds):
    if not isinstance(record, dict):
        raise ValueError("record must be a JSON object")
    _require_finite(record, "record")
    epochs = record["epochs"]
    if not isinstance(epochs, list):
        raise ValueError("epochs must be a list")
    for epoch in epochs:
        _check_epoch(epoch)
    detection = _reseed_detection(record, reseeds)
    if record.get("supported") is False:
        return {"status": "unsupported", "reason": f"{source}: probe was built against a product without start-phase fields",
                "reseed_detection": detection}
    # Player id 0 is never a joined player: such a record was observed before
    # the probe knew its id (older gui probe) and is reported apart.
    unattributed = [epoch for epoch in epochs if epoch.get("player_id") == 0]
    epochs = [epoch for epoch in epochs if epoch.get("player_id") != 0]
    if not epochs:
        detail = f" ({len(unattributed)} record(s) without a player id)" if unattributed else ""
        return {"status": "no_epoch_observed", "unattributed_epochs": unattributed, "reseed_detection": detection,
                "reason": f"{source}: the probe never observed an active movement epoch{detail}"}
    annotated = []
    for epoch in epochs:
        item = dict(epoch)
        if detection["status"] == "product":
            cancellation = _product_cancellation(item)
        elif detection["status"] == "recorded":
            cancellation = _trace_cancellation(item, reseeds["reseeds"], detection["source"])
            if cancellation:
                item["recorded_status"] = item["status"]
                item["status"] = "cancelled_by_reseed"
        else:
            cancellation = None
        if cancellation and measurement_start_seconds is not None and cancellation["steady_ns"] is not None:
            cancellation["seconds_before_measurement"] = measurement_start_seconds - cancellation["steady_ns"] / 1e9
        item["cancelled_by_reseed"] = cancellation
        for side in ("host_wait", "client"):
            # Always present: null when there is no plan or the value was never set.
            item[side + "_set_seconds_before_measurement"] = _seconds_before(measurement_start_seconds, item, side)
        trace_cancelled = cancellation["steady_ns"] if cancellation and cancellation["source"] != "product" else None
        item["measurement_window"] = measurement_window(item, measurement_start_seconds, measurement_end_seconds,
                                                        trace_cancelled)
        annotated.append(item)
    conflicts = {side: sum(item.get(f"{name}_conflicting_frames") or 0 for item in annotated)
                 for side, name in (("host", "host_wait"), ("client", "client"))}
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
            "measured_epoch_withdrawn_during_measurement": window.get("withdrawn_during_measurement"),
            "measured_epoch_cancelled_by_reseed": None if entry is None else entry["status"] == "cancelled_by_reseed",
            "measured_epoch_shift_applied_seconds": window.get("shift_applied_seconds"),
            "measured_epoch_unaligned_seconds": window.get("unaligned_seconds"),
            "status_counts": dict(Counter(item["status"] for item in annotated)),
            "dropped_observations": dropped, "conflicting_frames": conflicts,
            "skip_reason_supported": record.get("skip_reason_supported"),
            "cancel_reason_supported": record.get("cancel_reason_supported"),
            "reseed_detection": detection, "unattributed_epochs": unattributed,
            "unattributed_client_frames": unattributed_frames, "epochs": annotated}


def summarize_start_phase(record, source, measurement_start_seconds=None, measured=None, measurement_end_seconds=None,
                          reseeds=None):
    """One probe's per-epoch record; ``measured`` names the epoch under measurement, if any.

    ``measurement_start_seconds`` and ``measurement_end_seconds`` are the plan's
    window in the probes' steady clock; without both, window states are null.
    ``reseeds`` is that probe's Client trace (read_reseed_evidence/reseed_evidence),
    used only when the record cannot report a reseed cancellation itself.
    """
    measured = dict(measured or NO_MEASUREMENT)
    base = {"source": source, "epochs": [], "status_scope": SHIFT_ARMED_SCOPE, "reseed_scope": TRACE_FALLBACK_SCOPE,
            "measured_epoch": measured.get("epoch"), "measured_epoch_source": measured.get("source"),
            "measured_epoch_reason": measured.get("reason")}
    if record is None:
        return {**base, "status": "absent",
                "reason": f"{source} missing: probe predates start-phase recording or failed before writing it"}
    try:
        return {**base, **_summary(record, source, measurement_start_seconds, measurement_end_seconds, measured, reseeds)}
    except (KeyError, TypeError, ValueError, AttributeError, IndexError, OverflowError, RecursionError) as error:
        detail = f"missing {error}" if isinstance(error, KeyError) else str(error)
        return {**base, "status": "invalid", "reason": f"{source}: {detail}"}
