#!/usr/bin/env python3
"""lag-l2: the view_lag distribution of one LAN session and the rewind bound R
(docs/object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-declaration.md).

  lag_l2_summary.py SESSION_DIR [--output summary.json]

The session folder has the shape lan15_judge.py reads (its Session class reads it):

  marks.txt                   s1_end=<the Mac's local time, ISO, to the second>
  mac/match.log               the Match's log (wall clock and mono_ns per line)
  mac/match*-shots.jsonl      the shadow records, version 3 only
  mac/gateway.log             (for the bookkeeping: who left normally)
  clients/<name>/*.log        every Client process log of one machine

S1 runs from the Match's last "join player=N accepted" line before s1_end to
s1_end. A record's time_ns and the log's mono_ns are the same clock (the
Match process's steady_clock), so s1_end is moved onto it through the Match
log line nearest to it in wall time.

Checks: S1 (enough data), S2 (bookkeeping), R1 (R by the declared rule), R2
(the J and A lags differ by the cooldown hold). Each is "passed", "failed" or
"undetermined"; R is only given when S1 and S2 passed. A malformed trace
(another version, a negative lag, not exactly one trace_end) is an error:
exit 2 with no summary. Exit 0 when every check passed, 1 otherwise.
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lan15_judge  # noqa: E402

SUMMARY_ID = "pvp-v8-lag-l2-1"
TRACE_VERSION = 3
Q16 = 65536
TICK_MS = 1000.0 / 60  # AuthorityTickRate (Movement.hpp)
MAXIMUM_R = 15         # protocol-v9: maximum_reference_age_ms 250 ms at 60 Hz
R_MARGIN = 2           # ticks above the declared quantile
R_QUANTILE = 0.99      # -r-quantile (a)
S1_MIN_SECONDS = 600   # at least 10 minutes of play
MIN_SHOTS = 500        # valid shots in S1
MIN_PLAYERS = 2        # players with at least MIN_SHOTS_PER_PLAYER each
MIN_SHOTS_PER_PLAYER = 100
REPORTED_QUANTILES = (0.50, 0.90, 0.95, 0.99)


class TraceError(Exception):
    pass


def nearest_rank(sorted_values, q):
    """The nearest-rank quantile: the ceil(q*n)-th smallest value, no interpolation."""
    if not sorted_values:
        raise ValueError("no values")
    rank = max(1, math.ceil(q * len(sorted_values)))
    return sorted_values[rank - 1]


def bound_r(p_ticks):
    return min(MAXIMUM_R, math.ceil(p_ticks) + R_MARGIN)


def read_trace(path):
    """The decisions and the one trace_end of a version 3 shadow record file."""
    decisions, ends = [], []
    for number, text in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not text.strip():
            continue
        record = json.loads(text)
        if record.get("version") != TRACE_VERSION:
            raise TraceError(f"{path.name}:{number}: version {record.get('version')}, not {TRACE_VERSION}")
        if record.get("kind") == "trace_end":
            ends.append(record)
        elif record.get("kind") == "decision":
            for key in ("view_lag_q16", "view_lag_a_q16"):
                if record.get(key) is not None and record[key] < 0:
                    raise TraceError(f"{path.name}:{number}: negative {key} {record[key]}")
            decisions.append(record)
    if len(ends) != 1:
        raise TraceError(f"{path.name}: {len(ends)} trace_end lines, not 1")
    return decisions, ends[0]


def mono_at(match_lines, wall):
    """The Match log's mono_ns at a wall time, through the nearest line in wall time."""
    timed = [l for l in match_lines if l.wall is not None and l.mono is not None]
    if not timed:
        return None
    nearest = min(timed, key=lambda l: abs((l.wall - wall).total_seconds()))
    return nearest.mono + int((wall - nearest.wall).total_seconds() * 1e9)


def s1_window(s):
    if s.marks is None or "s1_end" not in s.marks or s.match is None:
        return None, "marks.txt s1_end or mac/match.log missing"
    end_wall = s.marks["s1_end"]
    joins = [l for l in s.match_lines(r"\bjoin player=\d+ accepted") if l.wall is not None and l.wall <= end_wall]
    if not joins:
        return None, "no join accepted before s1_end"
    start = joins[-1].mono
    end = mono_at(s.match, end_wall)
    return (start, end), None


