"""Bounded v5 action GUI regression with SDL-injected input: the same command on every platform.

Besides the v5 actions it owns the first-person weapon checks of the retired weapon short probe:
local feedback on the first successful presentation, an authority cooldown rejection, wall and hit
decisions, one mouse delta per combined frame, window interactions without ghost shots, ESC and
rejoin. Native OS input is not part of this runner; it stays the manual checklist of batch 04.

The rapid-* cases (v7 batch 09 step S5) are record-only rapid-fire runs for the C2 comparison: SDL-injected
clicks through the product's own local fire gate, one per-click record each. They run only when named with
--case; the default matrix is the action short above.
"""
import argparse
import json
import math
from pathlib import Path
import signal
import socket
import subprocess
import time
import urllib.request

from run_network import free_port, steady_clock_ns, wait_for_match_ready
from gui_bots import PassiveBots, add_arguments as add_bot_arguments, check_arguments as check_bot_arguments
from acceptance_util import digest

CASES = {'action30': ('--action-short', 30), 'action60': ('--action-short', 60),
         'action144': ('--action-short', 144), 'capture': ('--action-capture', 60)}
# Rapid-fire record scenarios (gui_main.cpp --action-rapid): intervalN clicks every N authority ticks,
# edge clicks every frame for 5 s, pause clicks every frame for 6 s after a Gateway pause (non-clean).
RAPID_SCENARIOS = ('interval12', 'edge', 'interval10', 'interval11', 'interval13', 'pause')
RAPID_CASES = {f'rapid-{scenario}-{fps}': (scenario, fps) for scenario in RAPID_SCENARIOS for fps in (30, 60, 144)}
RAPID_PAUSE_SECONDS = .25
COOLDOWN_REJECTION = 3  # ShotRejection::Cooldown (Combat.hpp)
# The action short's create role deliberately submits one request inside the authority cooldown (action_short.hpp
# 'held-shot'; its report names it in cooldown_injection_action_id). A Cooldown judge over action short records
# must exclude that one rejection. The rapid cases inject none, so their excluded_injections is empty.
CAPTURES_PER_ROLE = {'create': 9, 'join': 5}
REQUIRED_CHECKS = {
    'create': ('held_fire_one_shot', 'reload-shot_suppressed', 'reload-again_suppressed', 'move_while_reloading',
               'reload_completes_and_refills', 'empty-shot_suppressed', 'empty_magazine_reload',
               'remote_death_held_then_new_life', 'remote_actions_never_replay',
               'movement_turn_shoot_mouse_once', 'mouse_delta_not_replayed', 'focus-loss_suppressed',
               'focus-gain-held_suppressed', 'tab-release_suppressed', 'drag_suppressed',
               'authority_cooldown_wall_and_hit', 'local_feedback_before_decision', 'escape_clears_weapon',
               'rejoin_full_hp_no_stale_effects'),
    'join': ('dead-click_suppressed', 'dead-move_suppressed', 'dead_suppresses_move_jump_fire_reload',
             'respawn_full_hp_and_magazine', 'respawned_player_can_shoot', 'remote_shot_reload_and_jump_presented',
             'remote_actions_never_replay'),
}


def frame_errors(role, report, frames):
    """Per-frame first-person evidence: a dead local player never presents a weapon mesh, and every
    SDL shot has one finite submission-to-first-successful-Presented duration."""
    errors = []
    for frame in frames:
        weapon = frame.get('presented_weapon') if frame.get('presented') else None
        if isinstance(weapon, dict) and weapon.get('dead') is True and (weapon.get('submitted_meshes') or 0) > 0:
            errors.append(f"{role}: frame {frame.get('frame_id')} presented first-person weapon meshes while dead")
            break
    delays = [frame['submission_to_presented_seconds'] for frame in frames if 'submission_to_presented_seconds' in frame]
    if report.get('passed') is True and len(delays) != report.get('injected_sdl_shots'):
        errors.append(f"{role}: {len(delays)} shot(s) reached a successful presentation, "
                      f"{report.get('injected_sdl_shots')} were injected")
    if any(not isinstance(delay, (int, float)) or not math.isfinite(delay) or delay < 0 for delay in delays):
        errors.append(f'{role}: invalid submitted-shot-to-Presented duration')
    return errors


