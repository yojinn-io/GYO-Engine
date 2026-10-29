"""Discrimination check of the lan15 judge (lan15-declaration.md section 7).

A synthetic session that follows the declared procedure must pass; each
tampered copy (one declared failure each) must fail, or be undetermined where
the declaration says evidence is incomplete. ingress_evidence's own equations
have their own tests; here its result is fixed, and only how J2 reads it is checked.
"""
import datetime as dt
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "build/acceptance/object_fps_pvp"))
import lan15_judge  # noqa: E402

TZ = dt.timezone(dt.timedelta(hours=9))
T0 = dt.datetime(2026, 10, 29, 20, 0, 0, tzinfo=TZ)
WIN_OFFSET = 5.0  # the Windows clock runs 5 s behind the Mac's


class Writer:
    def __init__(self):
        self.files = {}

    def cpp(self, path, seconds, text, behind=0.0):
        wall = T0 + dt.timedelta(seconds=seconds - behind)
        stamp = wall.strftime("%Y-%m-%dT%H:%M:%S.") + f"{wall.microsecond // 1000:03d}" + wall.strftime("%z")
        mono = int((seconds + 100000) * 1e9)
        self.files.setdefault(path, []).append((seconds, f"{stamp} mono_ns={mono} {text}"))

    def go(self, seconds, text):
        wall = T0 + dt.timedelta(seconds=seconds)
        self.files.setdefault("mac/gateway.log", []).append((seconds, wall.strftime("%Y/%m/%d %H:%M:%S.%f ") + text))

    def raw(self, path, line):
        self.files.setdefault(path, []).append((len(self.files.get(path, [])), line))

    def write(self, root):
        for path, entries in self.files.items():
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            ordered = [text for _, text in sorted(entries, key=lambda e: e[0])] if not path.endswith(".jsonl") and not path.endswith(".txt") and not path.endswith(".json") else [text for _, text in entries]
            target.write_text("\n".join(ordered) + "\n", encoding="utf-8")


