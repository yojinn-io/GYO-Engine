"""Interleaved before/after comparison of the Waiter's wakes (os-subsystem os-wait-backend, D71).

Runs two copies of the wake record programs, A (before a change) and B (after it), in the order
A B A B ... on one machine, and judges B against A with the rule declared before any B result
was seen (os-subsystem PLAN open decision 13 (a), frozen by D71):

  for each mode (cli, window) and each measure of the Waiter (deadline, notify), over the pairs:
    median of B's P99 <= median of A's P99 * 1.25 + 0.05 ms
    median of B's P50 <= median of A's P50 + 0.02 ms
  B never wakes before a deadline (min_ms >= 0)
  A and B report the same Waiter backend

The medians are taken over A's runs and over B's runs separately (statistics.median, so the mean
of the two middle values for an even count). Only the Waiter is judged; condition_variable and
sleep_until are measured by the same programs and recorded as a sign of how busy the host was,
like the 5 s sleeper before each run. A run that exits non-zero or reports no samples makes the
comparison invalid. A failed or invalid comparison is reported, never re-run to replace it
(AGENTS.md section 14.1).

Usage (from the repository root):
  python3 tests/common/time/wake_compare.py --a <dir> --b <dir> --output <evidence directory>
      [--pairs 4] [--seconds 30] [--modes cli window] [--a-label TEXT] [--b-label TEXT]
Each of --a and --b is a directory holding gyo_time_wake_record (and gyo_time_wake_record_window
for the window mode), copied out of a build so later builds cannot replace them. The same
directory may be passed as both A and B to calibrate the rule (A/A). Exit status: 0 when B
passes, 1 when it fails, 2 on a usage error or an invalid comparison.
"""
import argparse
import datetime
import hashlib
import json
import platform
import statistics
import subprocess
import sys
from pathlib import Path

import wake_baseline

P99_FACTOR = 1.25
P99_MARGIN_MS = 0.05
P50_MARGIN_MS = 0.02
MEASURES = ("deadline", "notify")


def program(directory, mode):
    name = wake_baseline.PROGRAMS[mode]
    for suffix in ("", ".exe"):
        path = directory / (name + suffix)
        if path.is_file():
            return path
    return None


def waiter_rows(result):
    return {row["measure"]: row for row in result["primitives"] if row["primitive"] == "waiter"}


def run_once(path, seconds, output):
    sleeper = wake_baseline.sleeper(5.0)
    completed = subprocess.run([str(path), "--seconds", str(seconds), "--output", str(output)],
                               capture_output=True, text=True, timeout=seconds * 8 + 120)
    log = completed.stdout + completed.stderr
    if completed.returncode != 0 or not output.is_file():
        return None, sleeper, f"exited {completed.returncode}: {log.strip()}"
    result = json.loads(output.read_text(encoding="utf-8"))
    rows = waiter_rows(result)
    if any(rows.get(measure, {}).get("samples", 0) == 0 for measure in MEASURES):
        return None, sleeper, "no Waiter samples"
    return result, sleeper, None


