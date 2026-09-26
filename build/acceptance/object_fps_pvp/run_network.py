"""Owner-local, real Go/C++ acceptance. No mock World or generated traffic state."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
import urllib.request

from impaired_network import impaired_gateway
from presentation_evidence import analyze_presentation


def free_port(kind=socket.SOCK_STREAM):
    with socket.socket(socket.AF_INET, kind) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


@contextmanager
def isolated_runtime(processes):
    # Windows cannot remove a running executable. Stop children before the
    # temporary directory exits, including every failed assertion path.
    with tempfile.TemporaryDirectory(prefix="gyo-pvp-headless-") as directory:
        try:
            yield directory
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
            for process in processes:
                process.wait(timeout=10)


def main():
    parser = argparse.ArgumentParser()
    for name in ("match", "gateway", "probe", "arena", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--gui-probe", type=Path)
    parser.add_argument("--arena-root", type=Path)
    parser.add_argument("--network-impairments", action="store_true",
                        help="Repeat the real socket probe with LAN delay, jitter and packet loss")
    args = parser.parse_args()
    for name in ("match", "gateway", "probe", "arena"):
        if not getattr(args, name).is_file():
            parser.error(f"Missing required {name}: {getattr(args, name)}")
    args.output.mkdir(parents=True, exist_ok=True)
    if args.gui_probe and (not args.gui_probe.is_file() or not args.arena_root or
                           not args.arena_root.is_dir()):
        parser.error("--gui-probe requires an executable and --arena-root with deployed assets")
    ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
    processes = []
    handles = []
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    try:
        # Prove that Match starts without the product's SDL DLLs or asset catalog.
        with isolated_runtime(processes) as isolated:
            staged = Path(isolated) / args.match.name
            shutil.copy2(args.match, staged)

            def start(name, command, executable=None):
                log = (args.output / f"{name}.log").open("w", encoding="utf-8")
                handles.append(log)
                process = subprocess.Popen(command, cwd=isolated, stdout=log,
                                           stderr=subprocess.STDOUT, creationflags=flags,
                                           executable=executable)
                processes.append(process)
                return process

            for invalid_port in ("0", "65536", "27016junk"):
                rejected = subprocess.run([str(staged), "--arena", str(args.arena.resolve()),
                                           "--listen", "127.0.0.1:" + invalid_port],
                                          cwd=isolated, capture_output=True, text=True, timeout=8,
                                          creationflags=flags)
                if rejected.returncode != 1 or "IPC port" not in rejected.stderr:
                    raise RuntimeError(f"Invalid-port check failed for {invalid_port}: "
                                       f"exit={rejected.returncode}, error={rejected.stderr!r}")
            # argv[0] is deliberately a bare filename and cwd contains no assets.
            # The deployed executable must locate its own default Arena.
            default_port = free_port()
            default_match = start("match-default-arena", [args.match.name, "--listen",
                                  f"127.0.0.1:{default_port}"], executable=str(args.match.resolve()))
            deadline = time.monotonic() + 15
            while True:
                if default_match.poll() is not None:
                    raise RuntimeError("Match could not load its executable-relative Arena")
                try:
                    with socket.create_connection(("127.0.0.1", default_port), timeout=0.2):
                        break
                except OSError:
                    if time.monotonic() > deadline:
                        raise RuntimeError("Default Arena Match did not become ready")
                    time.sleep(0.05)
            default_match.terminate()
            default_match.wait(timeout=10)
            match = start("match", [str(staged), "--arena", str(args.arena.resolve()),
                                    "--listen", f"127.0.0.1:{ipc}"])
            gateway_command = [str(args.gateway.resolve()), "--runtime", f"127.0.0.1:{ipc}",
                               "--http", f"127.0.0.1:{http}", "--udp", f"127.0.0.1:{udp}",
                               "--advertise-ip", "127.0.0.1"]
            gateway = start("gateway", gateway_command)
            deadline = time.monotonic() + 30
            while True:
                if match.poll() is not None or gateway.poll() is not None:
                    raise RuntimeError("A service exited during startup; inspect logs")
                try:
                    with urllib.request.urlopen(f"http://127.0.0.1:{http}/rooms", timeout=0.5) as response:
                        if response.status == 200:
                            break
                except (OSError, TimeoutError):
                    pass
                if time.monotonic() > deadline:
                    raise RuntimeError("Gateway did not become reachable")
                time.sleep(0.05)
            time.sleep(0.25)
            result = subprocess.run([str(args.probe.resolve()), "--gateway", f"127.0.0.1:{http}",
                                     "--arena", str(args.arena.resolve())],
                                    cwd=isolated, capture_output=True, text=True, timeout=45,
                                    creationflags=flags)
            (args.output / "network.log").write_text(result.stdout + result.stderr, encoding="utf-8")
            if result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
            impairment_results = []
            if args.network_impairments:
                for rtt in (0, 20, 40):
                    # Each invocation leaves its sessions; allow those HTTP
                    # operations to complete before the next pair joins.
                    time.sleep(0.1)
                    case = None
                    proxy = None
                    try:
                        with impaired_gateway(http, udp, rtt) as proxy:
                            case = subprocess.run(
                                [str(args.probe.resolve()), "--gateway", proxy.gateway,
                                 "--arena", str(args.arena.resolve())], cwd=isolated,
                                capture_output=True, text=True, timeout=45, creationflags=flags)
                        evidence = proxy.evidence()
                    except BaseException:
                        if proxy is not None:
                            (args.output / f"network-rtt-{rtt}.json").write_text(
                                json.dumps(proxy.evidence(), indent=2), encoding="utf-8")
                        raise
                    (args.output / f"network-rtt-{rtt}.log").write_text(
                        case.stdout + case.stderr, encoding="utf-8")
                    evidence["probe_output"] = case.stdout.strip()
                    evidence["coverage_complete"] = (
                        evidence["input"]["dropped_first"] >= 2 and
                        evidence["input"]["dropped_burst"] >= 4 and
                        evidence["snapshot"]["received"] > 0 and
                        evidence["maximum_datagram_bytes"] <= 1200)
                    evidence["passed"] = (case.returncode == 0 and evidence["relay_error"] is None and
                                          evidence["coverage_complete"])
                    (args.output / f"network-rtt-{rtt}.json").write_text(
                        json.dumps(evidence, indent=2), encoding="utf-8")
                    impairment_results.append(evidence)
                    if not evidence["passed"]:
                        raise RuntimeError(f"RTT {rtt} ms acceptance failed: " + case.stdout +
                                           case.stderr + str(evidence["relay_error"] or ""))
            presentation_evidence = None
            if args.gui_probe:
                time.sleep(0.25)
                gui_processes = []
                for role in ("create", "join"):
                    command = [str(args.gui_probe.resolve()), "--arena-root", str(args.arena_root.resolve()),
                               "--gateway", f"127.0.0.1:{http}", "--role", role, "--duration", "5",
                               "--output", str((args.output / "gui").resolve())]
                    if role == "create":
                        command.append("--move")
                    gui_processes.append(start("gui-" + role, command))
                for process in gui_processes:
                    if process.wait(timeout=50):
                        raise RuntimeError("GUI acceptance failed; inspect gui-create.log and gui-join.log")
                presentation_evidence = analyze_presentation(args.output / "gui")
                if not presentation_evidence["passed"]:
                    raise RuntimeError("Cross-window presentation latency failed: " +
                                       "; ".join(presentation_evidence["errors"]))
            # A protocol-invalid IPC connection must not crash the executable.
            gateway.terminate()
            gateway.wait(timeout=10)
            time.sleep(0.1)
            with socket.create_connection(("127.0.0.1", ipc), timeout=2) as invalid:
                invalid.sendall(b"\x00\x01\x00\x01")  # 65537 exceeds the framing limit.
            time.sleep(0.1)
            if match.poll() is not None:
                raise RuntimeError("Match crashed on malformed IPC")
            # A replacement Gateway must receive a fresh, cleared Match. This
            # probe then observes actual IPC failure through two real clients.
            gateway = start("gateway-reconnected", gateway_command)
            time.sleep(0.3)
            ready = Path(isolated) / "failure-probe-ready"
            failure_probe = start("failure-probe", [str(args.probe.resolve()), "--gateway",
                                  f"127.0.0.1:{http}", "--disconnect-ready", str(ready)])
            deadline = time.monotonic() + 15
            while not ready.exists():
                if failure_probe.poll() is not None or gateway.poll() is not None:
                    raise RuntimeError("Failure probe could not join the replacement Gateway; inspect logs")
                if time.monotonic() > deadline:
                    raise RuntimeError("Failure probe did not become ready")
                time.sleep(0.05)
            match.terminate()
            match.wait(timeout=10)
            if failure_probe.wait(timeout=12):
                raise RuntimeError("Clients failed to handle Match disconnection; inspect failure-probe.log")
            with urllib.request.urlopen(f"http://127.0.0.1:{http}/rooms", timeout=2) as response:
                rooms = json.load(response)["rooms"]
            if len(rooms) != 1 or rooms[0]["status"] != "unavailable" or rooms[0]["players"] != 0:
                raise RuntimeError("Gateway retained a usable room or sessions after IPC failure")
            (args.output / "result.json").write_text(json.dumps({
                "passed": True, "transport": "localhost TCP + UDP + HTTP",
                "isolated_headless": True, "physical_lan": "not tested",
                "strict_listen_port_and_executable_relative_arena": True,
                "gui_render_and_movement": bool(args.gui_probe),
                "gui_local_prediction_observation": bool(args.gui_probe),
                "gui_cross_window_presentation_latency": presentation_evidence,
                "network_impairments": impairment_results,
                "ipc_failure_returns_both_clients_to_lobby": True,
                "replacement_gateway_joins_cleared_match": True,
                "probe_output": result.stdout.strip()}, indent=2), encoding="utf-8")
            print(result.stdout.strip())
    finally:
        for handle in handles:
            handle.close()


if __name__ == "__main__":
    main()