def summarize(root):
    s = lan15_judge.Session(root)
    if len(s.shots) != 1:
        raise TraceError(f"one mac/match*-shots.jsonl expected, found {len(s.shots)}")
    decisions, end = read_trace(s.shots[0])
    checks = {}

    window, why = s1_window(s)
    if window is None:
        checks["S1"] = lan15_judge.result("undetermined", reason=why)
        shots = []
    else:
        start, stop = window
        shots = [d for d in decisions if d.get("action_kind") == "shot" and d.get("view_lag_q16") is not None
                 and start <= d["time_ns"] <= stop]
        per_player = {}
        for d in shots:
            per_player[d["player_id"]] = per_player.get(d["player_id"], 0) + 1
        seconds = (stop - start) / 1e9
        enough = [p for p, n in per_player.items() if n >= MIN_SHOTS_PER_PLAYER]
        checks["S1"] = lan15_judge.passed_if(
            seconds >= S1_MIN_SECONDS and len(shots) >= MIN_SHOTS and len(enough) >= MIN_PLAYERS,
            seconds=round(seconds, 3), valid_shots=len(shots), per_player=per_player)

    k2 = lan15_judge.check_k2(s)
    dropped = end.get("dropped")
    s2_status = "failed" if dropped != 0 or k2["status"] == "failed" else k2["status"]
    checks["S2"] = lan15_judge.result(s2_status, dropped=dropped, view_tick_invalid=end.get("view_tick_invalid"),
                                      records=end.get("records"), decisions=len(decisions), per_player_counts=k2)

    hold_mismatch = []
    for d in decisions:
        if d.get("view_lag_q16") is None or d.get("action_kind") != "shot":
            continue
        hold = d.get("cooldown_hold_ticks") or 0
        if d["view_lag_q16"] - d["view_lag_a_q16"] != hold * Q16:
            hold_mismatch.append({"player_id": d["player_id"], "action_id": d["action_id"]})
    checks["R2"] = lan15_judge.passed_if(not hold_mismatch, mismatches=hold_mismatch[:20],
                                         mismatch_count=len(hold_mismatch))

    distribution = None
    if shots:
        ticks = sorted(d["view_lag_q16"] / Q16 for d in shots)
        distribution = {
            "count": len(ticks),
            "quantiles_ticks": {f"p{round(q * 100)}": nearest_rank(ticks, q) for q in REPORTED_QUANTILES},
            "max_ticks": ticks[-1],
            "clamped_fraction_by_r": {str(r): sum(1 for t in ticks if t > r) / len(ticks) for r in range(8, MAXIMUM_R + 1)},
        }
        distribution["quantiles_ms"] = {k: v * TICK_MS for k, v in distribution["quantiles_ticks"].items()}
        distribution["max_ms"] = ticks[-1] * TICK_MS
    if checks["S1"]["status"] == "passed" and checks["S2"]["status"] == "passed":
        p = nearest_rank(sorted(d["view_lag_q16"] / Q16 for d in shots), R_QUANTILE)
        checks["R1"] = lan15_judge.result("passed", r=bound_r(p), quantile=R_QUANTILE, quantile_ticks=p,
                                          margin=R_MARGIN, maximum=MAXIMUM_R)
    else:
        checks["R1"] = lan15_judge.result("undetermined", reason="S1 and S2 must pass before R is given")
    return {"summary": SUMMARY_ID, "checks": checks, "distribution": distribution}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("session")
    parser.add_argument("--output")
    args = parser.parse_args(argv)
    try:
        report = summarize(Path(args.session))
    except (TraceError, json.JSONDecodeError, KeyError) as error:
        print(f"lag_l2_summary: {error}", file=sys.stderr)
        return 2
    text = json.dumps(report, indent=2, ensure_ascii=False)
    if args.output:
        Path(args.output).write_text(text + "\n", encoding="utf-8")
    print(text)
    return 0 if all(c["status"] == "passed" for c in report["checks"].values()) else 1


if __name__ == "__main__":
    sys.exit(main())
