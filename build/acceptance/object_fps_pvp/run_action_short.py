"""Bounded v5 action GUI regression with SDL-injected input: the same command on every platform.

Native OS input is not part of this runner; it stays the manual checklist of batch 04.
"""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import time
import urllib.request

from run_network import free_port, wait_for_match_ready
from run_weapon_short import digest

CASES = {'action30': ('--action-short', 30), 'action60': ('--action-short', 60),
         'action144': ('--action-short', 144), 'capture': ('--action-capture', 60)}
CAPTURES_PER_ROLE = 4
REQUIRED_CHECKS = {
    'create': ('held_fire_one_shot', 'reload-shot_suppressed', 'reload-again_suppressed', 'move_while_reloading',
               'reload_completes_and_refills', 'empty-shot_suppressed', 'empty_magazine_reload',
               'remote_death_held_then_new_life', 'remote_actions_never_replay'),
    'join': ('dead-click_suppressed', 'dead-move_suppressed', 'dead_suppresses_move_jump_fire_reload',
             'respawn_full_hp_and_magazine', 'respawned_player_can_shoot', 'remote_shot_reload_and_jump_presented',
             'remote_actions_never_replay'),
}


def summarize(directory, capture):
    """Both role reports must pass with every required check; each failure is named, never inferred."""
    roles, errors, platforms = {}, [], {}
    for role, checks in REQUIRED_CHECKS.items():
        path = directory / f'{role}-action.json'
        try:
            report = json.loads(path.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError) as error:
            errors.append(f'{path.name}: {error}')
            continue
        if not isinstance(report, dict):
            errors.append(f'{path.name}: not a JSON object')
            continue
        recorded = report.get('checks') if isinstance(report.get('checks'), dict) else {}
        missing = [name for name in checks if recorded.get(name) is not True]
        if report.get('passed') is not True:
            errors.append(f"{role}: {report.get('error') or 'probe did not pass'}")
        if missing:
            errors.append(f"{role}: missing checks {', '.join(missing)}")
        captures = report.get('captures') if isinstance(report.get('captures'), list) else []
        if capture and len(captures) != CAPTURES_PER_ROLE:
            errors.append(f'{role}: expected {CAPTURES_PER_ROLE} GPU captures, found {len(captures)}')
        platforms[role] = report.get('platform')
        roles[role] = {'passed': report.get('passed') is True and not missing, 'remote': report.get('remote')}
    return {'passed': not errors and len(roles) == len(REQUIRED_CHECKS), 'errors': errors, 'roles': roles,
            'platform': platforms}


def run_case(args, name):
    directory = args.output / name
    directory.mkdir(parents=True, exist_ok=False)
    mode, fps = CASES[name]
    ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
    processes, logs, commands = [], [], []
    result = {'passed': False, 'case': name, 'commands': commands, 'input': 'sdl_injected', 'long_run_executed': False}

    def start(label, command):
        commands.append(command)
        log = (directory / (label + '.log')).open('w')
        logs.append(log)
        process = subprocess.Popen(command, cwd=directory, stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    try:
        match = start('match', [str(args.match), '--arena', str(args.arena), '--listen', f'127.0.0.1:{ipc}'])
        wait_for_match_ready(match, directory / 'match.log', f'127.0.0.1:{ipc}')
        gateway = start('gateway', [str(args.gateway), '--runtime', f'127.0.0.1:{ipc}', '--http', f'127.0.0.1:{http}',
                                    '--udp', f'127.0.0.1:{udp}', '--advertise-ip', '127.0.0.1'])
        deadline = time.monotonic() + 10
        while True:
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError('Real service exited during startup')
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms', timeout=.2):
                    break
            except OSError:
                if time.monotonic() >= deadline:
                    raise RuntimeError('Real Gateway startup timed out')
                time.sleep(.03)
        gui = [start(role, [str(args.gui_probe), mode, '--fps', str(fps), '--arena-root', str(args.arena_root),
                            '--gateway', f'127.0.0.1:{http}', '--role', role, '--gpu-driver', args.gpu_driver,
                            '--output', str(directory)]) for role in ('create', 'join')]
        deadline = time.monotonic() + 60
        # Both roles finish their own bounded runs, so each report is saved even
        # when the other role fails: the actor signals the target either way.
        while any(process.poll() is None for process in gui):
            if time.monotonic() >= deadline:
                raise RuntimeError('Bounded action GUI deadline exceeded')
            time.sleep(.05)
        result['evidence'] = summarize(directory, mode == '--action-capture')
        result['exit_codes'] = {role: process.poll() for role, process in zip(('create', 'join'), gui)}
        result['passed'] = result['evidence']['passed'] and all(code == 0 for code in result['exit_codes'].values())
        if not result['passed']:
            result['error'] = '; '.join(result['evidence']['errors']) or 'GUI exited with failure'
    except Exception as error:
        result['error'] = str(error)
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
        result['artifacts'] = {str(path): digest(path) for path in
                               (args.match, args.gateway, args.gui_probe, args.arena,
                                args.arena_root / 'asset_catalog.json', Path(__file__).resolve())}
        (directory / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('match', 'gateway', 'gui-probe', 'arena', 'arena-root', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--gpu-driver', default='auto', choices=('auto', 'vulkan', 'd3d12', 'metal'))
    parser.add_argument('--case', action='append', choices=tuple(CASES))
    args = parser.parse_args()
    for name in ('match', 'gateway', 'gui_probe', 'arena', 'arena_root', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    for name in ('match', 'gateway', 'gui_probe', 'arena'):
        if not getattr(args, name).is_file():
            parser.error(f'Missing {name}: {getattr(args, name)}')
    if not (args.arena_root / 'asset_catalog.json').is_file():
        parser.error('Missing deployed asset catalog')
    args.output.mkdir(parents=True, exist_ok=True)
    names = args.case or list(CASES)
    results = []
    for name in names:
        result = run_case(args, name)
        results.append({'case': name, 'passed': result['passed'], 'error': result.get('error'),
                        'platform': (result.get('evidence') or {}).get('platform')})
        print(json.dumps(results[-1]), flush=True)
        if not result['passed']:
            break
    summary = {'passed': len(results) == len(names) and all(value['passed'] for value in results),
               'requested_cases': names, 'cases': results, 'input': 'sdl_injected', 'long_run_executed': False,
               'scope': 'SDL-injected functional regression; native OS input is the manual checklist'}
    (args.output / 'action-short-matrix.json').write_text(json.dumps(summary, indent=2) + '\n')
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
