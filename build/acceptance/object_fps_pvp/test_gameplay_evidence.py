"""Counterexamples for v5 per-life evidence and wire mapping; no live services."""
from pathlib import Path
import subprocess
import sys
import unittest
from gameplay_evidence import combat_expected, life_reset_matches, validate_accepted_actions, client_disturbance, fault_expiry_exception, is_wire_send_event, start_phase_by_player
from command_evidence import recovery_actual_intervals
from action_probe import decision, encoded
from run_gameplay import matrix


def action(identity,tick,kind=0,life=1,damage=0,target=0,target_life=0,accepted=True):
    return dict(action_id=identity,resolved_tick=tick,action_kind=kind,life_generation=life,
                accepted=accepted,damage=damage,target_id=target,target_life_generation=target_life)


class GameplayEvidenceTests(unittest.TestCase):
    def test_damage_scoped_by_target_life(self):
        ds=[(1,action(1,5,damage=25,target=2,target_life=1)),(1,action(2,205,damage=25,target=2,target_life=2))]
        self.assertEqual(combat_expected(2,1,300,ds)['hp'],75)
        self.assertEqual(combat_expected(2,2,300,ds)['hp'],75)

    def test_reload_completes_at_ninety_before_shot(self):
        ds=[(1,action(1,1)),(1,action(2,2,kind=1)),(1,action(3,92))]
        self.assertEqual(combat_expected(1,1,91,ds)['ammo'],11)
        self.assertEqual(combat_expected(1,1,91,ds)['reload_end_tick'],92)
        self.assertEqual(combat_expected(1,1,92,ds)['ammo'],11)
        self.assertEqual(combat_expected(1,1,92,ds)['reload_end_tick'],0)

    def test_death_cancels_reload(self):
        ds=[(2,action(1,1)),(2,action(2,2,kind=1))]+[(1,action(i+1,20+i,damage=25,target=2,target_life=1)) for i in range(4)]
        state=combat_expected(2,1,100,ds)
        self.assertEqual((state['hp'],state['ammo'],state['reload_end_tick']),(0,11,0))
        self.assertEqual(combat_expected(2,2,300,ds)['ammo'],12)

    def test_rejected_action_has_no_effect(self):
        ds=[(1,action(1,3,accepted=False)),(1,action(2,4,kind=1,accepted=False))]
        self.assertEqual(combat_expected(1,1,100,ds)['ammo'],12)
        self.assertEqual(combat_expected(1,1,100,ds)['last_shot_id'],0)

    def test_predeath_delayed_result_stays_old_life(self):
        ds=[(1,action(1,20,damage=25,target=2,target_life=1))]
        self.assertEqual(combat_expected(2,2,250,ds)['hp'],100)
        self.assertEqual(combat_expected(1,2,250,ds)['ammo'],12)

    def test_wire_rejection_mapping_is_not_enum_cast(self):
        for wire,domain in ((4,5),(5,6),(6,4),(7,7)):
            payload=encoded([(1,0,1),(2,0,60),(4,0,wire),(8,0,2),(9,0,3)])
            d=decision(payload)
            self.assertEqual((d['action_kind'],d['life_generation'],d['rejection']),(1,3,domain))

    def test_target_life_is_preserved(self):
        d=decision(encoded([(1,0,1),(2,0,60),(3,0,1),(8,0,1),(9,0,4),(10,0,8)]))
        self.assertEqual((d['life_generation'],d['target_life_generation']),(4,8))

    def test_life_reset_must_match_identity_generation_and_tick(self):
        event=dict(reset_reason='life_respawn',player_id=2,life_generation=2,authority_tick=200)
        self.assertTrue(life_reset_matches(event,[dict(player_id=2,life=2,tick=200)]))
        for field,value in (('player_id',1),('life_generation',3),('authority_tick',199),('reset_reason','starvation')):
            self.assertFalse(life_reset_matches(dict(event,**{field:value}),[dict(player_id=2,life=2,tick=200)]))

    def test_held_motion_is_not_actual_recovery(self):
        events=[dict(player_id=1,epoch=1,sequence=n+1,authority_tick=n+1,time_ns=1_000_000_000+n*16_666_667,queued=1,source='held') for n in range(30)]
        generated={(1,1,n+1):e['time_ns']-1 for n,e in enumerate(events)}
        self.assertEqual(recovery_actual_intervals(events,generated,[1],900_000_000)[1],[])

    def test_backlog_cannot_masquerade_as_recovery(self):
        events=[dict(player_id=1,epoch=1,sequence=n+1,authority_tick=n+1,time_ns=1_000_000_000+n*16_666_667,queued=4,source='actual') for n in range(60)]
        generated={(1,1,n+1):e['time_ns']-1 for n,e in enumerate(events)}
        release=events[35]['time_ns']-2
        self.assertEqual(recovery_actual_intervals(events,generated,[1],release)[1],[])

    def test_wrong_accepted_reload_cannot_extend_existing_reload(self):
        ds=[(1,action(1,1)),(1,action(2,2,kind=1)),(1,action(3,20,kind=1))]
        self.assertIn('Accepted action during active reload',validate_accepted_actions(ds,{(1,1):0}))

    def test_wrong_accepted_full_reload_and_early_shot_fail(self):
        self.assertIn('Accepted reload with full magazine',validate_accepted_actions([(1,action(1,1,kind=1))],{(1,1):0}))
        ds=[(1,action(1,1)),(1,action(2,10))]
        self.assertIn('Accepted shot before10tick cooldown',validate_accepted_actions(ds,{(1,1):0}))

    def test_client_stall_cannot_be_clean_or_life_clamp(self):
        event=dict(kind='runtime_gap',frame_seconds=.108,dropped_seconds=.09,player_id=2,epoch=2,time_ns=1000,authority_tick=200)
        errors,clamps=client_disturbance([event],[dict(time_ns=1000,frame_seconds=.108)],[],30)
        self.assertTrue(errors);self.assertFalse(clamps)

    def test_normal_thirty_fps_life_seed_clamp_is_explicit(self):
        reset=dict(player_id=2,epoch=2,time_ns=1_000_000_000,authority_tick=200)
        event=dict(kind='runtime_gap',frame_seconds=1/30,dropped_seconds=1/60,player_id=2,epoch=2,time_ns=1_020_000_000,authority_tick=201)
        errors,clamps=client_disturbance([event],[dict(time_ns=1_020_000_000,frame_seconds=1/30)],[reset],30)
        self.assertFalse(errors);self.assertEqual(len(clamps),1)
        errors,_=client_disturbance([event],[],[],30)
        self.assertTrue(errors)

    def test_fault_expiry_requires_original_reference_and_generation_inside_fault(self):
        sub=dict(player_id=1,time_ns=1_500_000_000,request=dict(action_id=1,observed_tick=10))
        d=dict(rejection=2,resolved_tick=30);fault=dict(start_ns=1_000_000_000,release_ns=2_000_000_000)
        ticks={10:1_000_000_000,30:1_333_333_333}
        self.assertIsNotNone(fault_expiry_exception(sub,d,fault,ticks))
        self.assertIsNone(fault_expiry_exception(dict(sub,time_ns=2_000_000_001),d,fault,ticks))
        self.assertIsNone(fault_expiry_exception(sub,d,fault,{10:1_100_000_000,30:1_333_333_333}))
        self.assertIsNone(fault_expiry_exception(sub,dict(d,rejection=0),fault,ticks))

    def test_hold_observation_is_not_an_actual_datagram(self):
        for label in ('received','cross_life_request_held','cross_life_result_held','dropped_shot'):
            self.assertFalse(is_wire_send_event(dict(event=label)))
        for label in ('forwarded','conflicting_kind','cross_life_result_released'):
            self.assertTrue(is_wire_send_event(dict(event=label)))

    def test_bounded_matrix_contains_declared_axes(self):
        cases=matrix();self.assertEqual(len(cases),25)
        self.assertEqual({c['fps'] for c in cases if c['mode']=='baseline'},{30,60,144})
        self.assertTrue({'network0','network20','network40','burst2','loss-shot','loss-result','loss-ack','duplicate-reorder-conflict','drain-stall'} <= {c['mode'] for c in cases})
        for mode in ('upstream','downstream','socket-path','gateway','host-ipc'):
            self.assertEqual({c['milliseconds'] for c in cases if c['mode']==mode},{250,1000})

    def test_start_phase_record_per_session_is_carried_or_explicitly_absent(self):
        epoch={'player_id':7,'movement_epoch':1,'life_generation':1,'status':'shift_armed','host_wait_micros':2900,
               'client_wait_seconds':.0029,'client_shift_seconds':.0007,'client_first_steady_ns':5_000_000_000}
        respawn=dict(epoch,life_generation=2,status='not_armed_or_invalidated',client_wait_seconds=None,client_shift_seconds=None,
                     client_first_steady_ns=None)
        result=start_phase_by_player({'clients':[{'player_id':7,'start_phase':{'supported':True,'epochs':[epoch,respawn]}},
                                                 {'player_id':8}]})
        self.assertEqual(result['7']['first_epoch_status'],'shift_armed')
        self.assertEqual(result['7']['status_counts'],{'shift_armed':1,'not_armed_or_invalidated':1})
        self.assertIsNone(result['7']['first_epoch']['client_set_seconds_before_measurement'])
        self.assertIsNone(result['7']['measured_epoch'])
        self.assertEqual(result['8']['status'],'absent')
        self.assertIn('clients[1].start_phase missing',result['8']['reason'])
        self.assertEqual(result['7']['reseed_detection']['status'],'not_checked')

    def test_start_phase_stall_reseed_in_the_shared_client_trace_cancels_only_its_own_record(self):
        from start_phase_evidence import reseed_evidence
        epoch={'player_id':7,'movement_epoch':1,'life_generation':1,'status':'shift_armed','host_wait_micros':2900,
               'client_wait_seconds':.0029,'client_shift_seconds':.0007,'client_first_steady_ns':5_000_000_000}
        other=dict(epoch,player_id=8)
        def generated(player,sequence,seeded,time_ns):
            return {'kind':'generated','player_id':player,'epoch':1,'life_generation':1,'sequence':sequence,
                    'seeded_neutral':seeded,'time_ns':time_ns}
        # Both players share clients-commands.jsonl; only player 7 reseeds after its decision.
        events=[generated(7,1,True,4_000_000_000),generated(7,2,True,4_000_000_000),generated(8,1,True,4_000_000_000),
                generated(8,2,True,4_000_000_000),generated(7,30,True,6_000_000_000),generated(7,31,True,6_000_000_000)]
        result=start_phase_by_player({'clients':[{'player_id':7,'start_phase':{'supported':True,'epochs':[epoch]}},
                                                 {'player_id':8,'start_phase':{'supported':True,'epochs':[other]}}]},
                                     reseed_evidence(events,'clients-commands.jsonl'))
        self.assertEqual(result['7']['first_epoch_status'],'cancelled_by_reseed')
        self.assertEqual(result['7']['first_epoch']['recorded_status'],'shift_armed')
        self.assertAlmostEqual(result['7']['first_epoch']['cancelled_by_reseed']['seconds_after_decision'],1)
        self.assertEqual(result['8']['first_epoch_status'],'shift_armed')
        self.assertEqual(result['8']['reseed_detection']['stall_reseeds'],{'7/1/1':1})

    def test_gameplay_reader_does_not_depend_on_the_gui_latency_reader(self):
        code='import sys,gameplay_evidence;sys.exit(int("presentation_evidence" in sys.modules))'
        self.assertEqual(subprocess.run([sys.executable,'-c',code],cwd=Path(__file__).resolve().parent).returncode,0)

if __name__=='__main__':unittest.main()
