"""Product-owned real-process transport blockage acceptance; no production hooks.

Host IPC: pause a byte-preserving proxy inside a frame after its first three
header bytes. Gateway: SIGSTOP only the child Gateway. Downstream: hold the most
recent UDP snapshot per peer while continuing to forward commands/keepalives.
Kernel send-buffer saturation is NOT implied by a paused transport path.
"""
import argparse
from collections import deque
import csv
import json
import math
from pathlib import Path
import signal
import socket
import subprocess
import threading
import time
import urllib.request

from impaired_network import _ImpairedGateway
from command_evidence import analyze_commands, read_trace_events, recovery_actual_intervals
from run_network import free_port


class IpcPause:
    def __init__(self, upstream_port):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(1)
        self.listener.settimeout(.1)
        self.port = self.listener.getsockname()[1]
        self.upstream = upstream_port
        self.stop = threading.Event()
        self.released = threading.Event()
        self.lock = threading.Lock()
        self.duration = None
        self.sockets = []
        self.workers = []
        self.stats = {'frames': 0, 'maximum_frame_bytes': 0, 'partial_header_bytes': 0,
                      'start_ns': 0, 'release_ns': 0, 'relay_error': None}
        self.thread = threading.Thread(target=self._accept, name='pvp-backpressure-ipc')
        self.thread.start()

    def arm(self, duration):
        with self.lock:
            self.duration = duration

    def _accept(self):
        try:
            while not self.stop.is_set():
                try:
                    downstream, _ = self.listener.accept()
                    break
                except socket.timeout:
                    continue
            else:
                return
            upstream = None
            deadline = time.monotonic() + 5
            while upstream is None and not self.stop.is_set():
                try:
                    upstream = socket.create_connection(('127.0.0.1', self.upstream), timeout=.2)
                except OSError:
                    if time.monotonic() > deadline:
                        raise
                    time.sleep(.02)
            if upstream is None:
                downstream.close()
                return
            upstream.settimeout(None)
            upstream.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            downstream.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.sockets = [downstream, upstream]
            forward = threading.Thread(target=self._commands, args=(downstream, upstream), name='pvp-backpressure-command')
            self.workers.append(forward)
            forward.start()
            while not self.stop.is_set():
                header = self._read(upstream, 4)
                length = int.from_bytes(header, 'big')
                if not 1 <= length <= 65536:
                    raise ValueError('IPC frame length corrupted')
                payload = self._read(upstream, length)
                if payload[:2] != b'\x08\x03':
                    raise ValueError('IPC envelope lost protocol v3')
                frame = header + payload
                with self.lock:
                    self.stats['frames'] += 1
                    self.stats['maximum_frame_bytes'] = max(self.stats['maximum_frame_bytes'], len(frame))
                    duration, self.duration = self.duration, None
                if duration is not None:
                    downstream.sendall(frame[:3])
                    with self.lock:
                        self.stats['partial_header_bytes'] = 3
                        self.stats['start_ns'] = time.monotonic_ns()
                    self.stop.wait(duration)
                    # Fragment the rest too: both decoders must preserve framing.
                    for offset in range(3, len(frame), 7):
                        downstream.sendall(frame[offset:offset+7])
                    with self.lock:
                        self.stats['release_ns'] = time.monotonic_ns()
                    self.released.set()
                else:
                    downstream.sendall(frame)
        except EOFError:
            pass
        except Exception as failure:
            if not self.stop.is_set():
                with self.lock:
                    self.stats['relay_error'] = str(failure)

    @staticmethod
    def _read(conn, size):
        result = bytearray()
        while len(result) < size:
            chunk = conn.recv(size-len(result))
            if not chunk:
                raise EOFError()
            result.extend(chunk)
        return bytes(result)

    def _commands(self, source, destination):
        try:
            while not self.stop.is_set():
                chunk = source.recv(65536)
                if not chunk:
                    break
                destination.sendall(chunk)
        except OSError as failure:
            if not self.stop.is_set():
                with self.lock:
                    self.stats['relay_error'] = str(failure)

    def evidence(self):
        with self.lock:
            return dict(self.stats)

    def close(self):
        self.stop.set()
        self.listener.close()
        for conn in self.sockets:
            try:
                conn.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            conn.close()
        self.thread.join(timeout=5)
        for worker in self.workers:
            worker.join(timeout=5)
        if self.thread.is_alive() or any(worker.is_alive() for worker in self.workers):
            raise RuntimeError('IPC proxy worker did not terminate')


