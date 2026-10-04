"""Counterexamples for the character acceptance evidence, without GUI fixtures."""
import unittest
import player_presentation_evidence as evidence
from player_presentation_evidence import validate_samples


def sample(index, distance, phase, *, holding=False, backward=False):
    return {'frame_id':index,'remote':{'player_id':1,'position':[0,0,distance],
        'character':{'player_id':1,'ready':True,'prepared_instances':1,'body_meshes':4,'hair_meshes':1,
        'weapon_meshes':1,'upper_body_mask_count':20,'foot_anchor':[0,-1,0],
        'weapon_world_position':[0,1,distance],'scale':.9,'cycle_distance':3.0,'jog_weight':.4,'phase_cycles':phase%1,
        'unwrapped_phase_cycles':phase,'signed_distance':distance,'total_distance':abs(distance),
        'distance_delta':abs(distance),'speed':3,'playback_rate':1,'phase_reset':index==0,
        'reset_count':1,'reset_reason':'new-player' if index==0 else '',
        'holding':holding,'backward':backward}}}


class PlayerEvidenceTests(unittest.TestCase):
    def test_actual_distance_phase_passes(self):
        result=validate_samples([sample(0,0,0),sample(1,.1,.1/3)])
        self.assertEqual(result['errors'],[])

    def test_time_based_phase_at_rest_is_rejected(self):
        result=validate_samples([sample(0,0,0),sample(1,0,.1)])
        self.assertTrue(any('Stationary' in error for error in result['errors']))

    def test_wrong_stride_is_rejected(self):
        result=validate_samples([sample(0,0,0),sample(1,.1,.1)])
        self.assertTrue(any('proportional' in error for error in result['errors']))

    def test_hold_cannot_walk_in_place(self):
        result=validate_samples([sample(0,0,0,holding=True),sample(1,0,.1,holding=True)])
        self.assertTrue(any('hold' in error for error in result['errors']))

    def test_backward_reverses_existing_jog(self):
        result=validate_samples([sample(0,0,0),sample(1,-.1,-.1/3,backward=True)])
        self.assertEqual(result['errors'],[])
        self.assertEqual(result['backward_pairs'],1)

    def test_backward_forward_phase_fails(self):
        result=validate_samples([sample(0,0,0),sample(1,-.1,.1/3,backward=True)])
        self.assertTrue(any('Backward' in error for error in result['errors']))

    def test_reference_anchor_does_not_follow_animation(self):
        value=sample(1,.1,.1/3)
        value['remote']['character']['foot_anchor'][1]=-.9
        result=validate_samples([sample(0,0,0),value])
        self.assertTrue(any('anchor' in error for error in result['errors']))

    def test_gpu_attachments_must_really_be_submitted(self):
        value=sample(0,0,0)
        value['remote']['character']['weapon_meshes']=0
        self.assertTrue(validate_samples([value])['errors'])

    def test_discontinuity_cannot_count_as_walked_distance(self):
        value=sample(1,5,0)
        value['remote']['character'].update(phase_reset=True,reset_count=2,reset_reason='discontinuity')
        result=validate_samples([sample(0,0,0),value])
        self.assertTrue(any('discontinuous' in error for error in result['errors']))

    def test_submitted_geometry_is_the_distance_source(self):
        value=sample(1,.1,.1/3)
        value['remote']['position'][2]=.2
        result=validate_samples([sample(0,0,0),value])
        self.assertTrue(any('submitted position' in error for error in result['errors']))

    def test_nonfinite_or_zero_cycle_distance_cannot_bypass_phase_check(self):
        for distance in (float('nan'),float('inf'),0):
            with self.subTest(distance=distance):
                value=sample(1,.1,.1)
                value['remote']['character']['cycle_distance']=distance
                self.assertTrue(validate_samples([sample(0,0,0),value])['errors'])

    def test_blended_cycle_distance_is_used_instead_of_fixed_time_rate(self):
        values=[sample(0,0,0),sample(1,.1,.1/1.5)]
        for value in values:value['remote']['character']['cycle_distance']=1.5
        self.assertEqual(validate_samples(values)['errors'],[])

    def test_reset_must_clear_old_phase_not_only_last_delta(self):
        value=sample(1,0,.2)
        value['remote']['character'].update(phase_reset=True,reset_count=2,reset_reason='discontinuity')
        self.assertTrue(any('retained old phase' in error for error in validate_samples([sample(0,0,0),value])['errors']))

    def test_hold_recovery_cannot_omit_first_actual_displacement(self):
        value=sample(1,0,0)
        value['remote']['position'][2]=.1
        result=validate_samples([sample(0,0,0,holding=True),value])
        self.assertTrue(any('submitted position' in error for error in result['errors']))

    def test_jog_weight_outside_the_blend_is_rejected(self):
        for weight in (-.1,1.1,float('nan')):
            with self.subTest(weight=weight):
                value=sample(1,.1,.1/3)
                value['remote']['character']['jog_weight']=weight
                self.assertTrue(validate_samples([sample(0,0,0),value])['errors'])

    def test_skipped_frames_do_not_infer_path_from_equal_endpoints(self):
        value=sample(3,.2,.2/3,backward=True)
        value['remote']['position'][2]=0
        value['remote']['character']['distance_delta']=.01
        self.assertEqual(validate_samples([sample(0,0,0),value])['errors'],[])



class ScriptTimingTests(unittest.TestCase):
    SCHEDULE = [{"ordinal": 1, "seconds": .75, "name": "forward"}, {"ordinal": 2, "seconds": 1.55, "name": "stop"}]

    def test_windows_follow_when_the_actor_input_really_fired(self):
        # Old misjudgement: a 120 ms late wake delays the actor's key, but a fixed script window
        # still starts at 0.95 s, only 80 ms after the key really entered.
        frames = [{"event": "", "seconds": .7}, {"event": "forward", "seconds": .87}, {"event": "stop", "seconds": 1.67}]
        times = evidence.event_times(self.SCHEDULE, frames)
        begin, end = evidence.anchored(times, .95, 1, 1.5, 2)
        self.assertAlmostEqual(begin, 1.07)
        self.assertAlmostEqual(end, 1.62)
        self.assertAlmostEqual(begin - times[1]["actual"], .2)
        # A punctual run keeps exactly the declared window.
        punctual = evidence.event_times(self.SCHEDULE, [{"event": "forward", "seconds": .75}, {"event": "stop", "seconds": 1.55}])
        self.assertEqual(evidence.anchored(punctual, .95, 1, 1.5, 2), (.95, 1.5))

    def test_missing_or_reordered_actor_input_is_still_an_error(self):
        # Real defect: an input that never fired, or fired out of order, cannot be re-anchored away.
        for frames in ([{"event": "forward", "seconds": .75}],
                       [{"event": "stop", "seconds": .75}, {"event": "forward", "seconds": 1.55}]):
            with self.subTest(frames=frames):
                with self.assertRaises(ValueError):
                    evidence.event_times(self.SCHEDULE, frames)

    def test_cadence_below_85_percent_is_invalid_not_failed(self):
        self.assertEqual(evidence.cadence_status(120, 144, "none"), "invalid_capacity")
        self.assertEqual(evidence.cadence_status(122.4, 144, "none"), "valid")
        self.assertEqual(evidence.cadence_status(30, 144, "actual scene GPU readback"), "not_applicable")

if __name__=='__main__':unittest.main()
