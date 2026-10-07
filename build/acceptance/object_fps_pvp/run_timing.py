"""Owner-local real-clock measurement runner with preserved per-round evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time
import traceback
import urllib.request
from command_evidence import analyze_commands
from run_network import free_port, steady_clock_ns, wait_for_match_ready
from gui_bots import PassiveBots, add_arguments as add_bot_arguments, check_arguments as check_bot_arguments


def require_verified_life_resets(evidence):
    """A LifeRespawn reset is a contract epoch only when the combat evidence saw that death and new life."""
    resets = evidence.get('life_respawn_resets') or []
    verified = not resets or evidence.get('combat', {}).get('life_respawns_verified') is True
    if not verified:
        message = 'Host LifeRespawn resets lack verified deaths and new lives'
        if message not in evidence.setdefault('errors', []):
            evidence['errors'].append(message)
        evidence['passed'] = False
    return verified


def require_clean_gui_round(evidence):
    """Full GUI qualification includes the declared clean-run requirement.

    Verified LifeRespawn resets are v5 gameplay, not interference; every other reset is."""
    clean = evidence.get('disturbed') is False and evidence.get('unexpected_resets', evidence.get('resets')) == 0
    evidence['gui_clean_passed'] = clean
    if not clean:
        message = 'Full GUI round was disturbed or reset epoch; cannot qualify as clean'
        errors = evidence.setdefault('errors', [])
        if message not in errors:
            errors.append(message)
        evidence['overall_passed'] = False
    return clean


WINDOW_EVIDENCE_REASONS = {'window_evidence_missing': 'presentation evidence carries no window record',
                           'window_evidence_invalid': 'window evidence is malformed',
                           'window_evidence_unknown': 'window evidence has no recognised status'}


def window_evidence_kind(presentation):
    """Why usable window evidence is absent: missing, invalid (duplicate/malformed report) or unknown.

    Only presentation_evidence's per-role statuses are read: a role whose
    report was present but rejected is invalid, roles without any report are
    missing, and anything else (no per-role statuses, or statuses that do not
    explain the absence) is unknown.
    """
    windows = presentation.get('window')
    if windows is None:
        return 'window_evidence_missing'
    statuses = [window.get('status') if isinstance(window, dict) else None
                for window in (windows.values() if isinstance(windows, dict) else ())]
    if 'invalid' in statuses:
        return 'window_evidence_invalid'
    if 'absent' in statuses and all(status in ('absent', 'recorded') for status in statuses):
        return 'window_evidence_missing'
    return 'window_evidence_unknown'


def gate_window_evidence(evidence, report_only):
    """A counted GUI round needs window evidence without detected interference; report-only rounds are only flagged.

    Disturbed (True) window evidence, and window evidence that is missing,
    invalid (a duplicate or malformed report) or unknown (no per-role status
    explains why window_disturbed is not a bool), each fail a counted round
    with its own explicit status; the evidence itself is kept, and the round's
    own result before this gate is kept as thresholds_passed.
    """
    underlying = evidence.get('overall_passed')
    underlying = underlying if isinstance(underlying, bool) else None
    presentation = evidence.get('presentation')
    presentation = presentation if isinstance(presentation, dict) else {}
    disturbed = presentation.get('window_disturbed')
    reasons = [str(item) for item in presentation.get('window_disturbances') or []]
    if disturbed is False:
        kind = None
    elif disturbed is True:
        kind = 'window_disturbed'
    else:
        kind = window_evidence_kind(presentation)
        reasons = reasons or [WINDOW_EVIDENCE_REASONS[kind]]
    if kind is None:
        gate = {'status': 'clean', 'enforced': not report_only, 'reasons': []}
    elif report_only:
        gate = {'status': 'flagged_' + kind, 'enforced': False, 'reasons': reasons}
    else:
        gate = {'status': 'invalid_' + kind, 'enforced': True, 'reasons': reasons}
        evidence['overall_passed'] = False
        message = f"Counted GUI round is {gate['status']}; it cannot qualify (see window interference note)"
        errors = evidence.setdefault('errors', [])
        if message not in errors:
            errors.append(message)
    # Pre-gate verdict (thresholds and clean-run requirement), shown beside an invalid_window_* status.
    gate['thresholds_passed'] = underlying
    evidence['window_gate'] = gate
    return gate


def underlying_status(thresholds_passed):
    return 'passed' if thresholds_passed is True else 'failed' if thresholds_passed is False else 'unknown'


def _milliseconds(value, scale):
    if value is None:
        return 'unset'
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return f'invalid {value!r}'
    return f'{value*scale:.3f} ms'


PHASE_NOTE = ('tracking = the Client decided its phase at least once in this epoch and life and keeps correcting it '
              '(settling while a correction slews; a stall reseed returns it to acquiring); acquiring = Host samples '
              'but no decision; host_samples_absent = no Host slack sample reached the probe; for the measured epoch '
              'its state during measurement splits into tracking and acquiring seconds')


def _seconds_or_unknown(value):
    return 'unknown' if not isinstance(value, (int, float)) or isinstance(value, bool) else f'{value:.3f} s'


def _decided(entry):
    before = entry.get('decided_seconds_before_measurement')
    if entry.get('first_decision_frame') is None:
        return 'never decided'
    if not isinstance(before, (int, float)) or isinstance(before, bool):
        return f"decided at frame {entry.get('first_decision_frame')}"
    return f'decided {before:.3f} s before measurement began' if before >= 0 else \
        f'decided {-before:.3f} s after measurement began'


def _phase_epoch(entry):
    text = (f"epoch {entry.get('movement_epoch')}/life {entry.get('life_generation')} {entry.get('status')}: "
            f"{_decided(entry)}, first error {_milliseconds(entry.get('first_error_seconds'), 1e3)}, "
            f"{entry.get('corrections')} correction(s) ({entry.get('late_corrections')} late), "
            f"last state {entry.get('last_state')}, Host samples {entry.get('host_samples')}")
    if entry.get('host_samples'):
        text += (f" (late {entry.get('host_late_samples')}, slack {_milliseconds(entry.get('host_slack_min_micros'), 1e-3)} to "
                 f"{_milliseconds(entry.get('host_slack_max_micros'), 1e-3)})")
    if entry.get('connection_quality_failures_max'):
        text += f", connection-quality failures up to {entry['connection_quality_failures_max']}"
    if entry.get('stall_reseeds'):
        text += f", {entry['stall_reseeds']} stall reseed(s) in the Client trace"
    return text


def _measurement_state(window):
    if not isinstance(window, dict):
        return ''
    text = f"; during measurement: {window.get('status')}"
    if window.get('reason'):
        return text + f" ({window['reason']})"
    if window.get('tracking_seconds') is None:
        return text
    text += (f"; tracking {_seconds_or_unknown(window.get('tracking_seconds'))}, acquiring "
             f"{_seconds_or_unknown(window.get('acquiring_seconds'))}, {window.get('corrections_in_window')} correction(s)")
    if window.get('reacquisitions_in_window'):
        text += f", {window['reacquisitions_in_window']} reacquisition(s)"
    observed, total = window.get('observed_seconds'), window.get('window_seconds')
    numbers = all(isinstance(value, (int, float)) and not isinstance(value, bool) for value in (observed, total))
    if not numbers or abs(observed - total) > 5e-4:
        text += f" (observed {_seconds_or_unknown(observed)} of the {_seconds_or_unknown(total)} window)"
    return text


def _phase_role(role, phase):
    measured = phase.get('measured_epoch')
    if phase.get('status') != 'recorded':
        epoch = f"; measured epoch {measured}" if measured is not None else ''
        return f"{role} {phase.get('status')} ({phase.get('reason')}){epoch}"
    entry, first = phase.get('measured_epoch_record'), phase.get('first_epoch') or {}
    if entry:
        text = f"{role} measured {_phase_epoch(entry)}" + _measurement_state(entry.get('measurement_window'))
    else:
        text = (f"{role} measured epoch {'unknown' if measured is None else measured} not recorded "
                f"({phase.get('measured_epoch_reason')})")
    key = ('player_id', 'movement_epoch', 'life_generation')
    if not entry or any(entry.get(name) != first.get(name) for name in key):
        text += f"; first {_phase_epoch(first)}"
    count = phase.get('epoch_count') or 0
    if count > 1:
        text += f"; {count} epochs {phase.get('status_counts')}"
    if phase.get('dropped_observations'):
        text += f"; dropped observations {phase['dropped_observations']}"
    unattributed = [f"{len(phase['unattributed_epochs'])} record(s)"] if phase.get('unattributed_epochs') else []
    if phase.get('unattributed_client_frames'):
        unattributed.append(f"{phase['unattributed_client_frames']} active Client frame(s)")
    if unattributed:
        text += f"; {' and '.join(unattributed)} without a player id excluded"
    detection = phase.get('reseed_detection') or {}
    if detection.get('status') not in (None, 'recorded', 'not_checked'):
        text += f"; stall reseeds not counted ({detection.get('status')}: {detection.get('reason')})"
    return text


def start_phase_note(round_number, presentation):
    """One explicit line per GUI round; absent or unanalysed values are named."""
    phases = presentation.get('start_phase')
    if not isinstance(phases, dict):
        return f'Round {round_number} phase tracking: not analysed (no presentation start_phase evidence)'
    try:
        parts = [_phase_role(role, phases.get(role) or {'status': 'absent', 'reason': 'not analysed'})
                 for role in ('create', 'join')]
    except (AttributeError, TypeError, ValueError) as error:
        return f'Round {round_number} phase tracking: unreadable evidence ({error})'
    line = f'Round {round_number} phase tracking: ' + '; '.join(parts)
    recorded = any(isinstance(phases.get(role), dict) and phases[role].get('status') == 'recorded'
                   for role in ('create', 'join'))
    return line + (f' ({PHASE_NOTE})' if recorded else '')


def _milliseconds_or_unknown(value):
    return 'unknown' if not isinstance(value, (int, float)) or isinstance(value, bool) else f'{value * 1e3:.3f} ms'


def movement_note(round_number, evidence):
    """Host movement resets by reason and each GUI role's frame intervals during measurement."""
    if 'reset_reasons' not in evidence:
        resets = 'Host movement reset reasons not analysed'
    else:
        reasons = evidence.get('reset_reasons') or {}
        detail = ', '.join(f'{reason} {count}' for reason, count in sorted(reasons.items()))
        resets = f"Host movement resets during measurement: {evidence.get('resets')}" + (f' ({detail})' if detail else '')
    intervals = evidence.get('presentation_frame_intervals')
    roles = intervals.get('roles') if isinstance(intervals, dict) else None
    if not isinstance(roles, dict):
        frames = 'not analysed'
    else:
        parts = []
        for role in ('create', 'join'):
            stats = roles.get(role)
            if not isinstance(stats, dict) or not stats.get('count'):
                parts.append(f'{role} not recorded')
                continue
            parts.append(f"{role} {stats['count']} intervals: median {_milliseconds_or_unknown(stats.get('median_seconds'))}, "
                         f"p95 {_milliseconds_or_unknown(stats.get('p95_seconds'))}, "
                         f"max {_milliseconds_or_unknown(stats.get('maximum_seconds'))}, "
                         f">1.1 tick {stats.get('over_1_1_tick')}, >=2 ticks {stats.get('at_least_2_ticks')}")
        frames = '; '.join(parts)
    return f'Round {round_number} movement: {resets}; frame intervals during measurement: {frames}'


