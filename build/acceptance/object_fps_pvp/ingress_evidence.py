"""Ingress v1 evidence (v7 batch 08c F3): judge one run's Gateway and Match ingress statistics.

  python3 ingress_evidence.py --gateway gateway.log --match match.log [--trace match-ingress.jsonl]
                              [--injection] [--output ingress-evidence.json]
  python3 ingress_evidence.py DIRECTORY   (gateway.log, match.log and, when present, match-ingress.jsonl)

Reads the line families of tests/object_fps_pvp/fixtures/ingress_v1/README.zh-Hant.md (version=1 only), sums the
per-window increments by process and player, and judges the conservations of
docs/object_fps_pvp/plans/v7/08c-drop-report.md exactly:

- Gateway: IG (per player), the Gateway-wide datagram conservation, I0 (per player), T3; J4 is reported only.
- Match: I2 (commands, inputs, action batches and shots, per bucket), J5a, S1, S2 (per player).
- Across the IPC: I1 only when its three preconditions hold (else "undetermined" with the reasons); I3 only when
  the Gateway routed nothing for a future limit, a conflict, the link cap or a written conflict.
- match-ingress.jsonl: schema, version and kinds, trace_end as the last line, and the reconciliation with the
  Match lines that its suppressed and dropped counts allow.
- link_shots (D59): per player, the shot requests handed to the runtime link equal those it wrote plus its three
  drops (fwd_handed_actions_shots == fwd_link_written_actions_shots + drop_{discarded_on_leave,cleared_on_close,
  abandoned_on_close}_actions_shots), exactly. The eight D56/D59 keys stay optional (D59 (11)): unless every player
  ingress line of the Gateway log carries them, link_shots is "undetermined" with the reason and does not decide
  the run; "complete" gives the lines that carried them out of all player ingress lines.
- action_drops (D56) is reported only: what the runtime link dropped of the actions before writing them, and the
  action acknowledgements the Match discarded on a Leave.
- gateway_contentless_drops (D58) is reported only: the drops of the Gateway that the Match never sees content of
  (08c known gap 1: rate limited, malformed, unauthorized, stale Actions/other, unknown kind, inactive), by reason
  and kind, per player, and the Gateway-wide datagram drops.
- J5a's never_arrived (decisive with --injection) is unarrived_overflow + unarrived_end (D58 (2)); records closed
  removed (the run's final Leave) are reported apart, as unarrived_removed.

A missing final=1 line, a missing window line or a missing trace_end makes the affected checks "undetermined";
they are never read as zero. A line of another version, or one that breaks the line format, stops the analysis
("stopped"). Exit status: 0 when the status is "passed", 1 otherwise. Standard library only.
"""
import argparse
import json
from pathlib import Path
import re
import sys

ANALYZER_ID = "pvp-v7-ingress-2"
INGRESS_VERSION = 1
TRACE_SCHEMA = "object_fps_pvp.match_ingress"
TRACE_VERSION = 1

# Key lists of version 1 in line order (apps/object_fps_pvp/gateway/ingress_statistics.go and
# apps/object_fps_pvp/include/RetroFPS/Pvp/IngressStatistics.hpp); test_ingress_evidence.py compares them with
# the golden lines. Keys appended later are kept apart (unknown_keys) and enter no conservation, except the
# known appended keys below.
GATEWAY_PLAYER_KEYS = (
    "received_datagrams", "fwd_main_input_packets", "fwd_rejected_lane_input_packets",
    "fwd_written_duplicate_input_packets", "fwd_stale_sequence_input_packets", "fwd_admitted_hello_packets",
    "fwd_handed_actions_packets", "drop_rate_limited_hello_packets", "drop_rate_limited_input_packets",
    "drop_rate_limited_actions_packets", "drop_rate_limited_other_packets", "drop_unauthorized_hello_packets",
    "drop_unauthorized_input_packets", "drop_unauthorized_actions_packets", "drop_unauthorized_other_packets",
    "drop_stale_sequence_actions_packets", "drop_stale_sequence_other_packets", "drop_malformed_hello_packets",
    "drop_malformed_input_packets", "drop_malformed_actions_packets", "drop_inactive_input_packets",
    "drop_inactive_actions_packets", "drop_invalid_actions_packets", "drop_link_rejected_actions_packets",
    "drop_unknown_kind_packets", "drop_admission_unknown_packets", "drop_invalid_actions_shots",
    "drop_link_rejected_actions_shots", "fwd_handed_main_commands", "fwd_handed_rejected_lane_commands",
    "fwd_link_written_main_commands", "fwd_link_written_rejected_lane_commands", "fwd_merged_duplicate_commands",
    "fwd_written_duplicate_commands", "drop_resolved_trim_commands", "drop_rejected_lane_overflow_commands",
    "drop_discarded_on_leave_commands", "drop_cleared_on_close_commands", "drop_abandoned_on_close_commands",
    "fwd_resolved_at_write_commands", "fwd_retention_expired_commands", "fwd_routed_epoch_mismatch_inputs",
    "fwd_routed_future_limit_inputs", "fwd_routed_conflict_inputs", "fwd_routed_link_cap_inputs",
    "fwd_routed_written_conflict_inputs", "fwd_routed_rotation_inputs", "fwd_routed_link_epoch_mismatch_inputs",
    "fwd_routed_malformed_inputs")
GATEWAY_GLOBAL_KEYS = (
    "received_datagrams", "drop_undecodable_datagrams", "drop_other_version_datagrams",
    "drop_room_unavailable_datagrams", "drop_unknown_session_datagrams", "drop_rate_limited_packets",
    "drop_old_tick_snapshots", "drop_regressed_snapshots")
INPUT_REJECTIONS = ("resetting", "unknown_player", "malformed", "epoch_old", "epoch_future", "life_old",
                    "life_future", "beyond_window", "conflict_queued", "conflict_staged", "staged_over_window")
