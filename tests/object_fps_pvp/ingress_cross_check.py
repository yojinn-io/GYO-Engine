"""Cross-process ingress check (v7 batch 08c F3): a real Gateway and a real Match, one short clean run.

  python3 ingress_cross_check.py --match MATCH --probe TIMING_PROBE --arena ARENA --output DIR [--go GO]

It lives with the product tests, not in build/acceptance/object_fps_pvp: the frozen test_service_startup.py
requires every Gateway-starting script there to be one of its listed runners.

Builds the Gateway from apps/object_fps_pvp (go build, GOWORK=off), starts the Match with a movement trace (so it
also writes match-ingress.jsonl), the Gateway directly on the Match's IPC port, and the two-Client timing probe for
an 8-second run, so the Gateway (10-second windows) also writes one window before its final one. The ending is ordered so that I1's preconditions hold by construction: the probe leaves, the
Gateway stops (its final=1 lines follow the link's close), the Match logs its 'ipc closed' line, and only then
does the Match stop. ingress_evidence.py must judge IG, the Gateway-wide conservation, I0, T3, I2, J5a, S1, S2,
I1 and the trace as passed, on non-trivial counts.

The Gateway closes its runtime link with a plain Close while Match snapshots may still sit unread in its socket;
the kernel then resets the connection and the Match logs 'ipc closed reason=read: ...' instead of eof (seen in
about 1 of 13 development runs). I1 is then undetermined by its own precondition, never failed. Such an attempt
is kept (attemptN/) and the run is repeated, at most ATTEMPTS times; every other judgement must pass on every
attempt.

POSIX only: on Windows Popen.terminate() is TerminateProcess, so neither process could write its final window;
the check exits 77 (skipped) there. Exit status 0 when every judgement holds, 1 otherwise.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import time
import urllib.request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "build" / "acceptance" / "object_fps_pvp"))
import ingress_evidence  # noqa: E402
from run_network import free_port, wait_for_match_ready  # noqa: E402

GATEWAY_MODULE = ROOT / "apps" / "object_fps_pvp"
SKIPPED = 77
REQUIRED = ("IG", "gateway_global", "I0", "T3", "I2", "J5a", "S1", "S2", "I1", "trace")
CLOSED_LINE = "[ObjectFPS/PvP Match] ipc closed reason="
ATTEMPTS = 3


def wait_until(test, timeout, what, processes=()):
    deadline = time.monotonic() + timeout
    while not test():
        for process in processes:
            if process.poll() is not None:
                raise RuntimeError(f"{what}: a process exited with {process.returncode}")
        if time.monotonic() > deadline:
            raise RuntimeError(f"{what}: timed out after {timeout} s")
        time.sleep(.05)


def build_gateway(go, output):
    executable = output / "gateway"
    environment = dict(os.environ, GOWORK="off")
    completed = subprocess.run([go, "build", "-o", str(executable), "./gateway/cmd"], cwd=GATEWAY_MODULE,
                               env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                               timeout=600)
    (output / "gateway-build.log").write_text(completed.stdout, encoding="utf-8")
    if completed.returncode:
        raise RuntimeError(f"go build failed; inspect {output / 'gateway-build.log'}")
    return executable


def stop(process, timeout=15):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
    return process.wait(timeout=timeout)


def run(args):
    if args.output.exists():
        shutil.rmtree(args.output)
    args.output.mkdir(parents=True)
    gateway_executable = build_gateway(args.go, args.output)
    attempts = []
    for attempt in range(1, ATTEMPTS + 1):
        output = args.output / f"attempt{attempt}"
        output.mkdir()
        failures, retry = run_once(args, gateway_executable, output)
        attempts.append({"attempt": attempt, "failures": failures, "retried_for_reset": retry})
        if not retry:
            break
    (args.output / "attempts.json").write_text(json.dumps(attempts, indent=2) + "\n", encoding="utf-8")
    failures = attempts[-1]["failures"] + [f for a in attempts[:-1] for f in a["failures"] if not a["retried_for_reset"]]
    if attempts[-1]["retried_for_reset"]:
        failures.append(f"I1 stayed undetermined (connection reset at the close) in all {ATTEMPTS} attempts")
    for failure in failures:
        print(f"ingress cross check: {failure}"[:4000], file=sys.stderr)
    return 1 if failures else 0


def run_once(args, gateway_executable, output):
    """(failures, retry): retry when only I1 is undetermined because the IPC close was not an EOF."""
    processes, logs = [], []

    def start(name, command):
        log = (output / f"{name}.log").open("w", encoding="utf-8")
        logs.append(log)
        process = subprocess.Popen([str(part) for part in command], stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process

    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start("match", [args.match, "--arena", args.arena, "--listen", f"127.0.0.1:{ipc}",
                                "--movement-trace", output / "match-commands.jsonl"])
        wait_for_match_ready(match, output / "match.log", f"127.0.0.1:{ipc}")
        # Straight to the Match's port: no relay, so I1's no-IpcPause precondition holds.
        gateway = start("gateway", [gateway_executable, "--runtime", f"127.0.0.1:{ipc}", "--http", f"127.0.0.1:{http}",
                                    "--udp", f"127.0.0.1:{udp}", "--advertise-ip", "127.0.0.1"])

        def rooms():
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{http}/rooms", timeout=.3):
                    return True
            except OSError:
                return False
        wait_until(rooms, 15, "Gateway lobby", (match, gateway))
        probe = start("timing", [args.probe, "--gateway", f"127.0.0.1:{http}", "--arena", args.arena,
                                 "--output", output, "--duration", 8, "--fps", 60])
        if probe.wait(timeout=60):
            raise RuntimeError("Timing probe failed; inspect timing.log")
        if stop(gateway):
            raise RuntimeError(f"Gateway exited with {gateway.returncode}; inspect gateway.log")
        match_log = output / "match.log"
        wait_until(lambda: CLOSED_LINE in match_log.read_text(encoding="utf-8", errors="replace"), 10,
                   "Match IPC close", (match,))
        if stop(match):
            raise RuntimeError(f"Match exited with {match.returncode}; inspect match.log")
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.kill()
                process.wait()
        for log in logs:
            log.close()

    result = ingress_evidence.analyze_text(
        (output / "gateway.log").read_text(encoding="utf-8", errors="replace"),
        (output / "match.log").read_text(encoding="utf-8", errors="replace"),
        (output / "match-ingress.jsonl").read_text(encoding="utf-8", errors="replace"))
    (output / "ingress-evidence.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    failures = []
    if result["status"] != "passed":
        failures.append(f"status {result['status']}: {result.get('stops', '')}")
    for name in REQUIRED:
        status = result.get("checks", {}).get(name, {}).get("status")
        if status != "passed":
            failures.append(f"{name} is {status}: {result.get('checks', {}).get(name)}")
    if not failures:
        totals = result["totals"]
        written = sum(ingress_evidence.total(c, ingress_evidence.LINK_WRITTEN)
                      for c in totals["gateway"]["players"].values())
        published = sum(c.get("slack_published_samples", 0) for c in totals["match"]["players"].values())
        players = [p for p in totals["match"]["players"] if p != "0"]
        if written < 300 or published == 0 or len(players) != 2 or totals["gateway"]["windows"] < 2:
            failures.append(f"trivial run: written={written} published={published} players={players}")
        else:
            print(f"ingress cross check: passed (2 players, {written} commands written and received, "
                  f"{published} slack samples published, Gateway windows {totals['gateway']['windows']}, "
                  f"Match windows {totals['match']['windows']})")
    i1 = result.get("checks", {}).get("I1", {})
    reset = (i1.get("status") == "undetermined" and i1.get("preconditions", {}).get("eof_before_final") is False
             and all(v for k, v in i1["preconditions"].items() if k != "eof_before_final"))
    others = [f for f in failures if not f.startswith("I1 is undetermined")]
    if reset and not others:
        closes = result["totals"]["match"]["ipc_closed"]
        print(f"ingress cross check: {output.name}: IPC closed with {closes}, not EOF; I1 undetermined, retrying")
        return [], True
    return failures, False


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ("match", "probe", "arena", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--go", default="go")
    args = parser.parse_args(argv)
    if os.name == "nt":
        print("ingress cross check: skipped on Windows (no graceful stop for the final window)")
        return SKIPPED
    for name in ("match", "probe", "arena"):
        setattr(args, name, getattr(args, name).resolve())
    args.output = args.output.resolve()
    if shutil.which(args.go) is None:
        print(f"ingress cross check: Go executable {args.go!r} not found", file=sys.stderr)
        return 1
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