def platform_note(round_number, presentation):
    """One line naming where each role ran; a pass on one platform never stands for another."""
    platforms = presentation.get('platform')
    if not isinstance(platforms, dict):
        return f'Round {round_number} platform: not analysed (no presentation platform evidence)'
    parts = []
    for role in ('create', 'join'):
        record = platforms.get(role)
        if not isinstance(record, dict) or record.get('status') != 'recorded':
            parts.append(f"{role} {record.get('status') if isinstance(record, dict) else 'absent'}")
            continue
        refresh = record.get('refresh_hz')
        parts.append(f"{role} {record.get('os')}/{record.get('architecture')} video {record.get('video_driver')}, "
                     f"GPU {record.get('gpu_driver')}, "
                     f"{'unknown' if not refresh else format(refresh, 'g') + ' Hz'} display, input {record.get('input')}")
    return f"Round {round_number} platform: {'; '.join(parts)}"


def window_note(round_number, presentation, gate=None):
    if 'window' not in presentation:
        state = 'not analysed (no presentation window evidence)'
        placed, details, platform_default = [], [], False
    else:
        disturbed = presentation.get('window_disturbed')
        # Named apart from the movement table's "Disturbed" column. Only the
        # recorded window events and flags are checked, so absence is not "clean".
        state = ('unknown' if disturbed is None else 'window interference' if disturbed
                 else 'no window-state interference detected')
        placed = []
        platform_default = False
        windows = presentation.get('window')
        for role, window in (windows.items() if isinstance(windows, dict) else ()):
            if isinstance(window, dict) and window.get('status') == 'recorded':
                sync = window.get('sync')
                platform_default = platform_default or window.get('placement') == 'platform_default'
                placed.append(f"{role} {window.get('placement')} at {tuple(window.get('final_position') or ())}"
                              + (f", sync {sync}" if sync not in (None, 'succeeded') else ''))
        details = [str(item) for item in presentation.get('window_disturbances') or []]
    line = (f'Round {round_number} window: {state}' + (f" ({'; '.join(placed)})" if placed else '') +
            (': ' + '; '.join(details) if details else ''))
    if placed and platform_default:
        line += (' [platform-default placement (the platform did not let the probe position its window): the two '
                 'windows may overlap, and a Wayland compositor may report OCCLUDED for the covered one]')
    if isinstance(gate, dict) and gate.get('status') != 'clean':
        if gate.get('enforced'):
            line += f" -> counted round {gate.get('status')}"
            if 'thresholds_passed' in gate:
                line += f" (underlying threshold result: {underlying_status(gate.get('thresholds_passed'))})"
        else:
            line += f" -> report-only: {gate.get('status')}, not enforced"
    return line


