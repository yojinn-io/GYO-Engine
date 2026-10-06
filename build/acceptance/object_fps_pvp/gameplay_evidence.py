"""v5 short real-socket evidence: immutable actions, per-life effects and recovery."""
from collections import Counter, defaultdict, deque
import json
import math
from pathlib import Path
from action_evidence import records, frames
from command_evidence import match_life_seed_clamps, read_trace_events, recovery_actual_intervals
from start_phase_evidence import reseed_evidence, summarize_start_phase
from acceptance_util import PROTOCOL_VERSION, STALL_RULE


def percentile(values, percent):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered)*percent/100)-1)] if ordered else math.inf


def combat_expected(player, life, tick, decisions, maximum_hp=100, capacity=12, reload_ticks=90):
    """Replay effects by authority tick; reload completes before same-tick actions."""
    damage = [(d['resolved_tick'], d['damage']) for _, d in decisions
              if d['accepted'] and d['target_id'] == player and d['target_life_generation'] == life and d['damage']]
    hp = maximum_hp-sum(amount for at, amount in damage if at <= tick)
    death_tick = next((at for at, _ in sorted(damage)
                       if sum(amount for when, amount in damage if when <= at) >= maximum_hp), None)
    ammo, end, reload_id, reload_start, last_id, last_tick = capacity, 0, 0, 0, 0, 0
    own = sorted((d for owner, d in decisions if owner == player and d['life_generation'] == life
                  and d['accepted'] and d['resolved_tick'] <= tick), key=lambda d:(d['resolved_tick'],d['action_id']))
    for d in own:
        at = d['resolved_tick']
        if end and end <= at:
            if death_tick is None or end <= death_tick:
                ammo = capacity
            end = reload_id = reload_start = 0
        if d['action_kind'] == 0:
            ammo -= 1; last_id, last_tick = d['action_id'], at
        else:
            reload_id, reload_start, end = d['action_id'], at, at+reload_ticks
    if end and end <= tick:
        if death_tick is None or end <= death_tick:
            ammo = capacity
        end = reload_id = reload_start = 0
    if death_tick is not None and death_tick <= tick:
        end = reload_id = reload_start = 0
    return dict(hp=hp, ammo=ammo, reload_action_id=reload_id, reload_start_tick=reload_start,
                reload_end_tick=end, last_shot_id=last_id, last_shot_tick=last_tick)


def start_phase_by_player(client, reseeds=None):
    """Each Session's per-epoch phase-tracking record from action-client.json; absence is explicit.
    The gameplay run has no latency plan, so measured-epoch fields are explicit nulls. ``reseeds``
    is the shared Client trace's stall reseeds (reseed_evidence), keyed by player, epoch and life."""
    return {str(entry.get('player_id', index)): summarize_start_phase(entry.get('start_phase'), f'action-client.json clients[{index}].start_phase',
                                                                      reseeds=reseeds)
            for index, entry in enumerate(client.get('clients', []))}


def is_wire_send_event(event):
    # Only labels emitted by ActionRelay._send are actual datagrams. A hold
    # observation can coexist with a forwarded packet with its action removed.
    return event['event'] in {'forwarded','released','network_forwarded','duplicate_action',
        'conflicting_atomic_batch','conflicting_life','conflicting_kind','malformed_action',
        'reordered_old_action','cross_life_result_released'}


def fault_expiry_exception(submission, decision, fault, tick_times):
    if decision['rejection']!=2 or not fault.get('start_ns',0)<=submission['time_ns']<=fault.get('release_ns',0):
        return None
    observed=submission['request']['observed_tick'];resolved=decision['resolved_tick']
    if observed not in tick_times or resolved not in tick_times:return None
    age_ms=(tick_times[resolved]-tick_times[observed])/1e6
    if age_ms<=250:return None
    return {'player_id':submission['player_id'],'action_id':submission['request']['action_id'],
            'observed_tick':observed,'resolved_tick':resolved,'original_reference_age_ms':age_ms,
            'generated_ns':submission['time_ns'],'fault_start_ns':fault['start_ns'],'fault_release_ns':fault['release_ns'],
            'reason':'Predeclared downstream fault concealed current snapshots; original reference exceeded250ms and terminal Expired is required.'}


