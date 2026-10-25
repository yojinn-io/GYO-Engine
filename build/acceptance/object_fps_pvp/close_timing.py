#!/usr/bin/env python3
"""v7.1 quick-close acceptance: the time from the request to close a playing Client connection to the process exit.

  close_timing.py --probe <gyo_object_fps_pvp_worker_probe> [--runs 10] [--output result.json]

The worker probe (--close MODE) joins its mock Gateway, prints "close requested" and destroys the product's
ClientConnection, whose network worker sends the closing /leave; this script times that line to the process exit.

Declared before the run and approved (docs/object_fps_pvp/plans/v7.1/quick-close.md, D202): every one of the runs
must stay within its limit, no retries; the numbers are not loosened after a run.
- responding (the Gateway answers the /leave): <= 500 ms
- down (the Gateway's HTTP listener is gone): <= 1000 ms
- stalled (the Gateway accepts the /leave and holds it): recorded only, the Red of the fix (2 s and more before it)
"""
import argparse
import json
import platform
import subprocess
import sys
import time
from pathlib import Path

LIMITS_MS = {"responding": 500, "down": 1000}
RECORDED = ("stalled",)


def run_once(probe: Path, mode: str) -> dict:
    process = subprocess.Popen([str(probe), "--close", mode], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding="utf-8", errors="replace")
    requested = None
    lines = []
    deadline = time.monotonic() + 30
    for line in process.stdout:
        lines.append(line.rstrip("\n"))
        if line.strip() == "close requested":
            requested = time.perf_counter()
            break
        if time.monotonic() > deadline:
            break
    rest, errors = process.communicate(timeout=30)
    exited = time.perf_counter()
    lines += rest.splitlines()
    report = next((json.loads(line) for line in lines if line.startswith("{")), {})
    return {"mode": mode, "exit_code": process.returncode,
            "close_to_exit_ms": None if requested is None else round((exited - requested) * 1000, 1),
            "probe_close_ms": report.get("close_ms"), "leaves": report.get("leaves"),
            "stderr": errors.strip().splitlines()[-1:] if errors.strip() else []}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    runs = [run_once(args.probe, mode) for mode in (*LIMITS_MS, *RECORDED) for _ in range(args.runs)]
    failures = [run for run in runs if run["mode"] in LIMITS_MS and
                (run["exit_code"] != 0 or run["close_to_exit_ms"] is None or run["close_to_exit_ms"] > LIMITS_MS[run["mode"]])]
    summary = {mode: [run["close_to_exit_ms"] for run in runs if run["mode"] == mode] for mode in (*LIMITS_MS, *RECORDED)}
    result = {"platform": platform.platform(), "runs_per_mode": args.runs, "limits_ms": LIMITS_MS,
              "close_to_exit_ms": summary, "max_ms": {mode: max((v for v in values if v is not None), default=None)
                                                      for mode, values in summary.items()},
              "passed": not failures, "failures": failures, "runs": runs}
    if args.output:
        args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({key: result[key] for key in ("platform", "max_ms", "passed")}))
    for failure in failures:
        print(f"FAILED {failure}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