def read_frames(path):
    try:
        return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines() if line.strip()]
    except (OSError, json.JSONDecodeError):
        return None


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
        if report.get('disturbed') is True:
            errors.append(f"{role}: disturbed by external input ({report.get('unscheduled_yaw_frames')} frame(s) of "
                          'unscheduled mouse motion); not a product verdict')
        if report.get('passed') is not True:
            errors.append(f"{role}: {report.get('error') or 'probe did not pass'}")
        if missing:
            errors.append(f"{role}: missing checks {', '.join(missing)}")
        captures = report.get('captures') if isinstance(report.get('captures'), list) else []
        if capture and len(captures) != CAPTURES_PER_ROLE[role]:
            errors.append(f'{role}: expected {CAPTURES_PER_ROLE[role]} GPU captures, found {len(captures)}')
        frames = read_frames(directory / f'{role}-action-frames.jsonl')
        if frames is None:
            errors.append(f'{role}-action-frames.jsonl: missing or malformed')
        else:
            errors.extend(frame_errors(role, report, frames))
        platforms[role] = report.get('platform')
        roles[role] = {'passed': report.get('passed') is True and not missing, 'remote': report.get('remote'),
                       'disturbed': report.get('disturbed') is True}
    disturbed = [role for role in REQUIRED_CHECKS if role in roles and roles[role].get('disturbed')]
    return {'passed': not errors and len(roles) == len(REQUIRED_CHECKS), 'errors': errors, 'roles': roles,
            'platform': platforms, 'disturbed': disturbed}


def rapid_counts(clicks):
    """Per-click tallies of one actor record. A verdict is exact when the product's newest decision named it or
    the frame's counters proved every other drained decision accepted; otherwise it is ambiguous."""
    sent = [click for click in clicks if click.get('outcome') == 'sent']
    verdicts = [click.get('verdict') for click in sent]
    states = [verdict.get('state') if isinstance(verdict, dict) else 'missing' for verdict in verdicts]
    within = [click for click in clicks if click.get('previous_sent_action') is not None and click.get('segment_click', 0) > 0]
    within_sent = [click for click in within if click.get('outcome') == 'sent']
    return {
        'clicks': len(clicks), 'sent': len(sent),
        'local_blocks': sum(click.get('outcome') == 'local_block' for click in clicks),
        'neither': sum(click.get('outcome') == 'none' for click in clicks),
        'accepted': states.count('accepted'), 'rejected': states.count('rejected'),
        'ambiguous': states.count('ambiguous'), 'missing': states.count('missing'),
        'cooldown_rejections_attributed': sum(isinstance(verdict, dict) and verdict.get('rejection') == COOLDOWN_REJECTION
                                              for verdict in verdicts),
        # Whether the previous submitted shot's decision had reached the client when the gate saw this click,
        # inside one segment (the first click after a reload always follows a decided shot).
        'previous_verdict_arrived': {'eligible': len(within),
                                     'arrived': sum(click.get('previous_verdict_arrived') is True for click in within)},
        'previous_verdict_arrived_sent': {'eligible': len(within_sent),
                                          'arrived': sum(click.get('previous_verdict_arrived') is True
                                                         for click in within_sent)},
    }


def summarize_rapid(directory, scenario):
    """Record-only: the run completed when both roles finished their bounded schedule undisturbed. The authority
    Cooldown count is reported, never judged here; the rapid mode injects no rejection, so nothing is excluded."""
    errors, roles = [], {}
    for role in ('create', 'join'):
        path = directory / f'{role}-rapid.json'
        try:
            report = json.loads(path.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError) as error:
            errors.append(f'{path.name}: {error}')
            continue
        if not isinstance(report, dict):
            errors.append(f'{path.name}: not a JSON object')
            continue
        if report.get('disturbed') is True:
            errors.append(f"{role}: disturbed by external input ({report.get('unscheduled_yaw_frames')} frame(s) of "
                          'unscheduled mouse motion); not a product record')
        if report.get('passed') is not True:
            errors.append(f"{role}: {report.get('error') or 'probe did not complete'}")
        roles[role] = report
    actor = roles.get('create') or {}
    clicks = read_frames(directory / 'create-rapid-clicks.jsonl')
    if clicks is None:
        errors.append('create-rapid-clicks.jsonl: missing or malformed')
    counts = rapid_counts(clicks or [])
    product = actor.get('counts') if isinstance(actor.get('counts'), dict) else {}
    counts['authority_cooldown_rejections'] = product.get('authority_cooldown_rejections')
    counts['product_local_cooldown_blocks'] = product.get('local_cooldown_blocks')
    counts['player_hits'] = product.get('player_hits')
    counts['attribution_mismatches'] = product.get('attribution_mismatches')
    if clicks is not None and product and product.get('local_cooldown_blocks') != counts['local_blocks']:
        errors.append('create: per-click local blocks do not equal the product counter')
    return {'passed': not errors and len(roles) == 2, 'errors': errors, 'scenario': scenario,
            'clean': scenario != 'pause', 'record_only': True, 'excluded_injections': [],
            'counts': counts, 'platform': {role: report.get('platform') for role, report in roles.items()},
            'disturbed': [role for role, report in roles.items() if report.get('disturbed') is True]}


