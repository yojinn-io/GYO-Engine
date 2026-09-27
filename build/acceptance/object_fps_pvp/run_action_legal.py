"""Explicit product-owned legal-fire headless round; owns only its child services.

--duration is measured real time, plus two seconds warmup and two seconds drain.
Long mode requires --soak --duration1800; default is one ten-second short round.
No GUI clients, user-started services or unrelated processes are controlled.
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

from action_evidence import analyze_legal
from run_network import free_port


def run(args):
    output = args.output
    output.mkdir(parents=True, exist_ok=False)
    manifest = {'artifacts': {name: {'path': str(getattr(args, name)),
        'sha256': hashlib.sha256(getattr(args, name).read_bytes()).hexdigest()}
        for name in ('match', 'gateway', 'probe', 'arena')}, 'fps': args.fps,
        'measurement_seconds': args.duration, 'warmup_seconds': 2, 'drain_seconds': 2,
        'soak_requested': args.soak, 'monotonic_start_ns': time.monotonic_ns()}
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
                               '--output', output, '--duration', args.duration, '--fps', args.fps, '--legal-shots', 'true'])
        if probe.wait(timeout=args.duration+30):
            raise RuntimeError('Action probe failed; see probe.log/client-failure.json')
        gateway.terminate(); gateway.wait(timeout=10)
        match.terminate(); match.wait(timeout=10)
        if match.returncode:
            raise RuntimeError('Match diagnostic trace did not flush cleanly')
        result = analyze_legal(output)
        counters = re.search(r'rate_accepted_packets=(\d+) rate_limited_packets=(\d+) max_session_window_packets=(\d+)',
                             (output/'gateway.log').read_text())
        if not counters:
            raise RuntimeError('Missing actual Gateway Session rate counters')
        result['gateway_counters'] = dict(zip(('accepted_packets', 'rate_limited_packets', 'maximum_window_packets'),
                                              map(int, counters.groups())))
        if int(counters[2]) != 0 or int(counters[3]) > 120:
            result['errors'].append('Gateway rate limit drop or accepted count exceeds120')
        result['passed'] = not result['errors']
        result['full_30_minute_qualified'] = args.soak and result['full_30_minute_qualified'] and result['passed']
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
    (output/'report.txt').write_text('\n'.join([
        'PASS' if result['passed'] else 'FAIL / INCOMPLETE',
        f'Measured {args.duration}s at nominal {args.fps}Hz; legal shots >=400ms; headless only.',
        f'Full30-minute qualification: {result["full_30_minute_qualified"]}',
        f'Action submitted/delivered/accepted: {result.get("submitted")}/{result.get("delivered")}/{result.get("accepted")}',
        f'Adjudication/Client decision P95ms: {result.get("adjudication_p95_ms")}/{result.get("client_decision_p95_ms")}',
        *result['errors'], 'Detailed thresholds, intervals and movement evidence: result.json'])+'\n')
    print(json.dumps({'passed': result['passed'], 'output': str(output), 'errors': result['errors'],
                      'full_30_minute_qualified': result['full_30_minute_qualified']}), flush=True)
    return 0 if result['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--fps', choices=(30, 60, 144), type=int, default=60)
    parser.add_argument('--duration', type=int, default=10)
    parser.add_argument('--soak', action='store_true')
    args = parser.parse_args()
    if args.soak and (args.duration != 1800 or args.fps not in (60, 144)):
        parser.error('--soak requires --duration1800 and --fps60 or144')
    if not args.soak and not 5 <= args.duration <= 30:
        parser.error('Short measurement must be5..30s; long runs require explicit --soak')
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
