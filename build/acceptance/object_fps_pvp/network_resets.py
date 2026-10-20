"""Every non-respawn movement reset of a run_network.py run, classified (observability-tools OT-3).

  python3 -I -B network_resets.py DIR [DIR ...]

run_network.py's own D44 check (E3, network_main.cpp:282-297) compares the movement epoch before and after its six
second application stall only (INVENTORY N-d44-network-detector-narrow). This module reads the whole Match trace
(DIR/match-commands.jsonl, one Match for every probe stage) instead.

Expected, by the probe's design: one starvation reset per probe stage (the stop/resume step: the stopped simulated
player's queue runs dry about 0.54 s after it stops), the first reset of that player's life (epoch 2). Stages: the base
run, plus one per DIR/network-rtt-*.log (--network-impairments). Every other non-respawn reset is a candidate event:
another reason, a starvation at another epoch, a player reset twice, or more starvation resets than stages.
Tool problems (no reading): the trace is missing, does not end with one trace_end, or dropped events; fewer expected
resets than stages (the stop/resume step did not happen as designed).
"""
import json
import sys
from collections import Counter
from pathlib import Path

EXPECTED_REASON = "starvation"
EXPECTED_EPOCH = 2


def _rows(path):
    rows = []
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            if line.strip():
                rows.append(json.loads(line))
    return rows


def classify(directory):
    directory = Path(directory)
    stages = 1 + len(list(directory.glob("network-rtt-*.log")))
    out = {"stages": stages, "resets": [], "expected": [], "candidates": [], "tool_problems": []}
    path = directory / "match-commands.jsonl"
    if not path.is_file():
        out["tool_problems"].append("match-commands.jsonl missing")
        return out
    rows = _rows(path)
    ends = [r for r in rows if r.get("kind") == "trace_end"]
    if len(ends) != 1 or rows[-1] is not ends[0]:
        out["tool_problems"].append("match-commands.jsonl does not end with one trace_end")
    elif ends[0].get("dropped"):
        out["tool_problems"].append(f"match-commands.jsonl dropped {ends[0]['dropped']} event(s)")
    resets = [r for r in rows if r.get("kind") == "reset" and r.get("reset_reason") != "life_respawn"]
    seen = Counter()
    for r in resets:
        tag = {"time_ns": r.get("time_ns"), "player_id": r.get("player_id"), "epoch": r.get("epoch"),
               "reason": r.get("reset_reason"), "authority_tick": r.get("authority_tick")}
        out["resets"].append(tag)
        seen[r.get("player_id")] += 1
        if r.get("reset_reason") != EXPECTED_REASON:
            out["candidates"].append(dict(tag, why="reason other than starvation"))
        elif r.get("epoch") != EXPECTED_EPOCH:
            out["candidates"].append(dict(tag, why=f"starvation at epoch {r.get('epoch')}, not the life's first reset"))
        elif seen[r.get("player_id")] > 1:
            out["candidates"].append(dict(tag, why="the player's second reset"))
        else:
            out["expected"].append(tag)
    if len(out["expected"]) > stages:
        extra = out["expected"][stages:]
        out["expected"] = out["expected"][:stages]
        out["candidates"] += [dict(tag, why=f"more starvation resets than the {stages} stage(s)") for tag in extra]
    elif len(out["expected"]) < stages:
        out["tool_problems"].append(f"{len(out['expected'])} expected stop/resume reset(s) for {stages} stage(s)")
    return out


def main(argv):
    if not argv:
        print(__doc__.splitlines()[2].strip())
        return 2
    for directory in argv:
        found = classify(directory)
        print(json.dumps({"dir": directory, "stages": found["stages"], "expected": len(found["expected"]),
                          "candidates": found["candidates"], "tool_problems": found["tool_problems"]}))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