COMMAND_CLASSES = ("accepted_new", "pending_copy", "late_first", "late_copy", "resolved_copy", "resolved_untracked")
ACTION_REJECTIONS = ("over_batch", "malformed", "resetting", "invalid_player", "invalid_batch", "conflict",
                     "outside_window", "full")
SUBSTITUTION_CLOSES = ("aged", "epoch", "life", "removed", "reset", "overflow", "end")
MATCH_KEYS = (
    ("received_inputs", "received_commands", "accepted_inputs")
    + tuple(f"{name}_commands" for name in COMMAND_CLASSES)
    + ("late_only_inputs",)
    + tuple(f"{reason}_{unit}" for reason in INPUT_REJECTIONS for unit in ("inputs", "commands"))
    + ("handoff_rejected_commands", "rotation_discarded_commands", "leave_discarded_commands",
       "received_actions_batches", "received_actions_shots", "accepted_actions_batches", "accepted_actions_shots")
    + tuple(f"{reason}_actions_{unit}" for reason in ACTION_REJECTIONS for unit in ("batches", "shots"))
    + ("substituted_commands",)
    + tuple(f"unarrived_{close}_commands" for close in SUBSTITUTION_CLOSES)
    + ("reset_discarded_commands", "reset_discarded_actions_shots", "leave_discarded_actions_shots",
       "reset_discarded_actions_acks", "slack_executed_samples", "slack_late_samples", "slack_merged_samples",
       "slack_discarded_samples", "slack_published_samples", "slack_published_negative_samples",
       "slack_overwritten_samples", "slack_taken_samples", "slack_coalesced_samples", "slack_unclaimed_samples"))
# Known keys appended after the v1 keys, in line order, by group: D56 (2026-10-11), then D59 (Gateway only). A line
# carries the groups in order, each whole, right after the v1 keys: none (written before D56), D56 only (before
# D59) or all. The D56 keys are reported (action_drops); the eight Gateway keys together make link_shots.
GATEWAY_PLAYER_APPENDED = (
    "drop_discarded_on_leave_actions_shots", "drop_discarded_on_leave_actions_acks",
    "drop_cleared_on_close_actions_shots", "drop_cleared_on_close_actions_acks",
    "drop_abandoned_on_close_actions_shots", "drop_abandoned_on_close_actions_acks")
GATEWAY_PLAYER_APPENDED_D59 = ("fwd_handed_actions_shots", "fwd_link_written_actions_shots")
GATEWAY_PLAYER_GROUPS = (GATEWAY_PLAYER_APPENDED, GATEWAY_PLAYER_APPENDED_D59)
MATCH_APPENDED = ("leave_discarded_actions_acks",)

# IG: every *_packets outcome from fwd_main_input_packets to drop_admission_unknown_packets.
IG_OUTCOMES = GATEWAY_PLAYER_KEYS[GATEWAY_PLAYER_KEYS.index("fwd_main_input_packets"):
                                  GATEWAY_PLAYER_KEYS.index("drop_admission_unknown_packets") + 1]
GLOBAL_DROPS = GATEWAY_GLOBAL_KEYS[1:5]
RATE_LIMITED = ("drop_rate_limited_hello_packets", "drop_rate_limited_input_packets",
                "drop_rate_limited_actions_packets", "drop_rate_limited_other_packets")
I0_HANDED = ("fwd_handed_main_commands", "fwd_handed_rejected_lane_commands")
I0_OUTCOMES = GATEWAY_PLAYER_KEYS[GATEWAY_PLAYER_KEYS.index("fwd_link_written_main_commands"):
                                  GATEWAY_PLAYER_KEYS.index("drop_abandoned_on_close_commands") + 1]
LINK_WRITTEN = ("fwd_link_written_main_commands", "fwd_link_written_rejected_lane_commands")
# I3 is judged only when these Gateway attributes are 0 for the run (the plan's rejectFuture, rejectConflict,
# rejectLinkCap and written_conflict, by their Gateway key names).
I3_GATES = ("fwd_routed_future_limit_inputs", "fwd_routed_conflict_inputs", "fwd_routed_link_cap_inputs",
            "fwd_routed_written_conflict_inputs")
I3_MATCH_COPIES = ("pending_copy_commands", "late_copy_commands", "resolved_copy_commands")
I3_GATEWAY_COPIES = ("fwd_link_written_rejected_lane_commands", "fwd_retention_expired_commands")
# J5a as the L2 judgement reads it (D58 (2)): records that closed before any copy arrived for a reason other than
# aged, epoch, life, reset or removed. Removed records (closed by the run's final Leave) are reported apart.
NEVER_ARRIVED = ("unarrived_overflow_commands", "unarrived_end_commands")
UNARRIVED_REMOVED = ("unarrived_removed_commands",)
J4_KEYS = ("drop_rejected_lane_overflow_commands", "drop_resolved_trim_commands")
# link_shots (D59): fwd_handed_actions_shots == fwd_link_written_actions_shots + these three.
LINK_SHOT_DROPS = ("drop_discarded_on_leave_actions_shots", "drop_cleared_on_close_actions_shots",
                   "drop_abandoned_on_close_actions_shots")
# The Gateway's drops the Match never sees content of (08c known gap 1, D58), reported only: reason -> datagram
# kinds of its player keys ("" for a key without a kind). The Gateway-wide datagram drops are reported with them.
CONTENTLESS_DROPS = (
    ("rate_limited", ("hello", "input", "actions", "other")),
    ("malformed", ("hello", "input", "actions")),
    ("unauthorized", ("hello", "input", "actions", "other")),
    ("stale_sequence", ("actions", "other")),
    ("unknown_kind", ("",)),
    ("inactive", ("input", "actions")))


def contentless_key(reason, kind):
    return f"drop_{reason}_{kind}_packets" if kind else f"drop_{reason}_packets"