def validate_accepted_actions(decisions, life_starts, *, capacity=12, reload_ticks=90, cooldown_ticks=10):
    """Independent legality replay; a returned accepted flag is never its own proof."""
    states={identity:dict(hp=100,ammo=capacity,reload_end=0,next_shot=0) for identity in life_starts}
    errors=[]
    for player,d in sorted(decisions,key=lambda item:(item[1]['resolved_tick'],item[0],item[1]['action_id'])):
        if not d['accepted']:continue
        tick=d['resolved_tick'];identity=player,d['life_generation']
        available=[life for (owner,life),at in life_starts.items() if owner==player and at<=tick]
        if not available or identity not in states or d['life_generation']!=max(available):
            errors.append('Accepted action has wrong current life');continue
        for key,st in states.items():
            if st['reload_end'] and st['reload_end']<=tick:
                if st['hp']>0:st['ammo']=capacity
                st['reload_end']=0
        st=states[identity]
        if st['hp']<=0:errors.append('Accepted action by dead player')
        if st['reload_end']:errors.append('Accepted action during active reload')
        if d['action_kind']==1:
            if st['ammo']>=capacity:errors.append('Accepted reload with full magazine')
            if d['damage'] or d['target_id'] or d['target_life_generation']:errors.append('Reload produced shot effect')
            st['reload_end']=tick+reload_ticks
        else:
            if st['ammo']<=0:errors.append('Accepted shot with empty magazine')
            if tick<st['next_shot']:errors.append('Accepted shot before10tick cooldown')
            st['ammo']-=1;st['next_shot']=tick+cooldown_ticks
            if d['damage']:
                target=d['target_id'],d['target_life_generation']
                living=[life for (owner,life),at in life_starts.items() if owner==target[0] and at<=tick]
                if target not in states or not living or target[1]!=max(living) or states[target]['hp']<=0:
                    errors.append('Accepted damage targeted a dead/old life')
                else:
                    states[target]['hp']-=d['damage']
                    if states[target]['hp']<=0:states[target]['reload_end']=0
    return errors


def client_disturbance(events, frame_samples, life_resets):
    errors=[];previous=None
    for f in frame_samples:
        if f['frame_seconds']>=.1 or previous is not None and f['time_ns']-previous>=100_000_000:
            errors.append('Client full-run frame reached100ms')
        previous=f['time_ns']
    gaps=[e for e in events if e['kind']=='runtime_gap' and (e['frame_seconds']>=.1 or e['dropped_seconds']>0)]
    clamps,unmatched=match_life_seed_clamps(gaps,life_resets)
    errors+=['Client runtime gap/dropped time is not a matched normal LifeRespawn or session-start seed clamp']*len(unmatched)
    return errors,clamps


def life_reset_matches(event, respawns):
    return event['reset_reason']=='life_respawn' and any(
        r['player_id']==event['player_id'] and r['life']==event['life_generation'] and
        r['tick']==event['authority_tick'] for r in respawns)


