"""Explicit product-owned v5 gameplay soak; owns only its child services.

The probe repeats the 16-second v5 gameplay plan (shots, empty magazine, reload,
death, respawn, jumps) for whole cycles; player B's life advances once per cycle.
Short mode runs 2..4 cycles. Long mode requires --soak and runs 113 cycles
(1808 s, the smallest whole-cycle run covering 1800 s) at 60 or 144 Hz.
Headless and clean: no relay, fault or GUI. No user-started services or
unrelated processes are controlled, and nothing is rerun automatically.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import signal
import socket
import subprocess
import time
import urllib.request

from gameplay_soak_evidence import analyze_soak
from run_network import free_port, wait_for_match_ready

CYCLE_SECONDS = 16
SOAK_CYCLES = 113


def gateway_counters(log):
    counters = re.search(r'rate_accepted_packets=(\d+) rate_limited_packets=(\d+) max_session_window_packets=(\d+)', log)
    if not counters:
        raise RuntimeError('Missing actual Gateway Session rate counters')
    return dict(zip(('rate_accepted_packets', 'rate_limited_packets', 'max_session_window_packets'),
                    map(int, counters.groups())))


def run(args):
    output = args.output
    output.mkdir(parents=True, exist_ok=False)
    manifest = {'artifacts': {name: {'path': str(getattr(args, name)),
        'sha256': hashlib.sha256(getattr(args, name).read_bytes()).hexdigest()}
        for name in ('match', 'gateway', 'probe', 'arena')}, 'fps': args.fps, 'cycles': args.cycles,
        'cycle_seconds': CYCLE_SECONDS, 'soak_requested': args.soak, 'monotonic_start_ns': time.monotonic_ns()}
    (output/'artifacts.json').write_text(json.dumps(manifest, indent=2)+'\n')
    processes, logs = [], []
    def start(name, command):
        log = (output/(name+'.log')).open('w'); logs.append(log)
        process = subprocess.Popen([str(item) for item in command], stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    result = {'passed': False, 'errors': ['Run did not complete'], 'full_30_minute_qualified': False}
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start('match', [args.match, '--arena', args.arena, '--listen', f'127.0.0.1:{ipc}',
                               '--movement-trace', output/'match-commands.jsonl'])
        wait_for_match_ready(match, output/'match.log', f'127.0.0.1:{ipc}')
        gateway = start('gateway', [args.gateway, '--runtime', f'127.0.0.1:{ipc}', '--http', f'127.0.0.1:{http}',
                                    '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        deadline = time.monotonic()+15
        while True:
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError('Service failed during startup')
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=.2):
                    break
            except OSError:
                if time.monotonic() >= deadline:
                    raise RuntimeError('Service startup timeout')
                time.sleep(.02)
        probe = start('probe', [args.probe, '--gateway', f'127.0.0.1:{http}', '--arena', args.arena,
                               '--output', output, '--fps', args.fps, '--gameplay-v5', 'true', '--cycles', args.cycles])
        if probe.wait(timeout=args.cycles*CYCLE_SECONDS+60):
            raise RuntimeError('Gameplay probe failed; see probe.log/client-failure.json')
        gateway.terminate(); gateway.wait(timeout=10)
        match.terminate(); match.wait(timeout=10)
        if match.returncode:
            raise RuntimeError('Match diagnostic trace did not flush cleanly')
        result = analyze_soak(output, gateway_counters((output/'gateway.log').read_text()), soak=args.soak)
    except BaseException as error:
        result['passed'] = result['full_30_minute_qualified'] = False
        result.setdefault('errors', []).append(str(error) or type(error).__name__)
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()
        for log in logs:
            log.close()
    result['artifacts'] = manifest
    (output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    movement = result.get('movement', {})
    (output/'report.txt').write_text('\n'.join([
        'PASS' if result['passed'] else 'FAIL / INCOMPLETE',
        f'{args.cycles} cycles x {CYCLE_SECONDS}s at nominal {args.fps}Hz; repeated v5 gameplay plan; headless, clean, no relay.',
        f'Full 30-minute qualification: {result["full_30_minute_qualified"]}',
        f'Actions planned/delivered: {result.get("planned_actions")}/{result.get("delivered_actions")}; '
        f'verdict signatures per cycle: {result.get("per_cycle_verdict_signatures")}',
        f'Deaths/respawns: {result.get("deaths")}/{result.get("respawns")}',
        f'Legal Match/Client P95ms: {result.get("legal_match_p95_ms")}/{result.get("legal_client_p95_ms")}',
        f'Movement Actual P50/P95ms: {movement.get("actual_p50_ms")}/{movement.get("actual_p95_ms")}, '
        f'send P95ms: {movement.get("send_p95_ms")}, Actual fraction: {movement.get("actual_fraction")}',
        *result['errors'], 'Per-cycle, per-life and phase-tracking detail: result.json'])+'\n')
    print(json.dumps({'passed': result['passed'], 'output': str(output), 'errors': result['errors'],
                      'full_30_minute_qualified': result['full_30_minute_qualified']}), flush=True)
    return 0 if result['passed'] else 1


def parse(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--fps', choices=(30, 60, 144), type=int, default=60)
    parser.add_argument('--cycles', type=int, help=f'short mode 2..4 (default 2); --soak always runs {SOAK_CYCLES}')
    parser.add_argument('--soak', action='store_true', help=f'explicit long run: {SOAK_CYCLES} cycles, --fps 60 or 144')
    args = parser.parse_args(argv)
    if args.soak:
        if args.cycles not in (None, SOAK_CYCLES) or args.fps not in (60, 144):
            parser.error(f'--soak runs exactly {SOAK_CYCLES} cycles at --fps 60 or 144')
        args.cycles = SOAK_CYCLES
    else:
        args.cycles = 2 if args.cycles is None else args.cycles
        if not 2 <= args.cycles <= 4:
            parser.error('Short mode runs 2..4 cycles; long runs require explicit --soak')
    return parser, args


def main():
    parser, args = parse()
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        setattr(args, name, getattr(args, name).resolve())
        if name != 'output' and not getattr(args, name).is_file():
            parser.error('Missing '+name)
    if args.output.exists():
        parser.error('Output must be a fresh directory; old evidence is never overwritten')
    def interrupt(_signal, _frame):
        raise KeyboardInterrupt('Interrupted; run incomplete, evidence retained')
    signal.signal(signal.SIGTERM, interrupt)
    return run(args)


if __name__ == '__main__':
    raise SystemExit(main())