class DownstreamPause(_ImpairedGateway):
    def __init__(self, http_port, udp_port):
        self.hold_duration = None
        self.hold_until = None
        self.held = {}
        self.released = threading.Event()
        self.fault = {'start_ns': 0, 'release_ns': 0, 'maximum_held_datagrams': 0,
                      'coalesced_snapshots': 0, 'maximum_datagram_bytes': 0}
        super().__init__(http_port, udp_port, 0)

    def arm(self, duration):
        with self.lock:
            self.hold_duration = duration

    def _receive(self, payload, source):
        if len(payload) < 24:
            self._fail('Truncated datagram in downstream relay')
            return
        kind = int.from_bytes(payload[6:8], 'big')
        session = int.from_bytes(payload[8:16], 'big')
        now = time.monotonic()
        with self.lock:
            if source == self.upstream_udp:
                destination = self.clients.get(session)
                if destination is None:
                    return
            else:
                self.clients[session] = source
                destination = self.upstream_udp
            self.fault['maximum_datagram_bytes'] = max(self.fault['maximum_datagram_bytes'], len(payload))
            if self.hold_until is not None and now >= self.hold_until:
                for pending, peer in self.held.values():
                    self.udp.sendto(pending, peer)
                self.held.clear()
                self.hold_until = None
                self.fault['release_ns'] = time.monotonic_ns()
                self.released.set()
            if source == self.upstream_udp and kind == 4:
                if self.hold_duration is not None:
                    self.hold_until = now + self.hold_duration
                    self.hold_duration = None
                    self.fault['start_ns'] = time.monotonic_ns()
                if self.hold_until is not None:
                    if session in self.held:
                        self.fault['coalesced_snapshots'] += 1
                    self.held[session] = (payload, destination)
                    self.fault['maximum_held_datagrams'] = max(self.fault['maximum_held_datagrams'], len(self.held))
                    return
            self.udp.sendto(payload, destination)

    def evidence(self):
        with self.lock:
            return dict(self.fault, relay_error=self.error)