WINDOW_INTERFERENCE_NOTE = (
    'Window interference note: a counted GUI round qualifies only with window evidence from both probes and no detected '
    'window interference; otherwise it is invalid_window_disturbed, invalid_window_evidence_missing, '
    'invalid_window_evidence_invalid or invalid_window_evidence_unknown, with its threshold result kept beside the status. '
    'The two probe windows open in opposite corners of the usable display bounds wherever the platform lets a program '
    'position its windows (macOS, Windows, Linux X11). Where it does not (Linux/Wayland) they keep the default positions '
    'and may overlap; a compositor may then report OCCLUDED for the covered window, which invalidates a counted round '
    '(invalid_window_disturbed) even when its thresholds pass.')


def wayland_warning(gui, report_only, environ=None, platform=None):
    """The warning printed before counted GUI rounds in a Linux Wayland session; None otherwise."""
    environ = os.environ if environ is None else environ
    platform = sys.platform if platform is None else platform
    if not gui or report_only or not platform.startswith('linux'):
        return None
    if not environ.get('WAYLAND_DISPLAY') and environ.get('XDG_SESSION_TYPE') != 'wayland':
        return None
    return ('Warning: counted GUI rounds in a Linux Wayland session. The two probe windows keep their default positions '
            'and may overlap; if the compositor reports OCCLUDED for the covered window during measurement, the round is '
            'invalid_window_disturbed even when its thresholds pass.')