# Fixed log strings (file:line of the writer at batch 08c).
GATEWAY_PLAYER_LINE = re.compile(r"player ingress statistics (?P<fields>.*)$")  # ingress_statistics.go
GATEWAY_GLOBAL_LINE = re.compile(r"gateway ingress statistics (?P<fields>.*)$")  # ingress_statistics.go
MATCH_LINE = re.compile(r"\[ObjectFPS/PvP Match\] ingress statistics (?P<fields>.*)$")  # IngressStatistics.hpp
# The Gateway's window: "gateway statistics" (server.go logStatisticsWindow) precedes the Gateway ingress line
# of every window until the final one; each current player's "player send statistics" line precedes its line.
GATEWAY_WINDOW = re.compile(r"gateway statistics players=\d+ ")
GATEWAY_SEND_WINDOW = re.compile(r"player send statistics player=(?P<player>\d+) ")
GATEWAY_START = "Object FPS Gateway start executable_sha256="  # gateway/cmd/main.go logStartup
# The address the Gateway dialed, once, in gateway.New (gateway/cmd/main.go, after New).
GATEWAY_RUNTIME = re.compile(r"Object FPS Gateway HTTP=\S+ UDP=\S+ advertise=\S+ runtime=(?P<address>\S+)")
GATEWAY_EVICTED = re.compile(r"player evicted player=(?P<player>\d+) ")  # server.go
MATCH_START = "[ObjectFPS/PvP Match] start executable_sha256="  # match_main.cpp
MATCH_READY = re.compile(r"Object_FPS_PVP Match ready: (?P<address>\S+) arena=")  # match_main.cpp
# Every window but the final one writes its network statistics line before its ingress lines (match_main.cpp).
MATCH_WINDOW = "[ObjectFPS/PvP Match] network statistics "
# IpcHost.cpp: Close() logs once per session that reached Connected; a read of EOF closes with reason=eof.
MATCH_IPC_CLOSED = re.compile(r"\[ObjectFPS/PvP Match\] ipc closed reason=(?P<reason>.*)$")
MATCH_IPC_ACCEPT_FAILED = "[ObjectFPS/PvP Match] ipc accept failed"
MATCH_JOIN = re.compile(r"\[ObjectFPS/PvP Match\] join player=(?P<player>\d+) ")
MATCH_EVICTED = re.compile(r"\[ObjectFPS/PvP Match\] evicted player=(?P<player>\d+) ")

TRACE_FIELDS = {
    "rejection": {"time_ns": "int", "player_id": "int", "reason": "rejection", "epoch": "int", "life": "int",
                  "first_sequence": "int?", "last_sequence": "int?", "commands": "int", "cursor": "int",
                  "current_epoch": "int", "current_life": "int"},
    "substitution": {"player_id": "int", "epoch": "int", "life": "int", "sequence": "int", "substituted_ns": "int",
                     "substituted_tick": "int", "first_arrival_ns": "int?", "late_us": "int?",
                     "copies_after_first": "int", "reference_age_us": "int?", "close": "close"},
    "trace_end": {"records": "int", "suppressed": "int", "dropped": "int"},
}


class ContractStop(Exception):
    """A line this analyzer must not interpret: another version, or a broken line format."""


def parse_line(fields, head, keys, number, appended=()):
    """(head values, counts, unknown keys) of one ingress line; raises ContractStop. appended holds the groups of
    known appended keys in line order: a group is counted when the line carries it whole right after keys and the
    groups before it (head["appended"] is how many groups it carried)."""
    pairs = []
    for token in fields.split():
        name, separator, value = token.partition("=")
        if not separator or not name:
            raise ContractStop(f"line {number}: not key=value: {token!r}")
        pairs.append((name, value))
    if not pairs or pairs[0][0] != "version":
        raise ContractStop(f"line {number}: the line does not start with version=")
    if pairs[0][1] != str(INGRESS_VERSION):
        raise ContractStop(f"line {number}: ingress version {pairs[0][1]!r}; this analyzer reads only "
                           f"version={INGRESS_VERSION}")
    names = [name for name, _ in pairs]
    if len(set(names)) != len(names):
        raise ContractStop(f"line {number}: a key appears twice")
    for name, value in pairs:
        if not re.fullmatch(r"\d+", value):
            raise ContractStop(f"line {number}: {name}={value!r} is not a decimal integer")
    if tuple(names[:len(head)]) != head:
        raise ContractStop(f"line {number}: head {names[:len(head)]} differs from {list(head)}")
    body = pairs[len(head):]
    if tuple(name for name, _ in body[:len(keys)]) != keys:
        raise ContractStop(f"line {number}: the keys differ from version {INGRESS_VERSION} (missing or reordered)")
    values = dict(pairs[:len(head)])
    final = values.get("final")
    if final not in ("0", "1"):
        raise ContractStop(f"line {number}: final={final!r}")
    known = len(keys)
    carried = 0
    for group in appended:
        if tuple(name for name, _ in body[known:known + len(group)]) != group:
            break
        known += len(group)
        carried += 1
    return ({"player": int(values["player"]) if "player" in values else None,
             "window_ms": int(values["window_ms"]), "final": final == "1", "line": number, "appended": carried},
            {name: int(value) for name, value in body[:known]},
            {name: int(value) for name, value in body[known:]})


def add_counts(total, counts):
    for name, value in counts.items():
        total[name] = total.get(name, 0) + value


def window_problem(state, text):
    state["gaps"].append(text)