def analyze(output, fault):
    start, release = fault['start_ns'], fault['release_ns']
    if not start or release <= start:
        raise AssertionError('Requested blockage was not applied/released')
    events = list(read_trace_events(output/'match-commands.jsonl'))
    tick_times = {}
    for event in events:
        if event['kind'] == 'snapshot_produced' and start <= event['time_ns'] < release:
            tick_times.setdefault(event['authority_tick'], event['time_ns'])
    ticks = sorted(tick_times)
    times = sorted(tick_times.values())
    boundaries = [start, *times, release]
    maximum_gap = max(b-a for a, b in zip(boundaries, boundaries[1:]))/1e9
    if maximum_gap >= .1:
        raise AssertionError(f'Authority experienced a {maximum_gap:.3f}s gap during the blocked path')
    expected = (release-start)/1e9*60
    if len(ticks) < max(1, math.floor(expected)-3):
        raise AssertionError(f'Authority stalled during transport blockage: {len(ticks)} ticks; expected about {expected:.1f}')
    with (output/'frames.csv').open() as stream:
        frames = list(csv.DictReader(stream))
    client_events = list(read_trace_events(output/'clients-commands.jsonl'))
    generated = {}
    for event in client_events:
        if event['kind'] != 'generated' or event['seeded_neutral']:
            continue
        identity = (event['player_id'], event['epoch'], event['sequence'])
        if identity in generated:
            raise AssertionError('Duplicate generated command identity')
        generated[identity] = event['time_ns']
    player_ids = sorted({key[0] for key in generated})
    if len(player_ids) != 2:
        raise AssertionError('Both players must generate real commands')
    resolutions = sorted((event for event in events if event['kind'] == 'resolved'),
                         key=lambda event: event['time_ns'])
    identities = {(event['player_id'], event['epoch'], event['sequence']) for event in resolutions}
    if len(identities) != len(resolutions):
        raise AssertionError('Duplicate resolved command identity')
    # One strict authority interval cannot contain Held/Neutral, an epoch
    # transition, a missing step, or an Actual unrelated to a fresh command.
    # Intersect both players' intervals with continuous healthy render frames.
    intervals = recovery_actual_intervals(resolutions, generated, player_ids, release)
    previous_frame_ns = 0
    for frame in frames:
        timestamp = int(frame['time_ns'])
        if timestamp <= previous_frame_ns:
            raise AssertionError('Frame timestamps are not increasing')
        previous_frame_ns = timestamp
        for field in ('pending_a', 'pending_b', 'queued_a', 'queued_b'):
            value = int(frame[field])
            if value < 0 or value > (12 if field.startswith('pending') else 32):
                raise AssertionError('Application/authority command queue exceeded bound')
        if any(not math.isfinite(float(frame[field])) or float(frame[field]) < 0
               for field in ('frame_seconds', 'remote_age_a', 'remote_age_b')):
            raise AssertionError('Invalid frame duration or snapshot age')
    candidates = []
    for first in intervals[player_ids[0]]:
        for second in intervals[player_ids[1]]:
            begin = max(first['start_ns'], second['start_ns'])
            end = min(first['end_ns'], second['end_ns'])
            if begin <= release+1_500_000_000 and end-begin >= 250_000_000:
                candidates.append((begin, end, first['epoch'], second['epoch']))
    stable_start = recovered = None
    stable_epochs = None
    for begin, end, epoch_a, epoch_b in sorted(candidates):
        stable_start = previous = None
        for frame in frames:
            timestamp = int(frame['time_ns'])
            if timestamp < begin:
                continue
            if timestamp > end:
                break
            healthy = max(int(frame['pending_a']), int(frame['pending_b'])) < 12 and \
                max(int(frame['queued_a']), int(frame['queued_b'])) <= 3 and \
                max(float(frame['remote_age_a']), float(frame['remote_age_b'])) < .1 and \
                int(frame['epoch_a']) == epoch_a and int(frame['epoch_b']) == epoch_b
            if not healthy or (previous is not None and timestamp-previous >= 100_000_000):
                stable_start = None
            elif stable_start is None:
                stable_start = timestamp
            if stable_start is not None and timestamp-stable_start >= 250_000_000 and \
                    stable_start <= release+1_500_000_000:
                recovered = timestamp
                stable_epochs = {player_ids[0]: epoch_a, player_ids[1]: epoch_b}
                break
            previous = timestamp
        if recovered is not None:
            break
    if recovered is None:
        raise AssertionError('Did not begin uninterrupted fresh-Actual recovery within 1.5 s and remain stable for 250 ms')
    actual_counts = {player: sum(event['player_id'] == player and event['epoch'] == stable_epochs[player]
                                and stable_start <= event['time_ns'] <= recovered
                                for event in resolutions) for player in player_ids}
    queue_history, stable_queue_max = {}, 0
    for event in resolutions:
        identity = (event['player_id'], event['epoch'])
        history = queue_history.setdefault(identity, deque(maxlen=30))
        history.append(event['queued'])
        if stable_start <= event['time_ns'] <= recovered:
            stable_queue_max = max(stable_queue_max, sum(history))
    return {'passed': True, 'authority_ticks_during_blockage': len(ticks),
            'maximum_authority_gap_seconds': maximum_gap,
            'blockage_seconds': (release-start)/1e9, 'recovery_with_stability_seconds': (recovered-release)/1e9,
            'recovery_requires_actual_execution': True,
            'recovery_requires_uninterrupted_fresh_actual': True,
            'recovery_start_seconds': (stable_start-release)/1e9,
            'stable_new_actual_commands_per_player': actual_counts,
            'stable_queue_30_tick_sum_max': stable_queue_max,
            'application_pending_bound': 12, 'authority_future_bound': 32,
            'ipc_kernel_saturation_proven': False,
            'partial_ipc_header_exercised': fault.get('partial_header_bytes', 0) == 3}


