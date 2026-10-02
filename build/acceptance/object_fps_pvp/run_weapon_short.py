"""Explicit, bounded two-GUI batch-04 probes; never runs the long latency gate."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import socket
import subprocess
import sys
import time
import urllib.request

from presentation_evidence import analyze_short_latency
from run_network import free_port, wait_for_match_ready
from run_timing import WINDOW_INTERFERENCE_NOTE, gate_window_evidence, wayland_warning


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def rank(values, quantile):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * quantile) - 1)] if ordered else None


def weapon_result(directory, fps, capture):
    result = {"passed": True, "nominal_fps": fps, "capture": capture,
              "long_run_certification": False, "clients": {}, "errors": []}
    for role in ("create", "join"):
        report = json.loads((directory / f"{role}-weapon.json").read_text(encoding="utf-8"))
        frames = [json.loads(line) for line in
                  (directory / f"{role}-weapon-frames.jsonl").read_text(encoding="utf-8").splitlines()]
        all_presented = [frame for frame in frames if frame["presented"]]
        all_stamps = [frame["presented_seconds"] for frame in all_presented]
        all_intervals = [b - a for a, b in zip(all_stamps, all_stamps[1:])]
        if any(interval <= 0 for interval in all_intervals):
            result["errors"].append(f"{role}: full-run successful presentation timestamps repeat or move backwards")
        feedback = [frame for frame in all_presented if "submission_to_presented_seconds" in frame]
        feedback_delays = [frame["submission_to_presented_seconds"] for frame in feedback]
        if len(feedback) != report.get("injected_sdl_shots", 0):
            result["errors"].append(f"{role}: not every submitted SDL shot has first-successful-Presented timing")
        if any(not math.isfinite(delay) or delay < 0 for delay in feedback_delays):
            result["errors"].append(f"{role}: invalid submitted-shot-to-Presented duration")
        for frame in feedback:
            if not frame["weapon"]["shooting"] or frame["weapon"]["last_action"] != frame["update"]["last_action"]:
                result["errors"].append(f"{role}: shot absent from its first successful Presented frame")
        presented = [frame for frame in all_presented if 0 <= frame["seconds"] <= 5]
        stamps = [frame["presented_seconds"] for frame in presented]
        if any(b <= a for a, b in zip(stamps, stamps[1:])):
            result["errors"].append(f"{role}: successful presentation timestamps repeat or move backwards")
        intervals = [b - a for a, b in zip(stamps, stamps[1:])]
        measured = (len(stamps) - 1) / (stamps[-1] - stamps[0]) if len(stamps) > 1 else 0
        result["clients"][role] = {"probe_passed": report["passed"], "checks": report["checks"],
            "successful_frames_before_lifecycle": len(stamps), "measured_mean_fps": measured,
            "frame_interval_p50_seconds": rank(intervals, .5),
            "frame_interval_p95_seconds": rank(intervals, .95),
            "maximum_frame_interval_seconds": max(intervals, default=None),
            "cadence_window_seconds": [0, 5],
            "cadence_window_frame_intervals_seconds": intervals,
            "full_run_successful_frames": len(all_stamps),
            "full_run_frame_intervals_seconds": all_intervals,
            "full_run_frame_interval_p50_seconds": rank(all_intervals, .5),
            "full_run_frame_interval_p95_seconds": rank(all_intervals, .95),
            "full_run_maximum_frame_interval_seconds": max(all_intervals, default=None),
            "all_shot_to_first_presented_seconds": feedback_delays,
            "shot_to_first_presented_p50_seconds": rank(feedback_delays, .5),
            "shot_to_first_presented_p95_seconds": rank(feedback_delays, .95),
            "shot_to_first_presented_maximum_seconds": max(feedback_delays, default=None),
            "skipped_frames": report.get("skipped_frames"),
            "first_feedback_seconds": report.get("first_feedback_seconds")}
        if not report["passed"]:
            result["errors"].append(f"{role}: {report.get('error', 'probe failed')}")
        if not capture and measured < fps * .85:
            result["errors"].append(f"{role}: measured {measured:.2f} FPS is below 85% of requested {fps} FPS")
    result["passed"] = not result["errors"]
    return result


def gate_latency_case(result):
    """The latency case decides pass/fail, so it takes the counted GUI round's window gate.

    Detected window interference or missing window evidence fails the case
    with an explicit window_gate status; the latency result before the gate is
    kept as window_gate.thresholds_passed.
    """
    counted = {"overall_passed": bool(result.get("passed")), "presentation": result,
               "errors": result.setdefault("errors", [])}
    gate = gate_window_evidence(counted, report_only=False)
    result["window_gate"] = gate
    result["window_interference_note"] = WINDOW_INTERFERENCE_NOTE
    result["passed"] = counted["overall_passed"]
    return gate


def run_case(args, name):
    directory = args.output / name
    directory.mkdir(parents=True, exist_ok=False)
    fps = 30 if name == "weapon30" else 144 if name == "weapon144" else 60
    mode = "--latency-short" if name == "latency" else "--weapon-capture" if name == "capture" else "--weapon-short"
    ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
    processes, logs = [], []
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    commands = []

    def start(label, command):
        commands.append(command)
        log = (directory / f"{label}.log").open("w", encoding="utf-8")
        logs.append(log)
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                   cwd=directory, creationflags=flags)
        processes.append(process)
        return process

    result = {"passed": False, "case": name, "commands": commands}
    try:
        match = start("match", [str(args.match), "--arena", str(args.arena), "--listen", f"127.0.0.1:{ipc}"])
        wait_for_match_ready(match, directory / "match.log", f"127.0.0.1:{ipc}")
        gateway = start("gateway", [str(args.gateway), "--runtime", f"127.0.0.1:{ipc}",
            "--http", f"127.0.0.1:{http}", "--udp", f"127.0.0.1:{udp}", "--advertise-ip", "127.0.0.1"])
        deadline = time.monotonic() + 10
        while True:
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError("Real service exited during startup; inspect Match/Gateway logs")
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{http}/rooms", timeout=.2) as response:
                    if response.status == 200:
                        break
            except (OSError, TimeoutError):
                pass
            if time.monotonic() >= deadline:
                raise RuntimeError("Real Gateway startup timed out")
            time.sleep(.03)
        gui = []
        for role in ("create", "join"):
            gui.append(start(role, [str(args.gui_probe), mode, "--fps", str(fps),
                "--duration", "16" if name == "latency" else "8", "--arena-root", str(args.arena_root),
                "--gateway", f"127.0.0.1:{http}", "--role", role, "--gpu-driver", args.gpu_driver,
                "--output", str(directory)]))
        deadline = time.monotonic() + (50 if name == "latency" else 35)
        while any(process.poll() is None for process in gui):
            if any(process.poll() not in (None, 0) for process in gui):
                raise RuntimeError("GUI probe failed; inspect create/join logs and raw weapon evidence")
            if time.monotonic() >= deadline:
                raise RuntimeError("Bounded GUI case timed out")
            time.sleep(.03)
        if any(process.returncode != 0 for process in gui):
            raise RuntimeError("GUI probe failed")
        measured = analyze_short_latency(directory) if name == "latency" else weapon_result(directory, fps, name == "capture")
        result.update(measured)
        if name == "latency":
            gate_latency_case(result)
        if not result["passed"]:
            raise RuntimeError("Short evidence failed: " + "; ".join(result["errors"]))
    except Exception as error:
        result["passed"] = False
        result["error"] = str(error)
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
        for process in reversed(processes):
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
        for log in logs:
            log.close()
        result["binary_and_asset_hashes"] = {str(path): digest(path) for path in
            (args.match, args.gateway, args.gui_probe, args.arena, args.arena_root / "asset_catalog.json")}
        (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("match", "gateway", "gui-probe", "arena", "arena-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--gpu-driver", default="auto", choices=("auto", "vulkan", "d3d12", "metal"))
    parser.add_argument("--case", action="append", choices=("weapon30", "weapon60", "weapon144", "capture", "latency"),
                        help="Run only selected bounded cases; repeat for multiple cases")
    args = parser.parse_args()
    for name in ("match", "gateway", "gui_probe", "arena", "arena_root", "output"):
        setattr(args, name, getattr(args, name).resolve())
    for name in ("match", "gateway", "gui_probe", "arena"):
        if not getattr(args, name).is_file():
            parser.error(f"Missing required {name}: {getattr(args, name)}")
    if not (args.arena_root / "asset_catalog.json").is_file():
        parser.error("--arena-root must contain the deployed asset_catalog.json")
    args.output.mkdir(parents=True, exist_ok=True)
    cases = args.case or ["weapon30", "weapon60", "weapon144", "capture", "latency"]
    # The latency case takes the counted GUI window gate.
    warning = wayland_warning("latency" in cases, report_only=False)
    if warning:
        print(warning, file=sys.stderr, flush=True)
    results = []
    for name in cases:
        result = run_case(args, name)
        results.append({"case": name, "passed": result["passed"], "error": result.get("error")})
        print(json.dumps(results[-1]), flush=True)
        if not result["passed"]:
            break
    summary = {"passed": len(results) == len(cases) and all(result["passed"] for result in results),
               "requested_cases": cases, "cases": results, "long_run_executed": False,
               "scope": "Short real two-GUI localhost regressions; no physical LAN, monitor scanout or 120-second certification."}
    (args.output / "weapon-short-matrix.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
