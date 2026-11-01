"""Cross-process hit-part check (v8-product-hitbox-shot J6): a real Gateway, the real Match executable, two Clients.

  python3 hit_part_cross_check.py --match MATCH --probe NETWORK_PROBE --arena ARENA --output DIR [--go GO]

The Match executable must judge by the shapes on the posed body (match_main.cpp passes the posed hit model to
its runtime) and the parts must reach the Client: the network probe's --hit-parts-only mode shoots the standing
target's head and then its leg, aiming from the same posed model on the content next to the Match, and requires
HEAD with 50000 and LEG with 18750 in the Client's decisions and the target's hp. An in-process IPC test cannot
see the executable's wiring, so this one runs the executable.

It lives with the product tests for the reason ingress_cross_check.py gives. Exit status 0 when every judgement
holds, 1 otherwise.
"""
import argparse
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import urllib.request

from ingress_cross_check import build_gateway, stop, wait_until  # noqa: E402
from run_network import free_port, wait_for_match_ready  # noqa: E402  (on the path ingress_cross_check sets)

# match_main.cpp writes this once it built the posed model; 7 shapes in the product hitbox.
POSED_LINE = "[ObjectFPS/PvP Match] hit model=posed shapes=7"
PASSED_LINE = "hit parts passed: the real Match judged the head and the leg of the posed body"


def run(args):
    if args.output.exists():
        shutil.rmtree(args.output)
    args.output.mkdir(parents=True)
    gateway_executable = build_gateway(args.go, args.output)
    processes, logs = [], []

    def start(name, command):
        log = (args.output / f"{name}.log").open("w", encoding="utf-8")
        logs.append(log)
        process = subprocess.Popen([str(part) for part in command], stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process

    failures = []
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start("match", [args.match, "--arena", args.arena, "--listen", f"127.0.0.1:{ipc}"])
        wait_for_match_ready(match, args.output / "match.log", f"127.0.0.1:{ipc}")
        gateway = start("gateway", [gateway_executable, "--runtime", f"127.0.0.1:{ipc}", "--http", f"127.0.0.1:{http}",
                                    "--udp", f"127.0.0.1:{udp}", "--advertise-ip", "127.0.0.1"])

        def rooms():
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{http}/rooms", timeout=.3):
                    return True
            except OSError:
                return False
        wait_until(rooms, 15, "Gateway lobby", (match, gateway))
        content = args.match.parent / "assets" / "object_fps_pvp"
        probe = subprocess.run([str(args.probe), "--gateway", f"127.0.0.1:{http}", "--arena", str(args.arena),
                                "--content", str(content), "--hit-parts-only"],
                               capture_output=True, text=True, timeout=60)
        (args.output / "probe.log").write_text(probe.stdout + probe.stderr, encoding="utf-8")
        if probe.returncode or PASSED_LINE not in probe.stdout:
            failures.append(f"network probe exited with {probe.returncode}: {(probe.stdout + probe.stderr)[-2000:]}")
        stop(gateway)
        stop(match)
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.kill()
                process.wait()
        for log in logs:
            log.close()
    if POSED_LINE not in (args.output / "match.log").read_text(encoding="utf-8", errors="replace"):
        failures.append(f"the Match log lacks '{POSED_LINE}'")
    for failure in failures:
        print(f"hit part cross check: {failure}", file=sys.stderr)
    if not failures:
        print("hit part cross check: passed (head 50000, leg 18750 from the real Match through the Gateway)")
    return 1 if failures else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ("match", "probe", "arena", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--go", default="go")
    args = parser.parse_args(argv)
    for name in ("match", "probe", "arena", "output"):
        setattr(args, name, getattr(args, name).resolve())
    if shutil.which(args.go) is None:
        print(f"hit part cross check: Go executable {args.go!r} not found", file=sys.stderr)
        return 1
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