def analyze(output, relay, fault, mode):
    # Local import avoids the relay module importing itself during startup.
    from action_probe import maximum_window
    output=Path(output); errors=[]
    def check(ok, message):
        if not ok and message not in errors: errors.append(message)
    client=json.loads((output/'action-client.json').read_text())
    plan=json.loads((output/'gameplay-plan.json').read_text())
    events=list(records(output/'actions.jsonl')); all_frames=list(frames(output))
    check(client.get('gameplay_v5') and client['protocol']==PROTOCOL_VERSION and client['passed'],'v5 client did not finish/retire all actions')
    check(plan['protocol']==PROTOCOL_VERSION and len(plan['actions'])==client['planned_actions'],'Predeclared v5 denominator missing')
    submitted={}; delivered={}; by_plan={}; decisions=[]
    for e in events:
        if e['kind']=='submitted':
            identity=(e['player_id'],e['request']['action_id']); ordinal=e['plan_ordinal']
            check(identity not in submitted and ordinal not in by_plan,'Duplicate original action identity/plan ordinal')
            submitted[identity]=e; by_plan[ordinal]=e
        elif e['kind']=='decision':
            identity=(e['player_id'],e['decision']['action_id'])
            check(identity not in delivered,'Duplicate delivered decision'); delivered[identity]=e
            decisions.append((e['player_id'],e['decision']))
    check(set(by_plan)==set(range(len(plan['actions']))),'A predeclared action was omitted')
    check(set(submitted)==set(delivered),'Missing/unknown terminal decision')
    clean=mode=='baseline'; legal_latency=[]; delivery_latency=[]; verdicts=Counter(); fault_expiry_reasons=[]
    match_events=list(read_trace_events(output/'match-commands.jsonl'))
    client_events=list(read_trace_events(output/'clients-commands.jsonl'))
    ticks={e['authority_tick']:e['time_ns'] for e in match_events
           if e['kind']=='snapshot_produced' and client['start_ns']<=e['time_ns']<=client['end_ns']}
    for ordinal, declared in enumerate(plan['actions']):
        e=by_plan.get(ordinal)
        if not e: continue
        identity=(e['player_id'],e['request']['action_id']); result=delivered.get(identity)
        check(e['player_id']==client['player_ids'][declared['player_index']],'Plan attributed to wrong Session')
        check(e['request']['life_generation']==declared['life_generation'] and e['request']['action_kind']==declared['action_kind'],'Action immutable content differs from plan')
        check(e['time_ns']>=client['start_ns']+int(declared['at_seconds']*1e9),'Action generated before scheduled edge')
        if not result: continue
        d=result['decision']; verdicts[str(d['rejection'])]+=1
        check(d['life_generation']==e['request']['life_generation'] and d['action_kind']==e['request']['action_kind'],'Decision changed request kind/life')
        expected={declared['expected_rejection']}
        if mode=='cross-life' and declared['name']=='dead-shot': expected={5}
        if not clean and declared['at_seconds']<=5.6: expected|={0,1,2,3,7,8,9}
        expiry=fault_expiry_exception(e,d,fault,ticks) if not clean and declared['expected_rejection'] in (0,3,7,8,9) else None
        if expiry:
            expected.add(2);fault_expiry_reasons.append(expiry)
        check(d['rejection'] in expected,'Unexpected verdict for '+declared['name']+': '+str(d['rejection']))
        check(d['accepted']==(d['rejection']==0),'Accepted/rejection contradiction')
        check(d['accepted'] or d['damage']==0,'Rejected action damaged a player')
        check(d['damage'] in (0,25) and (not d['damage'] or d['hit_kind']==2 and d['target_life_generation']>0),'Invalid v5 damage attribution')
        if declared['target']: check(d['accepted'] and d['damage']==25 and d['target_life_generation']==1,'Predeclared lethal hit did not damage life1 exactly once')
        if clean and declared['expected_rejection']==0:
            resolved=ticks.get(d['resolved_tick'])
            check(resolved is not None,'Missing Match tick for legal action')
            legal_latency.append((resolved-e['time_ns'])/1e6 if resolved else math.inf)
            delivery_latency.append((result['time_ns']-e['time_ns'])/1e6)
    if clean:
        check(percentile(legal_latency,95)<=100,'Legal generation-to-Match P95 exceeds100ms')
        check(percentile(delivery_latency,95)<=150,'Legal generation-to-client P95 exceeds150ms')
    originals={}; repeats=0
    for e in relay['events']:
        if e['event']=='received' and e['kind']==7:
            check(len(e['decisions'])<=8,'Result batch exceeds8')
            for d in e['decisions']:
                identity=(int(relay['players'][str(e['session'])] if str(e['session']) in relay['players'] else relay['players'][e['session']]),d['action_id'])
                if identity in originals:
                    repeats+=1;check(originals[identity]==d,'Repeated wire decision changed')
                originals[identity]=d
    check(set(originals)==set(delivered),'Wire/delivery identity mismatch')
    for identity,e in delivered.items(): check(originals.get(identity)==e['decision'],'Delivered decision differs from immutable wire original')
    check(relay['relay_error'] is None,'Relay error: '+str(relay['relay_error']))
    check(max((e['bytes'] for e in relay['events']),default=0)<=1200,'UDP datagram exceeds1200')
    rates={}
    for session,player in relay['players'].items():
        sent=[e for e in relay['events'] if e['upstream'] and str(e['session'])==str(session) and is_wire_send_event(e)]
        rate=maximum_window([e['time_ns'] for e in sent]);rates[str(player)]=rate
        check(rate<=120,'Authenticated send rate exceeds120pps')
        received=[e for e in relay['events'] if e['upstream'] and str(e['session'])==str(session) and e['event']=='received']
        check(maximum_window([e['time_ns'] for e in received if e['kind']==6])<=31,'Action/ACK schedule exceeds30Hz')
        check(all(len(e.get('shots',[]))<=8 for e in received),'Action batch exceeds8')
    counters=relay.get('gateway_counters',{})
    check(counters.get('max_session_window_packets',121)<=120,'Actual Gateway limiter exceeds120')
    if mode!='gateway': check(counters.get('rate_limited_packets',1)==0,'Actual Gateway rate rejection')
    life_samples=defaultdict(list); hp_count=0; previous_time=0; max_frame=0; all_intervals=[]
    for frame in all_frames:
        t=frame['time_ns'];dt=frame['frame_seconds'];check(t>previous_time and math.isfinite(dt) and dt>=0,'Invalid/nonmonotonic frame timestamp');previous_time=t
        max_frame=max(max_frame,dt);all_intervals.append(dt)
        for suffix in ('a','b'):
            check(frame['pending_'+suffix]<=12 and frame['queued_'+suffix]<=32 and frame['retained_'+suffix]<=32,'Bounded command/action window exceeded')
            tick=frame['snapshot_tick_'+suffix]
            for c in frame['combat_'+suffix]:
                expected=combat_expected(c['player_id'],c['life_generation'],tick,decisions)
                check(all(c[k]==v for k,v in expected.items()),'Authority HP/ammo/reload/last-shot differs from unique per-life effects')
                hp_count+=1
            for p in frame['players_'+suffix]:
                life_samples[(p['player_id'],p['life_generation'])].append((tick,p))
    first,second=client['player_ids'];check((first,1) in life_samples and (second,1) in life_samples and (second,2) in life_samples,'Required alive/death/respawn lives absent')
    deaths=[]; jumps={}; respawns=[]
    for identity,samples in life_samples.items():
        unique={t:p for t,p in samples}; ordered=sorted(unique.items()); dead=[(t,p) for t,p in ordered if p['life_state']==1]
        jumps[str(identity)]=max((p['position'][1] for _,p in ordered),default=0)
        if dead:
            t,p=dead[0];deaths.append({'player_id':identity[0],'life':identity[1],'tick':p['life_state_tick'],'respawn_tick':p['respawn_tick']})
            check(p['respawn_tick']-p['life_state_tick']==180,'Death wait is not180 authority ticks')
            x,z=p['position'][0],p['position'][2]
            check(all(abs(q['position'][0]-x)<1e-5 and abs(q['position'][2]-z)<1e-5 for _,q in dead),'Dead player moved horizontally')
            check(all(q['life_state_tick']==p['life_state_tick'] for _,q in dead),'Death occurred more than once within life')
        if identity[1]>1:
            earlier=life_samples.get((identity[0],identity[1]-1),[]);olddead=[q for _,q in earlier if q['life_state']==1]
            check(bool(olddead),'New life lacks preceding observed death')
            if olddead:
                q=ordered[0][1];check(q['life_state_tick']>=olddead[0]['respawn_tick'],'Respawn occurred before deadline')
                check(q['epoch']>olddead[-1]['epoch'],'Respawn did not establish new movement epoch')
                respawns.append({'player_id':identity[0],'life':identity[1],'tick':q['life_state_tick']})
    check(len(deaths)==1 and len(respawns)==1,'Expected one death and one respawn')
    for player in client['player_ids']:
        check(jumps.get(str((player,1)),0)>.4,'Jump never raised authoritative feet')
        samples=life_samples.get((player,1),[]);check(any(p['grounded'] and t>samples[0][0]+120 for t,p in samples),'Jump never returned to grounded state')
    # Diagnose life reset explicitly; it cannot disguise starvation/backlog.
    resets=[e for e in match_events if e['kind']=='reset' and client['start_ns']<=e['time_ns']<=client['end_ns']]
    life_resets=[e for e in resets if e['reset_reason']=='life_respawn']
    check(len(life_resets)==len(respawns)==1,'LifeRespawn reset count differs from actual new life')
    for event in life_resets:
        check(life_reset_matches(event,respawns),
              'LifeRespawn reset does not match authoritative new life/tick')
    life_starts={(player,1):0 for player in client['player_ids']}
    life_starts.update({(r['player_id'],r['life']):r['tick'] for r in respawns})
    for error in validate_accepted_actions(decisions,life_starts):check(False,error)
    client_gaps,life_clamps=client_disturbance(client_events,all_frames,life_resets)
    if clean:
        for error in client_gaps:check(False,error)
    generated={(e['player_id'],e['epoch'],e['sequence']):e for e in client_events if e['kind']=='generated'}
    resolved={(e['player_id'],e['epoch'],e['sequence']):e for e in match_events if e['kind']=='resolved'}
    sent={}
    for e in client_events:
        if e['kind']=='sent':
            key=e['player_id'],e['epoch'],e['sequence']
            if key not in sent:sent[key]=e
    cancelled={}
    for e in client_events:
        if e['kind']=='lifecycle_cancelled':
            key=e['player_id'],e['epoch'],e['sequence']
            check(key in generated and e['player_id']==second and e['life_generation']==1 and
                  any(r['epoch']>e['epoch'] and r['time_ns']<=e['time_ns'] for r in life_resets),
                  'Lifecycle cancellation lacks matching old-life command/new-life reset')
            if key not in resolved:cancelled[key]=e
    start,end=client['start_ns']+1_000_000_000,client['start_ns']+15_000_000_000
    measured={k:e for k,e in generated.items() if start<=e['time_ns']<end and not e['seeded_neutral']}
    effective={k:e for k,e in measured.items() if k not in cancelled}
    actual=[(resolved[k]['time_ns']-e['time_ns'])/1e6 if k in resolved and resolved[k]['source']=='actual' else math.inf for k,e in effective.items()]
    first_send=[(sent[k]['time_ns']-e['time_ns'])/1e6 if k in sent else math.inf for k,e in effective.items()]
    queue=defaultdict(lambda:deque(maxlen=30));queue_max=0
    for e in match_events:
        if e['kind']=='resolved':
            history=queue[e['player_id'],e['epoch']];history.append(e['queued'])
            if start<=e['time_ns']<end:queue_max=max(queue_max,sum(history))
    movement={'fixed_window_seconds':[1,15],'generated_originals':len(measured),'lifecycle_cancelled':sum(k in measured for k in cancelled),
              'remaining_originals':len(effective),'actual':sum(math.isfinite(v) for v in actual),
              'actual_p50_ms':percentile(actual,50),'actual_p95_ms':percentile(actual,95),'send_p95_ms':percentile(first_send,95),
              'queue_30_tick_sum_max':queue_max,'resets':resets,'client_disturbance_errors':client_gaps,'matched_life_seed_clamps':life_clamps}
    if clean:
        check(all(e['reset_reason']=='life_respawn' for e in resets),'Clean run had backlog/starvation reset')
        check(queue_max<105,'Clean run reached30tick backlog threshold')
        check(percentile(actual,50)<=50 and percentile(actual,95)<=66.7,'Clean movement Actual exceeds50/66.7ms')
        check(percentile(first_send,95)<=22,'Clean movement first send P95 exceeds22ms')
        check(sum(math.isfinite(v) for v in actual)>=.99*len(effective),'Clean original movement Actual below99%')
        check(not any(e['kind']=='runtime_gap' and start<=e['time_ns']<end and (e['frame_seconds']>=.1 or e['dropped_seconds']>0) for e in match_events),
              'Clean Match runtime gap/dropped time')
    recovery=None
    release=fault.get('release_ns',0)
    if release:
        pending=[key for key,e in submitted.items() if e['time_ns']<=release and delivered.get(key,{}).get('time_ns',10**30)>release]
        convergence=max(((delivered.get(k,{}).get('time_ns',10**30)-release)/1e9 for k in pending),default=0)
        check(convergence<=1.5,'Outstanding original decisions failed1.5s recovery')
        fresh=min((e['time_ns'] for key,e in delivered.items() if key[0]==first and submitted[key]['time_ns']>=release),default=release+10**12)
        check((fresh-release)/1e9<=1.5,'Alive actor fresh decision failed1.5s recovery')
        stable=None;healthy_start=None
        for f in all_frames:
            if f['time_ns']<release:continue
            healthy=not f['frozen_a'] and f['pending_a']<12 and f['retained_a']<32 and f['remote_age_a']<.1 and f['frame_seconds']<.1
            if not healthy:healthy_start=None
            elif healthy_start is None:healthy_start=f['time_ns']
            if healthy_start and f['time_ns']-healthy_start>=250_000_000:
                stable=(healthy_start-release)/1e9;break
        check(stable is not None and stable<=1.5,'Alive actor failed250ms stable movement recovery within1.5s')
        intervals=recovery_actual_intervals([e for e in match_events if e['kind']=='resolved'],{k:e['time_ns'] for k,e in generated.items()},[first],release)[first]
        stable_actual=[i for i in intervals if i['end_ns']-i['start_ns']>=250_000_000 and i['start_ns']<=release+1_500_000_000]
        check(bool(stable_actual),'No250ms uninterrupted new Actual execution with30tickqueued<105 within1.5s')
        moved=False
        for interval in stable_actual:
            positions=[(f['authority_x_a'],f['authority_z_a']) for f in all_frames if interval['start_ns']<=f['time_ns']<=interval['end_ns']]
            if positions and max(math.dist(positions[0],p) for p in positions)>.05:moved=True
        check(moved,'Recovery Actual commands did not produce controlled horizontal movement')
        recovery={'actual_intervals':stable_actual,'pending':len(pending),'convergence_seconds':convergence,'fresh_decision_seconds':(fresh-release)/1e9,'stable_movement_start_seconds':stable,
                  'scope':'player A remains alive; player B 180-tick death wait is recorded separately'}
    for c in client['clients']:
        check(c['retained']==0 and c['retired_through']==c['submitted'],'Action IDs did not remain contiguous and retire across lives')
    if mode.startswith('loss-'):check(sum(e['event']=='dropped_'+mode[5:] for e in relay['events'])==2,'First loss not applied to both Sessions')
    if mode=='burst2':check(sum(e['event']=='dropped_burst' for e in relay['events'])==4,'Two consecutive packets not lost per Session')
    if mode.startswith('network'):
        check(relay['network']['random_dropped']>0 and relay['network']['delayed']>0,'Configured jitter/loss not exercised')
        check(relay['network']['remaining_queue']==0,'Undrained impairment queue')
    if mode=='cross-life':
        check(any(e['event']=='cross_life_result_held' for e in relay['events']) and any(e['event']=='cross_life_request_held' for e in relay['events']), 'Cross-life result/request holds not exercised')
        check(fault.get('release_ns',0)-fault.get('start_ns',0)>=4_500_000_000,'Cross-life result did not span full death wait')
    if mode=='duplicate-reorder-conflict':
        for label in ('duplicate_action','conflicting_atomic_batch','conflicting_life','conflicting_kind','reordered_old_action'):
            check(sum(e['event']==label for e in relay['events'])==2,'Missing '+label)
    return {'passed':not errors,'errors':errors,'protocol':PROTOCOL_VERSION,'mode':mode,'fps':client['fps'],
            'timer_baseline':client.get('timer'),  # Interpretation only; no check reads it.
            'stall_rule':STALL_RULE,
           'scope':'16-second real-socket v5 gameplay and delivery, no long certification',
            'fault_expiry_reasons':fault_expiry_reasons,'planned_actions':len(plan['actions']),'delivered_actions':len(delivered),'verdicts':dict(verdicts),'deaths':deaths,'respawns':respawns,
            'unique_wire_decisions':len(originals),'identical_repeats':repeats,'combat_observations':hp_count,'jump_maximum_foot_y':jumps,
            'legal_match_p95_ms':percentile(legal_latency,95) if clean else None,'legal_client_p95_ms':percentile(delivery_latency,95) if clean else None,
            'movement':movement,'recovery':recovery,'fault':fault,'maximum_packet_rate':rates,'full_frame_count':len(all_frames),'maximum_frame_seconds':max_frame,
            'all_frame_intervals_seconds':all_intervals,'start_phase':start_phase_by_player(client,reseed_evidence(client_events,'clients-commands.jsonl')),'client':client}
