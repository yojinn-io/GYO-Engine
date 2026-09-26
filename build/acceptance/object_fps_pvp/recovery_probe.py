"""Real worker/render recovery at fixed RTT; product-owned, no production hooks.

RTT 0/20/40 ms remains active before, during and after the render stall. This
matrix adds no jitter or loss: the separate impairment acceptance covers those.
The final released fault is therefore precisely timing.json's measured
release_ns (the render thread resumes), not a guessed proxy transition time.
"""
import argparse
import hashlib
import heapq
import json
from pathlib import Path
import socket
import subprocess
import time
import urllib.request

from backpressure_probe import analyze as analyze_recovery
from command_evidence import analyze_commands
from impaired_network import _ImpairedGateway
from run_network import free_port


class FixedRTTGateway(_ImpairedGateway):
    """Byte-preserving relay; all UDP types receive the same one-way delay."""
    def __init__(self, http_port, udp_port, rtt_ms):
        super().__init__(http_port, udp_port, rtt_ms)
        with self.lock:
            self.stats.update(one_way_jitter_ms=0, random_loss_probability=0,
                              forced_input_loss_batches_per_session=[], maximum_queued_datagrams=0,
                              rtt_retained_after_render_resume=True, control=self._new_counts(),
                              fixed_delay_applies_to_all_udp=True)

    def evidence(self):
        result = super().evidence()
        result['control_forwarded'] = result['control']['forwarded']
        return result

    def _receive(self, payload, source):
        if len(payload) < 24:
            self._fail('Truncated datagram in recovery relay')
            return
        kind = int.from_bytes(payload[6:8], 'big')
        session = int.from_bytes(payload[8:16], 'big')
        now = time.monotonic()
        with self.lock:
            if source == self.upstream_udp:
                destination = self.clients.get(session)
                if destination is None:
                    self.stats['unroutable'] += 1
                    return
            else:
                self.clients[session] = source
                destination = self.upstream_udp
            self.stats['maximum_datagram_bytes'] = max(self.stats['maximum_datagram_bytes'], len(payload))
            direction = 'snapshot' if source == self.upstream_udp and kind == 4 else (
                'input' if source != self.upstream_udp and kind == 3 else 'control')
            # Hello/Welcome share the packet sequence space with gameplay.
            # Bypassing delay for controls would create reordering even with
            # fixed RTT, spuriously invalidating queued earlier input/snapshots.
            counts = self.stats[direction]
            counts['received'] += 1
            counts['received_bytes'] += len(payload)
            counts['minimum_scheduled_delay_ms'] = self.rtt_ms/2
            counts['maximum_scheduled_delay_ms'] = self.rtt_ms/2
            self.stats['maximum_queued_datagrams'] = max(self.stats['maximum_queued_datagrams'], len(self.queue)+1)
        self.order += 1
        heapq.heappush(self.queue, (now+self.rtt_ms/2000, self.order, payload, destination, direction, now))


def run_case(args, rtt, stall):
    output = args.output/f'rtt-{rtt}-stall-{stall}ms'
    output.mkdir(parents=True, exist_ok=True)
    def fingerprint(path):
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024*1024), b''):
                digest.update(block)
        return {'path': str(path), 'sha256': digest.hexdigest()}
    manifest = {name: fingerprint(getattr(args, name))
                for name in ('match', 'gateway', 'probe', 'arena')}
    (output/'run-manifest.json').write_text(json.dumps({'artifacts': manifest, 'rtt_ms': rtt,
        'stall_ms': stall, 'fps': args.fps, 'duration': 15, 'noise': 'none',
        'rtt_retained_after_release': True}, indent=2)+'\n')
    processes, handles, proxy = [], [], None
    def start(name, command):
        handle = (output/(name+'.log')).open('w')
        handles.append(handle)
        process = subprocess.Popen([str(value) for value in command], stdout=handle, stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start('match', [args.match, '--arena', args.arena, '--listen', f'127.0.0.1:{ipc}',
                                '--movement-trace', output/'match-commands.jsonl'])
        gateway = start('gateway', [args.gateway, '--runtime', f'127.0.0.1:{ipc}', '--http', f'127.0.0.1:{http}',
                                    '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
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
        proxy = FixedRTTGateway(http, udp, rtt)
        probe = start('timing', [args.probe, '--gateway', proxy.gateway, '--arena', args.arena,
                                 '--output', output, '--duration', 15, '--fps', args.fps,
                                 '--stall-at', 5, '--stall-ms', stall])
        probe_status = probe.wait(timeout=45)
        proxy.close()
        relay = proxy.evidence()
        proxy = None
        (output/'relay.json').write_text(json.dumps(relay, indent=2)+'\n')
        gateway.terminate(); gateway.wait(timeout=10)
        match.terminate(); match.wait(timeout=10)
        if match.returncode:
            raise RuntimeError('Authority did not flush diagnostics cleanly')
        if relay['relay_error'] or relay['maximum_datagram_bytes'] > 1200 or relay['sessions'] != 2:
            raise AssertionError('Relay failed its two-player datagram contract')
        evidence = analyze_commands(output, enforce=False)
        timing = json.loads((output/'timing.json').read_text())
        release = timing['release_ns']
        if not timing['injected'] or not release:
            raise AssertionError('Requested render stall was not injected')
        # The requested final portion of the actual sleep is inside its measured
        # stall. Authority must continue ticking throughout this interval.
        recovery = analyze_recovery(output, {'start_ns': release-stall*1_000_000, 'release_ns': release})
        result = {'passed': probe_status == 0 and evidence['passed'] and recovery['passed'],
                  'rtt_ms': rtt, 'stall_ms': stall, 'release_ns': release,
                  'release_definition': 'measured render resume; fixed RTT remains active',
                  'command_evidence': evidence, 'recovery': recovery, 'relay': relay}
        (output/'recovery.json').write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps({'case': output.name, 'passed': result['passed'],
                          'recovery_start_seconds': recovery['recovery_start_seconds'],
                          'stable_new_actual_commands_per_player': recovery['stable_new_actual_commands_per_player']}), flush=True)
        if not result['passed']:
            raise AssertionError('Recovery acceptance failed; inspect recovery.json and timing.log')
        return result
    except BaseException as failure:
        (output/'failure.json').write_text(json.dumps({'passed': False, 'error': str(failure)}, indent=2)+'\n')
        raise
    finally:
        if proxy is not None:
            proxy.close()
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()
        for handle in handles:
            handle.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--rtt-ms', type=int, nargs='+', choices=(0, 20, 40), default=[0, 20, 40])
    parser.add_argument('--stall-ms', type=int, nargs='+', choices=(108, 250, 6000), default=[108, 250, 6000])
    parser.add_argument('--fps', type=int, choices=(30, 60, 144), default=60)
    args = parser.parse_args()
    for name in ('match', 'gateway', 'probe', 'arena', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    results = [run_case(args, rtt, stall) for rtt in args.rtt_ms for stall in args.stall_ms]
    (args.output/'recovery-results.json').write_text(json.dumps(results, indent=2)+'\n')


if __name__ == '__main__':
    main()
