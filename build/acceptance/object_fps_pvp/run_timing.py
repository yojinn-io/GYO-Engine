"""Owner-local real-clock measurement runner with preserved per-round evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import traceback
import urllib.request
from command_evidence import analyze_commands
from run_network import free_port


def require_clean_gui_round(evidence):
    """Full GUI qualification includes the declared clean-run requirement."""
    clean = evidence.get('disturbed') is False and evidence.get('resets') == 0
    evidence['gui_clean_passed'] = clean
    if not clean:
        message = 'Full GUI round was disturbed or reset epoch; cannot qualify as clean'
        errors = evidence.setdefault('errors', [])
        if message not in errors:
            errors.append(message)
        evidence['overall_passed'] = False
    return clean


def run_round(args, output, fps):
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
        'stall_ms':args.stall_ms,'monotonic_start_ns':time.monotonic_ns()},indent=2)+'\n')
    processes, handles = [], []
    def start(name, command):
        log=(output/(name+'.log')).open('w');handles.append(log)
        process=subprocess.Popen([str(v) for v in command],stdout=log,stderr=subprocess.STDOUT)
        processes.append(process);return process
    ipc,http,udp=free_port(),free_port(),free_port(socket.SOCK_DGRAM)
    try:
        match=start('match',[args.match,'--arena',args.arena,'--listen',f'127.0.0.1:{ipc}',
            '--movement-trace',output/'match-commands.jsonl'])
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
        if args.gui:
            for role in ('create','join'):
                command = [args.probe,'--latency-short' if args.short else '--latency','--role',role,'--arena-root',args.arena.parent,
                    '--gateway',f'127.0.0.1:{http}','--output',output,'--duration',args.duration,
                    '--fps',fps,'--events',args.events]
                if args.combat:command.append('--combat')
                clients.append(start(role,command))
        else:
            command=[args.probe,'--arena',args.arena,'--gateway',f'127.0.0.1:{http}',
                '--output',output,'--duration',args.duration,'--fps',fps]
            if args.stall_ms:command.extend(['--stall-at',5,'--stall-ms',args.stall_ms])
            clients.append(start('timing',command))
        for client in clients:
            if client.wait(timeout=args.duration+60):raise RuntimeError('Client probe failed; inspect logs')
        # Flush server diagnostics before analyzing; terminate is graceful SIGTERM.
        gateway.terminate();gateway.wait(timeout=10)
        match.terminate();match.wait(timeout=10)
        if match.returncode:raise RuntimeError('Match failed while flushing diagnostics')
        evidence=analyze_commands(output,enforce=not args.report_only and not args.stall_ms)
        if args.gui:
            from presentation_evidence import analyze_latency, analyze_short_latency
            evidence['presentation']=(analyze_short_latency if args.short else analyze_latency)(output)
        if args.combat:
            from combat_gui_evidence import analyze_combat_gui
            evidence['combat']=analyze_combat_gui(output)
        if args.soak:
            evidence['soak_clean_passed']=evidence['passed'] and not evidence['disturbed'] and evidence['resets']==0
        evidence['overall_passed']=evidence['passed'] and (not args.gui or evidence['presentation']['passed']) and (not args.soak or evidence['soak_clean_passed']) and (not args.combat or evidence['combat']['passed'])
        if args.gui and not args.short:
            require_clean_gui_round(evidence)
        (output/'round.json').write_text(json.dumps(evidence,indent=2)+'\n')
        print(json.dumps({'directory':str(output),'passed':evidence['overall_passed'],
            'movement_passed':evidence['passed'],'disturbed':evidence['disturbed'],
            'actual_p50_ms':evidence['actual_p50_ms'],'actual_p95_ms':evidence['actual_p95_ms'],
            'actual_fraction':evidence['actual_fraction']},indent=2),flush=True)
        if not args.report_only and not evidence['overall_passed']:
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
        lines.append(f"| {record['round']} | {record['status']} | {evidence.get('disturbed', 'unknown')} | "
                     f"{number(evidence.get('actual_fraction'), 100)} | "
                     f"{number(evidence.get('actual_p50_ms'))} / {number(evidence.get('actual_p95_ms'))} | "
                     f"{number(presentation.get('p50_seconds'), 1000)} / {number(presentation.get('p95_seconds'), 1000)} | "
                     f"{presentation.get('matched_event_count', 'unknown')} / {presentation.get('planned_event_count', 'unknown')} |")
        for error in evidence.get('errors', []) + presentation.get('errors', []) + combat.get('errors', []):
            notes.append(f"Round {record['round']}: {error}")
        if combat:
            notes.append(f"Round {record['round']} combat: {'PASS' if combat.get('passed') else 'FAIL'}; full details: round-{record['round']}/combat-gui-evidence.json")
        if record.get('error') or record.get('reason'):
            notes.append(f"Round {record['round']}: {record.get('error', record.get('reason'))}")
        if record.get('evidence_error'):
            notes.append(f"Round {record['round']} evidence is invalid: {record['evidence_error']}")
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
    p.add_argument('--combat',action='store_true',help='Concurrent SDL shots and authority HP; requires --gui')
    p.add_argument('--short',action='store_true',help='Explicit 16..25 second GUI regression; never full certification')
    p.add_argument('--report-only',action='store_true');p.add_argument('--rounds',type=int,default=1)
    p.add_argument('--duration',type=float,default=120);p.add_argument('--fps',type=int,default=60)
    p.add_argument('--events',type=int,default=200);p.add_argument('--stall-ms',type=int,default=0)
    args=p.parse_args()
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
    try:
        return execute_rounds(args)
    except ValueError as error:
        p.error(str(error))
if __name__=='__main__':raise SystemExit(main())
