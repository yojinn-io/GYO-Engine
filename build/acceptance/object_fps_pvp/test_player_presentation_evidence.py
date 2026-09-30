"""Counterexamples for the character acceptance evidence, without GUI fixtures."""
import unittest
from player_presentation_evidence import validate_samples


def sample(index, distance, phase, *, holding=False, backward=False):
    return {'frame_id':index,'remote':{'player_id':1,'position':[0,0,distance],
        'character':{'player_id':1,'ready':True,'prepared_instances':1,'body_meshes':4,'hair_meshes':1,
        'weapon_meshes':1,'upper_body_mask_count':20,'foot_anchor':[0,-1,0],
        'weapon_world_position':[0,1,distance],'scale':.9,'stride_distance':2.8,'jog_duration_seconds':2.8/3,'phase_seconds':phase%(.933333),
        'unwrapped_phase_seconds':phase,'signed_distance':distance,'total_distance':abs(distance),
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

    def test_nonfinite_or_zero_jog_duration_cannot_bypass_phase_check(self):
        for duration in (float('nan'),float('inf'),0):
            with self.subTest(duration=duration):
                value=sample(1,.1,.1)
                value['remote']['character']['jog_duration_seconds']=duration
                self.assertTrue(validate_samples([sample(0,0,0),value])['errors'])

    def test_calibrated_stride_is_used_instead_of_fixed_time_rate(self):
        values=[sample(0,0,0),sample(1,.1,.2/3)]
        for value in values:value['remote']['character']['stride_distance']=1.4
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

    def test_skipped_frames_do_not_infer_path_from_equal_endpoints(self):
        value=sample(3,.2,.2/3,backward=True)
        value['remote']['position'][2]=0
        value['remote']['character']['distance_delta']=.01
        self.assertEqual(validate_samples([sample(0,0,0),value])['errors'],[])


if __name__=='__main__':unittest.main()
