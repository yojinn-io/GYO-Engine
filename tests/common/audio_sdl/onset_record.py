"""Engine L2 of the audio plan (v8-engine-audio-record; record only, never a pass condition).

Runs gyo_audio_onset_record (a 30 FPS frame loop that plays an impulse for events on a 60 Hz tick
grid, each sent in the frame after it happened) in two modes:
  disk    SDL's disk driver writes what the device plays to a file (480-frame blocks, 10 ms exactly,
          real-time pacing); the onsets are found in that file and compared with the events' times.
  device  the default playback device; only the mixer's counters and the callback statistics.
SDL's driver and hints are set through the environment; nothing here calls SDL.

The disk analysis reports, for consecutive events:
  - the onset interval minus the timestamp interval, with 48000 frames per second (nominal) and with
    the device's rate estimated from (time, rendered frames) samples (rate-corrected);
  - for intervals of an odd number of ticks (5 or 7: not a multiple of the 33.3 ms frame), whether
    the onset interval is nearer the tick-correct value than the nearest multiple of a frame;
  - the spread of each onset's placement relative to its timestamp (median removed).

Usage (from the repository root):
  python3 tests/common/audio_sdl/onset_record.py --build-dir <build tree> --output <evidence directory>
      [--seconds 30] [--modes disk device]
Exit status: 0 when every mode ran, 1 when one could not run, 2 on a usage error.
"""
import argparse
import csv
import hashlib
import os
import platform
import statistics
import struct
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[3]
PROGRAM = "gyo_audio_onset_record"
RATE = 48000
TICK_MS = 1000.0 / 60.0
FRAME_MS = 1000.0 / 30.0


def find_program(build_dir):
    for suffix in ("", ".exe"):
        for path in sorted(build_dir.rglob(PROGRAM + suffix)):
            if path.is_file():
                return path
    return None


def read_summary(path):
    values = {}
    if path.exists():
        for line in path.read_text().splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                values[key] = value
    return values


def percentile(values, q):
    ordered = sorted(values)
    if not ordered:
        return float("nan")
    index = min(len(ordered) - 1, max(0, round(q * (len(ordered) - 1))))
    return ordered[index]


def describe(values):
    absolute = [abs(v) for v in values]
    return (f"n={len(values)} median={statistics.median(values):+.4f} p50_abs={percentile(absolute, 0.5):.4f} "
            f"p99_abs={percentile(absolute, 0.99):.4f} max_abs={max(absolute):.4f}")


