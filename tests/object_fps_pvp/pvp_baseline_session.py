"""v8-product-pvp-baseline: today's real Match process, recorded and not judged.

Owner object_fps_pvp. Not a CTest: run by hand
(docs/object_fps_pvp/plans/v8/v8-product-pvp-baseline-declaration.md).

  start   --cold-starts N cold starts of the Match alone: the time from spawning
          the process to its ready line (the IPC listener accepts the Gateway,
          so a join can follow), then a clean SIGINT stop.
  session --runs N sessions of a Match, a Gateway and the four-client quad probe
          (bots that fight) for --duration seconds: the Match's own statistics
          lines (advance = one Advance from the clock sample to the wait,
          tick_overrun, tick_stalls) and its RSS sampled every second.

Every child is started and stopped here; nothing else is touched. Each output
file starts with the gyo.kernel-baseline header.
"""
import argparse
import json
import math
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import time
import urllib.request

READY_PREFIX = 'Object_FPS_PVP Match ready: '
BIN_UPPER_US = (250, 500, 1000, 2000, 4000, 8000, 16000, 32000, 64000)  # engine/time LateWakeStats
STATISTICS = '[ObjectFPS/PvP Match] network statistics '
HEADER = '# format=gyo.kernel-baseline, version=1\n'


def free_port(kind=socket.SOCK_STREAM):
    with socket.socket(socket.AF_INET, kind) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def rss_kib(pid):
    """Resident set size in KiB from ps (macOS and Linux both report KiB); None once gone."""
    result = subprocess.run(['ps', '-o', 'rss=', '-p', str(pid)], capture_output=True, text=True)
    text = result.stdout.strip()
    return int(text) if result.returncode == 0 and text else None


def stop(process, seconds=10):
    if process.poll() is None:
        process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    return process.returncode


def wait_ready(process, log_path, listen, timeout):
    """Seconds from now until the ready line for ``listen`` is in the log; raises on exit or timeout."""
    expected = READY_PREFIX + listen
    deadline = time.monotonic() + timeout
    while True:
        text = log_path.read_text(encoding='utf-8', errors='replace') if log_path.exists() else ''
        if any(line == expected or line.startswith(expected + ' ') for line in text.splitlines()):
            return
        if process.poll() is not None:
            raise RuntimeError(f'Match exited before ready (code {process.returncode}); see {log_path}')
        if time.monotonic() >= deadline:
            raise RuntimeError(f'Match not ready in {timeout} s; see {log_path}')
        time.sleep(0.001)


def cold_starts(args):
    rows = []
    for index in range(args.cold_starts):
        listen = f'127.0.0.1:{free_port()}'
        log_path = args.output / f'start-{index:02d}.log'
        with log_path.open('w') as log:
            spawned = time.monotonic()
            process = subprocess.Popen([str(args.match), '--arena', str(args.arena), '--listen', listen],
                                       stdout=log, stderr=subprocess.STDOUT)
            try:
                wait_ready(process, log_path, listen, args.ready_timeout)
                ready = time.monotonic()
            finally:
                code = stop(process)
        if code != 0:
            raise RuntimeError(f'Match start {index} stopped with code {code}; see {log_path}')
        rows.append((index, (ready - spawned) * 1000.0))
        time.sleep(args.pause)
    with (args.output / 'start.csv').open('w') as out:
        out.write(HEADER + f'# kind=real-start arena={args.arena.name} poll_ms=1 '
                  'span=spawn to the ready line in the Match log (log polled every 1 ms)\n')
        out.write('index,ready_ms\n')
        for index, millis in rows:
            out.write(f'{index},{millis:.3f}\n')
    return rows


def parse_statistics(text):
    windows = []
    for line in text.splitlines():
        start = line.find(STATISTICS)
        if start < 0:
            continue
        fields = dict(item.split('=', 1) for item in line[start + len(STATISTICS):].split() if '=' in item)
        windows.append(fields)
    return windows


def bins(fields, name):
    return [int(value) for value in fields[name + '_bins'].split(',')]


def quantile_upper_us(counts, maximum_us, q):
    """LateWakeStats::QuantileUpperBound on merged bins: the bin edge holding rank ceil(q n), capped at max."""
    total = sum(counts)
    if not total:
        return 0
    rank = math.ceil(q * total)
    seen = 0
    for index, count in enumerate(counts):
        seen += count
        if seen >= rank:
            return maximum_us if index == len(BIN_UPPER_US) else min(maximum_us, BIN_UPPER_US[index])
    return maximum_us