def read_gateway(text):
    """Windows, per-player and Gateway-wide totals and lifecycle facts of one Gateway log."""
    state = {"starts": 0, "runtime_address": None, "evicted": set(), "windows": 0, "final": False,
             "final_line": None, "gaps": [], "players": {}, "global": {}, "unknown_keys": set(),
             "appended_lines": {"with": 0, "without": 0}, "link_lines": {"carried": 0, "lines": 0}}
    awaiting = None  # line of a "gateway statistics" line still waiting for its ingress window
    current = None  # {"window_ms", "final", "players": set}
    waiting_players = {}  # player -> line of its "player send statistics" line in the current window
    seen_players = set()

    def close_window():
        for player, line in sorted(waiting_players.items()):
            window_problem(state, f"line {line}: player {player} has a send statistics line but no ingress line "
                                  "in its window")
        waiting_players.clear()

    for number, line in enumerate(text.splitlines(), 1):
        if GATEWAY_START in line:
            state["starts"] += 1
            continue
        if match := GATEWAY_RUNTIME.search(line):
            state["runtime_address"] = match["address"]
            continue
        if match := GATEWAY_EVICTED.search(line):
            state["evicted"].add(int(match["player"]))
            continue
        if match := GATEWAY_GLOBAL_LINE.search(line):
            head, counts, unknown = parse_line(match["fields"], ("version", "window_ms", "final"),
                                               GATEWAY_GLOBAL_KEYS, number)
            state["unknown_keys"].update(unknown)
            if state["final"]:
                window_problem(state, f"line {number}: an ingress window after the final one")
            close_window()
            if not head["final"] and awaiting is None:
                window_problem(state, f"line {number}: an ingress window without its gateway statistics line")
            if head["final"] and awaiting is not None:
                window_problem(state, f"line {awaiting}: a gateway statistics line without its ingress window")
            awaiting = None
            current = {"window_ms": head["window_ms"], "final": head["final"], "players": set()}
            state["windows"] += 1
            add_counts(state["global"], counts)
            if head["final"]:
                state["final"], state["final_line"] = True, number
            continue
        if match := GATEWAY_PLAYER_LINE.search(line):
            head, counts, unknown = parse_line(match["fields"], ("version", "player", "window_ms", "final"),
                                               GATEWAY_PLAYER_KEYS, number, GATEWAY_PLAYER_GROUPS)
            state["unknown_keys"].update(unknown)
            state["appended_lines"]["with" if head["appended"] >= 1 else "without"] += 1
            state["link_lines"]["lines"] += 1
            if head["appended"] == len(GATEWAY_PLAYER_GROUPS):
                state["link_lines"]["carried"] += 1
            player = head["player"]
            if current is None:
                window_problem(state, f"line {number}: a player ingress line before any Gateway ingress window")
            elif player in current["players"]:
                window_problem(state, f"line {number}: player {player} has two lines in one window")
            elif head["window_ms"] != current["window_ms"] or head["final"] != current["final"]:
                window_problem(state, f"line {number}: player {player}'s window_ms/final differ from its window")
            if current is not None:
                current["players"].add(player)
            waiting_players.pop(player, None)
            seen_players.add(player)
            add_counts(state["players"].setdefault(player, {}), counts)
            continue
        if GATEWAY_WINDOW.search(line):
            if state["final"]:
                continue  # after runtimeFailed the statistics go on without ingress lines
            if awaiting is not None:
                window_problem(state, f"line {awaiting}: a gateway statistics line without its ingress window")
            close_window()
            awaiting = number
            continue
        if (match := GATEWAY_SEND_WINDOW.search(line)) and not state["final"] and current is not None \
                and awaiting is None:
            waiting_players[int(match["player"])] = number
    close_window()
    if awaiting is not None:
        window_problem(state, f"line {awaiting}: a gateway statistics line without its ingress window")
    if state["final"]:
        missing = sorted(seen_players - current["players"])
        if missing:
            window_problem(state, f"players {missing} have no line in the final window")
    return state


def read_match(text):
    """Windows, per-bucket totals and IPC lifecycle facts of one Match log."""
    state = {"starts": 0, "listen_address": None, "ipc_closed": [], "accept_failed": False, "joins": [],
             "evicted": set(), "windows": 0, "final": False, "final_line": None, "gaps": [], "players": {},
             "unknown_keys": set(), "appended_lines": {"with": 0, "without": 0}}
    awaiting = None  # line of a network statistics line still waiting for its ingress window
    current = None
    seen_players = set()
    for number, line in enumerate(text.splitlines(), 1):
        if MATCH_START in line:
            state["starts"] += 1
            continue
        if match := MATCH_READY.search(line):
            state["listen_address"] = match["address"]
            continue
        if match := MATCH_IPC_CLOSED.search(line):
            state["ipc_closed"].append((number, match["reason"].strip()))
            continue
        if MATCH_IPC_ACCEPT_FAILED in line:
            state["accept_failed"] = True
            continue
        if match := MATCH_JOIN.search(line):
            state["joins"].append((number, int(match["player"])))
            continue
        if match := MATCH_EVICTED.search(line):
            state["evicted"].add(int(match["player"]))
            continue
        if MATCH_WINDOW in line:
            if state["final"]:
                window_problem(state, f"line {number}: a network statistics line after the final window")
            if awaiting is not None:
                window_problem(state, f"line {awaiting}: a network statistics line without its ingress window")
            awaiting = number
            continue
        if match := MATCH_LINE.search(line):
            head, counts, unknown = parse_line(match["fields"], ("version", "player", "window_ms", "final"),
                                               MATCH_KEYS, number, (MATCH_APPENDED,))
            state["unknown_keys"].update(unknown)
            state["appended_lines"]["with" if head["appended"] >= 1 else "without"] += 1
            player = head["player"]
            if player == 0:  # every window starts with the player=0 bucket (lines are in player order)
                if state["final"]:
                    window_problem(state, f"line {number}: an ingress window after the final one")
                if not head["final"] and awaiting is None:
                    window_problem(state, f"line {number}: an ingress window without its network statistics line")
                if head["final"] and awaiting is not None:
                    window_problem(state, f"line {awaiting}: a network statistics line without its ingress window")
                awaiting = None
                current = {"window_ms": head["window_ms"], "final": head["final"], "players": [0]}
                state["windows"] += 1
                if head["final"]:
                    state["final"], state["final_line"] = True, number
            elif current is None:
                window_problem(state, f"line {number}: a Match ingress line before any player=0 line")
            elif player <= current["players"][-1]:
                window_problem(state, f"line {number}: player {player} out of order or twice in its window "
                                      "(a player=0 line is missing)")
            elif head["window_ms"] != current["window_ms"] or head["final"] != current["final"]:
                window_problem(state, f"line {number}: player {player}'s window_ms/final differ from its window "
                                      "(a player=0 line is missing)")
            if current is not None and player != 0:
                current["players"].append(player)
            seen_players.add(player)
            add_counts(state["players"].setdefault(player, {}), counts)
    if awaiting is not None:
        window_problem(state, f"line {awaiting}: a network statistics line without its ingress window")
    if state["final"]:
        missing = sorted(seen_players - set(current["players"]))
        if missing:
            window_problem(state, f"players {missing} have no line in the final window")
    return state


