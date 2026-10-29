"""Wake precision baseline (os-subsystem os-wake-record, D68; record only, never a pass condition).

Runs the declared procedure on one machine: for each mode (cli: gyo_time_wake_record, a process without
a window; window: gyo_time_wake_record_window, an Engine SDL window), a 5 s sleeper, the program with
--seconds per primitive, then another 5 s sleeper. The sleeper waits on 60 Hz absolute deadlines with
the interpreter's sleep and records how late it woke, which shows how busy the host was around each
run. meta.txt records the source commit, the programs' SHA-256, the OS, the CPU and the load.

Usage (from the repository root):
  python3 tests/common/time/wake_baseline.py --build-dir <build tree> --output <evidence directory>
      [--seconds 30] [--modes cli window]
Evidence goes to build/target/_build/test/logs/os-wake-baseline-<date>/ by convention (os-subsystem
PLAN section 7.13). Exit status: 0 when every mode ran, 1 when one could not run, 2 on a usage error.
"""
import argparse
import datetime
import hashlib
import json
import os
import platform
import subprocess
import sys
import time
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[3]
PROGRAMS = {"cli": "gyo_time_wake_record", "window": "gyo_time_wake_record_window"}
PERIOD = 1.0 / 60.0


def find_program(build_dir, name):
    for suffix in ("", ".exe"):
        for path in sorted(build_dir.rglob(name + suffix)):
            if path.is_file():
                return path
    return None


def sleeper(seconds):
    late = []
    start = time.perf_counter()
    deadline = start
    while time.perf_counter() - start < seconds:
        deadline += PERIOD
        remaining = deadline - time.perf_counter()
        if remaining > 0:
            time.sleep(remaining)
        late.append((time.perf_counter() - deadline) * 1000.0)
        if deadline + PERIOD < time.perf_counter():
            deadline = time.perf_counter()
    late.sort()
    pick = lambda q: late[min(len(late) - 1, int(q * (len(late) - 1)))]
    return {"samples": len(late), "p50_ms": round(pick(0.5), 3), "p99_ms": round(pick(0.99), 3),
            "max_ms": round(late[-1], 3)}


def cpu_name():
    try:
        if sys.platform == "darwin":
            return subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True,
                                  timeout=10).stdout.strip()
        if sys.platform.startswith("linux"):
            for line in Path("/proc/cpuinfo").read_text(encoding="utf-8", errors="replace").splitlines():
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except (OSError, subprocess.SubprocessError):
        pass
    return platform.processor() or "unknown"


def load_average():
    try:
        return " ".join(f"{value:.2f}" for value in os.getloadavg())
    except (AttributeError, OSError):
        return "unavailable on this platform"


def source_commit():
    try:
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPOSITORY, capture_output=True, text=True,
                              timeout=10).stdout.strip()
        dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=REPOSITORY,
                               capture_output=True, text=True, timeout=30).stdout.strip()
        return head + (" (with uncommitted changes)" if dirty else "")
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--modes", nargs="+", choices=sorted(PROGRAMS), default=["cli", "window"])
    args = parser.parse_args(argv)
    programs = {mode: find_program(args.build_dir.resolve(), PROGRAMS[mode]) for mode in args.modes}
    missing = [PROGRAMS[mode] for mode, path in programs.items() if path is None]
    if missing:
        print(f"wake baseline: not built under {args.build_dir}: {', '.join(missing)}", file=sys.stderr)
        return 2
    args.output.mkdir(parents=True, exist_ok=True)
    meta = [
        f"recorded {datetime.datetime.now().isoformat(timespec='seconds')}",
        f"source commit {source_commit()}",
        f"os {platform.platform()} ({platform.machine()})",
        f"cpu {cpu_name()}, {os.cpu_count()} logical processors",
        f"load average at start {load_average()}",
        f"seconds per primitive {args.seconds}, modes {' '.join(args.modes)}",
    ]
    for mode, path in programs.items():
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        meta.append(f"{digest}  {path.name}")
    (args.output / "meta.txt").write_text("\n".join(meta) + "\n", encoding="utf-8")
    failed = False
    for mode, path in programs.items():
        (args.output / f"sleeper-before-{mode}.json").write_text(json.dumps(sleeper(5.0)) + "\n", encoding="utf-8")
        result = subprocess.run([str(path), "--seconds", str(args.seconds), "--output", str(args.output / f"{mode}.json")],
                                capture_output=True, text=True, timeout=args.seconds * 8 + 120)
        (args.output / f"{mode}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode != 0:
            print(f"wake baseline: {mode} exited {result.returncode}; see {mode}.log", file=sys.stderr)
            failed = True
        (args.output / f"sleeper-after-{mode}.json").write_text(json.dumps(sleeper(5.0)) + "\n", encoding="utf-8")
    with (args.output / "meta.txt").open("a", encoding="utf-8") as meta_file:
        meta_file.write(f"load average at end {load_average()}\n")
    print(f"wake baseline: recorded in {args.output}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