def session(args, index):
    folder = args.output / f'session-{index:02d}'
    folder.mkdir()
    processes, logs = [], []

    def start(name, command):
        log = (folder / (name + '.log')).open('w')
        logs.append(log)
        process = subprocess.Popen([str(part) for part in command], stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process

    rss = []
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        listen = f'127.0.0.1:{ipc}'
        match = start('match', [args.match, '--arena', args.arena, '--listen', listen])
        wait_ready(match, folder / 'match.log', listen, args.ready_timeout)
        gateway = start('gateway', [args.gateway, '--runtime', listen, '--http', f'127.0.0.1:{http}',
                                    '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        deadline = time.monotonic() + 15
        while True:
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=0.2):
                    break
            except OSError:
                if gateway.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('Gateway did not serve /rooms')
                time.sleep(0.02)
        began = time.monotonic()
        probe = start('probe', [args.probe, '--gateway', f'127.0.0.1:{http}', '--arena', args.arena, '--output', folder,
                                '--fps', 60, '--duration', args.duration, '--clients', 4, '--create', 'true'])
        while probe.poll() is None:
            value = rss_kib(match.pid)
            if value is not None:
                rss.append((time.monotonic() - began, value))
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError('Match or Gateway exited during the session')
            time.sleep(1.0)
        if probe.returncode != 0:
            raise RuntimeError(f'Quad probe failed with code {probe.returncode}; see {folder}/probe.log')
        stop(gateway)
        if stop(match) != 0:
            raise RuntimeError('Match did not stop cleanly')
    finally:
        for process in reversed(processes):
            stop(process, 5)
        for log in logs:
            log.close()
    text = (folder / 'match.log').read_text(encoding='utf-8', errors='replace')
    windows = parse_statistics(text)
    end = re.search(r'\] end version=1 reason=(\S+) exit_code=(\d+) uptime_s=([\d.]+) authority_tick=(\d+)', text)
    if not windows or not end:
        raise RuntimeError('Match log lacks statistics or its end line')
    advance, overrun = [0] * 10, [0] * 10
    advance_max = overrun_max = stalls = wakes = 0
    window_seconds = 0.0
    identity_breaks = 0
    for fields in windows:
        a, o = bins(fields, 'advance'), bins(fields, 'tick_overrun')
        advance = [x + y for x, y in zip(advance, a)]
        overrun = [x + y for x, y in zip(overrun, o)]
        advance_max = max(advance_max, int(fields['advance_max_us']))
        overrun_max = max(overrun_max, int(fields['tick_overrun_max_us']))
        stalls += int(fields['tick_stalls'])
        window_seconds += float(fields['window_s'])
        window_wakes = int(fields['tick_deadline_wakes']) + int(fields['tick_notified'])
        wakes += window_wakes
        # The loop records a wake, an interval and an advance together (MatchRuntimeHost::Body).
        if sum(a) != window_wakes or sum(o) != window_wakes:
            identity_breaks += 1
    steady = [kib for at, kib in rss if at >= args.steady_after]
    steady.sort()
    result = {
        'session': index, 'windows': len(windows), 'advances': sum(advance), 'wakes': wakes,
        # The end line's authority_tick is not a count: IpcHost resets the runtime when the Gateway leaves.
        'identity_breaks': identity_breaks, 'window_s': round(window_seconds, 3),
        'expected_ticks': round(window_seconds * 60), 'advance_ratio': round(sum(advance) / (window_seconds * 60), 4),
        'uptime_s': float(end.group(3)),
        'advance_p50_le_us': quantile_upper_us(advance, advance_max, 0.50),
        'advance_p95_le_us': quantile_upper_us(advance, advance_max, 0.95),
        'advance_p99_le_us': quantile_upper_us(advance, advance_max, 0.99),
        'advance_max_us': advance_max, 'advance_bins': advance,
        'advance_ge_16ms': sum(advance[7:]),
        'overrun_p99_le_us': quantile_upper_us(overrun, overrun_max, 0.99), 'overrun_max_us': overrun_max,
        'overrun_bins': overrun, 'stalls': stalls,
        'rss_samples': len(rss), 'rss_steady_samples': len(steady),
        'rss_steady_median_kib': steady[len(steady) // 2] if steady else None,
        'rss_peak_kib': max((kib for _, kib in rss), default=None),
    }
    (folder / 'rss.csv').write_text(HEADER + '# kind=real-rss source=ps interval_s=1\nt_s,rss_kib\n'
                                    + ''.join(f'{at:.3f},{kib}\n' for at, kib in rss))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('mode', choices=('start', 'session'))
    parser.add_argument('--match', type=Path, required=True)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gateway', type=Path)
    parser.add_argument('--probe', type=Path, help='gyo_object_fps_pvp_quad_probe')
    parser.add_argument('--cold-starts', type=int, default=10)
    parser.add_argument('--pause', type=float, default=1.0, help='seconds between cold starts')
    parser.add_argument('--runs', type=int, default=5)
    parser.add_argument('--first', type=int, default=0, help='index of the first session (resume)')
    parser.add_argument('--duration', type=int, default=600)
    parser.add_argument('--steady-after', type=float, default=60.0, help='RSS samples after this many seconds are steady')
    parser.add_argument('--ready-timeout', type=float, default=15.0)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.mode == 'start':
        rows = cold_starts(args)
        print(json.dumps({'cold_starts': len(rows), 'ready_ms': [round(m, 3) for _, m in rows]}))
        return 0
    if not args.gateway or not args.probe:
        parser.error('session needs --gateway and --probe')
    results = []
    for index in range(args.first, args.first + args.runs):
        result = session(args, index)
        results.append(result)
        print(json.dumps({k: v for k, v in result.items() if not k.endswith('_bins')}), flush=True)
    path = args.output / f'sessions-{args.first:02d}.json'
    path.write_text(json.dumps({'format': 'gyo.kernel-baseline', 'version': 1, 'kind': 'real-session',
                                'duration_s': args.duration, 'clients': 4, 'fps': 60, 'arena': args.arena.name,
                                'sessions': results}, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
