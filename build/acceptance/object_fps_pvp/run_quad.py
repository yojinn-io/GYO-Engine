"""Room-capacity acceptance runner: full rooms, a GUI with bots and mixed-version refusals.

Cases (pv6 contract §1, batch 16):
  clean-60, clean-30  four headless Clients in one room for 16 s; a fifth join is refused.
                      clean-30 is recorded, not judged (D21).
  gui-bots            the GUI participant creates the room and three bots fill it.
  mixed               an older Client, Gateway or Match against this one fails explicitly;
                      needs the --base-* artifacts of the previous capacity.
Every segment starts its own Match and Gateway and first asserts GET /rooms shows no player.
The two-Client runners and analyzers are unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import time
import urllib.request

from acceptance_capacity import MAX_PLAYERS
from acceptance_util import PROTOCOL_VERSION
from quad_evidence import analyze_quad
from run_network import free_port, wait_for_match_ready

CASES = {
    'clean-60': {'fps': 60, 'duration': 16, 'judged': True},
    'clean-30': {'fps': 30, 'duration': 16, 'judged': False},
    'gui-bots': {'fps': 60, 'duration': 30, 'judged': True},
}
BOTS_OUTLAST_GUI_SECONDS = 2
STARTUP_SECONDS = 10


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Services:
    """The processes and logs of one segment; everything is stopped on exit."""

    def __init__(self, output):
        self.output, self.processes, self.logs = output, [], []

    def start(self, label, command):
        log = (self.output / (label + '.log')).open('w')
        self.logs.append(log)
        process = subprocess.Popen([str(part) for part in command], stdout=log, stderr=subprocess.STDOUT)
        self.processes.append(process)
        return process

    def close(self):
        for process in reversed(self.processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        for log in self.logs:
            log.close()


def rooms(http, processes):
    """GET /rooms once the Gateway serves it."""
    until = time.monotonic() + STARTUP_SECONDS
    while True:
        if any(process.poll() is not None for process in processes):
            raise RuntimeError('Service exited during startup')
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=.2) as response:
                return json.loads(response.read())
        except OSError:
            if time.monotonic() >= until:
                raise
            time.sleep(.01)


def assert_no_players(listing):
    # The listing is a bare list or {"rooms": [...]}; each room reports its players.
    entries = listing.get('rooms', []) if isinstance(listing, dict) else listing
    occupied = [room for room in entries if room.get('players', 0) != 0]
    if occupied:
        raise RuntimeError(f'Segment started with players already in a room: {occupied}')


def exited(process, seconds):
    try:
        return process.wait(timeout=seconds)
    except subprocess.TimeoutExpired:
        return None


def run_segment(args, case, output):
    """One full-room segment: Match, Gateway, then the probe (and the GUI participant for gui-bots)."""
    output.mkdir(parents=True, exist_ok=False)
    spec = CASES[case]
    gui = case == 'gui-bots'
    binaries = {'match': args.match, 'gateway': args.gateway, 'probe': args.probe, 'arena': args.arena}
    if gui:
        binaries['gui_probe'] = args.gui_probe
    (output / 'artifacts.json').write_text(json.dumps(
        {name: {'path': str(path), 'sha256': sha256(path)} for name, path in binaries.items()}, indent=2) + '\n')
    services = Services(output)
    start = services.start
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start('match', [args.match, '--arena', args.arena, '--listen', f'127.0.0.1:{ipc}',
                                '--movement-trace', output / 'match-commands.jsonl'])
        wait_for_match_ready(match, output / 'match.log', f'127.0.0.1:{ipc}')
        gateway = start('gateway', [args.gateway, '--runtime', f'127.0.0.1:{ipc}', '--http', f'127.0.0.1:{http}',
                                    '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        assert_no_players(rooms(http, [match, gateway]))
        address = f'127.0.0.1:{http}'
        probe_command = [args.probe, '--gateway', address, '--arena', args.arena, '--output', output, '--fps', spec['fps']]
        participant = None
        if gui:
            participant = start('gui', [args.gui_probe, '--arena-root', args.arena_root, '--gateway', address,
                                        '--output', output, '--duration', spec['duration'], '--fps', spec['fps'],
                                        '--gpu-driver', args.gpu_driver])
            until = time.monotonic() + 20
            while not (output / 'gui-ready.json').exists():
                if participant.poll() is not None or time.monotonic() >= until:
                    raise RuntimeError('The GUI participant did not create a room')
                time.sleep(.01)
            probe = start('probe', probe_command + ['--duration', spec['duration'] + BOTS_OUTLAST_GUI_SECONDS,
                                                    '--clients', MAX_PLAYERS - 1, '--create', 'false',
                                                    '--expect-players', MAX_PLAYERS])
        else:
            probe = start('probe', probe_command + ['--duration', spec['duration'], '--clients', MAX_PLAYERS,
                                                    '--create', 'true', '--fifth', 'true'])
        if exited(probe, spec['duration'] + 60) != 0:
            raise RuntimeError('Quad probe failed; see probe.log and client-failure.json')
        if participant and exited(participant, 60) != 0:
            raise RuntimeError('GUI participant failed; see gui.log')
        gateway.terminate()
        gateway.wait(timeout=5)
        match.terminate()
        match.wait(timeout=5)
        if match.returncode:
            raise RuntimeError('Match did not flush its diagnostic trace cleanly')
        result = analyze_quad(output, judged=spec['judged'])
    except Exception as failure:  # The failed segment is kept with its reason.
        result = {'passed': False, 'judged': spec['judged'], 'errors': [str(failure)]}
    finally:
        services.close()
    result['case'] = case
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def refused_client(args, output):
    """A Client of the previous capacity loads its own arena; this Match's arena differs."""
    output.mkdir(parents=True, exist_ok=False)
    services = Services(output)
    start = services.start
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start('match', [args.match, '--arena', args.arena, '--listen', f'127.0.0.1:{ipc}'])
        wait_for_match_ready(match, output / 'match.log', f'127.0.0.1:{ipc}')
        gateway = start('gateway', [args.gateway, '--runtime', f'127.0.0.1:{ipc}', '--http', f'127.0.0.1:{http}',
                                    '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        assert_no_players(rooms(http, [match, gateway]))
        probe = start('probe', [args.base_probe, '--gateway', f'127.0.0.1:{http}', '--arena', args.base_arena,
                                '--output', output, '--duration', 6, '--gameplay-v5', 'true', '--fps', 60])
        code = exited(probe, 30)
        failure = output / 'client-failure.json'
        message = json.loads(failure.read_text()).get('error', '') if failure.exists() else ''
        return {'name': 'older-client', 'passed': code not in (None, 0) and message.startswith('arena_content_mismatch'),
                'exit_code': code, 'error': message}
    finally:
        services.close()


def refused_gateway(args, output, *, match_binary, match_arena, gateway_binary, name):
    """A Gateway and a Match of different capacities: readiness fails and the Gateway exits."""
    output.mkdir(parents=True, exist_ok=False)
    services = Services(output)
    start = services.start
    try:
        ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
        match = start('match', [match_binary, '--arena', match_arena, '--listen', f'127.0.0.1:{ipc}'])
        wait_for_match_ready(match, output / 'match.log', f'127.0.0.1:{ipc}')
        gateway = start('gateway', [gateway_binary, '--runtime', f'127.0.0.1:{ipc}', '--http', f'127.0.0.1:{http}',
                                    '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        code = exited(gateway, STARTUP_SECONDS)
        log = (output / 'gateway.log').read_text(errors='replace')
        return {'name': name, 'passed': code not in (None, 0) and 'runtime readiness contract mismatch' in log,
                'exit_code': code}
    finally:
        services.close()


def refused_arena(args, output):
    """This Match refuses the previous arena: it has fewer spawns than the room holds."""
    output.mkdir(parents=True, exist_ok=False)
    services = Services(output)
    try:
        match = services.start('match', [args.match, '--arena', args.base_arena, '--listen', f'127.0.0.1:{free_port()}'])
        code = exited(match, STARTUP_SECONDS)
        log = (output / 'match.log').read_text(errors='replace')
        return {'name': 'older-arena', 'exit_code': code,
                'passed': code not in (None, 0) and f'a room of {MAX_PLAYERS} players needs at least that many' in log}
    finally:
        services.close()


def run_mixed(args, output):
    output.mkdir(parents=True, exist_ok=False)
    binaries = {name: getattr(args, name) for name in ('match', 'gateway', 'arena', 'base_match', 'base_gateway',
                                                       'base_probe', 'base_arena')}
    (output / 'artifacts.json').write_text(json.dumps(
        {name: {'path': str(path), 'sha256': sha256(path)} for name, path in binaries.items()}, indent=2) + '\n')
    checks = []
    for name, run in (
            ('older-client', lambda d: refused_client(args, d)),
            ('older-match', lambda d: refused_gateway(args, d, match_binary=args.base_match, match_arena=args.base_arena,
                                                      gateway_binary=args.gateway, name='older-match')),
            ('older-gateway', lambda d: refused_gateway(args, d, match_binary=args.match, match_arena=args.arena,
                                                        gateway_binary=args.base_gateway, name='older-gateway')),
            ('older-arena', lambda d: refused_arena(args, d))):
        try:
            checks.append(run(output / name))
        except Exception as failure:
            checks.append({'name': name, 'passed': False, 'error': str(failure)})
    result = {'case': 'mixed', 'judged': True, 'passed': all(check['passed'] for check in checks), 'checks': checks,
              'errors': [check['name'] for check in checks if not check['passed']]}
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ('match', 'gateway', 'arena', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--probe', type=Path, help='quad probe (full-room and gui-bots cases)')
    parser.add_argument('--case', choices=(*CASES, 'mixed'), required=True)
    parser.add_argument('--rounds', type=int, default=1)
    parser.add_argument('--gui-probe', type=Path)
    parser.add_argument('--arena-root', type=Path)
    parser.add_argument('--gpu-driver', default='auto')
    for name in ('base-match', 'base-gateway', 'base-probe', 'base-arena'):
        parser.add_argument('--' + name, type=Path, help='previous-capacity artifact (mixed case)')
    args = parser.parse_args()
    needed = {'mixed': ('match', 'gateway', 'arena', 'base_match', 'base_gateway', 'base_probe', 'base_arena'),
              'gui-bots': ('match', 'gateway', 'arena', 'probe', 'gui_probe')}.get(args.case, ('match', 'gateway', 'arena', 'probe'))
    for name in needed:
        path = getattr(args, name)
        if path is None or not path.resolve().is_file():
            parser.error('Missing ' + name)
        setattr(args, name, path.resolve())
    if args.case == 'gui-bots' and (args.arena_root is None or not args.arena_root.is_dir()):
        parser.error('gui-bots needs --arena-root')
    if args.arena_root:
        args.arena_root = args.arena_root.resolve()
    if args.rounds < 1 or (args.case == 'mixed' and args.rounds != 1):
        parser.error('--rounds must be positive (one for mixed)')
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / 'planned.json').write_text(json.dumps({
        'protocol': PROTOCOL_VERSION, 'capacity': MAX_PLAYERS, 'case': args.case, 'rounds': args.rounds,
        'spec': CASES.get(args.case), 'fail_fast': False, 'long_run': False}, indent=2) + '\n')
    results = []
    for round_number in range(1, args.rounds + 1):
        if args.case == 'mixed':
            result = run_mixed(args, args.output / 'mixed')
        else:
            result = run_segment(args, args.case, args.output / f'{args.case}-{round_number}')
        results.append(result)
        print(json.dumps({'case': args.case, 'round': round_number, 'passed': result['passed'],
                          'judged': result['judged'], 'errors': result['errors']}), flush=True)
    summary = {'case': args.case, 'rounds': len(results), 'judged': results[0]['judged'],
               'passed': all(result['passed'] for result in results), 'results': [
                   {'round': n, 'passed': r['passed'], 'errors': r['errors']} for n, r in enumerate(results, 1)]}
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    # A recorded-only case reports its outcome without deciding the exit status.
    return 0 if summary['passed'] or not summary['judged'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