def completeness(state, name):
    """Reasons why the totals of one log are not the process's totals (empty when they are)."""
    if state is None:
        return [f"no {name} log"]
    reasons = []
    if state["starts"] != 1:
        reasons.append(f"{name} log has {state['starts']} start lines (expected one process)")
    if not state["windows"]:
        reasons.append(f"{name} log has no ingress line")
    elif not state["final"]:
        reasons.append(f"{name} log has no final=1 window: the process did not reach its end path")
    reasons.extend(state["gaps"])
    return reasons


def total(counts, keys):
    return sum(counts.get(key, 0) for key in keys)


def equality(lhs, rhs, lhs_text, rhs_text):
    return {"lhs": lhs, "rhs": rhs, "equation": f"{lhs_text} == {rhs_text}", "holds": lhs == rhs}


def per_bucket_check(players, terms):
    """One exact check over every bucket: terms(counts) -> list of equality dicts."""
    buckets, holds = {}, True
    for player in sorted(players):
        parts = terms(players[player])
        buckets[str(player)] = parts
        holds = holds and all(part["holds"] for part in parts)
    return {"status": "passed" if holds else "failed", "players": buckets}


def undetermined(reasons):
    return {"status": "undetermined", "reasons": list(reasons)}


def gateway_checks(gateway):
    reasons = completeness(gateway, "Gateway")
    if reasons:
        return {name: undetermined(reasons) for name in ("IG", "gateway_global", "I0", "T3", "J4")}
    players, glob = gateway["players"], gateway["global"]
    checks = {
        "IG": per_bucket_check(players, lambda c: [equality(
            c.get("received_datagrams", 0), total(c, IG_OUTCOMES), "received_datagrams", "sum(outcome *_packets)")]),
        "I0": per_bucket_check(players, lambda c: [equality(
            total(c, I0_HANDED), total(c, I0_OUTCOMES), "handed commands", "sum(written, deduplicated, dropped)")]),
    }
    received = glob.get("received_datagrams", 0)
    accounted = total(glob, GLOBAL_DROPS) + sum(c.get("received_datagrams", 0) for c in players.values())
    part = equality(received, accounted, "gateway received_datagrams",
                    "four drop_*_datagrams + sum(player received_datagrams)")
    checks["gateway_global"] = dict(part, status="passed" if part["holds"] else "failed")
    part = equality(sum(total(c, RATE_LIMITED) for c in players.values()), glob.get("drop_rate_limited_packets", 0),
                    "sum(player drop_rate_limited_*_packets)", "gateway drop_rate_limited_packets")
    checks["T3"] = dict(part, status="passed" if part["holds"] else "failed")
    checks["J4"] = {"status": "reported",
                    "total": {key: sum(c.get(key, 0) for c in players.values()) for key in J4_KEYS},
                    "players": {str(p): {key: c.get(key, 0) for key in J4_KEYS} for p, c in sorted(players.items())}}
    return checks


def match_checks(match, injection):
    reasons = completeness(match, "Match")
    if reasons:
        return {name: undetermined(reasons) for name in ("I2", "J5a", "S1", "S2")}
    players = match["players"]

    def i2(c):
        rejected = lambda unit: sum(c.get(f"{reason}_{unit}", 0) for reason in INPUT_REJECTIONS)
        actions = lambda unit: sum(c.get(f"{reason}_actions_{unit}", 0) for reason in ACTION_REJECTIONS)
        return [
            equality(c.get("received_commands", 0),
                     sum(c.get(f"{name}_commands", 0) for name in COMMAND_CLASSES) + rejected("commands"),
                     "received_commands", "six classes + sum(<reason>_commands)"),
            equality(c.get("received_inputs", 0), c.get("accepted_inputs", 0) + rejected("inputs"),
                     "received_inputs", "accepted_inputs + sum(<reason>_inputs)"),
            equality(c.get("received_actions_batches", 0), c.get("accepted_actions_batches", 0) + actions("batches"),
                     "received_actions_batches", "accepted_actions_batches + sum(<reason>_actions_batches)"),
            equality(c.get("received_actions_shots", 0), c.get("accepted_actions_shots", 0) + actions("shots"),
                     "received_actions_shots", "accepted_actions_shots + sum(<reason>_actions_shots)")]

    checks = {
        "I2": per_bucket_check(players, i2),
        "J5a": per_bucket_check(players, lambda c: [equality(
            c.get("substituted_commands", 0),
            c.get("late_first_commands", 0) + sum(c.get(f"unarrived_{close}_commands", 0)
                                                  for close in SUBSTITUTION_CLOSES),
            "substituted_commands", "late_first_commands + seven unarrived_<close>_commands")]),
        "S1": per_bucket_check(players, lambda c: [equality(
            c.get("slack_executed_samples", 0) + c.get("slack_late_samples", 0),
            c.get("slack_merged_samples", 0) + c.get("slack_discarded_samples", 0) + c.get("slack_published_samples", 0),
            "slack_executed + slack_late", "slack_merged + slack_discarded + slack_published")]),
        "S2": per_bucket_check(players, lambda c: [equality(
            c.get("slack_published_samples", 0),
            c.get("slack_overwritten_samples", 0) + c.get("slack_taken_samples", 0) + c.get("slack_unclaimed_samples", 0),
            "slack_published", "slack_overwritten + slack_taken + slack_unclaimed")]),
    }
    never = {str(p): total(c, NEVER_ARRIVED) for p, c in sorted(players.items())}
    removed = {str(p): total(c, UNARRIVED_REMOVED) for p, c in sorted(players.items())}
    j5a = checks["J5a"]
    j5a["never_arrived"] = {"terms": list(NEVER_ARRIVED), "players": never, "total": sum(never.values()),
                            "zero": not any(never.values()), "decisive": injection}
    j5a["unarrived_removed"] = {"terms": list(UNARRIVED_REMOVED), "players": removed,
                                "total": sum(removed.values()), "decisive": False}
    if injection and any(never.values()) and j5a["status"] == "passed":
        j5a["status"] = "failed"
    return checks


