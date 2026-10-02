"""Bounded runner lifecycle tests; no sockets, GUI or real-clock measurement."""

from contextlib import nullcontext, redirect_stdout
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import run_timing
import run_weapon_short


class TimingRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='pvp-timing-runner-')
        self.addCleanup(self.temporary.cleanup)
        self.args = SimpleNamespace(output=Path(self.temporary.name)/'new-run', rounds=3,
                                    fps=60, duration=120., events=200, gui=True,
                                    combat=False, short=False, soak=False, report_only=False)

    def run_rounds(self, function):
        with redirect_stdout(io.StringIO()):
            return run_timing.execute_rounds(self.args, function)

    def results(self):
        return json.loads((self.args.output/'results.json').read_text())

    def test_all_rounds_pass_and_have_readable_summary(self):
        calls = []
        def measurement(args, output, fps):
            calls.append(output.name)
            return {'overall_passed': True, 'actual_fraction': 1., 'disturbed': False,
                    'actual_p50_ms': 45., 'actual_p95_ms': 50.,
                    'presentation': {'p50_seconds': .048, 'p95_seconds': .05,
                                     'matched_event_count': 200, 'planned_event_count': 200}}
        self.assertEqual(self.run_rounds(measurement), 0)
        self.assertEqual(calls, ['round-1', 'round-2', 'round-3'])
        self.assertTrue(self.results()['passed'])
        self.assertFalse(self.results()['full_product_acceptance'])
        self.assertIn('48.000 / 50.000', (self.args.output/'summary.md').read_text())

    def test_failed_round_keeps_evidence_and_marks_remaining_not_run(self):
        evidence = {'overall_passed': False, 'passed': True,
                    'presentation': {'passed': False, 'errors': ['P50 exceeds 50 ms']}}
        def measurement(args, output, fps):
            output.mkdir()
            (output/'round.json').write_text(json.dumps(evidence))
            (output/'create.log').write_text('original raw log\n')
            raise RuntimeError('fixed threshold failed')
        self.assertEqual(self.run_rounds(measurement), 1)
        result = self.results()
        self.assertFalse(result['passed'])
        self.assertEqual([item['status'] for item in result['rounds']], ['failed', 'not_run', 'not_run'])
        self.assertEqual(result['rounds'][0]['evidence'], evidence)
        self.assertIn('fixed threshold failed', (self.args.output/'round-1/runner-failure.txt').read_text())
        self.assertEqual((self.args.output/'round-1/create.log').read_text(), 'original raw log\n')
        self.assertFalse((self.args.output/'round-2').exists())

    def test_startup_failure_also_writes_terminal_summary(self):
        def measurement(args, output, fps):
            raise OSError('could not start service')
        self.assertEqual(self.run_rounds(measurement), 1)
        self.assertIn('could not start service', self.results()['rounds'][0]['error'])
        self.assertTrue((self.args.output/'summary.md').is_file())

    def test_interrupt_also_marks_remaining_not_run(self):
        def measurement(args, output, fps):
            raise KeyboardInterrupt()
        self.assertEqual(self.run_rounds(measurement), 1)
        self.assertIn('KeyboardInterrupt', self.results()['rounds'][0]['error'])

    def test_existing_nonempty_output_is_never_overwritten(self):
        self.args.output.mkdir()
        marker = self.args.output/'previous-evidence.txt'
        marker.write_text('keep')
        with self.assertRaisesRegex(ValueError, 'never overwritten'):
            self.run_rounds(lambda *args: self.fail('must not run'))
        self.assertEqual(marker.read_text(), 'keep')

    def test_returned_failure_also_stops_without_exception(self):
        self.assertEqual(self.run_rounds(lambda *args: {'overall_passed': False}), 1)
        self.assertEqual([item['status'] for item in self.results()['rounds']], ['failed', 'not_run', 'not_run'])

    @staticmethod
    def phase_epoch(**values):
        epoch = {'player_id': 1, 'movement_epoch': 1, 'life_generation': 1, 'status': 'shift_armed', 'host_wait_micros': 3214,
                 'client_wait_seconds': .003214, 'client_shift_seconds': .0012, 'client_skip_reason': None}
        epoch.update(values)
        return epoch

    @staticmethod
    def windows(**extra):
        return {'window': {'create': {'status': 'recorded', 'placement': 'usable_bounds_top_left', 'final_position': [0, 53],
                                      'sync': 'succeeded'},
                           'join': {'status': 'recorded', 'placement': 'usable_bounds_bottom_right', 'final_position': [256, 180],
                                    'sync': 'timed_out'}}, **extra}

    def test_summary_names_start_phase_and_window_state_per_round(self):
        armed = self.phase_epoch()
        late = self.phase_epoch(player_id=2, status='skipped_host_late', host_wait_micros=25000, client_wait_seconds=.025,
                                client_shift_seconds=None, client_skip_reason='host_late')
        measured = self.phase_epoch(movement_epoch=3, status='not_armed_or_invalidated', host_wait_micros=4000,
                                    client_wait_seconds=None, client_shift_seconds=None)
        rounds = iter([
            {'start_phase': {'create': {'status': 'recorded', 'first_epoch': armed, 'measured_epoch': 1,
                                        'measured_epoch_record': armed, 'epoch_count': 1,
                                        'conflicting_frames': {'host': 0, 'client': 0}, 'dropped_observations': 0},
                             'join': {'status': 'recorded', 'first_epoch': late, 'measured_epoch': 1, 'measured_epoch_record': late,
                                      'epoch_count': 2, 'status_counts': {'skipped_host_late': 1, 'host_wait_absent': 1}}},
             **self.windows(window_disturbed=False, window_disturbances=[])},
            {'start_phase': {'create': {'status': 'absent', 'reason': 'create-start-phase.json missing', 'measured_epoch': 1},
                             'join': {'status': 'recorded', 'first_epoch': armed, 'measured_epoch': 3, 'measured_epoch_record': measured,
                                      'epoch_count': 2, 'status_counts': {'shift_armed': 1, 'not_armed_or_invalidated': 1},
                                      'conflicting_frames': {'host': 2, 'client': 0}, 'dropped_observations': 3}},
             **self.windows(window_disturbed=False, window_disturbances=[])},
            {'start_phase': {'create': {'status': 'recorded', 'first_epoch': armed, 'measured_epoch': 7, 'measured_epoch_record': None,
                                        'measured_epoch_reason': 'no record for measured epoch 7', 'epoch_count': 1},
                             'join': {'status': 'invalid', 'reason': 'join-start-phase.json: client_first_steady_ns must be an integer'}},
             'window': {'create': {'status': 'recorded', 'placement': 'p', 'final_position': None}, 'join': 'garbage'},
             'window_disturbed': None, 'window_disturbances': ['join: window evidence invalid: truncated']}])
        def measurement(args, output, fps):
            return {'overall_passed': True, 'presentation': next(rounds)}
        self.args.report_only = True
        self.assertEqual(self.run_rounds(measurement), 0)
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('Round 1 start phase: create measured epoch 1/life 1 shift_armed: Host wait 3.214 ms, Client wait 3.214 ms, '
                      'shift 1.200 ms; join measured epoch 1/life 1 skipped_host_late: Host wait 25.000 ms, Client wait 25.000 ms, '
                      "shift unset, skip reason host_late; 2 epochs {'skipped_host_late': 1, 'host_wait_absent': 1}", summary)
        self.assertIn('a stall reseed that cancelled the start phase reads cancelled_by_reseed', summary)
        self.assertIn('slew completion is not observable', summary)
        self.assertIn('Window interference note: ', summary)
        self.assertIn('Round 1 window: no window-state interference detected (create usable_bounds_top_left at (0, 53); '
                      'join usable_bounds_bottom_right at (256, 180), sync timed_out)', summary)
        self.assertNotIn('window: clean', summary)
        self.assertNotIn('platform-default placement', summary)
        self.assertIn('Round 2 start phase: create absent (create-start-phase.json missing); measured epoch 1; '
                      'join measured epoch 3/life 1 not_armed_or_invalidated: Host wait 4.000 ms, Client wait unset, shift unset; '
                      'first epoch 1/life 1 shift_armed', summary)
        self.assertIn('conflicting frames host 2/client 0; dropped observations 3', summary)
        self.assertIn('Round 3 start phase: create measured epoch 7 not recorded (no record for measured epoch 7); first epoch 1/life 1 '
                      'shift_armed', summary)
        self.assertIn('join invalid (join-start-phase.json: client_first_steady_ns must be an integer)', summary)
        self.assertIn('Round 3 window: unknown (create p at ()): join: window evidence invalid: truncated', summary)
        self.assertNotIn('DISTURBED', summary)
        saved = self.results()['rounds'][0]['evidence']['presentation']
        self.assertEqual(saved['start_phase']['join']['first_epoch']['host_wait_micros'], 25000)

    def test_window_gate_invalidates_only_counted_rounds(self):
        for disturbed, report_only, status, passed in (
                (False, False, 'clean', True), (True, False, 'invalid_window_disturbed', False),
                (None, False, 'invalid_window_evidence_missing', False), (True, True, 'flagged_window_disturbed', True),
                (None, True, 'flagged_window_evidence_missing', True), ('yes', False, 'invalid_window_evidence_missing', False)):
            with self.subTest(disturbed=disturbed, report_only=report_only):
                evidence = {'overall_passed': True, 'errors': [],
                            'presentation': {'window_disturbed': disturbed, 'window_disturbances': ['join: 1 OS moved event(s) during measurement']}}
                gate = run_timing.gate_window_evidence(evidence, report_only)
                self.assertEqual((gate['status'], evidence['overall_passed']), (status, passed))
                self.assertEqual(evidence['window_gate'], gate)
                self.assertEqual(bool(evidence['errors']), not passed)
                if status != 'clean':
                    self.assertIn('join: 1 OS moved event(s) during measurement', gate['reasons'])
        missing = {'overall_passed': True}
        self.assertEqual(run_timing.gate_window_evidence(missing, False)['reasons'], ['presentation evidence carries no window record'])
        # The round's own verdict before the gate is kept beside the gate status.
        for underlying in (True, False, None):
            with self.subTest(underlying=underlying):
                evidence = {'overall_passed': underlying, 'presentation': {'window_disturbed': True, 'window_disturbances': ['x']}}
                self.assertIs(run_timing.gate_window_evidence(evidence, False)['thresholds_passed'], underlying)
                self.assertFalse(evidence['overall_passed'])

    def test_window_gate_names_invalid_and_unknown_window_evidence_apart_from_missing(self):
        invalid_reason = 'create: window evidence invalid: create-report.txt: duplicate window_* key(s): window_final_position'
        cases = (
            ({'create': {'status': 'invalid', 'reason': 'duplicate'}, 'join': {'status': 'recorded'}}, [invalid_reason],
             'window_evidence_invalid'),
            ({'create': {'status': 'absent'}, 'join': {'status': 'invalid'}}, [], 'window_evidence_invalid'),
            ({'create': {'status': 'absent'}, 'join': {'status': 'absent'}}, [], 'window_evidence_missing'),
            ({'create': {'status': 'absent'}, 'join': {'status': 'recorded'}}, [], 'window_evidence_missing'),
            ({'create': 'garbage', 'join': {'status': 'recorded'}}, [], 'window_evidence_unknown'),
            ({}, [], 'window_evidence_unknown'),
            ('garbage', [], 'window_evidence_unknown'))
        for window, reasons, kind in cases:
            for report_only in (False, True):
                with self.subTest(window=window, report_only=report_only):
                    evidence = {'overall_passed': True, 'errors': [],
                                'presentation': {'window_disturbed': None, 'window': window, 'window_disturbances': reasons}}
                    gate = run_timing.gate_window_evidence(evidence, report_only)
                    self.assertEqual(gate['status'], ('flagged_' if report_only else 'invalid_') + kind)
                    self.assertEqual(evidence['overall_passed'], report_only)
                    self.assertEqual(gate['reasons'], reasons or [run_timing.WINDOW_EVIDENCE_REASONS[kind]])

        def measurement(args, output, fps):
            evidence = {'overall_passed': True, 'presentation': {
                'passed': True, 'window_disturbed': None, 'window_disturbances': [invalid_reason],
                'window': {'create': {'status': 'invalid', 'reason': 'duplicate'},
                           'join': {'status': 'recorded', 'placement': 'usable_bounds_bottom_right', 'final_position': [1, 2]}}}}
            run_timing.gate_window_evidence(evidence, args.report_only)
            return evidence
        self.args.rounds = 1
        self.assertEqual(self.run_rounds(measurement), 1)
        record = self.results()['rounds'][0]
        self.assertEqual((record['status'], record['passed'], record['underlying_status']),
                         ('invalid_window_evidence_invalid', False, 'passed'))
        self.assertEqual(record['window_reasons'], [invalid_reason])
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('| 1 | invalid_window_evidence_invalid (thresholds passed) |', summary)
        self.assertIn('-> counted round invalid_window_evidence_invalid (underlying threshold result: passed)', summary)

    def test_summary_names_reseed_cancellation_and_shift_applied_seconds(self):
        traced = self.phase_epoch(status='cancelled_by_reseed', recorded_status='shift_armed', host_wait_micros=2606,
                                  client_wait_seconds=.002606, client_shift_seconds=-.000912,
                                  cancelled_by_reseed={'source': 'client_trace_fallback', 'trace': 'create-commands.jsonl',
                                                       'steady_ns': 5, 'frame': None, 'reseed_sequence': 42,
                                                       'after_decision': True, 'seconds_after_decision': .532779,
                                                       'stall_reseeds_in_epoch': 1, 'seconds_before_measurement': 1.25},
                                  measurement_window={'status': 'cancelled_by_reseed', 'states': ['cancelled_by_reseed'],
                                                      'state_seconds': {'cancelled_by_reseed': 15.5},
                                                      'withdrawn_during_measurement': False,
                                                      'cancelled_during_measurement': True, 'shift_applied_seconds': 0.,
                                                      'unaligned_seconds': 15.5, 'observed_seconds': 15.5,
                                                      'window_seconds': 16.})
        product = self.phase_epoch(player_id=2, status='cancelled_by_reseed', host_wait_micros=5522, client_wait_seconds=.005522,
                                   client_shift_seconds=.0021, client_skip_reason=None,
                                   cancelled_by_reseed={'source': 'product', 'steady_ns': 9, 'frame': 400,
                                                        'seconds_before_measurement': -3.5},
                                   measurement_window={'status': 'cancelled_by_reseed',
                                                       'states': ['shift_armed', 'cancelled_by_reseed'],
                                                       'state_seconds': {'shift_armed': 3.5, 'cancelled_by_reseed': 12.5},
                                                       'withdrawn_during_measurement': False,
                                                       'cancelled_during_measurement': True, 'shift_applied_seconds': 3.5,
                                                       'unaligned_seconds': 12.5, 'observed_seconds': 16.,
                                                       'window_seconds': 16.})
        evidence = {'overall_passed': True, 'presentation': {'start_phase': {
            'create': {'status': 'recorded', 'first_epoch': traced, 'measured_epoch': 1, 'measured_epoch_record': traced,
                       'epoch_count': 1},
            'join': {'status': 'recorded', 'first_epoch': product, 'measured_epoch': 1, 'measured_epoch_record': product,
                     'epoch_count': 1}}}}
        self.args.rounds, self.args.report_only = 1, True
        self.assertEqual(self.run_rounds(lambda *args: evidence), 0)
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('create measured epoch 1/life 1 cancelled_by_reseed (recorded shift_armed): Host wait 2.606 ms, '
                      'Client wait 2.606 ms, shift -0.912 ms, cancelled by a stall reseed 0.533 s after the decision '
                      '(Client trace create-commands.jsonl: neutral reseed from sequence 42), 1.250 s before measurement began; '
                      'during measurement: cancelled_by_reseed (measured epoch CANCELLED by a stall reseed '
                      'for 15.500 s of the 16.000 s window); shift applied 0.000 s, unaligned 15.500 s '
                      '(observed 15.500 s of the 16.000 s window)', summary)
        self.assertIn('join measured epoch 1/life 1 cancelled_by_reseed: Host wait 5.522 ms, Client wait 5.522 ms, '
                      'shift 2.100 ms, cancelled by a stall reseed (reported by the product from frame 400), 3.500 s after '
                      'measurement began; during measurement: cancelled_by_reseed (measured epoch CANCELLED by a stall reseed '
                      'for 12.500 s of the 16.000 s window); shift applied 3.500 s, unaligned 12.500 s', summary)
        self.assertNotIn('unaligned 12.500 s (observed', summary)
        phase = {'status': 'recorded', 'first_epoch': traced, 'measured_epoch': 1, 'measured_epoch_record': traced,
                 'epoch_count': 1, 'unattributed_epochs': [{'player_id': 0}], 'unattributed_client_frames': 2,
                 'reseed_detection': {'status': 'invalid', 'source': 'create-commands.jsonl', 'reason': 'missing trace_end'}}
        self.assertIn('; 1 record(s) and 2 active Client frame(s) without a player id excluded; reseed cancellation not '
                      'checked (invalid: missing trace_end)', run_timing._phase_role('create', phase))
        phase['reseed_detection'] = {'status': 'recorded'}
        self.assertNotIn('not checked', run_timing._phase_role('create', phase))
        older = run_timing._measurement_state({'status': 'not_recorded', 'reason': 'record carries no client state history '
                                               '(older recorder)', 'cancelled_before_window': True})
        self.assertEqual(older, '; during measurement: not_recorded (record carries no client state history (older recorder)) '
                                '(cancelled before the window began, so no shift was applied in this epoch and life while measured)')
        self.assertIn('cancelled by a stall reseed before any decision',
                      run_timing._phase_epoch(self.phase_epoch(status='cancelled_by_reseed', cancelled_by_reseed={
                          'source': 'client_trace_fallback', 'trace': 't', 'after_decision': False, 'reseed_sequence': 7})))

    def test_wayland_warning_is_printed_only_for_counted_gui_rounds_in_wayland_sessions(self):
        wayland = {'WAYLAND_DISPLAY': 'wayland-0'}
        self.assertIn('OCCLUDED', run_timing.wayland_warning(True, False, wayland, 'linux'))
        self.assertIn('invalid_window_disturbed', run_timing.wayland_warning(True, False, {'XDG_SESSION_TYPE': 'wayland'}, 'linux'))
        for gui, report_only, environ, platform in ((True, True, wayland, 'linux'), (False, False, wayland, 'linux'),
                                                    (True, False, {'XDG_SESSION_TYPE': 'x11'}, 'linux'),
                                                    (True, False, wayland, 'darwin'), (True, False, wayland, 'win32')):
            with self.subTest(gui=gui, report_only=report_only, environ=environ, platform=platform):
                self.assertIsNone(run_timing.wayland_warning(gui, report_only, environ, platform))
        self.assertIn('Linux/Wayland', run_timing.WINDOW_INTERFERENCE_NOTE)
        self.assertIn('OCCLUDED', run_timing.WINDOW_INTERFERENCE_NOTE)
        self.args.rounds, self.args.gui = 1, False
        self.assertEqual(self.run_rounds(lambda *args: {'overall_passed': True}), 0)
        self.assertNotIn('Window interference note', (self.args.output/'summary.md').read_text())
        self.assertIsNone(self.results()['window_interference_note'])

    def test_counted_disturbed_round_keeps_explicit_status_and_evidence(self):
        def measurement(args, output, fps):
            # As run_round: round.json is written, then the counted failure raises.
            evidence = {'overall_passed': True, 'passed': True, 'disturbed': False, 'errors': [],
                        'presentation': {'passed': True, 'p50_seconds': .03, 'p95_seconds': .05, 'window_disturbed': True,
                                         'window_disturbances': ['join: 2 OS occluded event(s) during measurement'],
                                         'window': {}}}
            run_timing.gate_window_evidence(evidence, args.report_only)
            output.mkdir()
            (output/'round.json').write_text(json.dumps(evidence))
            (output/'join-report.txt').write_text('raw\n')
            raise RuntimeError(f"Counted GUI round is {evidence['window_gate']['status']} (window interference); see round.json")
        self.assertEqual(self.run_rounds(measurement), 1)
        rounds = self.results()['rounds']
        self.assertEqual([item['status'] for item in rounds], ['invalid_window_disturbed', 'not_run', 'not_run'])
        self.assertFalse(rounds[0]['passed'])
        self.assertEqual(rounds[0]['window_reasons'], ['join: 2 OS occluded event(s) during measurement'])
        self.assertEqual(rounds[0]['evidence']['window_gate']['status'], 'invalid_window_disturbed')
        self.assertEqual((self.args.output/'round-1/join-report.txt').read_text(), 'raw\n')
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('| 1 | invalid_window_disturbed (thresholds passed) | False |', summary)
        self.assertIn('Round 1 window: window interference: join: 2 OS occluded event(s) during measurement '
                      '-> counted round invalid_window_disturbed (underlying threshold result: passed)', summary)
        self.assertEqual((rounds[0]['thresholds_passed'], rounds[0]['underlying_status']), (True, 'passed'))

    def test_summary_names_withdrawal_pending_resets_and_frame_intervals(self):
        withdrawn = self.phase_epoch(status='shift_withdrawn_below_cut', host_wait_micros=9000, client_wait_seconds=.009,
                                     client_shift_seconds=.005, client_armed_shift_seconds=.005, withdrawn_frames=96,
                                     withdrawn_first_frame=400, withdrawals=1, restorations=1, last_client_state='shift_armed',
                                     measurement_window={'status': 'withdrawn_below_cut', 'states': ['shift_armed', 'withdrawn_below_cut'],
                                                         'state_seconds': {'shift_armed': 14., 'withdrawn_below_cut': 2.},
                                                         'withdrawn_during_measurement': True, 'window_seconds': 16.})
        pending = self.phase_epoch(player_id=2, status='pending_frame_window', host_wait_micros=9000, client_wait_seconds=None,
                                   client_shift_seconds=None, client_active_frames_at_last_undecided=6,
                                   measurement_window={'status': 'undecided', 'states': ['undecided'],
                                                       'state_seconds': {'undecided': 16.}, 'withdrawn_during_measurement': False,
                                                       'window_seconds': 16.})
        restored = self.phase_epoch(status='shift_armed_after_below_cut_decision', client_shift_seconds=None,
                                    client_skip_reason='frame_rate_below_tick', client_armed_shift_seconds=.0012,
                                    client_first_armed_frame=40)
        frames = {'roles': {'create': {'count': 959, 'median_seconds': .016667, 'p95_seconds': .0171, 'maximum_seconds': .0334,
                                       'over_1_1_tick': 3, 'at_least_2_ticks': 1}}}
        evidence = {'overall_passed': True, 'resets': 2, 'reset_reasons': {'starvation': 1, 'backlog': 1},
                    'presentation_frame_intervals': frames,
                    'presentation': {'start_phase': {
                        'create': {'status': 'recorded', 'first_epoch': withdrawn, 'measured_epoch': 1,
                                   'measured_epoch_record': withdrawn, 'epoch_count': 1},
                        'join': {'status': 'recorded', 'first_epoch': pending, 'measured_epoch': 1,
                                 'measured_epoch_record': pending, 'epoch_count': 2,
                                 'status_counts': {'pending_frame_window': 1, 'shift_armed_after_below_cut_decision': 1}}},
                        'window': {'create': {'status': 'recorded', 'placement': 'platform_default', 'final_position': [320, 180]},
                                   'join': {'status': 'recorded', 'placement': 'platform_default', 'final_position': [320, 180],
                                            'sync': 'not_requested'}},
                        'window_disturbed': False, 'window_disturbances': []}}
        self.args.rounds, self.args.report_only = 1, True
        self.assertEqual(self.run_rounds(lambda *args: evidence), 0)
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('create measured epoch 1/life 1 shift_withdrawn_below_cut: Host wait 9.000 ms, Client wait 9.000 ms, '
                      'shift 5.000 ms, withdrawn below the cut for 96 frame(s) from frame 400 (1 withdrawal(s)/1 restoration(s)), '
                      'last state shift_armed; during measurement: withdrawn_below_cut (measured epoch WITHDRAWN below the cut '
                      'during measurement for 2.000 s of 16.000 s)', summary)
        self.assertIn('join measured epoch 1/life 1 pending_frame_window: Host wait 9.000 ms, Client wait unset, shift unset, '
                      'Client still undecided after 6 active frame(s); during measurement: undecided', summary)
        self.assertIn('withdrawal below 60 FPS reads shift_withdrawn_below_cut', summary)
        self.assertIn('Round 1 movement: Host movement resets during measurement: 2 (backlog 1, starvation 1); '
                      'frame intervals during measurement: create 959 intervals: median 16.667 ms, p95 17.100 ms, '
                      'max 33.400 ms, >1.1 tick 3, >=2 ticks 1; join not recorded', summary)
        self.assertIn('Round 1 window: no window-state interference detected (create platform_default at (320, 180); '
                      'join platform_default at (320, 180), sync not_requested) [platform-default placement (non-macOS): '
                      'the two windows may overlap, and a Wayland compositor may report OCCLUDED for the covered one]', summary)
        self.assertIn('armed later at frame 40 with shift 1.200 ms', run_timing._phase_epoch(restored))

    def test_counted_round_without_window_evidence_is_invalid_not_failed(self):
        def measurement(args, output, fps):
            evidence = {'overall_passed': True, 'presentation': {'passed': True}}
            run_timing.gate_window_evidence(evidence, False)
            return evidence
        self.args.rounds = 1
        self.assertEqual(self.run_rounds(measurement), 1)
        record = self.results()['rounds'][0]
        self.assertEqual((record['status'], record['passed']), ('invalid_window_evidence_missing', False))
        self.assertIn('-> counted round invalid_window_evidence_missing', (self.args.output/'summary.md').read_text())

    def test_not_run_rounds_have_no_start_phase_claim(self):
        self.assertEqual(self.run_rounds(lambda *args: {'overall_passed': False}), 1)
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('Round 1 start phase: not analysed', summary)
        self.assertNotIn('Round 2 start phase', summary)

    def test_run_round_records_counted_disturbed_round_with_threshold_result(self):
        # run_round itself, with services, probes and analysers replaced: no sockets, GUI or clock.
        root = Path(self.temporary.name)
        for name in ('match', 'gateway', 'probe', 'arena'):
            (root/name).write_text(name)
        launched = []

        class Process:
            def __init__(self, command, stdout=None, stderr=None):
                launched.append([str(part) for part in command])
                self.returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                self.returncode = 0 if self.returncode is None else self.returncode
                return self.returncode

            def terminate(self):
                self.returncode = 0

            def kill(self):
                self.returncode = -9

        movement = {'passed': True, 'errors': [], 'disturbed': False, 'resets': 1, 'reset_reasons': {'starvation': 1},
                    'reset_causality': [{'player_id': 2, 'epoch': 2, 'reason': 'starvation', 'time_ns': 1,
                                         'preceding_interference': [{'kind': 'runtime_gap'}]}],
                    'actual_p50_ms': 40., 'actual_p95_ms': 50., 'actual_fraction': 1.,
                    'presentation_frame_intervals': {'roles': {}}}
        for presentation_passed, expected in ((True, 'passed'), (False, 'failed')):
            with self.subTest(thresholds=expected):
                launched.clear()
                args = SimpleNamespace(output=root/f'counted-{expected}', rounds=2, fps=60, duration=16., events=20, gui=True,
                                       combat=False, short=True, soak=False, report_only=False, stall_ms=0,
                                       match=root/'match', gateway=root/'gateway', probe=root/'probe', arena=root/'arena')
                presentation = {'passed': presentation_passed, 'errors': [] if presentation_passed else ['P95 exceeds 80 ms'],
                                'p50_seconds': .03, 'p95_seconds': .05, 'matched_event_count': 20, 'planned_event_count': 20,
                                'window_disturbed': True, 'window': {},
                                'window_disturbances': ['join: 1 OS occluded event(s) during measurement']}
                ready_calls = []

                def ready(process, log_path, listen):
                    # The Match was started and the Gateway not yet.
                    ready_calls.append((launched[-1][0], Path(log_path).name, listen, len(launched)))

                with mock.patch.object(run_timing.subprocess, 'Popen', Process), \
                        mock.patch.object(run_timing, 'wait_for_match_ready', side_effect=ready), \
                        mock.patch.object(run_timing, 'free_port', return_value=5000), \
                        mock.patch.object(run_timing.urllib.request, 'urlopen', return_value=nullcontext()), \
                        mock.patch.object(run_timing, 'analyze_commands', side_effect=lambda output, enforce=True: dict(movement)), \
                        mock.patch('presentation_evidence.analyze_short_latency', return_value=presentation), \
                        redirect_stdout(io.StringIO()):
                    self.assertEqual(run_timing.execute_rounds(args), 1)
                results = json.loads((args.output/'results.json').read_text())
                record = results['rounds'][0]
                self.assertEqual((record['status'], record['passed']), ('invalid_window_disturbed', False))
                self.assertEqual((record['thresholds_passed'], record['underlying_status']), (presentation_passed, expected))
                self.assertEqual(results['rounds'][1]['status'], 'not_run')
                saved = json.loads((args.output/'round-1/round.json').read_text())
                self.assertEqual(saved['window_gate']['status'], 'invalid_window_disturbed')
                self.assertIs(saved['window_gate']['thresholds_passed'], presentation_passed)
                self.assertFalse(saved['overall_passed'])
                self.assertIn('window interference', (args.output/'round-1/runner-failure.txt').read_text())
                self.assertEqual(sorted(command[command.index('--role')+1] for command in launched if '--latency-short' in command),
                                 ['create', 'join'])
                self.assertTrue(any('--movement-trace' in command for command in launched))
                self.assertEqual(ready_calls, [(str(root/'match'), 'match.log', '127.0.0.1:5000', 1)])
                self.assertEqual(launched[1][0], str(root/'gateway'))
                summary = (args.output/'summary.md').read_text()
                self.assertIn(f'| 1 | invalid_window_disturbed (thresholds {expected}) |', summary)
                self.assertIn('Round 1 movement: Host movement resets during measurement: 1 (starvation 1); '
                              'frame intervals during measurement: create not recorded; join not recorded', summary)

    def test_weapon_short_latency_case_takes_the_counted_window_gate(self):
        for disturbed, passed, status in ((False, True, 'clean'), (True, False, 'invalid_window_disturbed'),
                                          (None, False, 'invalid_window_evidence_missing')):
            with self.subTest(disturbed=disturbed):
                result = {'passed': True, 'errors': [], 'window_disturbed': disturbed,
                          'window_disturbances': ['join: 1 OS focus_lost event(s) during measurement'] if disturbed else []}
                gate = run_weapon_short.gate_latency_case(result)
                self.assertEqual((gate['status'], result['passed']), (status, passed))
                self.assertIs(result['window_gate'], gate)
                self.assertTrue(gate['enforced'])
                self.assertIs(gate['thresholds_passed'], True)
                self.assertEqual(bool(result['errors']), not passed)
                self.assertIn('Linux/Wayland', result['window_interference_note'])
        failed = {'passed': False, 'errors': ['P50 exceeds 50 ms'], 'window_disturbed': True, 'window_disturbances': ['x']}
        self.assertIs(run_weapon_short.gate_latency_case(failed)['thresholds_passed'], False)
        self.assertEqual(failed['errors'][0], 'P50 exceeds 50 ms')

    def test_full_gui_requires_clean_round_even_when_latency_passes(self):
        for disturbed, resets, expected in ((False, 0, True), (True, 0, False),
                                            (False, 1, False), (None, 0, False),
                                            (False, None, False)):
            with self.subTest(disturbed=disturbed, resets=resets):
                evidence = {'overall_passed': True, 'passed': True, 'errors': [],
                            'disturbed': disturbed, 'resets': resets,
                            'presentation': {'passed': True}, 'combat': {'passed': True}}
                self.assertEqual(run_timing.require_clean_gui_round(evidence), expected)
                self.assertEqual(evidence['overall_passed'], expected)
                self.assertEqual(evidence['gui_clean_passed'], expected)
                if not expected:
                    self.assertIn('cannot qualify as clean', evidence['errors'][0])


if __name__ == '__main__':
    unittest.main()