def judge(runs, modes):
    checks = []
    for mode in modes:
        for measure in MEASURES:
            side = {label: [waiter_rows(result)[measure] for result in runs[label][mode]] for label in ("A", "B")}
            a99 = statistics.median(row["p99_ms"] for row in side["A"])
            b99 = statistics.median(row["p99_ms"] for row in side["B"])
            a50 = statistics.median(row["p50_ms"] for row in side["A"])
            b50 = statistics.median(row["p50_ms"] for row in side["B"])
            limit99 = a99 * P99_FACTOR + P99_MARGIN_MS
            limit50 = a50 + P50_MARGIN_MS
            checks.append({"check": f"{mode} {measure} p99", "a_median_ms": a99, "b_median_ms": b99,
                           "limit_ms": limit99, "pass": b99 <= limit99})
            checks.append({"check": f"{mode} {measure} p50", "a_median_ms": a50, "b_median_ms": b50,
                           "limit_ms": limit50, "pass": b50 <= limit50})
        earliest = min(waiter_rows(result)["deadline"]["min_ms"] for result in runs["B"][mode])
        checks.append({"check": f"{mode} deadline never early", "b_min_ms": earliest, "pass": earliest >= 0})
        backends = sorted({result["waiter_backend"] for label in ("A", "B") for result in runs[label][mode]})
        checks.append({"check": f"{mode} same backend", "backends": backends, "pass": len(backends) == 1})
    return checks


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--a", type=Path, required=True)
    parser.add_argument("--b", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, default=4)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--modes", nargs="+", choices=sorted(wake_baseline.PROGRAMS), default=["cli", "window"])
    parser.add_argument("--a-label", default="")
    parser.add_argument("--b-label", default="")
    args = parser.parse_args(argv)
    if args.pairs < 1:
        print("wake compare: --pairs must be at least 1", file=sys.stderr)
        return 2
    sides = {"A": args.a.resolve(), "B": args.b.resolve()}
    programs = {(label, mode): program(directory, mode) for label, directory in sides.items() for mode in args.modes}
    missing = [f"{label} {wake_baseline.PROGRAMS[mode]}" for (label, mode), path in programs.items() if path is None]
    if missing:
        print(f"wake compare: missing programs: {', '.join(missing)}", file=sys.stderr)
        return 2
    args.output.mkdir(parents=True, exist_ok=True)
    meta = [
        f"recorded {datetime.datetime.now().isoformat(timespec='seconds')}",
        f"script source commit {wake_baseline.source_commit()}",
        f"os {platform.platform()} ({platform.machine()})",
        f"cpu {wake_baseline.cpu_name()}",
        f"load average at start {wake_baseline.load_average()}",
        f"pairs {args.pairs}, seconds per primitive {args.seconds}, modes {' '.join(args.modes)}, order A then B in each pair",
        f"rule p99 <= A*{P99_FACTOR}+{P99_MARGIN_MS} ms, p50 <= A+{P50_MARGIN_MS} ms (medians over the runs of each side)",
        f"A {sides['A']} {args.a_label}".rstrip(),
        f"B {sides['B']} {args.b_label}".rstrip(),
    ]
    meta += [f"{label} {digest(path)}  {path.name}" for (label, mode), path in sorted(programs.items())]
    (args.output / "meta.txt").write_text("\n".join(meta) + "\n", encoding="utf-8")

    runs = {label: {mode: [] for mode in args.modes} for label in sides}
    for pair in range(1, args.pairs + 1):
        for label in ("A", "B"):
            for mode in args.modes:
                stem = f"pair{pair}-{label}-{mode}"
                load = wake_baseline.load_average()
                result, sleeper, error = run_once(programs[(label, mode)], args.seconds, args.output / f"{stem}.json")
                sleeper["load_average"] = load
                (args.output / f"{stem}-sleeper.json").write_text(json.dumps(sleeper) + "\n", encoding="utf-8")
                if error:
                    print(f"wake compare: {stem} invalid: {error}", file=sys.stderr)
                    (args.output / "verdict.json").write_text(
                        json.dumps({"verdict": "invalid", "run": stem, "error": error}, indent=1) + "\n",
                        encoding="utf-8")
                    return 2
                runs[label][mode].append(result)
                print(f"wake compare: {stem} done", flush=True)

    checks = judge(runs, args.modes)
    passed = all(check["pass"] for check in checks)
    with (args.output / "meta.txt").open("a", encoding="utf-8") as meta_file:
        meta_file.write(f"load average at end {wake_baseline.load_average()}\n")
    (args.output / "verdict.json").write_text(
        json.dumps({"verdict": "pass" if passed else "fail", "checks": checks}, indent=1) + "\n", encoding="utf-8")
    for check in checks:
        values = {key: (round(value, 4) if isinstance(value, float) else value)
                  for key, value in check.items() if key not in ("check", "pass")}
        print(f"{'pass' if check['pass'] else 'FAIL'}  {check['check']}  {values}")
    print(f"wake compare: {'B passes' if passed else 'B fails'}; recorded in {args.output}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