def action_drops(gateway, match):
    """D56, reported only: per process, the appended keys' totals and per-bucket values, and how many ingress lines
    carried them (a line written before D56 does not; its share is unknown, not zero)."""
    report = {"status": "reported"}
    for name, state, keys in (("gateway", gateway, GATEWAY_PLAYER_APPENDED), ("match", match, MATCH_APPENDED)):
        if state is None:
            continue
        players = state["players"]
        report[name] = {"total": {key: sum(c.get(key, 0) for c in players.values()) for key in keys},
                        "players": {str(p): {key: c.get(key, 0) for key in keys} for p, c in sorted(players.items())},
                        "lines": dict(state["appended_lines"]),
                        "complete": state["appended_lines"]["without"] == 0}
    return report


def link_shots(gateway):
    """D59, exact per player: fwd_handed_actions_shots == fwd_link_written_actions_shots + three
    drop_*_actions_shots. Judged only when every player ingress line carries the eight D56/D59 keys: a line without
    them has an unknown share, not zero (the keys stay optional, D59 (11))."""
    reasons = completeness(gateway, "Gateway")
    if gateway is None:
        return undetermined(reasons)
    lines = gateway["link_lines"]
    complete = {"carried": lines["carried"], "lines": lines["lines"]}
    if not lines["lines"]:
        reasons.append("the Gateway log has no player ingress line")
    elif lines["carried"] != lines["lines"]:
        reasons.append(f"{lines['lines'] - lines['carried']} of {lines['lines']} player ingress lines lack the "
                       "D56/D59 keys (written before D59, or before D56): their share is unknown, not zero")
    if reasons:
        return dict(undetermined(reasons), complete=complete)
    check = per_bucket_check(gateway["players"], lambda c: [equality(
        c.get("fwd_handed_actions_shots", 0), c.get("fwd_link_written_actions_shots", 0) + total(c, LINK_SHOT_DROPS),
        "fwd_handed_actions_shots", "fwd_link_written_actions_shots + three drop_*_actions_shots")])
    return dict(check, complete=complete)


def contentless_drops(gateway):
    """D58, reported only: the Gateway's drops the Match never sees content of, by reason and kind, per player, and
    the Gateway-wide datagram drops; "incomplete" lists why the totals may not be the whole process's."""
    if gateway is None:
        return {"status": "absent"}

    def by_reason(counts):
        out = {}
        for reason, kinds in CONTENTLESS_DROPS:
            values = {kind or "any": counts.get(contentless_key(reason, kind), 0) for kind in kinds}
            out[reason] = dict(values, total=sum(values.values()))
        return out

    players = {str(p): by_reason(c) for p, c in sorted(gateway["players"].items())}
    reasons = {}
    for reason, kinds in CONTENTLESS_DROPS:
        reasons[reason] = {kind: sum(player[reason][kind] for player in players.values())
                           for kind in [kind or "any" for kind in kinds] + ["total"]}
    datagrams = {key: gateway["global"].get(key, 0) for key in GLOBAL_DROPS}
    return {"status": "reported", "total": sum(reason["total"] for reason in reasons.values()), "reasons": reasons,
            "players": {p: dict(by, total=sum(reason["total"] for reason in by.values())) for p, by in players.items()},
            "gateway_datagrams": dict(datagrams, total=sum(datagrams.values())),
            "incomplete": completeness(gateway, "Gateway")}


def judge_i1(gateway, match):
    """I1: the Gateway's written commands equal the Match's received_commands (whole link, then per player)."""
    reasons = completeness(gateway, "Gateway") + completeness(match, "Match")
    if reasons:
        return undetermined(reasons)
    preconditions = {}
    final_line = match["final_line"]
    closes = match["ipc_closed"]
    preconditions["eof_before_final"] = any(reason == "eof" and line < final_line for line, reason in closes)
    later_joins = [player for line, player in match["joins"] if closes and line > closes[0][0]]
    preconditions["single_connection"] = (len(closes) == 1 and not match["accept_failed"]
                                          and gateway["starts"] == 1 and not later_joins)
    preconditions["no_ipc_pause"] = (gateway["runtime_address"] is not None
                                     and gateway["runtime_address"] == match["listen_address"])
    if not all(preconditions.values()):
        why = []
        if not preconditions["eof_before_final"]:
            why.append("match.log has no 'ipc closed reason=eof' before its final=1 window")
        if not preconditions["single_connection"]:
            why.append(f"not a single IPC connection: {len(closes)} ipc closed line(s), accept failed="
                       f"{match['accept_failed']}, Gateway starts={gateway['starts']}, joins after the close="
                       f"{later_joins}")
        if not preconditions["no_ipc_pause"]:
            why.append(f"the Gateway dialed {gateway['runtime_address']} but the Match listened on "
                       f"{match['listen_address']} (a relay such as IpcPause sat between them)")
        return {"status": "undetermined", "reasons": why, "preconditions": preconditions}
    written = {p: total(c, LINK_WRITTEN) for p, c in gateway["players"].items()}
    received = {p: c.get("received_commands", 0) for p, c in match["players"].items()}
    link = equality(sum(written.values()), sum(received.values()),
                    "sum(Gateway fwd_link_written_*_commands)", "sum(Match received_commands, player=0 included)")
    excluded = sorted(gateway["evicted"] | match["evicted"])
    players = {}
    for player in sorted((set(written) | set(received)) - {0} - set(excluded)):
        players[str(player)] = equality(written.get(player, 0), received.get(player, 0),
                                        "Gateway written", "Match received_commands")
    holds = link["holds"] and all(part["holds"] for part in players.values())
    return {"status": "passed" if holds else "failed", "preconditions": preconditions, "link": link,
            "players": players, "excluded_players": excluded}