def run_round(args, output, fps):
    # Callers that predate passive bots pass no bot options: a room of the two GUIs.
    bot_count, quad_probe = getattr(args, 'bots', 0), getattr(args, 'quad_probe', None)
    output.mkdir(parents=True,exist_ok=False)
    def fingerprint(path):
        digest=hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
        return {'path':str(path),'sha256':digest.hexdigest()}
    (output/'run-manifest.json').write_text(json.dumps({
        'artifacts':{key:fingerprint(getattr(args,key)) for key in ('match','gateway','probe','arena')},
        'runner':fingerprint(Path(__file__).resolve()),
        'fps':fps,'duration':args.duration,'gui':args.gui,'combat':args.combat,'short':args.short,'soak':args.soak,
        'stall_ms':args.stall_ms,'monotonic_start_ns':time.monotonic_ns(),
        'steady_clock_start_ns':steady_clock_ns(),**({'bots':bot_count} if bot_count else {})},indent=2)+'\n')
    processes, handles = [], []
    def start(name, command):
        log=(output/(name+'.log')).open('w');handles.append(log)
        process=subprocess.Popen([str(v) for v in command],stdout=log,stderr=subprocess.STDOUT)
        processes.append(process);return process
    ipc,http,udp=free_port(),free_port(),free_port(socket.SOCK_DGRAM)
    try:
        match=start('match',[args.match,'--arena',args.arena,'--listen',f'127.0.0.1:{ipc}',
            '--movement-trace',output/'match-commands.jsonl'])
        # The Gateway dials the Match once and exits if it is not yet listening.
        wait_for_match_ready(match,output/'match.log',f'127.0.0.1:{ipc}')
        gateway=start('gateway',[args.gateway,'--runtime',f'127.0.0.1:{ipc}',
            '--http',f'127.0.0.1:{http}','--udp',f'127.0.0.1:{udp}','--advertise-ip','127.0.0.1'])
        deadline=time.monotonic()+20
        while True:
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError('Service startup failed')
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms',timeout=.3):break
            except OSError:
                if time.monotonic()>deadline:raise
                time.sleep(.05)
        clients=[]
        bots=PassiveBots(bot_count,quad_probe,args.arena,output)
        if args.gui:
            for role in ('create','join'):
                command = [args.probe,'--latency-short' if args.short else '--latency','--role',role,'--arena-root',args.arena.parent,
                    '--gateway',f'127.0.0.1:{http}','--output',output,'--duration',args.duration,
                    '--fps',fps,'--events',args.events]
                if args.combat:command.append('--combat')
                clients.append(start(role,command+bots.gui_arguments()))
        else:
            command=[args.probe,'--arena',args.arena,'--gateway',f'127.0.0.1:{http}',
                '--output',output,'--duration',args.duration,'--fps',fps]
            if args.stall_ms:command.extend(['--stall-at',5,'--stall-ms',args.stall_ms])
            clients.append(start('timing',command))
        if bot_count:
            # The bots join once both GUIs are in the room and leave after the GUIs finish.
            deadline=time.monotonic()+args.duration+60
            while any(client.poll() is None for client in clients):
                if time.monotonic()>deadline:raise RuntimeError('Client probe timed out; inspect logs')
                bots.poll(http,start);time.sleep(.05)
            if any(client.returncode for client in clients):raise RuntimeError('Client probe failed; inspect logs')
            if bots.stop()!=0:raise RuntimeError('Passive bots did not leave cleanly; inspect bots.log')
        for client in clients:
            if client.wait(timeout=args.duration+60):raise RuntimeError('Client probe failed; inspect logs')
        # Flush server diagnostics before analyzing; terminate is graceful SIGTERM.
        gateway.terminate();gateway.wait(timeout=10)
        match.terminate();match.wait(timeout=10)
        if match.returncode:raise RuntimeError('Match failed while flushing diagnostics')
        if bot_count:
            # command_evidence covers two players; the bots' room uses its N-player form.
            from quad_evidence import command_metrics
            evidence=command_metrics(output,enforce=not args.report_only)
            evidence['bots']=bots.evidence()
        else:
            evidence=analyze_commands(output,enforce=not args.report_only and not args.stall_ms)
        if args.gui:
            from presentation_evidence import analyze_latency, analyze_short_latency
            evidence['presentation']=(analyze_short_latency if args.short else analyze_latency)(output)
        if args.combat:
            from combat_gui_evidence import analyze_combat_gui
            evidence['combat']=analyze_combat_gui(output,evidence['life_respawn_resets'])
        require_verified_life_resets(evidence)
        if args.soak:
            evidence['soak_clean_passed']=evidence['passed'] and not evidence['disturbed'] and evidence['resets']==0
        evidence['overall_passed']=evidence['passed'] and (not args.gui or evidence['presentation']['passed']) and (not args.soak or evidence['soak_clean_passed']) and (not args.combat or evidence['combat']['passed'])
        if args.gui and not args.short:
            require_clean_gui_round(evidence)
        if args.gui:
            gate_window_evidence(evidence, args.report_only)
        (output/'round.json').write_text(json.dumps(evidence,indent=2)+'\n')
        print(json.dumps({'directory':str(output),'passed':evidence['overall_passed'],
            'movement_passed':evidence['passed'],'disturbed':evidence['disturbed'],
            'actual_p50_ms':evidence['actual_p50_ms'],'actual_p95_ms':evidence['actual_p95_ms'],
            'actual_fraction':evidence['actual_fraction']},indent=2),flush=True)
        if not args.report_only and not evidence['overall_passed']:
            gate = evidence.get('window_gate') or {}
            if str(gate.get('status', '')).startswith('invalid_'):
                raise RuntimeError(f"Counted GUI round is {gate['status']} (window interference; underlying threshold "
                                   f"result {underlying_status(gate.get('thresholds_passed'))}); see round.json")
            raise RuntimeError('Acceptance did not meet its fixed thresholds; see round.json')
        return evidence
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
        for handle in handles:handle.close()