def stop_in_order(processes, gateway, match, match_log, timeout=3, close_wait=2):
    """Stops the clients first, then the Gateway, and the Match last once its log shows the IPC session ended (the
    Gateway's close, read as EOF), at most close_wait seconds. A Match stopped while the Gateway is still connected
    ends that session without reading the Gateway's last frames (action_probe.py stops the Gateway before the Match
    too); stopping every process at once made that a race."""
    def halt(process):
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=timeout)
    for process in reversed(processes):
        if process is not gateway and process is not match:
            halt(process)
    if gateway is not None:
        halt(gateway)
        deadline = time.monotonic() + close_wait
        while match is not None and match.poll() is None and time.monotonic() < deadline:
            try:
                if 'ipc closed reason=' in Path(match_log).read_text(errors='replace'):
                    break
            except OSError:
                pass
            time.sleep(.02)
    if match is not None:
        halt(match)


def run_case(args, name):
    directory = args.output / name
    directory.mkdir(parents=True, exist_ok=False)
    rapid = RAPID_CASES.get(name)
    if rapid:
        scenario, fps = rapid
        mode_arguments = ['--action-rapid', scenario]
    else:
        mode, fps = CASES[name]
        mode_arguments = [mode]
    ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
    processes, logs, commands = [], [], []
    result = {'passed': False, 'case': name, 'commands': commands, 'input': 'sdl_injected', 'long_run_executed': False}
    if rapid:
        result.update({'scenario': scenario, 'record_only': True, 'clean': scenario != 'pause'})
    pause_pending = bool(rapid) and scenario == 'pause'
    match, gateway, stopped = None, None, False

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
        bots = PassiveBots(args.bots, args.quad_probe, args.arena, directory)
        gui = [start(role, [str(args.gui_probe)] + mode_arguments + ['--fps', str(fps), '--arena-root', str(args.arena_root),
                            '--gateway', f'127.0.0.1:{http}', '--role', role, '--gpu-driver', args.gpu_driver,
                            '--output', str(directory)] + bots.gui_arguments()) for role in ('create', 'join')]
        deadline = time.monotonic() + (100 if rapid else 60)
        # Both roles finish their own bounded runs, so each report is saved even
        # when the other role fails: the actor signals the target either way.
        while any(process.poll() is None for process in gui):
            if time.monotonic() >= deadline:
                raise RuntimeError('Bounded action GUI deadline exceeded')
            bots.poll(http, start)
            if pause_pending and (directory / 'rapid-pause-ready.txt').exists():
                # Stop only the child Gateway, then let the actor start clicking (action_probe.py's mechanism).
                fault = {'requested_seconds': RAPID_PAUSE_SECONDS, 'mechanism': 'SIGSTOP/SIGCONT Gateway',
                         'start_ns': steady_clock_ns()}
                gateway.send_signal(signal.SIGSTOP)
                stopped = True
                time.sleep(RAPID_PAUSE_SECONDS)
                gateway.send_signal(signal.SIGCONT)
                stopped = False
                fault['release_ns'] = steady_clock_ns()
                staged = directory / 'rapid-pause.json.tmp'
                staged.write_text(json.dumps(fault) + '\n')
                staged.replace(directory / 'rapid-pause.json')
                result['gateway_pause'] = fault
                pause_pending = False
            time.sleep(.005 if pause_pending else .05)
        bots.stop()
        result['bots'] = bots.evidence()
        result['evidence'] = summarize_rapid(directory, scenario) if rapid else summarize(directory, mode == '--action-capture')
        result['exit_codes'] = {role: process.poll() for role, process in zip(('create', 'join'), gui)}
        result['passed'] = (result['evidence']['passed'] and all(code == 0 for code in result['exit_codes'].values()) and
                            (not args.bots or result['bots']['exit_code'] == 0))
        if not result['passed']:
            result['error'] = '; '.join(result['evidence']['errors']) or 'GUI exited with failure'
    except Exception as error:
        result['error'] = str(error)
    finally:
        if stopped and gateway.poll() is None:
            gateway.send_signal(signal.SIGCONT)
        stop_in_order(processes, gateway, match, directory / 'match.log')
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
    parser.add_argument('--case', action='append', choices=tuple(CASES) + tuple(RAPID_CASES))
    add_bot_arguments(parser)
    args = parser.parse_args()
    check_bot_arguments(parser, args)
    for name in ('match', 'gateway', 'gui_probe', 'arena', 'arena_root', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    for name in ('match', 'gateway', 'gui_probe', 'arena'):
        if not getattr(args, name).is_file():
            parser.error(f'Missing {name}: {getattr(args, name)}')
    if not (args.arena_root / 'asset_catalog.json').is_file():
        parser.error('Missing deployed asset catalog')
    if any(RAPID_CASES.get(name, ('',))[0] == 'pause' for name in args.case or ()) and not hasattr(signal, 'SIGSTOP'):
        parser.error('The rapid pause case requires POSIX SIGSTOP/SIGCONT')
    args.output.mkdir(parents=True, exist_ok=True)
    names = args.case or list(CASES)
    results = []
    for name in names:
        result = run_case(args, name)
        results.append({'case': name, 'passed': result['passed'], 'error': result.get('error'),
                        'platform': (result.get('evidence') or {}).get('platform')})
        if name in RAPID_CASES:
            results[-1]['rapid_counts'] = (result.get('evidence') or {}).get('counts')
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