def judge_i3(gateway, match):
    reasons = completeness(gateway, "Gateway") + completeness(match, "Match")
    if reasons:
        return undetermined(reasons)
    gates = {key: sum(c.get(key, 0) for c in gateway["players"].values()) for key in I3_GATES}
    if any(gates.values()):
        return {"status": "undetermined", "reasons": [f"{key}={value}" for key, value in gates.items() if value],
                "gates": gates}
    copies = sum(total(c, I3_MATCH_COPIES) for c in match["players"].values())
    allowed = sum(total(c, I3_GATEWAY_COPIES) for c in gateway["players"].values())
    return {"status": "passed" if copies <= allowed else "failed", "gates": gates,
            "inequality": f"Match pending_copy+late_copy+resolved_copy ({copies}) <= Gateway "
                          f"rejected-lane written + retention expired ({allowed})",
            "match_copies": copies, "gateway_copies": allowed}


def field_ok(value, kind):
    integer = isinstance(value, int) and not isinstance(value, bool)
    if kind == "int":
        return integer
    if kind == "int?":
        return value is None or integer
    if kind == "rejection":
        return value in INPUT_REJECTIONS
    if kind == "close":
        return value in SUBSTITUTION_CLOSES
    return False


def toward_zero(numerator, denominator):
    quotient = abs(numerator) // denominator
    return quotient if numerator >= 0 else -quotient


def read_trace(text):
    """Records, trace_end and structural errors of match-ingress.jsonl; raises ContractStop on another version."""
    state = {"rejections": [], "substitutions": [], "trace_end": None, "lines": 0, "errors": []}
    for number, line in enumerate(text.splitlines(), 1):
        state["lines"] = number
        try:
            record = json.loads(line)
        except ValueError:
            raise ContractStop(f"trace line {number}: not JSON")
        if not isinstance(record, dict):
            raise ContractStop(f"trace line {number}: not a JSON object")
        if record.get("schema") != TRACE_SCHEMA:
            raise ContractStop(f"trace line {number}: schema {record.get('schema')!r}")
        if record.get("version") != TRACE_VERSION:
            raise ContractStop(f"trace line {number}: trace version {record.get('version')!r}; this analyzer reads "
                               f"only version {TRACE_VERSION}")
        kind = record.get("kind")
        if kind not in TRACE_FIELDS:
            raise ContractStop(f"trace line {number}: unknown kind {kind!r}")
        if state["trace_end"] is not None:
            state["errors"].append(f"trace line {number}: a record after trace_end")
        bad = [name for name, spec in TRACE_FIELDS[kind].items() if name not in record or not field_ok(record[name], spec)]
        if bad:
            state["errors"].append(f"trace line {number}: {kind} fields missing or invalid: {bad}")
            continue
        if kind == "rejection":
            if (record["first_sequence"] is None) != (record["last_sequence"] is None) or \
                    (record["first_sequence"] is None) != (record["commands"] == 0):
                state["errors"].append(f"trace line {number}: sequences must be null exactly when commands is 0")
            state["rejections"].append(record)
        elif kind == "substitution":
            arrived = record["first_arrival_ns"] is not None
            if not arrived and (record["late_us"] is not None or record["reference_age_us"] is not None
                                or record["copies_after_first"] != 0):
                state["errors"].append(f"trace line {number}: an unarrived record with late_us, reference_age_us "
                                       "or copies")
            if arrived and record["late_us"] != toward_zero(record["first_arrival_ns"] - record["substituted_ns"], 1000):
                state["errors"].append(f"trace line {number}: late_us differs from (first_arrival_ns - "
                                       "substituted_ns) / 1000")
            state["substitutions"].append(record)
        elif state["trace_end"] is None:
            state["trace_end"] = dict(record, line=number)
    end = state["trace_end"]
    if end is not None and end["records"] != end["line"] - 1:
        state["errors"].append(f"trace_end records={end['records']} but {end['line'] - 1} line(s) precede it")
    return state