def onsets(raw):
    """Frames whose left sample turns nonzero after a silent frame, with that sample."""
    found = []
    previous = 0
    for frame in range(len(raw) // 4):
        left, _ = struct.unpack_from("<hh", raw, frame * 4)
        if left != 0 and previous == 0:
            found.append((frame, left))
        previous = left
    return found


def estimate_rate(rate_rows):
    """Frames per second of the device against the Engine clock (least squares)."""
    points = [(int(r["now_ns"]) / 1e9, int(r["rendered_frames"])) for r in rate_rows if int(r["rendered_frames"]) > 0]
    if len(points) < 2:
        return float("nan")
    mean_t = statistics.fmean(t for t, _ in points)
    mean_f = statistics.fmean(f for _, f in points)
    num = sum((t - mean_t) * (f - mean_f) for t, f in points)
    den = sum((t - mean_t) ** 2 for t, _ in points)
    return num / den


def analyze_disk(directory, lines):
    summary = read_summary(directory / "summary.txt")
    with open(directory / "events.csv", newline="") as f:
        events = [row for row in csv.DictReader(f)]
    with open(directory / "rate.csv", newline="") as f:
        rate_rows = list(csv.DictReader(f))
    raw = (directory / "audio.raw").read_bytes()
    found = onsets(raw)
    accepted = [e for e in events if e["accepted"] == "1"]
    submitted = int(summary.get("submitted", "0"))
    played = int(summary.get("played_on_time", "0")) + int(summary.get("played_late", "0"))
    dropped = int(summary.get("dropped_queue_full", "0")) + int(summary.get("dropped_no_voice", "0"))
    balanced = submitted == len(events) and submitted == played + dropped and len(found) == len(accepted)
    lines.append(f"disk balance: events={len(events)} submitted={submitted} played={played} dropped={dropped} "
                 f"onsets={len(found)} amplitudes={sorted(set(a for _, a in found))} "
                 f"{'consistent' if balanced else 'INCONSISTENT'}")
    if len(found) != len(accepted) or len(found) < 3:
        lines.append("disk analysis skipped: onsets and accepted events do not pair up")
        return False

    rate = estimate_rate(rate_rows)
    lines.append(f"disk device rate: {rate:.2f} frames/s against the Engine clock ({(rate / RATE - 1) * 1e6:+.0f} ppm)")
    nominal, corrected, odd_near_tick, odd_total = [], [], 0, 0
    for (f0, _), (f1, _), e0, e1 in zip(found, found[1:], accepted, accepted[1:]):
        dt_ms = (int(e1["at_ns"]) - int(e0["at_ns"])) / 1e6
        onset_ms = (f1 - f0) * 1000.0 / RATE
        nominal.append(onset_ms - dt_ms)
        corrected.append((f1 - f0) * 1000.0 / rate - dt_ms)
        ticks = int(e1["tick"]) - int(e0["tick"])
        if ticks % 2 == 1:
            odd_total += 1
            frame_grid = round(onset_ms / FRAME_MS) * FRAME_MS
            if abs(onset_ms - ticks * TICK_MS) < abs(onset_ms - frame_grid):
                odd_near_tick += 1
    lines.append(f"disk interval error, nominal 48000 (ms): {describe(nominal)}")
    lines.append(f"disk interval error, rate-corrected (ms): {describe(corrected)}")
    lines.append(f"disk odd-tick intervals nearer the tick value than the frame grid: {odd_near_tick}/{odd_total}")
    placement = [f * 1000.0 / rate - int(e["at_ns"]) / 1e6 for (f, _), e in zip(found, accepted)]
    centre = statistics.median(placement)
    lines.append(f"disk placement spread around the median (ms): {describe([p - centre for p in placement])}")
    send_delay = [(int(e["sent_ns"]) - int(e["at_ns"])) / 1e6 for e in events]
    lines.append(f"send delay after the event (ms): {describe(send_delay)}")
    return True


def run_mode(program, mode, directory, seconds, lines):
    directory.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    if mode == "disk":
        env.update({
            "SDL_AUDIO_DRIVER": "disk",
            "SDL_AUDIO_DISK_OUTPUT_FILE": str(directory / "audio.raw"),
            "SDL_AUDIO_DISK_TIMESCALE": "1",
            "SDL_AUDIO_DEVICE_SAMPLE_FRAMES": "480",
            "SDL_AUDIO_FORMAT": "S16",
            "SDL_AUDIO_FREQUENCY": "48000",
            "SDL_AUDIO_CHANNELS": "2",
        })
    try:
        done = subprocess.run([str(program), str(seconds), str(directory)], env=env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, errors="replace", timeout=seconds + 60)
    except subprocess.TimeoutExpired:
        lines.append(f"{mode}: timeout")
        return False
    (directory / "program.log").write_text(done.stdout)
    summary = read_summary(directory / "summary.txt")
    lines.append(f"{mode}: exit={done.returncode} " + " ".join(f"{k}={v}" for k, v in summary.items()))
    if done.returncode != 0:
        return False
    return analyze_disk(directory, lines) if mode == "disk" else True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--modes", nargs="+", default=["disk", "device"], choices=["disk", "device"])
    args = parser.parse_args()
    program = find_program((REPOSITORY / args.build_dir).resolve())
    if program is None:
        print(f"{PROGRAM} not found under {args.build_dir}", file=sys.stderr)
        return 2
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPOSITORY, stdout=subprocess.PIPE, text=True).stdout.strip()
    dirty = subprocess.run(["git", "status", "--porcelain"], cwd=REPOSITORY, stdout=subprocess.PIPE, text=True).stdout.strip()
    meta = [f"commit={commit}{' (uncommitted changes)' if dirty else ''}",
            f"program={program}", f"program_sha256={hashlib.sha256(program.read_bytes()).hexdigest()}",
            f"os={platform.platform()}", f"machine={platform.machine()}", f"seconds={args.seconds}"]
    if hasattr(os, "getloadavg"):
        meta.append("load=" + " ".join(f"{v:.2f}" for v in os.getloadavg()))
    (output / "meta.txt").write_text("\n".join(meta) + "\n")

    lines = []
    ok = all([run_mode(program, mode, output / mode, args.seconds, lines) for mode in args.modes])
    (output / "report.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
