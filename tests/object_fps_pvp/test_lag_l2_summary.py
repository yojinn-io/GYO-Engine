"""Discrimination check of lag_l2_summary.py (v8-product-hitbox-lag-l2-declaration.md, T1).

A synthetic session that follows the declared procedure must give the hand-computed
R; each tampered copy must fail, be undetermined, or be refused as declared.
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
import lag_l2_summary  # noqa: E402

TZ = dt.timezone(dt.timedelta(hours=9))
T0 = dt.datetime(2026, 11, 5, 20, 0, 0, tzinfo=TZ)
Q16 = 65536
JOIN_2 = 20      # the last join, seconds after T0: S1 starts here
S1_END = 720     # s1_end: 700 s of S1


def mono(seconds):
    return int((seconds + 100000) * 1e9)


def cpp_line(seconds, text):
    wall = T0 + dt.timedelta(seconds=seconds)
    stamp = wall.strftime("%Y-%m-%dT%H:%M:%S.") + f"{wall.microsecond // 1000:03d}" + wall.strftime("%z")
    return f"{stamp} mono_ns={mono(seconds)} {text}"


def decision(player, action, seconds, lag_ticks, hold=0, kind="shot"):
    lag = None if lag_ticks is None else int(round(lag_ticks * Q16))
    return {"version": 3, "kind": "decision", "time_ns": mono(seconds), "player_id": player, "action_id": action,
            "action_kind": kind, "rejection": "none", "cooldown_hold_ticks": hold if kind == "shot" else None,
            "view_lag_q16": lag, "view_lag_a_q16": None if lag is None else lag - hold * Q16}


def baseline():
    """600 valid shots in S1: 594 at 3 ticks and 6 at 7.5 ticks, so the nearest-rank p99
    (the 594th) is 3 and R = 3 + 2 = 5; an interpolated p99 would be above 3 and give 6.
    Also: 50 shots with no view tick, one reload, 10 shots after s1_end at 14 ticks, and
    four held shots (hold 2)."""
    records = []
    action = {1: 0, 2: 0}

    def add(player, seconds, lag, hold=0, kind="shot"):
        action[player] += 1
        records.append(decision(player, action[player], seconds, lag, hold, kind))

    for i in range(600):
        player = 1 + i % 2
        lag = 7.5 if i in (5, 105, 205, 305, 405, 505) else 3.0
        add(player, JOIN_2 + 1 + i, lag, hold=2 if i in (11, 12, 13, 14) else 0)
    for i in range(50):
        add(1 + i % 2, JOIN_2 + 1 + i, None)
    add(1, JOIN_2 + 2, None, kind="reload")
    for i in range(10):
        add(1 + i % 2, S1_END + 5 + i, 14.0)
    records.sort(key=lambda r: r["time_ns"])
    return records, action


class Session:
    def __init__(self, root):
        self.root = root
        records, actions = baseline()
        self.records = records
        self.end = {"version": 3, "kind": "trace_end", "records": len(records), "dropped": 0, "view_tick_invalid": 0}
        self.actions = actions
        self.s1_end = S1_END
        self.client_extra = 0

    def write(self):
        mac = self.root / "mac"
        mac.mkdir(parents=True, exist_ok=True)
        (self.root / "marks.txt").write_text(
            "s1_end=" + (T0 + dt.timedelta(seconds=self.s1_end)).strftime("%Y-%m-%dT%H:%M:%S") + "\n", encoding="utf-8")
        match = [cpp_line(10, "[ObjectFPS/PvP Match] join player=1 accepted"),
                 cpp_line(JOIN_2, "[ObjectFPS/PvP Match] join player=2 accepted")]
        match += [cpp_line(s, "[ObjectFPS/PvP Match] statistics window") for s in range(30, 900, 30)]
        (mac / "match.log").write_text("\n".join(match) + "\n", encoding="utf-8")
        gateway = [(T0 + dt.timedelta(seconds=900)).strftime("%Y/%m/%d %H:%M:%S.%f ") + f"player left player={p}"
                   for p in (1, 2)]
        (mac / "gateway.log").write_text("\n".join(gateway) + "\n", encoding="utf-8")
        lines = [json.dumps(r) for r in self.records] + [json.dumps(self.end)]
        (mac / "match-shots.jsonl").write_text("\n".join(lines) + "\n", encoding="utf-8")
        for player, name in ((1, "win1"), (2, "win2")):
            folder = self.root / "clients" / name
            folder.mkdir(parents=True, exist_ok=True)
            n = sum(1 for r in self.records if r.get("player_id") == player) + (self.client_extra if player == 1 else 0)
            (folder / "client.log").write_text(cpp_line(
                890, f"[info] PvP summary player={player} tick=1 players=2 actions_submitted={n} accepted={n} "
                     "rejected=0 actions_pending=0") + "\n", encoding="utf-8")


class LagL2SummaryTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.session = Session(self.root)

    def tearDown(self):
        shutil.rmtree(self.root)

    def run_tool(self):
        self.session.write()
        return lag_l2_summary.main([str(self.root), "--output", str(self.root / "out.json")])

    def report(self):
        return json.loads((self.root / "out.json").read_text(encoding="utf-8"))

    def test_nearest_rank_by_hand(self):
        values = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
        self.assertEqual(lag_l2_summary.nearest_rank(values, 0.5), 5)
        self.assertEqual(lag_l2_summary.nearest_rank(values, 0.95), 10)
        self.assertEqual(lag_l2_summary.nearest_rank(values, 0.91), 10)
        self.assertEqual(lag_l2_summary.nearest_rank(values, 0.9), 9)

    def test_bound_r(self):
        self.assertEqual(lag_l2_summary.bound_r(3.0), 5)
        self.assertEqual(lag_l2_summary.bound_r(3.01), 6)
        self.assertEqual(lag_l2_summary.bound_r(13.0), 15)
        self.assertEqual(lag_l2_summary.bound_r(20.0), 15)

    def test_baseline_gives_the_hand_computed_r(self):
        self.assertEqual(self.run_tool(), 0)
        checks = self.report()["checks"]
        self.assertEqual(checks["S1"]["status"], "passed")
        self.assertEqual(checks["S1"]["valid_shots"], 600)   # no null, no reload, nothing after s1_end
        self.assertEqual(checks["S1"]["per_player"], {"1": 300, "2": 300})
        self.assertEqual(checks["S2"]["status"], "passed")
        self.assertEqual(checks["R2"]["status"], "passed")
        self.assertEqual(checks["R1"]["status"], "passed")
        self.assertEqual(checks["R1"]["quantile_ticks"], 3.0)
        self.assertEqual(checks["R1"]["r"], 5)
        distribution = self.report()["distribution"]
        self.assertEqual(distribution["max_ticks"], 7.5)
        self.assertEqual(distribution["clamped_fraction_by_r"]["8"], 0.0)

    def test_records_after_s1_end_are_not_counted(self):
        # Moving s1_end past them takes in the ten 14-tick shots: 610 shots, the
        # nearest-rank p99 is the 604th, one of the 14-tick shots (595th to 600th are
        # 7.5, 601st to 610th 14), so R = min(15, 14 + 2) = 15. At S1_END they are out (R 5).
        self.session.s1_end = S1_END + 20
        self.assertEqual(self.run_tool(), 0)
        self.assertEqual(self.report()["checks"]["S1"]["valid_shots"], 610)
        self.assertEqual(self.report()["checks"]["R1"]["r"], 15)

    def test_cap_at_fifteen(self):
        for r in self.session.records:
            if r["view_lag_q16"] is not None:
                r["view_lag_q16"] += 20 * Q16
                r["view_lag_a_q16"] += 20 * Q16
        self.assertEqual(self.run_tool(), 0)
        self.assertEqual(self.report()["checks"]["R1"]["r"], 15)

    def test_short_s1_fails(self):
        self.session.s1_end = JOIN_2 + 599
        self.assertEqual(self.run_tool(), 1)
        checks = self.report()["checks"]
        self.assertEqual(checks["S1"]["status"], "failed")
        self.assertEqual(checks["R1"]["status"], "undetermined")

    def test_one_player_is_not_enough(self):
        self.session.records = [r for r in self.session.records if r["player_id"] == 1 or r["view_lag_q16"] is None]
        self.session.end["records"] = len(self.session.records)
        self.assertEqual(self.run_tool(), 1)
        self.assertEqual(self.report()["checks"]["S1"]["status"], "failed")

    def test_dropped_fails_s2(self):
        self.session.end["dropped"] = 1
        self.assertEqual(self.run_tool(), 1)
        checks = self.report()["checks"]
        self.assertEqual(checks["S2"]["status"], "failed")
        self.assertEqual(checks["R1"]["status"], "undetermined")

    def test_client_match_mismatch_fails_s2(self):
        self.session.client_extra = 1   # a Client counts one accepted action the Match never decided
        self.assertEqual(self.run_tool(), 1)
        self.assertEqual(self.report()["checks"]["S2"]["status"], "failed")

    def test_hold_mismatch_fails_r2(self):
        held = next(r for r in self.session.records if r["cooldown_hold_ticks"] == 2)
        held["view_lag_a_q16"] = held["view_lag_q16"]
        self.assertEqual(self.run_tool(), 1)
        self.assertEqual(self.report()["checks"]["R2"]["status"], "failed")

    def test_version_two_is_refused(self):
        self.session.records[0]["version"] = 2
        self.assertEqual(self.run_tool(), 2)

    def test_negative_lag_is_refused(self):
        self.session.records[0]["view_lag_q16"] = -1
        self.assertEqual(self.run_tool(), 2)

    def test_two_trace_ends_are_refused(self):
        self.session.records.append(dict(self.session.end))
        self.assertEqual(self.run_tool(), 2)


if __name__ == "__main__":
    unittest.main()