def session():
    w = Writer()
    match = lambda t, text: w.cpp("mac/match.log", t, "[ObjectFPS/PvP Match] " + text)  # noqa: E731
    win1 = "clients/win1/client-20261029-195950.log"
    win2 = "clients/win1/client-20261029-201920.log"
    mac = "clients/mac/client-20261029-195955.log"
    cw = lambda path, t, text: w.cpp(path, t, text, behind=WIN_OFFSET)  # noqa: E731
    cm = lambda t, text: w.cpp(mac, t, text)  # noqa: E731
    match(-60, "start executable_sha256=aa arena_file=pvp_corners.json")
    w.go(-59, "Object FPS Gateway start executable_sha256=bb")
    cw(win1, -20, "[info] PvP client start executable_sha256=cc platform=Windows")
    cm(-15, "[info] PvP client start executable_sha256=cc platform=macOS")
    joins = {1: -10, 2: 0}
    for player, t in joins.items():
        match(t, f"join player={player} accepted")
        w.go(t, f"player active player={player}")
    cw(win1, -10, "[info] PvP connection phase=playing player=1 arena=pvp_corners_v1")
    cm(0, "[info] PvP connection phase=playing player=2 arena=pvp_corners_v1")
    for t in range(10, 1001, 10):
        for player in (1, 2):
            w.go(t, f"player send statistics player={player} window_ms=10000 results=30 results_min_interval_ms=33.4 link_action_min_interval_ms=31.0")
            w.go(t, f"player ingress statistics version=1 player={player} window_ms=10000 final=0 drop_rate_limited_hello_packets=0 drop_rate_limited_input_packets=0")
        cw(win1, t - 5, "[info] PvP summary player=1 tick=1 players=2 actions_submitted=1 accepted=1 rejected=0 actions_pending=0 rejected_cooldown=0")
        if t <= 900:
            cm(t - 5, f"[info] PvP summary player=2 tick=1 players=2 actions_submitted={1 if t <= 100 else 0} accepted={1 if t <= 80 else 0} rejected={1 if 80 < t <= 100 else 0} actions_pending=0 rejected_cooldown={1 if 80 < t <= 100 else 0}")
    # S2 on win1: 2 s at +1000, then 7 s at +1060 (Windows clock), rejoining as player 3.
    cw(win1, 1001, "[ObjectFPS/PvP] connection interrupted version=1 player=1 gap_ms=500 epoch=1")
    cw(win1, 1003, "[ObjectFPS/PvP] connection resumed version=1 player=1 gap_ms=2100 epoch_before=1 epoch_after=1")
    cw(win1, 1061, "[ObjectFPS/PvP] connection interrupted version=1 player=1 gap_ms=500 epoch=1")
    cw(win1, 1065.55, "[ObjectFPS/PvP] connection failed player=1 reason=Gateway connection timed out")
    w.go(1066, "player expired player=1 reason=session_timeout")
    match(1066, "leave player=1 reason=session_timeout")
    match(1080, "join player=3 accepted")
    w.go(1080, "player active player=3")
    cw(win1, 1080, "[info] PvP connection phase=playing player=3 arena=pvp_corners_v1")
    # Q1: the window of player 3 is closed; the Client comes back as player 4.
    w.go(1150, "player left player=3")
    match(1150, "leave player=3 reason=client_request")
    cw(win1, 1150, "[ObjectFPS/PvP] connection summary version=1 player=3 interruptions=2 max_gap_ms=7000 transient_socket_errors=0")
    cm(1155, "[info] PvP summary player=2 tick=1 players=1 actions_submitted=0 accepted=0 rejected=0 actions_pending=0 rejected_cooldown=0")
    cm(1175, "[info] PvP summary player=2 tick=1 players=2 actions_submitted=0 accepted=0 rejected=0 actions_pending=0 rejected_cooldown=0")
    cw(win2, 1160, "[info] PvP client start executable_sha256=cc platform=Windows")
    match(1170, "join player=4 accepted")
    w.go(1170, "player active player=4")
    cw(win2, 1170, "[info] PvP connection phase=playing player=4 arena=pvp_corners_v1")
    cw(win2, 1200, "[info] PvP summary player=4 tick=1 players=2 actions_submitted=3 accepted=3 rejected=0 actions_pending=0 rejected_cooldown=0")
    # Q2: two quad bots fill the room; the fifth join is refused.
    for player, t in ((5, 1250), (6, 1251)):
        match(t, f"join player={player} accepted")
        w.go(t, f"player active player={player}")
    w.go(1255, "join refused remote=192.168.1.5:5555 code=room_full")
    for player in (5, 6):
        w.go(1300, f"player left player={player}")
        match(1300, f"leave player={player} reason=client_request")
    w.raw("quad/out/result.json", json.dumps({"player_ids": [5, 6], "fifth": {"error": "room_full", "joined": False}}))
    # The end: players 4 and 2 leave with Esc.
    for path, player, t in ((win2, 4, 1400), (mac, 2, 1401)):
        (cw if path == win2 else lambda p, tt, text: cm(tt, text))(path, t, "[info] PvP connection phase=lobby player=0 arena=")
        w.go(t, f"player left player={player}")
        match(t, f"leave player={player} reason=client_request")
        (cw if path == win2 else lambda p, tt, text: cm(tt, text))(path, t, f"[ObjectFPS/PvP] connection summary version=1 player={player} interruptions=0 max_gap_ms=0 transient_socket_errors=0")
    for player in range(1, 7):
        match(1410, f"ingress statistics version=1 player={player} window_ms=1 final=1")
        match(1410, f"resync statistics version=1 player={player} final=1")
    match(1411, "end version=1 reason=signal exit_code=0 uptime_s=1471.0")
    w.raw("mac/match-ingress.jsonl", json.dumps({"schema": "object_fps_pvp.match_ingress", "version": 1, "kind": "trace_end", "records": 0, "suppressed": 0, "dropped": 0}))
    shots = [(2, "none")] * 8 + [(2, "cooldown")] * 2 + [(4, "none")] * 3 + [(1, "none")] * 2
    for player, rejection in shots:
        w.raw("mac/match-shots.jsonl", json.dumps({"version": 2, "kind": "decision", "player_id": player, "rejection": rejection}))
    w.raw("mac/match-shots.jsonl", json.dumps({"version": 2, "kind": "trace_end", "records": len(shots), "dropped": 0}))
    # Ping on win1 (its own clock): outages of 2.4 s and 7.4 s.
    for i in range(0, 600):
        t = 990 + i * 0.2
        ok = not (1000.2 <= t < 1002.6 or 1060.2 <= t < 1067.6)
        wall = (T0 + dt.timedelta(seconds=t - WIN_OFFSET)).isoformat()
        w.raw("clients/win1/ping.txt", f"{wall} {ok}")
    t2 = (T0 + dt.timedelta(seconds=1000 - WIN_OFFSET)).replace(tzinfo=None).isoformat()
    t7 = (T0 + dt.timedelta(seconds=1060 - WIN_OFFSET)).replace(tzinfo=None).isoformat()
    s1_end = (T0 + dt.timedelta(seconds=960)).replace(tzinfo=None).isoformat()
    q2s = (T0 + dt.timedelta(seconds=1245)).replace(tzinfo=None).isoformat()
    q2e = (T0 + dt.timedelta(seconds=1300)).replace(tzinfo=None).isoformat()
    for line in ("n=2", "n_w=1", f"s1_end={s1_end}", f"trial=win1,2s,{t2}", f"trial=win1,7s,{t7}", "q1=win1",
                 f"q2_start={q2s}", f"q2_end={q2e}"):
        w.raw("marks.txt", line)
    return w


