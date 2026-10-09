"""Network path statistics (PvP v7 P2-log): read the 10-second statistics lines of the Gateway, Match and Client logs.

  python3 network_statistics.py [--gateway LOG] [--match LOG] [--client LOG ...] [--output JSON]

Prints (and optionally writes) one JSON document: every window of every source, and a summary per source. Lines are
key=value; "na" and "-" (no value in the window) become null, "p50/p90/p99/max" interval summaries become objects and
the tick lateness bins a list. A log without any statistics line is an error: the run did not last one window, or the
build predates P2-log.
"""
import argparse
import json
import re
import statistics
import sys
from pathlib import Path

GATEWAY = re.compile(r"player send statistics (?P<fields>.*)$")
MATCH = re.compile(r"\[ObjectFPS/PvP Match\] network statistics (?P<fields>.*)$")
WORKER = re.compile(r"\[ObjectFPS/PvP\] worker statistics (?P<fields>.*)$")
QUANTILES = ("p50", "p90", "p99", "max")


def value(text):
    if text in ("na", "-"):
        return None
    if "/" in text:
        return dict(zip(QUANTILES, map(float, text.split("/"))))
    if "," in text:
        return [int(part) for part in text.split(",")]
    try:
        return int(text)
    except ValueError:
        return float(text)


def windows(text, pattern):
    found = []
    for line in text.splitlines():
        match = pattern.search(line)
        if match:
            found.append({key: value(raw) for key, raw in
                          (field.split("=", 1) for field in match.group("fields").split())})
    return found


def per_second(rows, key, seconds):
    total = sum(seconds(row) for row in rows)
    return sum(row[key] or 0 for row in rows) / total if total else None


def summarize_gateway(rows):
    players = {}
    for player in sorted({row["player"] for row in rows}):
        own = [row for row in rows if row["player"] == player]
        seconds = lambda row: row["window_ms"] / 1000
        players[str(player)] = {
            "windows": len(own),
            **{f"{key}_per_s": per_second(own, key, seconds) for key in ("results", "snapshots", "link_actions")},
            **{f"{key}_p50_median": statistics.median(p50s) if (p50s := [row[key]["p50"] for row in own if row[key]])
               else None for key in ("results_interval_ms", "snapshot_interval_ms", "link_action_interval_ms")},
        }
    return players


def summarize_match(rows):
    seconds = lambda row: row["window_s"]
    return {"windows": len(rows),
            "ipc_iterations_per_s": per_second(rows, "ipc_iterations", seconds),
            "cpu_per_s": per_second(rows, "cpu_s", seconds) if all(row["cpu_s"] is not None for row in rows) else None,
            "tick_deadline_wakes": sum(row["tick_deadline_wakes"] for row in rows),
            "tick_notified": sum(row["tick_notified"] for row in rows),
            "tick_early": sum(row["tick_early"] for row in rows),
            "tick_late_bins": [sum(column) for column in zip(*(row["tick_late_bins"] for row in rows))],
            "tick_late_max_us": max(row["tick_late_max_us"] for row in rows)}


def summarize_worker(rows):
    """Per player: one probe process hosts several Clients, each with its own worker (player 0: outside a session)."""
    seconds = lambda row: row["window_s"]
    players = {}
    for player in sorted({row["player"] for row in rows}):
        own = [row for row in rows if row["player"] == player]
        players[str(player)] = {
            "windows": len(own), "wakes_per_s": per_second(own, "wakes", seconds),
            "cpu_per_s": per_second(own, "cpu_s", seconds) if all(row["cpu_s"] is not None for row in own) else None}
    return players


def collect(gateway=None, match=None, clients=()):
    result = {}
    sources = [("gateway", gateway, GATEWAY, summarize_gateway), ("match", match, MATCH, summarize_match)]
    sources += [(f"client:{path}", path, WORKER, summarize_worker) for path in clients]
    for name, path, pattern, summarize in sources:
        if path is None:
            continue
        rows = windows(Path(path).read_text(encoding="utf-8", errors="replace"), pattern)
        if not rows:
            raise ValueError(f"{path}: no statistics lines")
        result[name] = {"summary": summarize(rows), "windows": rows}
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--gateway")
    parser.add_argument("--match")
    parser.add_argument("--client", action="append", default=[])
    parser.add_argument("--output")
    args = parser.parse_args(argv)
    try:
        result = collect(args.gateway, args.match, args.client)
    except (OSError, ValueError) as error:
        print(f"network_statistics: {error}", file=sys.stderr)
        return 1
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        Path(args.output).write_text(text, encoding="utf-8")
    print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