def trace_checks(trace, match):
    """Structure of the detail file and its reconciliation with the Match lines."""
    if trace is None:
        return {"status": "absent"}
    result = {"records": trace["lines"], "rejections": len(trace["rejections"]),
              "substitutions": len(trace["substitutions"])}
    if trace["errors"]:
        return dict(result, status="failed", errors=trace["errors"])
    end = trace["trace_end"]
    if end is None:
        return dict(result, status="undetermined",
                    reasons=["no trace_end line: the Match did not end normally"])
    result.update(suppressed=end["suppressed"], dropped=end["dropped"])
    reasons = completeness(match, "Match")
    if reasons:
        return dict(result, status="undetermined", reasons=reasons)
    players = match["players"]
    rejected_inputs = sum(c.get(f"{reason}_inputs", 0) for c in players.values() for reason in INPUT_REJECTIONS)
    substituted = sum(c.get("substituted_commands", 0) for c in players.values())
    rejections, substitutions = trace["rejections"], trace["substitutions"]
    parts = {"all_events": equality(
        len(rejections) + len(substitutions) + end["suppressed"] + end["dropped"], rejected_inputs + substituted,
        "rejection + substitution records + suppressed + dropped",
        "sum(<reason>_inputs) + sum(substituted_commands)")}
    not_reconciled = ["action rejections (counted only, no records)"]
    if end["dropped"] == 0:
        parts["rejection_events"] = equality(len(rejections) + end["suppressed"], rejected_inputs,
                                             "rejection records + suppressed", "sum(<reason>_inputs)")
        parts["substitution_records"] = equality(len(substitutions), substituted, "substitution records",
                                                 "sum(substituted_commands)")
        for player in sorted(set(players) | {r["player_id"] for r in substitutions}):
            parts[f"substitution_records_player_{player}"] = equality(
                sum(r["player_id"] == player for r in substitutions),
                players.get(player, {}).get("substituted_commands", 0),
                f"substitution records of player {player}", "its substituted_commands")
        parts["late_first"] = equality(sum(r["first_arrival_ns"] is not None for r in substitutions),
                                       sum(c.get("late_first_commands", 0) for c in players.values()),
                                       "arrived substitution records", "sum(late_first_commands)")
        parts["late_copy"] = equality(sum(r["copies_after_first"] for r in substitutions),
                                      sum(c.get("late_copy_commands", 0) for c in players.values()),
                                      "sum(copies_after_first)", "sum(late_copy_commands)")
        for close in SUBSTITUTION_CLOSES:
            parts[f"unarrived_{close}"] = equality(
                sum(r["first_arrival_ns"] is None and r["close"] == close for r in substitutions),
                sum(c.get(f"unarrived_{close}_commands", 0) for c in players.values()),
                f"unarrived records closed {close}", f"sum(unarrived_{close}_commands)")
    else:
        not_reconciled.append(f"per-record counts: {end['dropped']} record(s) dropped on a full buffer")
    if end["dropped"] == 0 and end["suppressed"] == 0:
        for reason in INPUT_REJECTIONS:
            kept = [r for r in rejections if r["reason"] == reason]
            parts[f"rejected_{reason}_inputs"] = equality(
                len(kept), sum(c.get(f"{reason}_inputs", 0) for c in players.values()),
                f"{reason} records", f"sum({reason}_inputs)")
            parts[f"rejected_{reason}_commands"] = equality(
                sum(r["commands"] for r in kept), sum(c.get(f"{reason}_commands", 0) for c in players.values()),
                f"sum({reason} record commands)", f"sum({reason}_commands)")
    elif end["suppressed"]:
        not_reconciled.append(f"per-reason rejections: {end['suppressed']} suppressed over the per-window bound")
    holds = all(part["holds"] for part in parts.values())
    return dict(result, status="passed" if holds else "failed", reconciliation=parts, not_reconciled=not_reconciled)


def analyze_text(gateway_text, match_text, trace_text=None, injection=False):
    """The judgement as a dictionary; any text may be None (its checks are then undetermined or absent)."""
    stops = []
    gateway = match = trace = None
    for name, text, reader in (("Gateway", gateway_text, read_gateway), ("Match", match_text, read_match),
                               ("trace", trace_text, read_trace)):
        if text is None:
            continue
        try:
            value = reader(text)
        except ContractStop as stop:
            stops.append(f"{name}: {stop}")
            continue
        if name == "Gateway":
            gateway = value
        elif name == "Match":
            match = value
        else:
            trace = value
    result = {"analyzer_id": ANALYZER_ID, "ingress_version": INGRESS_VERSION}
    if stops:
        return dict(result, status="stopped", passed=False, stops=stops)
    checks = {}
    checks.update(gateway_checks(gateway))
    checks.update(match_checks(match, injection))
    checks["I1"] = judge_i1(gateway, match)
    checks["I3"] = judge_i3(gateway, match)
    checks["trace"] = trace_checks(trace, match) if trace_text is not None else {"status": "absent"}
    checks["link_shots"] = link_shots(gateway)
    checks["action_drops"] = action_drops(gateway, match)
    checks["gateway_contentless_drops"] = contentless_drops(gateway)
    exact = ("IG", "gateway_global", "I0", "T3", "I2", "J5a", "S1", "S2")
    statuses = [checks[name]["status"] for name in exact] + [checks["I1"]["status"], checks["I3"]["status"],
                                                              checks["link_shots"]["status"], checks["trace"]["status"]]
    if "failed" in statuses:
        status = "failed"
    elif any(checks[name]["status"] == "undetermined" for name in exact) or checks["trace"]["status"] == "undetermined":
        # I1, I3 and link_shots are conditional: their undetermined alone does not decide the run
        status = "undetermined"
    else:
        status = "passed"
    summary = {}
    for name, state in (("gateway", gateway), ("match", match)):
        if state is not None:
            summary[name] = {"windows": state["windows"], "final": state["final"], "gaps": state["gaps"],
                             "unknown_keys": sorted(state["unknown_keys"]),
                             "evicted": sorted(state["evicted"]),
                             "players": {str(p): c for p, c in sorted(state["players"].items())}}
    if gateway is not None:
        summary["gateway"]["global"] = gateway["global"]
        summary["gateway"]["runtime_address"] = gateway["runtime_address"]
    if match is not None:
        summary["match"]["listen_address"] = match["listen_address"]
        summary["match"]["ipc_closed"] = [{"line": line, "reason": reason} for line, reason in match["ipc_closed"]]
    return dict(result, status=status, passed=status == "passed", checks=checks, totals=summary)


def read_optional(path):
    return None if path is None else Path(path).read_text(encoding="utf-8", errors="replace")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path)
    parser.add_argument("--gateway", type=Path)
    parser.add_argument("--match", type=Path)
    parser.add_argument("--trace", type=Path, help="match-ingress.jsonl")
    parser.add_argument("--injection", action="store_true",
                        help="an injection run: J5a also requires never_arrived == 0")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    if args.directory is not None:
        args.gateway = args.gateway or args.directory / "gateway.log"
        args.match = args.match or args.directory / "match.log"
        if args.trace is None and (args.directory / "match-ingress.jsonl").exists():
            args.trace = args.directory / "match-ingress.jsonl"
    if args.gateway is None and args.match is None:
        parser.error("give a run directory or --gateway/--match")
    result = analyze_text(read_optional(args.gateway), read_optional(args.match), read_optional(args.trace),
                          injection=args.injection)
    text = json.dumps(result, indent=2) + "\n"
    if args.output is not None:
        args.output.write_text(text, encoding="utf-8")
    sys.stdout.write(text)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