class Lan15JudgeTest(unittest.TestCase):
    def setUp(self):
        self.directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.directory)
        self.original = lan15_judge.ingress_evidence.analyze_text
        self.addCleanup(setattr, lan15_judge.ingress_evidence, "analyze_text", self.original)
        self.ingress_status = "passed"
        lan15_judge.ingress_evidence.analyze_text = lambda *a, **k: {"status": self.ingress_status, "analyzer_id": "fixed"}

    def run_session(self, tamper=None):
        w = session()
        if tamper: tamper(w)
        root = self.directory / "s"
        if root.exists(): shutil.rmtree(root)
        w.write(root)
        return lan15_judge.judge(root)

    def replace(self, w, path, old, new):
        entries = w.files[path]
        hits = [i for i, (_, text) in enumerate(entries) if old in text]
        self.assertTrue(hits, old)
        key, text = entries[hits[0]]
        entries[hits[0]] = (key, text.replace(old, new))

    def assertCheck(self, report, name, status):
        value = report[name]
        statuses = [r["status"] for r in value] if isinstance(value, list) else [value["status"]]
        self.assertIn(status, statuses, json.dumps(value, default=str)[:600])
        self.assertFalse(report["passed"])

    def test_the_declared_procedure_passes(self):
        report = self.run_session()
        failing = {k: v for k, v in report.items() if isinstance(v, dict) and v.get("status") not in (None, "passed")}
        failing.update({f"R{i}": r for i, r in enumerate(report["R"]) if r["status"] != "passed"})
        self.assertEqual(failing, {})
        self.assertTrue(report["passed"])
        self.assertEqual(report["client_offsets_s"]["win1/client-20261029-195950.log"], WIN_OFFSET)

    # Section 2: S1
    def test_j0_short_s1(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "marks.txt", "s1_end=2026-10-29T20:16:00", "s1_end=2026-10-29T20:15:00")), "J0", "failed")

    def test_j0_wrong_player_count(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "clients/mac/client-20261029-195955.log", "players=2", "players=1")), "J0", "failed")

    def test_j3_interruption_in_s1(self):
        self.assertCheck(self.run_session(lambda w: w.cpp("clients/win1/client-20261029-195950.log", 400, "[ObjectFPS/PvP] connection interrupted version=1 player=1 gap_ms=500 epoch=1", behind=WIN_OFFSET)), "J3", "failed")

    def test_j3_eviction_in_s1(self):
        self.assertCheck(self.run_session(lambda w: w.go(400, "player evicted player=1 reason=quality reference_age_ms=200")), "J3", "failed")

    def test_j3_offset_from_one_pair_is_undetermined(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "mac/gateway.log", "player left player=2", "player gone player=2")), "J3", "undetermined")

    def test_j4_rate_limited(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "mac/gateway.log", "drop_rate_limited_input_packets=0", "drop_rate_limited_input_packets=1")), "J4", "failed")

    def test_j5_below_33_3(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "mac/gateway.log", "results_min_interval_ms=33.4", "results_min_interval_ms=33.2")), "J5", "failed")

    def test_j5_never_a_value(self):
        def tamper(w):
            w.files["mac/gateway.log"] = [(k, t.replace("results_min_interval_ms=33.4", "results_min_interval_ms=-")) for k, t in w.files["mac/gateway.log"]]
        self.assertCheck(self.run_session(tamper), "J5", "failed")

    # Section 3: the whole session
    def test_j1_shots_dropped(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "mac/match-shots.jsonl", '"dropped": 0', '"dropped": 1')), "J1", "failed")

    def test_j1_missing_final_resync(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "mac/match.log", "resync statistics version=1 player=2 final=1", "resync statistics version=1 player=2 final=0")), "J1", "failed")

    def test_j2_reads_the_analyzer(self):
        self.ingress_status = "failed"
        self.assertCheck(self.run_session(), "J2", "failed")
        self.ingress_status = "undetermined"
        self.assertCheck(self.run_session(), "J2", "undetermined")

    def test_k1_pending_at_the_end(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "clients/win1/client-20261029-201920.log", "actions_pending=0", "actions_pending=1")), "K1", "failed")

    def test_k2_one_decision_missing(self):
        def tamper(w):
            entries = w.files["mac/match-shots.jsonl"]
            index = next(i for i, (_, t) in enumerate(entries) if '"player_id": 4' in t)
            del entries[index]
        self.assertCheck(self.run_session(tamper), "K2", "failed")

    # Section 4: S2
    def test_r1_failed_instead_of_resumed(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "clients/win1/client-20261029-195950.log", "connection resumed version=1 player=1 gap_ms=2100", "connection failed player=1 reason=Gateway connection timed out x=")), "R", "failed")

    def test_r2_t_max_from_the_interruption(self):
        def tamper(w):
            path = "clients/win1/client-20261029-195950.log"
            w.files[path] = [(k, t) for k, t in w.files[path] if "reason=Gateway connection timed out" not in t]
            w.cpp(path, 1066.05, "[ObjectFPS/PvP] connection failed player=1 reason=Gateway connection timed out", behind=WIN_OFFSET)
        self.assertCheck(self.run_session(tamper), "R", "failed")

    def test_r2_gateway_never_expires(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "mac/gateway.log", "player expired player=1 reason=session_timeout", "player kept player=1")), "R", "failed")

    def test_trial_too_long_is_undetermined(self):
        def tamper(w):
            w.files["clients/win1/ping.txt"] = [(k, t.replace("True", "False")) if "T20:16:4" in t or "T20:16:5" in t else (k, t)
                                                for k, t in w.files["clients/win1/ping.txt"]]
        self.assertCheck(self.run_session(tamper), "R", "undetermined")

    # Section 5: S3
    def test_q1_old_player_never_left_the_match(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "mac/match.log", "leave player=3 reason=client_request", "stay player=3")), "Q1", "failed")

    def test_q1_other_player_never_saw_the_dip(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "clients/mac/client-20261029-195955.log", "players=1 actions_submitted=0", "players=2 actions_submitted=0")), "Q1", "failed")

    def test_q1_other_player_interrupted(self):
        self.assertCheck(self.run_session(lambda w: w.cpp("clients/mac/client-20261029-195955.log", 1160, "[ObjectFPS/PvP] connection interrupted version=1 player=2 gap_ms=500 epoch=1")), "Q1", "failed")

    def test_q2_room_player_interrupted(self):
        self.assertCheck(self.run_session(lambda w: w.cpp("clients/mac/client-20261029-195955.log", 1256, "[ObjectFPS/PvP] connection interrupted version=1 player=2 gap_ms=500 epoch=1")), "Q2", "failed")

    def test_q2_fifth_joined(self):
        self.assertCheck(self.run_session(lambda w: self.replace(w, "quad/out/result.json", '"error": "room_full", "joined": false', '"error": "", "joined": true')), "Q2", "failed")

    # Section 6 and 7
    def test_s7_model_error(self):
        self.assertCheck(self.run_session(lambda w: w.cpp("clients/mac/client-20261029-195955.log", 5, "[error] missing player animation run")), "S7", "failed")

    def test_bookkeeping_dropped_lines(self):
        self.assertCheck(self.run_session(lambda w: w.cpp("mac/match.log", 1412, "[log] dropped_lines=3")), "bookkeeping", "undetermined")

    def test_bookkeeping_second_start(self):
        self.assertCheck(self.run_session(lambda w: w.cpp("mac/match.log", -30, "[ObjectFPS/PvP Match] start executable_sha256=aa")), "bookkeeping", "undetermined")


if __name__ == "__main__":
    unittest.main()
