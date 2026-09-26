"""Owner-local real-clock measurement runner, shared by saved baseline and v3."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import urllib.request
from command_evidence import analyze_commands
from run_network import free_port


def run_round(args, output, fps):
    output.mkdir(parents=True,exist_ok=True)
    def fingerprint(path):
        digest=hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
        return {'path':str(path),'sha256':digest.hexdigest()}
    (output/'run-manifest.json').write_text(json.dumps({
        'artifacts':{key:fingerprint(getattr(args,key)) for key in ('match','gateway','probe','arena')},
        'fps':fps,'duration':args.duration,'gui':args.gui,'soak':args.soak,
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
                clients.append(start(role,[args.probe,'--latency','--role',role,'--arena-root',args.arena.parent,
                    '--gateway',f'127.0.0.1:{http}','--output',output,'--duration',args.duration,
                    '--fps',fps,'--events',args.events]))
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
            from presentation_evidence import analyze_latency
            evidence['presentation']=analyze_latency(output)
        if args.soak:
            evidence['soak_clean_passed']=evidence['passed'] and not evidence['disturbed'] and evidence['resets']==0
        evidence['overall_passed']=evidence['passed'] and (not args.gui or evidence['presentation']['passed']) and (not args.soak or evidence['soak_clean_passed'])
        (output/'round.json').write_text(json.dumps(evidence,indent=2)+'\n')
        print(json.dumps({'directory':str(output),'passed':evidence['passed'],'disturbed':evidence['disturbed'],
            'actual_p50_ms':evidence['actual_p50_ms'],'actual_p95_ms':evidence['actual_p95_ms'],
            'actual_fraction':evidence['actual_fraction']},indent=2),flush=True)
        if not args.report_only and (not evidence['passed'] or
            (args.soak and not evidence['soak_clean_passed']) or
            (args.gui and not evidence['presentation']['passed'])):
            raise RuntimeError('Acceptance did not meet its fixed thresholds; see round.json')
        return evidence
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
        for handle in handles:handle.close()


def main():
    p=argparse.ArgumentParser()
    for key in ('match','gateway','probe','arena','output'):p.add_argument('--'+key,required=True,type=Path)
    p.add_argument('--gui',action='store_true');p.add_argument('--soak',action='store_true')
    p.add_argument('--report-only',action='store_true');p.add_argument('--rounds',type=int,default=1)
    p.add_argument('--duration',type=float,default=120);p.add_argument('--fps',type=int,default=60)
    p.add_argument('--events',type=int,default=200);p.add_argument('--stall-ms',type=int,default=0)
    args=p.parse_args()
    if args.rounds<1:p.error('--rounds must be positive')
    if args.fps not in (30,60,144):p.error('--fps must be 30, 60 or 144')
    for key in ('match','gateway','probe','arena','output'):setattr(args,key,getattr(args,key).resolve())
    if args.soak and args.duration<1800:p.error('Soak requires at least 1800 real seconds')
    results=[run_round(args,args.output/f'round-{i+1}',args.fps) for i in range(args.rounds)]
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