def execute_rounds(args, round_runner=run_round):
    """Fail fast, but retain a terminal record for every requested round."""
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        raise ValueError('--output must be a new or empty directory; previous evidence is never overwritten')
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    stopped = False
    for index in range(args.rounds):
        directory = args.output / f'round-{index+1}'
        record = {'round': index+1, 'directory': str(directory), 'status': 'not_run', 'passed': False}
        if stopped:
            record['reason'] = 'A previous round failed; remaining rounds were not started'
            records.append(record)
            continue
        try:
            evidence = round_runner(args, directory, args.fps)
            record.update(status='passed' if evidence['overall_passed'] else 'failed',
                          passed=bool(evidence['overall_passed']), evidence=evidence)
        except (Exception, KeyboardInterrupt) as error:
            # run_round's finally has already cleaned up only its own children.
            directory.mkdir(parents=True, exist_ok=True)
            failure = traceback.format_exc()
            (directory / 'runner-failure.txt').write_text(failure, encoding='utf-8')
            record.update(status='failed', error=f'{type(error).__name__}: {error}')
            saved = directory / 'round.json'
            if saved.exists():
                try:
                    parsed = json.loads(saved.read_text(encoding='utf-8'))
                    if not isinstance(parsed, dict):
                        raise ValueError('round.json is not an object')
                    record['evidence'] = parsed
                except (OSError, ValueError) as parse_error:
                    record['evidence_error'] = str(parse_error)
        gate = record.get('evidence', {}).get('window_gate')
        if isinstance(gate, dict) and str(gate.get('status', '')).startswith('invalid_'):
            # Kept as its own terminal status, never deleted or folded into
            # "failed"; the round's own pre-gate result stays beside it.
            thresholds = gate.get('thresholds_passed')
            thresholds = thresholds if isinstance(thresholds, bool) else None
            record.update(status=gate['status'], passed=False, window_reasons=gate.get('reasons', []),
                          thresholds_passed=thresholds, underlying_status=underlying_status(thresholds))
        records.append(record)
        stopped = not record['passed']
    passed = all(record['passed'] for record in records)
    result = {'schema_version': 2, 'passed': passed, 'requested_rounds': args.rounds,
              'completed_rounds': sum(record['status'] != 'not_run' for record in records),
              'gui': args.gui, 'combat': args.combat, 'short': args.short, 'soak': args.soak, 'report_only': args.report_only,
              'duration_seconds': args.duration, 'nominal_fps': args.fps,
              'events_per_round': args.events if args.gui else None,
              'full_product_acceptance': False,
              'scope': 'This runner reports only its requested measurements; it does not promote a stable product baseline.',
              'window_interference_note': WINDOW_INTERFERENCE_NOTE if args.gui else None,
              'rounds': records}
    (args.output / 'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    lines = ['# PvP timing measurement', '', f"Result: {'PASS' if passed else 'FAIL'}", '',
             f'Requested rounds: {args.rounds}; duration: {args.duration:g} s; nominal FPS: {args.fps}.',
             f'GUI: {args.gui}; combat: {args.combat}; short: {args.short}; soak: {args.soak}; report-only: {args.report_only}.', '',
             'Each round keeps raw traces, service logs and its own thresholds. No rounds are pooled.', '',
             '| Round | Status | Disturbed | Actual % | Actual P50 / P95 ms | Visible P50 / P95 ms | Paired events |',
             '|---|---|---|---|---|---|---|']
    def number(value, multiplier=1):
        return 'unknown' if value is None else f'{value*multiplier:.3f}'
    notes = []
    for record in records:
        evidence = record.get('evidence', {})
        presentation = evidence.get('presentation', {})
        combat = evidence.get('combat', {})
        status = record['status'] + (f" (thresholds {record['underlying_status']})" if 'underlying_status' in record else '')
        lines.append(f"| {record['round']} | {status} | {evidence.get('disturbed', 'unknown')} | "
                     f"{number(evidence.get('actual_fraction'), 100)} | "
                     f"{number(evidence.get('actual_p50_ms'))} / {number(evidence.get('actual_p95_ms'))} | "
                     f"{number(presentation.get('p50_seconds'), 1000)} / {number(presentation.get('p95_seconds'), 1000)} | "
                     f"{presentation.get('matched_event_count', 'unknown')} / {presentation.get('planned_event_count', 'unknown')} |")
        for error in evidence.get('errors', []) + presentation.get('errors', []) + combat.get('errors', []):
            notes.append(f"Round {record['round']}: {error}")
        if combat:
            notes.append(f"Round {record['round']} combat: {'PASS' if combat.get('passed') else 'FAIL'}; full details: round-{record['round']}/combat-gui-evidence.json")
        if args.gui and record['status'] != 'not_run':
            notes.append(start_phase_note(record['round'], presentation))
            notes.append(movement_note(record['round'], evidence))
            notes.append(platform_note(record['round'], presentation))
            notes.append(window_note(record['round'], presentation, evidence.get('window_gate')))
        if record.get('error') or record.get('reason'):
            notes.append(f"Round {record['round']}: {record.get('error', record.get('reason'))}")
        if record.get('evidence_error'):
            notes.append(f"Round {record['round']} evidence is invalid: {record['evidence_error']}")
    if args.gui:
        notes.append(WINDOW_INTERFERENCE_NOTE)
    for note in notes:
        lines.extend(['', note])
    lines.extend(['', 'Full product acceptance remains separate. A failed, interrupted or not-run round is not a pass.',
                  'Report-only does not enforce latency thresholds. Same-host timestamps are not input-to-photon.', ''])
    (args.output / 'summary.md').write_text('\n'.join(lines), encoding='utf-8')
    print(json.dumps({'passed': passed, 'output': str(args.output),
                      'completed_rounds': result['completed_rounds'], 'requested_rounds': args.rounds}), flush=True)
    return 0 if passed else 1


