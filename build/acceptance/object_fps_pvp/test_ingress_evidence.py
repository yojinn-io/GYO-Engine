"""Self-test of the ingress v1 evidence (ingress_evidence.py) on the golden lines and synthetic logs."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import ingress_evidence as evidence

GOLDEN = Path(__file__).resolve().parents[3] / "tests" / "object_fps_pvp" / "fixtures" / "ingress_v1"
GATEWAY_START = "2026/10/11 04:00:00.000000 Object FPS Gateway start executable_sha256=00 go=go1 os=x/y args=[]"
GATEWAY_READY = ("2026/10/11 04:00:00.000001 Object FPS Gateway HTTP=127.0.0.1:1 UDP=127.0.0.1:2 "
                 "advertise=127.0.0.1 runtime=127.0.0.1:27016")
MATCH_START = "[ObjectFPS/PvP Match] start executable_sha256=00 arena_file=a.json"
MATCH_READY = "Object_FPS_PVP Match ready: 127.0.0.1:27016 arena=test authority=60Hz"
MATCH_EOF = "[ObjectFPS/PvP Match] ipc closed reason=eof"


def golden_lines(name):
    return (GOLDEN / name).read_text(encoding="utf-8").splitlines()


def with_value(line, key, value):
    """The line with key=value replaced; the key must be present exactly once."""
    tokens = line.split(" ")
    hits = [i for i, token in enumerate(tokens) if token.startswith(key + "=")]
    assert len(hits) == 1, (key, line)
    tokens[hits[0]] = f"{key}={value}"
    return " ".join(tokens)


def value_of(line, key):
    return int(next(token for token in line.split(" ") if token.startswith(key + "="))[len(key) + 1:])


def golden_gateway(global_lines=None, player_lines=None):
    """A Gateway log built from the golden lines: three windows, the last one final."""
    global_lines = global_lines or golden_lines("gateway-global.txt")
    player_lines = player_lines or golden_lines("gateway-player.txt")
    log = [GATEWAY_START, GATEWAY_READY]
    for index, (glob, player) in enumerate(zip(global_lines, player_lines)):
        final = index == len(global_lines) - 1
        if not final:
            log.append("2026/10/11 04:00:10.000000 gateway statistics players=1 rate_accepted_packets=0 "
                       "rate_limited_packets=0")
        log.append("2026/10/11 04:00:10.000001 " + glob)
        if not final:
            log.append("2026/10/11 04:00:10.000002 player statistics player=7 phase=2 udp=x received=0 "
                       "last_received_ms=1")
            log.append("2026/10/11 04:00:10.000003 player send statistics player=7 window_ms=10000 results=0")
        log.append("2026/10/11 04:00:10.000004 " + player)
    return "\n".join(log) + "\n"


def zero_bucket(line):
    """The player=0 line of the same window: every count 0 (the Match writes it every window)."""
    tokens = line.split(" ")
    out = []
    for token in tokens:
        key, separator, _ = token.partition("=")
        if not separator or key in ("version", "window_ms", "final"):
            out.append(token)
        elif key == "player":
            out.append("player=0")
        else:
            out.append(f"{key}=0")
    return " ".join(out)


def golden_match(lines=None, eof=True):
    """A Match log built from the golden lines with the player=0 bucket of each window."""
    lines = lines or golden_lines("match.txt")
    log = [MATCH_START, MATCH_READY]
    for index, line in enumerate(lines):
        final = " final=1 " in line
        if final and eof:
            log.append(MATCH_EOF)
        if not final:
            log.append("[ObjectFPS/PvP Match] network statistics window_s=10.000 cpu_s=0.1")
        log.append(zero_bucket(line))
        log.append(line)
    return "\n".join(log) + "\n"


def line_of(prefix, keys, values, head):
    return prefix + " ".join([f"version={evidence.INGRESS_VERSION}"] + head
                             + [f"{key}={values.get(key, 0)}" for key in keys])


def run_logs(players, *, windows=1, eof=True, written=None, match_received=None, runtime="127.0.0.1:27016",
             extra_closes=(), gateway_values=None, match_values=None):
    """Consistent Gateway and Match logs of one run: each player hands and writes `written` commands, which the
    Match receives and accepts as new; `windows` windows, the last one final."""
    gateway_values = gateway_values or {}
    match_values = match_values or {}
    gw = [GATEWAY_START, GATEWAY_READY.replace("runtime=127.0.0.1:27016", f"runtime={runtime}")]
    ma = [MATCH_START, MATCH_READY]
    for window in range(windows):
        final = window == windows - 1
        flag = f"final={int(final)}"
        share = lambda count: count if final else 0  # everything lands in the final window
        if not final:
            gw.append("gateway statistics players=2 rate_accepted_packets=0 rate_limited_packets=0")
        received = sum(share(written.get(p, 0) // 10) for p in players)
        gw.append(line_of("gateway ingress statistics ", evidence.GATEWAY_GLOBAL_KEYS,
                          dict({"received_datagrams": received}, **(gateway_values.get("global", {}) if final else {})),
                          ["window_ms=10000", flag]))
        for player in players:
            if not final:
                gw.append(f"player send statistics player={player} window_ms=10000 results=0")
            count = share(written.get(player, 0))
            values = {"received_datagrams": count // 10, "fwd_main_input_packets": count // 10,
                      "fwd_handed_main_commands": count, "fwd_link_written_main_commands": count}
            if final:
                values.update(gateway_values.get(player, {}))
            gw.append(line_of("player ingress statistics ", evidence.GATEWAY_PLAYER_KEYS, values,
                              [f"player={player}", "window_ms=10000", flag]))
        if final:
            if eof:
                ma.append(MATCH_EOF)
            ma.extend(extra_closes)
        else:
            ma.append("[ObjectFPS/PvP Match] network statistics window_s=10.000")
        for bucket in [0] + list(players):
            count = share((match_received or written).get(bucket, 0))
            values = {"received_inputs": count // 10, "accepted_inputs": count // 10, "received_commands": count,
                      "accepted_new_commands": count}
            if final:
                values.update(match_values.get(bucket, {}))
            ma.append(line_of("[ObjectFPS/PvP Match] ingress statistics ", evidence.MATCH_KEYS, values,
                              [f"player={bucket}", "window_ms=10000", flag]))
    return "\n".join(gw) + "\n", "\n".join(ma) + "\n"


class GoldenTests(unittest.TestCase):
    def test_key_lists_equal_the_golden_lines(self):
        for name, keys, head in (("gateway-player.txt", evidence.GATEWAY_PLAYER_KEYS, 4),
                                 ("gateway-global.txt", evidence.GATEWAY_GLOBAL_KEYS, 3),
                                 ("match.txt", evidence.MATCH_KEYS, 4)):
            for line in golden_lines(name):
                fields = line.split(" ingress statistics ")[1].split(" ")
                self.assertEqual(tuple(field.split("=")[0] for field in fields[head:]), keys, name)

    def test_every_golden_line_satisfies_its_conservations(self):
        """README: each golden line alone satisfies IG, I0, T3, I2, J5a, S1, S2 (and the Gateway-wide one)."""
        for index in range(3):
            glob = golden_lines("gateway-global.txt")[index]
            player = golden_lines("gateway-player.txt")[index]
            match = golden_lines("match.txt")[index]
            gw = evidence.read_gateway("\n".join([GATEWAY_START, glob, player]))
            ma = evidence.read_match("\n".join([MATCH_START, zero_bucket(match), match]))
            g = evidence.gateway_checks(dict(gw, final=True, gaps=[]))
            m = evidence.match_checks(dict(ma, final=True, gaps=[]), injection=False)
            for name in ("IG", "gateway_global", "I0", "T3"):
                self.assertEqual(g[name]["status"], "passed", (index, name, g[name]))
            for name in ("I2", "J5a", "S1", "S2"):
                self.assertEqual(m[name]["status"], "passed", (index, name, m[name]))

    def test_golden_run_passes_every_exact_check(self):
        result = evidence.analyze_text(golden_gateway(), golden_match())
        # The golden written and received counts are synthetic and unrelated, so I1 (judged: the three
        # preconditions hold) fails and with it the run; every other exact check passes.
        self.assertEqual(result["status"], "failed")
        for name in ("IG", "gateway_global", "I0", "T3", "I2", "J5a", "S1", "S2"):
            self.assertEqual(result["checks"][name]["status"], "passed", name)
        totals = result["totals"]
        self.assertEqual(totals["gateway"]["windows"], 3)
        self.assertEqual(totals["gateway"]["players"]["7"]["received_datagrams"], 0 + 719 + 445)
        self.assertEqual(totals["gateway"]["global"]["received_datagrams"], 733 + 455)
        self.assertEqual(totals["match"]["players"]["7"]["received_commands"], 4445 + 2230)
        self.assertEqual(totals["match"]["windows"], 3)
        # J4 is reported only; the golden values are its sum.
        self.assertEqual(result["checks"]["J4"]["total"],
                         {"drop_rejected_lane_overflow_commands": 410 + 205, "drop_resolved_trim_commands": 408 + 204})
        # never_arrived (tentative J5a reading): removed + overflow + end.
        self.assertEqual(result["checks"]["J5a"]["never_arrived"]["total"], 93 + 95 + 96 + 44 + 46 + 47)
        self.assertEqual(result["checks"]["I1"]["status"], "failed")
        # I3: the golden routes conflicts, so it is not judged.
        self.assertEqual(result["checks"]["I3"]["status"], "undetermined")

    def test_golden_trace_parses(self):
        trace = evidence.read_trace((GOLDEN / "match-ingress.jsonl").read_text(encoding="utf-8"))
        self.assertEqual(trace["errors"], [])
        self.assertEqual((len(trace["rejections"]), len(trace["substitutions"])), (3, 2))
        self.assertEqual(trace["trace_end"]["records"], 5)


def corrupt(text, prefix, key, delta, occurrence=-1):
    """Add delta to key in one line starting with (or containing) prefix."""
    lines = text.splitlines()
    hits = [i for i, line in enumerate(lines) if prefix in line]
    index = hits[occurrence]
    lines[index] = with_value(lines[index], key, value_of(lines[index], key) + delta)
    return "\n".join(lines) + "\n"


class ConservationBreakTests(unittest.TestCase):
    """One changed value per conservation must turn exactly that conservation to failed."""

    def assert_breaks(self, result, name):
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["checks"][name]["status"], "failed", name)

    def test_ig_break(self):
        self.assert_breaks(evidence.analyze_text(
            corrupt(golden_gateway(), "player ingress statistics", "fwd_main_input_packets", 1), golden_match()), "IG")

    def test_gateway_global_break(self):
        result = evidence.analyze_text(
            corrupt(golden_gateway(), "gateway ingress statistics", "drop_unknown_session_datagrams", 1), golden_match())
        self.assert_breaks(result, "gateway_global")
        self.assertEqual(result["checks"]["IG"]["status"], "passed")

    def test_i0_break(self):
        self.assert_breaks(evidence.analyze_text(
            corrupt(golden_gateway(), "player ingress statistics", "drop_cleared_on_close_commands", 1),
            golden_match()), "I0")

    def test_t3_break(self):
        result = evidence.analyze_text(
            corrupt(golden_gateway(), "gateway ingress statistics", "drop_rate_limited_packets", 1), golden_match())
        self.assert_breaks(result, "T3")
        self.assertEqual(result["checks"]["gateway_global"]["status"], "passed")

    def test_i2_commands_break(self):
        for key in ("resolved_untracked_commands", "staged_over_window_commands"):
            result = evidence.analyze_text(golden_gateway(), corrupt(golden_match(), "player=7", key, 1))
            self.assert_breaks(result, "I2")

    def test_i2_inputs_and_actions_break(self):
        for key in ("accepted_inputs", "full_actions_batches", "over_batch_actions_shots"):
            self.assert_breaks(evidence.analyze_text(golden_gateway(), corrupt(golden_match(), "player=7", key, 1)),
                               "I2")

    def test_j5a_break(self):
        for key in ("unarrived_end_commands", "unarrived_aged_commands", "late_first_commands"):
            match = corrupt(golden_match(), "player=7", key, 1)
            if key == "late_first_commands":  # keep I2 whole: move one command from late_copy to late_first
                match = corrupt(match, "player=7", "late_copy_commands", -1)
            result = evidence.analyze_text(golden_gateway(), match)
            self.assert_breaks(result, "J5a")
            self.assertEqual(result["checks"]["I2"]["status"], "passed", key)

    def test_s1_break(self):
        self.assert_breaks(evidence.analyze_text(
            golden_gateway(), corrupt(golden_match(), "player=7", "slack_discarded_samples", 1)), "S1")

    def test_s2_break(self):
        result = evidence.analyze_text(golden_gateway(),
                                       corrupt(golden_match(), "player=7", "slack_unclaimed_samples", 1))
        self.assert_breaks(result, "S2")
        self.assertEqual(result["checks"]["S1"]["status"], "passed")

    def test_player_zero_bucket_is_judged(self):
        self.assert_breaks(evidence.analyze_text(
            golden_gateway(), corrupt(golden_match(), "player=0", "received_commands", 1, occurrence=0)), "I2")

    def test_attribute_keys_are_outside_the_conservations(self):
        match = golden_match()
        for key in ("late_only_inputs", "slack_published_negative_samples", "slack_coalesced_samples"):
            match = corrupt(match, "player=7", key, 5)
        gateway = golden_gateway()
        for key in ("fwd_resolved_at_write_commands", "fwd_routed_rotation_inputs"):
            gateway = corrupt(gateway, "player ingress statistics", key, 5)
        gateway = corrupt(gateway, "gateway ingress statistics", "drop_old_tick_snapshots", 5)
        result = evidence.analyze_text(gateway, match)
        for name in ("IG", "gateway_global", "I0", "T3", "I2", "J5a", "S1", "S2"):
            self.assertEqual(result["checks"][name]["status"], "passed", name)


class CompletenessTests(unittest.TestCase):
    def test_missing_final_is_undetermined_never_zero(self):
        gateway = "\n".join(line for line in golden_gateway().splitlines() if " final=1 " not in line) + "\n"
        result = evidence.analyze_text(gateway, golden_match())
        self.assertEqual(result["status"], "undetermined")
        for name in ("IG", "gateway_global", "I0", "T3"):
            self.assertEqual(result["checks"][name]["status"], "undetermined", name)
            self.assertIn("no final=1 window", " ".join(result["checks"][name]["reasons"]))
        self.assertEqual(result["checks"]["I2"]["status"], "passed")  # the Match log is whole
        self.assertEqual(result["checks"]["I1"]["status"], "undetermined")

    def test_missing_match_final_is_undetermined(self):
        match = "\n".join(line for line in golden_match().splitlines() if " final=1 " not in line) + "\n"
        result = evidence.analyze_text(golden_gateway(), match)
        self.assertEqual(result["status"], "undetermined")
        for name in ("I2", "J5a", "S1", "S2"):
            self.assertEqual(result["checks"][name]["status"], "undetermined", name)

    def test_missing_gateway_window_is_undetermined(self):
        lines = golden_gateway().splitlines()
        # Drop the second window's Gateway ingress line: its statistics line now has no window.
        index = [i for i, line in enumerate(lines) if "gateway ingress statistics" in line][1]
        del lines[index]
        result = evidence.analyze_text("\n".join(lines) + "\n", golden_match())
        self.assertEqual(result["checks"]["IG"]["status"], "undetermined")
        self.assertIn("without its ingress window", " ".join(result["checks"]["IG"]["reasons"]))

    def test_missing_player_window_is_undetermined(self):
        lines = golden_gateway().splitlines()
        index = [i for i, line in enumerate(lines) if "player ingress statistics" in line][1]
        del lines[index]
        result = evidence.analyze_text("\n".join(lines) + "\n", golden_match())
        self.assertEqual(result["checks"]["I0"]["status"], "undetermined")
        self.assertIn("no ingress line in its window", " ".join(result["checks"]["I0"]["reasons"]))

    def test_missing_match_window_is_undetermined(self):
        lines = golden_match().splitlines()
        index = [i for i, line in enumerate(lines) if "ingress statistics" in line and "player=0" in line][1]
        del lines[index]  # the window's player=0 line
        result = evidence.analyze_text(golden_gateway(), "\n".join(lines) + "\n")
        self.assertEqual(result["checks"]["I2"]["status"], "undetermined")
        lines = golden_match().splitlines()
        first = [i for i, line in enumerate(lines) if "ingress statistics" in line][0:2]
        del lines[first[0]:first[1] + 1]  # a whole window: its network statistics line is left alone
        result = evidence.analyze_text(golden_gateway(), "\n".join(lines) + "\n")
        self.assertEqual(result["checks"]["S1"]["status"], "undetermined")

    def test_lines_after_the_final_window_are_undetermined(self):
        lines = golden_gateway().splitlines()
        lines.append(lines[[i for i, line in enumerate(lines) if "gateway ingress statistics" in line][0]])
        result = evidence.analyze_text("\n".join(lines) + "\n", golden_match())
        self.assertEqual(result["checks"]["T3"]["status"], "undetermined")

    def test_two_processes_in_one_log_are_undetermined(self):
        result = evidence.analyze_text(golden_gateway() + golden_gateway(), golden_match())
        self.assertEqual(result["checks"]["IG"]["status"], "undetermined")

    def test_other_version_stops(self):
        for gateway, match in ((golden_gateway().replace("version=1 player=7", "version=2 player=7", 1),
                                golden_match()),
                               (golden_gateway(), golden_match().replace("ingress statistics version=1",
                                                                         "ingress statistics version=2", 1))):
            result = evidence.analyze_text(gateway, match)
            self.assertEqual(result["status"], "stopped")
            self.assertIn("this analyzer reads only version=1", " ".join(result["stops"]))

    def test_broken_lines_stop(self):
        lines = golden_gateway().splitlines()
        index = [i for i, line in enumerate(lines) if "player ingress statistics" in line][1]
        for broken in (lines[index].replace(" fwd_main_input_packets=80", ""),
                       lines[index].replace("fwd_main_input_packets=80", "fwd_main_input_packets=-1"),
                       lines[index] + " received_datagrams=1"):
            changed = list(lines)
            changed[index] = broken
            self.assertEqual(evidence.analyze_text("\n".join(changed) + "\n", golden_match())["status"], "stopped")

    def test_appended_keys_are_kept_apart(self):
        lines = golden_gateway().splitlines()
        lines = [line + " drop_new_reason_packets=3" if "player ingress statistics" in line else line
                 for line in lines]
        result = evidence.analyze_text("\n".join(lines) + "\n", golden_match())
        self.assertEqual(result["checks"]["IG"]["status"], "passed")
        self.assertEqual(result["totals"]["gateway"]["unknown_keys"], ["drop_new_reason_packets"])


class LinkTests(unittest.TestCase):
    WRITTEN = {1: 600, 2: 590}

    def test_i1_holds_on_a_consistent_run(self):
        gateway, match = run_logs([1, 2], windows=2, written=self.WRITTEN)
        result = evidence.analyze_text(gateway, match)
        self.assertEqual(result["status"], "passed", json.dumps(result["checks"], indent=1)[:3000])
        self.assertEqual(result["checks"]["I1"]["status"], "passed")
        self.assertEqual(result["checks"]["I1"]["link"]["lhs"], 1190)
        self.assertEqual(result["checks"]["I3"]["status"], "passed")

    def test_i1_counts_the_player_zero_bucket_in_the_link_total(self):
        gateway, match = run_logs([1, 2], written=self.WRITTEN, match_received={0: 10, 1: 590, 2: 590})
        i1 = evidence.analyze_text(gateway, match)["checks"]["I1"]
        self.assertEqual(i1["link"]["holds"], True)
        self.assertEqual(i1["status"], "failed")  # player 1 differs and was not evicted
        gateway = gateway.replace("runtime=127.0.0.1:27016",
                                  "runtime=127.0.0.1:27016\nplayer evicted player=1 reason=x reference_age_ms=1")
        i1 = evidence.analyze_text(gateway, match)["checks"]["I1"]
        self.assertEqual(i1["status"], "passed")
        self.assertEqual(i1["excluded_players"], [1])

    def test_i1_break(self):
        gateway, match = run_logs([1, 2], written=self.WRITTEN, match_received={1: 600, 2: 589},
                                  match_values={2: {"accepted_new_commands": 589}})
        result = evidence.analyze_text(gateway, match)
        self.assertEqual(result["checks"]["I1"]["status"], "failed")
        self.assertEqual(result["status"], "failed")

    def test_i1_without_eof_before_final_is_undetermined(self):
        gateway, match = run_logs([1, 2], written=self.WRITTEN, eof=False)
        i1 = evidence.analyze_text(gateway, match)["checks"]["I1"]
        self.assertEqual(i1["status"], "undetermined")
        self.assertFalse(i1["preconditions"]["eof_before_final"])
        # The EOF after the final window does not count either.
        match = match.rstrip("\n") + "\n" + MATCH_EOF + "\n"
        self.assertEqual(evidence.analyze_text(gateway, match)["checks"]["I1"]["status"], "undetermined")
        # A close for another reason is not an EOF.
        gateway, match = run_logs([1, 2], written=self.WRITTEN, eof=False,
                                  extra_closes=["[ObjectFPS/PvP Match] ipc closed reason=read: reset"])
        self.assertEqual(evidence.analyze_text(gateway, match)["checks"]["I1"]["status"], "undetermined")

    def test_i1_with_a_second_connection_is_undetermined(self):
        gateway, match = run_logs([1, 2], written=self.WRITTEN,
                                  extra_closes=["[ObjectFPS/PvP Match] ipc closed reason=eof"])
        i1 = evidence.analyze_text(gateway, match)["checks"]["I1"]
        self.assertEqual(i1["status"], "undetermined")
        self.assertFalse(i1["preconditions"]["single_connection"])
        gateway, match = run_logs([1, 2], written=self.WRITTEN,
                                  extra_closes=["[ObjectFPS/PvP Match] join player=3 accepted"])
        self.assertFalse(evidence.analyze_text(gateway, match)["checks"]["I1"]["preconditions"]["single_connection"])

    def test_i1_through_an_ipc_pause_relay_is_undetermined(self):
        gateway, match = run_logs([1, 2], written=self.WRITTEN, runtime="127.0.0.1:40001")
        i1 = evidence.analyze_text(gateway, match)["checks"]["I1"]
        self.assertEqual(i1["status"], "undetermined")
        self.assertFalse(i1["preconditions"]["no_ipc_pause"])
        self.assertIn("relay", " ".join(i1["reasons"]))
        # The run itself still passes: I1 is conditional.
        self.assertEqual(evidence.analyze_text(gateway, match)["status"], "passed")

    def test_i3_is_conditional(self):
        copies = {1: {"accepted_new_commands": 590, "pending_copy_commands": 10}}
        gateway, match = run_logs([1, 2], written=self.WRITTEN, match_values=copies)
        self.assertEqual(evidence.analyze_text(gateway, match)["checks"]["I3"]["status"], "failed")
        gateway, match = run_logs(
            [1, 2], written=self.WRITTEN, match_values=copies,
            gateway_values={1: {"fwd_handed_main_commands": 590, "fwd_link_written_main_commands": 590,
                                "fwd_handed_rejected_lane_commands": 10, "fwd_link_written_rejected_lane_commands": 10}})
        self.assertEqual(evidence.analyze_text(gateway, match)["checks"]["I3"]["status"], "passed")
        gateway, match = run_logs([1, 2], written=self.WRITTEN, match_values=copies,
                                  gateway_values={2: {"fwd_routed_conflict_inputs": 1}})
        i3 = evidence.analyze_text(gateway, match)["checks"]["I3"]
        self.assertEqual(i3["status"], "undetermined")
        self.assertEqual(i3["reasons"], ["fwd_routed_conflict_inputs=1"])

    def test_injection_requires_never_arrived_zero(self):
        values = {1: {"substituted_commands": 3, "unarrived_end_commands": 1, "unarrived_reset_commands": 2}}
        gateway, match = run_logs([1, 2], written=self.WRITTEN, match_values=values)
        self.assertEqual(evidence.analyze_text(gateway, match)["checks"]["J5a"]["status"], "passed")
        j5a = evidence.analyze_text(gateway, match, injection=True)["checks"]["J5a"]
        self.assertEqual(j5a["status"], "failed")
        self.assertEqual(j5a["never_arrived"]["total"], 1)
        for close in ("removed", "overflow"):
            values = {1: {"substituted_commands": 1, f"unarrived_{close}_commands": 1}}
            gateway, match = run_logs([1, 2], written=self.WRITTEN, match_values=values)
            self.assertEqual(evidence.analyze_text(gateway, match, injection=True)["checks"]["J5a"]["status"],
                             "failed", close)
        values = {1: {"substituted_commands": 4, "unarrived_aged_commands": 1, "unarrived_epoch_commands": 1,
                      "unarrived_life_commands": 1, "unarrived_reset_commands": 1}}
        gateway, match = run_logs([1, 2], written=self.WRITTEN, match_values=values)
        self.assertEqual(evidence.analyze_text(gateway, match, injection=True)["checks"]["J5a"]["status"], "passed")


def trace_line(kind, **fields):
    return json.dumps(dict({"schema": evidence.TRACE_SCHEMA, "version": 1, "kind": kind}, **fields))


def rejection(player=1, reason="epoch_old", commands=2):
    return trace_line("rejection", time_ns=1, player_id=player, reason=reason, epoch=1, life=1,
                      first_sequence=5 if commands else None, last_sequence=4 + commands if commands else None,
                      commands=commands, cursor=4, current_epoch=2, current_life=1)


def substitution(player=1, arrived=True, close="reset", copies=0):
    return trace_line("substitution", player_id=player, epoch=1, life=1, sequence=9, substituted_ns=1_000_000,
                      substituted_tick=3, first_arrival_ns=1_041_999 if arrived else None,
                      late_us=41 if arrived else None, copies_after_first=copies if arrived else 0,
                      reference_age_us=5 if arrived else None, close=close)


def trace_text(records, suppressed=0, dropped=0, end=True, end_records=None):
    lines = list(records)
    if end:
        lines.append(trace_line("trace_end", records=len(records) if end_records is None else end_records,
                                suppressed=suppressed, dropped=dropped))
    return "\n".join(lines) + "\n"


class TraceTests(unittest.TestCase):
    MATCH_VALUES = {1: {"accepted_inputs": 58, "accepted_new_commands": 591, "epoch_old_inputs": 1,
                        "epoch_old_commands": 2, "conflict_queued_inputs": 1, "conflict_queued_commands": 4,
                        "pending_copy_commands": 0, "late_first_commands": 1, "late_copy_commands": 2,
                        "substituted_commands": 2, "unarrived_reset_commands": 1}}
    RECORDS = [rejection(), rejection(reason="conflict_queued", commands=4), substitution(copies=2),
               substitution(arrived=False)]

    def analyze(self, trace):
        # Two late copies at the Match: the Gateway forwarded two commands again after their retention (I3).
        gateway, match = run_logs([1, 2], written={1: 600, 2: 590}, match_values=self.MATCH_VALUES,
                                  gateway_values={1: {"fwd_retention_expired_commands": 2}})
        return evidence.analyze_text(gateway, match, trace)

    def test_a_reconciled_trace_passes(self):
        result = self.analyze(trace_text(self.RECORDS))
        self.assertEqual(result["checks"]["trace"]["status"], "passed", json.dumps(result["checks"]["trace"], indent=1))
        self.assertEqual(result["status"], "passed")
        self.assertIn("rejected_conflict_queued_commands", result["checks"]["trace"]["reconciliation"])

    def test_missing_trace_end_is_undetermined(self):
        result = self.analyze(trace_text(self.RECORDS, end=False))
        self.assertEqual(result["checks"]["trace"]["status"], "undetermined")
        self.assertEqual(result["status"], "undetermined")

    def test_trace_end_must_be_last_and_count_the_records(self):
        text = trace_text(self.RECORDS) + rejection() + "\n"
        self.assertEqual(self.analyze(text)["checks"]["trace"]["status"], "failed")
        self.assertIn("after trace_end", " ".join(self.analyze(text)["checks"]["trace"].get("errors", [])))
        self.assertEqual(self.analyze(trace_text(self.RECORDS, end_records=3))["checks"]["trace"]["status"], "failed")

    def test_other_trace_version_or_kind_stops(self):
        for line in (trace_line("rejection").replace('"version": 1', '"version": 2'),
                     trace_line("slack_publication"), trace_line("rejection").replace(evidence.TRACE_SCHEMA, "x")):
            self.assertEqual(self.analyze(trace_text(self.RECORDS + [line]))["status"], "stopped", line)

    def test_invalid_fields_fail(self):
        for record in (rejection().replace('"reason": "epoch_old"', '"reason": "nope"'),
                       rejection().replace('"commands": 2', '"commands": true'),
                       substitution().replace('"late_us": 41', '"late_us": 42'),
                       substitution(arrived=False).replace('"close": "reset"', '"close": "later"')):
            self.assertEqual(self.analyze(trace_text(self.RECORDS[:3] + [record]))["checks"]["trace"]["status"],
                             "failed", record)

    def test_reconciliation_breaks(self):
        # A substitution record too many, a close reason changed, and a copy too many.
        for records in (self.RECORDS + [substitution(arrived=False)],
                        self.RECORDS[:3] + [substitution(arrived=False, close="aged")],
                        self.RECORDS[:2] + [substitution(copies=3)] + self.RECORDS[3:],
                        self.RECORDS[1:]):
            result = self.analyze(trace_text(records))
            self.assertEqual(result["checks"]["trace"]["status"], "failed")

    def test_suppressed_and_dropped_limit_the_reconciliation(self):
        # One epoch_old rejection suppressed: totals reconcile, per reason does not apply.
        result = self.analyze(trace_text(self.RECORDS[1:], suppressed=1))
        trace = result["checks"]["trace"]
        self.assertEqual(trace["status"], "passed", json.dumps(trace, indent=1))
        self.assertNotIn("rejected_epoch_old_inputs", trace["reconciliation"])
        # One substitution dropped: only the all-events equation applies.
        trace = self.analyze(trace_text(self.RECORDS[:3], dropped=1))["checks"]["trace"]
        self.assertEqual(trace["status"], "passed", json.dumps(trace, indent=1))
        self.assertEqual(list(trace["reconciliation"]), ["all_events"])
        trace = self.analyze(trace_text(self.RECORDS[:3], dropped=2))["checks"]["trace"]
        self.assertEqual(trace["status"], "failed")


class CommandLineTests(unittest.TestCase):
    def test_directory_mode_writes_the_judgement(self):
        with tempfile.TemporaryDirectory(prefix="pvp-ingress-evidence-") as name:
            directory = Path(name)
            gateway, match = run_logs([1, 2], written={1: 600, 2: 590})
            (directory / "gateway.log").write_text(gateway, encoding="utf-8")
            (directory / "match.log").write_text(match, encoding="utf-8")
            script = Path(evidence.__file__)
            completed = subprocess.run([sys.executable, str(script), str(directory), "--output",
                                        str(directory / "ingress-evidence.json")],
                                       stdout=subprocess.PIPE, text=True, timeout=60)
            self.assertEqual(completed.returncode, 0, completed.stdout[-2000:])
            written = json.loads((directory / "ingress-evidence.json").read_text(encoding="utf-8"))
            self.assertEqual(written["analyzer_id"], evidence.ANALYZER_ID)
            self.assertEqual(written["checks"]["trace"]["status"], "absent")
            (directory / "match.log").write_text(match.replace(MATCH_EOF + "\n", ""), encoding="utf-8")
            (directory / "gateway.log").write_text(gateway.replace("fwd_main_input_packets=60", "fwd_main_input_packets=61"),
                                                   encoding="utf-8")
            completed = subprocess.run([sys.executable, str(script), str(directory)], stdout=subprocess.PIPE,
                                       text=True, timeout=60)
            self.assertEqual(completed.returncode, 1)


if __name__ == "__main__":
    unittest.main()