def run_case(args, layer, milliseconds):
    output = args.output/f'{layer}-{milliseconds}ms'
    output.mkdir(parents=True, exist_ok=True)
    processes, logs = [], []
    proxy = gateway = match = None
    stopped = False
    def start(name, command):
        log = (output/(name+'.log')).open('w')
        logs.append(log)
        process = subprocess.Popen([str(part) for part in command], stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start('match', [args.match, '--arena', args.arena, '--listen', f'127.0.0.1:{ipc}',
                                '--movement-trace', output/'match-commands.jsonl'])
        runtime_port = ipc
        if layer == 'host-ipc':
            proxy = IpcPause(ipc)
            runtime_port = proxy.port
        gateway = start('gateway', [args.gateway, '--runtime', f'127.0.0.1:{runtime_port}',
                                    '--http', f'127.0.0.1:{http}', '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        deadline = time.monotonic()+15
        while True:
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError('Service startup failed; inspect case logs')
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=.3):
                    break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(.05)
        address = f'127.0.0.1:{http}'
        if layer == 'downstream':
            proxy = DownstreamPause(http, udp)
            address = proxy.gateway
        client = start('timing', [args.probe, '--gateway', address, '--arena', args.arena,
                                  '--output', output, '--duration', 8, '--fps', 60])
        time.sleep(5)
        if client.poll() is not None:
            raise RuntimeError('Timing probe exited before fault injection')
        if layer == 'gateway':
            fault = {'start_ns': time.monotonic_ns(), 'relay_error': None}
            gateway.send_signal(signal.SIGSTOP)
            stopped = True
            time.sleep(milliseconds/1000)
            gateway.send_signal(signal.SIGCONT)
            stopped = False
            fault['release_ns'] = time.monotonic_ns()
        else:
            proxy.arm(milliseconds/1000)
            if not proxy.released.wait(milliseconds/1000+3):
                raise RuntimeError('Transport fault did not release')
            fault = proxy.evidence()
        if client.wait(timeout=30):
            raise RuntimeError('Timing probe failed; inspect timing.log')
        if proxy is not None:
            fault = proxy.evidence()
            proxy.close()
            proxy = None
        gateway.terminate(); gateway.wait(timeout=10)
        match.terminate(); match.wait(timeout=10)
        if match.returncode:
            raise RuntimeError('Authority did not flush diagnostics cleanly')
        if fault.get('relay_error'):
            raise AssertionError(fault['relay_error'])
        if fault.get('maximum_frame_bytes', 0) > 65540 or fault.get('maximum_held_datagrams', 0) > 2 or fault.get('maximum_datagram_bytes', 0) > 1200:
            raise AssertionError('Fault relay exceeded its bounded storage/packet contract')
        result = dict(analyze(output, fault), layer=layer, requested_ms=milliseconds, fault=fault)
        result['command_evidence'] = analyze_commands(output, enforce=False)
        result['passed'] = result['passed'] and result['command_evidence']['passed']
        (output/'backpressure.json').write_text(json.dumps(result, indent=2)+'\n')
        if not result['passed']:
            raise AssertionError('Command-stage evidence failed; inspect backpressure.json')
        print(json.dumps({'case': output.name, **{key: result[key] for key in ('passed', 'authority_ticks_during_blockage', 'recovery_with_stability_seconds')}}), flush=True)
        return result
    except BaseException as failure:
        (output/'failure.json').write_text(json.dumps({'passed': False, 'error': str(failure)}, indent=2)+'\n')
        raise
    finally:
        if stopped and gateway is not None and gateway.poll() is None:
            gateway.send_signal(signal.SIGCONT)
        if proxy is not None:
            proxy.close()
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()
        for log in logs:
            log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--layers', nargs='+', choices=('host-ipc', 'gateway', 'downstream'), default=['host-ipc', 'gateway', 'downstream'])
    parser.add_argument('--durations-ms', nargs='+', type=int, choices=(250, 1000), default=[250, 1000])
    args = parser.parse_args()
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    if not hasattr(signal, 'SIGSTOP') and 'gateway' in args.layers:
        parser.error('Gateway process-stall acceptance requires POSIX SIGSTOP/SIGCONT')
    results = [run_case(args, layer, duration) for layer in args.layers for duration in args.durations_ms]
    (args.output/'backpressure-results.json').write_text(json.dumps(results, indent=2)+'\n')


if __name__ == '__main__':
    main()