def main():
    p=argparse.ArgumentParser()
    for key in ('match','gateway','probe','arena','output'):p.add_argument('--'+key,required=True,type=Path)
    p.add_argument('--gui',action='store_true');p.add_argument('--soak',action='store_true')
    p.add_argument('--combat',action='store_true',help='Concurrent predeclared v5 SDL combat (kills, respawns, reload) checked per life; requires --gui')
    p.add_argument('--short',action='store_true',help='Explicit 16..25 second GUI regression; never full certification')
    p.add_argument('--report-only',action='store_true');p.add_argument('--rounds',type=int,default=1)
    p.add_argument('--duration',type=float,default=120);p.add_argument('--fps',type=int,default=60)
    p.add_argument('--events',type=int,default=200);p.add_argument('--stall-ms',type=int,default=0)
    add_bot_arguments(p)
    args=p.parse_args()
    check_bot_arguments(p,args)
    if args.bots and not args.gui:p.error('--bots fills the room of the --gui probes')
    if args.rounds<1:p.error('--rounds must be positive')
    if args.fps not in (30,60,144):p.error('--fps must be 30, 60 or 144')
    for key in ('match','gateway','probe','arena','output'):setattr(args,key,getattr(args,key).resolve())
    for key in ('match', 'gateway', 'probe', 'arena'):
        if not getattr(args, key).is_file():p.error(f'Missing required {key}: {getattr(args, key)}')
    if args.gui and not (args.arena.parent/'asset_catalog.json').is_file():
        p.error('--gui requires --arena within the deployed asset directory containing asset_catalog.json')
    if not 0 < args.duration <= 3600:p.error('--duration must be positive and at most 3600 seconds')
    if (args.combat or args.short) and not args.gui:p.error('--combat and --short require --gui')
    if args.combat and args.duration not in (16, 120):
        p.error('--combat supports explicit 16-second short or 120-second integration measurements')
    if args.short and (not 16 <= args.duration <= 25 or args.events < 20 or args.duration/args.events < .6):
        p.error('--short requires 16..25 seconds, at least 20 events and 600 ms per event')
    if args.gui and not args.short and (args.duration < 120 or args.events < 200 or args.duration/args.events < .6):
        p.error('--gui requires at least 120 seconds, 200 events and 600 ms per event')
    if args.gui and args.fps < 60:p.error('--gui requires nominal FPS >= 60')
    if args.soak and args.duration<1800:p.error('Soak requires at least 1800 real seconds')
    warning = wayland_warning(args.gui, args.report_only)
    if warning:
        print(warning, file=sys.stderr, flush=True)
    try:
        return execute_rounds(args)
    except ValueError as error:
        p.error(str(error))
if __name__=='__main__':raise SystemExit(main())
