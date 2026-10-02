"""Bounded batch-02 character probes: real GUI, v4 sockets, observer snapshot hold."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import time
import urllib.request

from impaired_network import _ImpairedGateway
from player_presentation_evidence import analyze
from run_network import free_port, steady_clock_ns, wait_for_match_ready
from run_weapon_short import digest


class SnapshotHold(_ImpairedGateway):
    """Only drop observer snapshots during one declared 300ms interval."""
    def __init__(self,http,udp):
        self.hold_start = self.hold_end = None
        self.session_order = []
        self.dropped = []
        self.forwarded_snapshots = 0
        super().__init__(http,udp,0)

    def arm(self,start,duration):
        with self.lock:
            self.hold_start,self.hold_end=start,start+duration

    def _receive(self,payload,source):
        if len(payload)<24:
            self._fail("Short datagram in character acceptance relay")
            return
        kind=int.from_bytes(payload[6:8],'big')
        session=int.from_bytes(payload[8:16],'big')
        downstream=source==self.upstream_udp
        now=steady_clock_ns()/1e9  # Compared with the GUI probe's C++ host_steady_seconds.
        with self.lock:
            if downstream:
                destination=self.clients.get(session)
                if destination is None:
                    self.stats['unroutable']+=1
                    return
            else:
                if session not in self.clients:
                    self.session_order.append(session)
                self.clients[session]=source
                destination=self.upstream_udp
            if (downstream and kind==4 and len(self.session_order)>=2 and session==self.session_order[1]
                    and self.hold_start is not None and self.hold_start<=now<self.hold_end):
                self.dropped.append({'steady_seconds':now,'bytes':len(payload)})
                return
            if downstream and kind==4:
                self.forwarded_snapshots+=1
            self.udp.sendto(payload,destination)

    def evidence(self):
        with self.lock:
            return {'scope':'Acceptance-only byte-preserving relay except deliberate observer snapshot drops; no synthetic world state.',
                'hold_start_seconds':self.hold_start,'hold_end_seconds':self.hold_end,
                'dropped_observer_snapshots':self.dropped,'forwarded_snapshots':self.forwarded_snapshots,
                'relay_error':self.error,'observed_sessions':len(self.clients)}


def run_case(args,name):
    directory=args.output/name
    directory.mkdir(parents=True,exist_ok=False)
    fps=30 if name=='player30' else 144 if name=='player144' else 60
    capture=name=='capture'
    ipc,http,udp=free_port(),free_port(),free_port(socket.SOCK_DGRAM)
    processes,logs,commands=[],[],[]
    relay=None
    result={'passed':False,'case':name,'commands':commands,'long_run_executed':False}
    def start(label,command):
        commands.append(command)
        log=(directory/(label+'.log')).open('w');logs.append(log)
        process=subprocess.Popen(command,cwd=directory,stdout=log,stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    try:
        match=start('match',[str(args.match),'--arena',str(args.arena),'--listen',f'127.0.0.1:{ipc}'])
        wait_for_match_ready(match,directory/'match.log',f'127.0.0.1:{ipc}')
        gateway=start('gateway',[str(args.gateway),'--runtime',f'127.0.0.1:{ipc}',
            '--http',f'127.0.0.1:{http}','--udp',f'127.0.0.1:{udp}','--advertise-ip','127.0.0.1'])
        deadline=time.monotonic()+10
        while True:
            if match.poll() is not None or gateway.poll() is not None:
                raise RuntimeError('Real service exited during startup')
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms',timeout=.2):break
            except OSError:
                if time.monotonic()>=deadline:raise RuntimeError('Real Gateway startup timed out')
                time.sleep(.03)
        relay=SnapshotHold(http,udp)
        gui=[]
        for role in ('create','join'):
            gui.append(start(role,[str(args.gui_probe),'--player-capture' if capture else '--player-short',
                '--fps',str(fps),'--duration','12','--arena-root',str(args.arena_root),'--gateway',relay.gateway,
                '--role',role,'--gpu-driver',args.gpu_driver,'--output',str(directory)]))
        deadline=time.monotonic()+40
        armed=False
        while any(process.poll() is None for process in gui):
            if any(process.poll() not in (None,0) for process in gui):
                raise RuntimeError('GUI failed; preserve and inspect raw create/join evidence')
            if not armed:
                try:
                    schedule=json.loads((directory/'player-start.json').read_text())
                    relay.arm(schedule['host_steady_seconds']+schedule['snapshot_hold_at_seconds'],
                              schedule['snapshot_hold_duration_seconds'])
                    armed=True
                except (FileNotFoundError,json.JSONDecodeError):pass
            if time.monotonic()>=deadline:raise RuntimeError('Bounded player GUI deadline exceeded')
            time.sleep(.02)
        if any(process.returncode!=0 for process in gui):raise RuntimeError('GUI exited with failure')
        result['presentation']=analyze(directory)
        result['relay']=relay.evidence()
        result['passed']=result['presentation']['passed'] and bool(result['relay']['dropped_observer_snapshots']) and result['relay']['relay_error'] is None
        if not result['passed']:result['error']='Player phase/GPU/hold evidence failed; inspect individual checks'
    except Exception as error:
        result['error']=str(error)
    finally:
        if relay:
            result['relay']=relay.evidence()
            relay.close()
        for process in reversed(processes):
            if process.poll() is None:process.terminate()
        for process in reversed(processes):
            try:process.wait(timeout=3)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=3)
        for log in logs:log.close()
        result['artifacts']={str(path):digest(path) for path in
            (args.match,args.gateway,args.gui_probe,args.arena,args.arena_root/'asset_catalog.json',Path(__file__).resolve())}
        (directory/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('match','gateway','gui-probe','arena','arena-root','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--gpu-driver',default='auto',choices=('auto','vulkan','d3d12','metal'))
    parser.add_argument('--case',action='append',choices=('player30','player60','player144','capture'))
    args=parser.parse_args()
    for name in ('match','gateway','gui_probe','arena','arena_root','output'):
        setattr(args,name,getattr(args,name).resolve())
    for name in ('match','gateway','gui_probe','arena'):
        if not getattr(args,name).is_file():parser.error(f'Missing {name}: {getattr(args,name)}')
    if not (args.arena_root/'asset_catalog.json').is_file():parser.error('Missing deployed asset catalog')
    args.output.mkdir(parents=True,exist_ok=True)
    names=args.case or ['player30','player60','player144','capture']
    results=[]
    measured={}
    for name in names:
        result=run_case(args,name)
        results.append({'case':name,'passed':result['passed'],'error':result.get('error')})
        if name!='capture' and result.get('presentation'):
            measured[name]=result['presentation'].get('equal_distance_forward_phase',{})
        print(json.dumps(results[-1]),flush=True)
        if not result['passed']:break
    comparisons={}
    for distance in ('0.5','1.0','1.5'):
        values={case:crossings[distance]['phase_seconds'] for case,crossings in measured.items() if distance in crossings}
        comparisons[distance]={'phase_seconds_by_case':values,'maximum_difference_seconds':max(values.values())-min(values.values()) if values else None}
    phase_consistent=all(value['maximum_difference_seconds'] is None or value['maximum_difference_seconds']<.0002 for value in comparisons.values())
    summary={'passed':len(results)==len(names) and all(value['passed'] for value in results) and phase_consistent,
        'requested_cases':names,'cases':results,'long_run_executed':False,'wire_gameplay':'v4 unchanged',
        'same_distance_across_fps':comparisons,'same_distance_phase_consistent':phase_consistent}
    (args.output/'player-short-matrix.json').write_text(json.dumps(summary,indent=2)+'\n')
    return 0 if summary['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
