"""One bounded native X11/XTest v5 GUI regression; no full GUI matrix or soak."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import socket
import subprocess
import time
import urllib.request
from run_network import free_port, wait_for_match_ready
from run_native_window import Desktop, require, state, wait_state, screenshot
from acceptance_util import PROTOCOL_VERSION


def run(args):
    out=args.output;out.mkdir(parents=True,exist_ok=False)
    processes=[];logs=[];desktop=None;gui={}
    result={'passed':False,'protocol':PROTOCOL_VERSION,'scope':'One bounded two-native-GUI batch03 input/HUD regression, no animation acceptance or timing certification',
            'checks':{},'events':[],'captures':[],'commands':[]}
    (out/'gameplay-gui-plan.json').write_text(json.dumps({'protocol':PROTOCOL_VERSION,'planned':[
        'capture click no shot','Space held one jump/no buffered air jump','four native clicks at12 ticks each accepted',
        'R reload authoritative90ticks','death HP0 freezes controlled movement/jump/fire/reload',
        '180tick automatic respawn new life','respawn can shoot/reload','actual HUD captures','native Escape leave'],
        'probe_deadline_seconds':60,'dimensions':[800,600],'fps':60,'not_long_run':True},indent=2)+'\n')
    def start(label,cmd):
        result['commands'].append(cmd);log=(out/(label+'.log')).open('w');logs.append(log)
        p=subprocess.Popen(cmd,cwd=out,stdout=log,stderr=subprocess.STDOUT);processes.append(p);return p
    def sample(role='create'):return state(out,role)
    def record(label):result['events'].append({'label':label,'time_ns':time.monotonic_ns(),'create':sample(),'join':sample('join')})
    try:
        ipc,http,udp=free_port(),free_port(),free_port(socket.SOCK_DGRAM)
        match=start('match',[str(args.match),'--arena',str(args.arena),'--listen',f'127.0.0.1:{ipc}'])
        wait_for_match_ready(match,out/'match.log',f'127.0.0.1:{ipc}')
        gateway=start('gateway',[str(args.gateway),'--runtime',f'127.0.0.1:{ipc}','--http',f'127.0.0.1:{http}','--udp',f'127.0.0.1:{udp}','--advertise-ip','127.0.0.1'])
        deadline=time.monotonic()+10
        while True:
            require(match.poll() is None and gateway.poll() is None,'Service startup failed')
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{http}/rooms',timeout=.2):break
            except OSError:
                require(time.monotonic()<deadline,'Gateway startup timeout');time.sleep(.02)
        for role in ('create','join'):
            gui[role]=start(role,[str(args.gui_probe),'--native-window','--duration','60','--fps','60',
                '--arena-root',str(args.arena_root),'--gateway',f'127.0.0.1:{http}','--role',role,'--gpu-driver',args.gpu_driver,'--output',str(out)])
        desktop=Desktop();ids={}
        for role in gui:
            ready=wait_state(out,role,lambda s:len(s['players'])==2 and s['presented'],'Two successful GUI presentations absent',15)
            ids[role]=ready['window_id'];desktop.own(ids[role],gui[role].pid,ready['title'])
        desktop.position_initial(ids['create'],60,100);desktop.position_initial(ids['join'],min(900,desktop.size[0]-830),100);time.sleep(.3)
        def capture(role='create'):
            w=ids[role];desktop.focus(w);time.sleep(.12);before=sample(role)['weapon']['submitted']
            if not sample(role)['weapon']['captured']:
                x,y,width,height=desktop.geometry(w);desktop.move_pointer(w,x+width//2,y+height//2);desktop.click(w,.03)
                wait_state(out,role,lambda s:s['weapon']['captured'],'Native pointer capture failed')
                require(sample(role)['weapon']['submitted']==before,'Capture click fired')
        def aim(yaw,role='create'):
            capture(role)
            for _ in range(10):
                w=sample(role)['weapon'];dx=math.remainder(yaw-w['yaw'],2*math.pi);dy=-w['pitch']
                if abs(dx)<.004 and abs(dy)<.004:return
                desktop.relative(ids[role],round(dx/.0025),round(dy/.0025));time.sleep(.08)
            raise RuntimeError('Native aim failed')
        aim(0);record('capture-no-shot');result['checks']['capture_no_shot']=True
        desktop.key(ids['create'],'space',True);time.sleep(.85);desktop.key(ids['create'],'space',False);time.sleep(.2)
        require(sample()['weapon']['grounded'],'Held Space did not land')
        record('held-space-landed')
        # The 200ms target is12 authority ticks, below the old20-tick clip gate.
        ticks=[];next_edge=time.monotonic();original=sample()['weapon']['submitted']
        for ordinal in range(4):
            time.sleep(max(0,next_edge-time.monotonic()));desktop.click(ids['create'],.025)
            shown=wait_state(out,'create',lambda s:s['presented'] and s['presented_weapon']['last_decision']>=original+ordinal+1,'Rapid click decision missing')
            require(shown['weapon']['accepted']==ordinal+1 and shown['weapon']['hits']==ordinal+1,'12tick native shot rejected or missed')
            ticks.append(shown['weapon']['last_decision_tick']);next_edge+=.2
        require(all(10<=b-a<20 for a,b in zip(ticks,ticks[1:])),'GUI fire retained old20tick clip gate or violated10tick cooldown')
        result['checks']['ten_tick_fire_gate']=ticks
        dead=wait_state(out,'join',lambda s:s['presented'] and s['presented_weapon']['dead'],'Death HUD not presented')
        require(dead['weapon']['hp']==0 and dead['weapon']['respawn_tick']-dead['weapon']['life_state_tick']==180,'Death/respawn deadline incorrect')
        desktop.tap(ids['create'],'r');reload_state=wait_state(out,'create',lambda s:s['presented'] and s['presented_weapon']['reloading'],'R did not present authoritative reload')
        require(reload_state['weapon']['reload_end_tick']-reload_state['weapon']['reload_start_tick']==90,'Reload duration not90ticks')
        result['captures'].append(screenshot(out,'actor-reload-hud',ids['create']))
        desktop.focus(ids['join']);time.sleep(.12);before=sample('join');p0=next(p for p in before['players'] if p['id']==before['player_id'])
        desktop.key(ids['join'],'w',True);desktop.key(ids['join'],'space',True);desktop.tap(ids['join'],'r');desktop.click(ids['join'],.05);time.sleep(.25)
        desktop.key(ids['join'],'w',False);desktop.key(ids['join'],'space',False)
        after=sample('join');p1=next(p for p in after['players'] if p['id']==after['player_id'])
        require(after['weapon']['dead'] and after['weapon']['submitted']==before['weapon']['submitted'],'Dead controls submitted action')
        require(math.hypot(p1['x']-p0['x'],p1['z']-p0['z'])<1e-5 and p1['y']==0,'Dead native controls moved/jumped')
        result['captures'].append(screenshot(out,'dead-respawn-countdown-hud',ids['join']));record('dead-controls-suppressed')
        result['checks']['dead_controls_suppressed']=True
        alive=wait_state(out,'join',lambda s:s['presented'] and s['presented_weapon']['life_generation']==2 and not s['presented_weapon']['dead'],'Automatic respawn absent',5)
        require(alive['weapon']['hp']==100 and alive['weapon']['ammo']==12,'Respawn did not restore authority HUD')
        result['captures'].append(screenshot(out,'respawn-hud',ids['join']));record('respawn')
        require(sample()['weapon']['ammo']==12 and not sample()['weapon']['reloading'],'Actor reload did not finish')
        aim(math.pi,'join');prior=sample('join')['weapon']['submitted'];desktop.click(ids['join'],.03)
        wait_state(out,'join',lambda s:s['weapon']['last_decision']>prior and s['weapon']['accepted']==1,'New life cannot shoot')
        desktop.tap(ids['join'],'r');wait_state(out,'join',lambda s:s['weapon']['reloading'],'New life cannot reload')
        wait_state(out,'join',lambda s:s['presented'] and not s['presented_weapon']['reloading'] and s['presented_weapon']['ammo']==12,'New life reload failed',3)
        result['checks']['respawn_controls_reload']=True;record('respawn-reload-complete')
        result['captures'].append(screenshot(out,'respawn-reload-complete-hud',ids['join']))
        for role in ('join','create'):
            desktop.focus(ids[role]);desktop.tap(ids[role],'Escape');wait_state(out,role,lambda s:s['phase']==0,'Escape did not leave')
        result['checks']['native_leave']=True
        for role in gui:(out/(role+'-native-stop')).write_text('Bounded v5 GUI complete\n')
        for role,p in gui.items():require(p.wait(timeout=5)==0,'GUI failed to flush '+role)
        actor_frames=[json.loads(line) for line in (out/'create-native-frames.jsonl').read_text().splitlines()]
        jumps=[f for f in actor_frames if f['local']['y']>.05];require(jumps and .45<max(f['local']['y'] for f in jumps)<.7,'Space jump height not observed')
        landings=sum(a['local']['y']>.05 and b['local']['y']<=.05 for a,b in zip(actor_frames,actor_frames[1:]))
        require(landings==1,'Held Space repeated/buffered jump');result['checks']['space_one_jump']={'landings':landings,'maximum_y':max(f['local']['y'] for f in jumps)}
        result['passed']=True
    except Exception as error:result['error']=str(error)
    finally:
        if desktop:desktop.close()
        for role in ('create','join'):(out/(role+'-native-stop')).write_text('Runner cleanup\n')
        for p in reversed(processes):
            if p.poll() is None:
                try:p.wait(timeout=2 if p in gui.values() else .1)
                except subprocess.TimeoutExpired:p.terminate()
        for p in reversed(processes):
            try:p.wait(timeout=3)
            except subprocess.TimeoutExpired:p.kill();p.wait(timeout=3)
        for log in logs:log.close()
        result['artifacts']={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.match,args.gateway,args.gui_probe,args.arena,args.arena_root/'asset_catalog.json')}
        (out/'gameplay-gui-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':result['passed'],'error':result.get('error'),'checks':result['checks']}),flush=True)
    return 0 if result['passed'] else 1


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('match','gateway','gui-probe','arena','arena-root','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--gpu-driver',default='vulkan');args=parser.parse_args()
    for name in ('match','gateway','gui_probe','arena','arena_root','output'):setattr(args,name,getattr(args,name).resolve())
    return run(args)


if __name__=='__main__':raise SystemExit(main())
