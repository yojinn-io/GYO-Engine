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
        epoch = {'player_id': 1, 'movement_epoch': 1, 'life_generation': 1, 'status': 'tracking', 'first_decision_frame': 3,
                 'decided_seconds_before_measurement': .984, 'first_error_seconds': .0012, 'corrections': 2,
                 'late_corrections': 0, 'last_state': 'tracking', 'host_samples': 900, 'host_late_samples': 0,
                 'host_slack_min_micros': 1200, 'host_slack_max_micros': 3214, 'connection_quality_failures_max': 0,
                 'stall_reseeds': 0}
        epoch.update(values)
        return epoch

    @staticmethod
    def acquiring_epoch(**values):
        """Host samples arrived but the Client never decided."""
        return TimingRunnerTests.phase_epoch(**{'status': 'acquiring', 'first_decision_frame': None,
                                                'decided_seconds_before_measurement': None, 'first_error_seconds': None,
                                                'corrections': 0, 'last_state': 'acquiring', **values})

    @staticmethod
    def windows(**extra):
        return {'window': {'create': {'status': 'recorded', 'placement': 'usable_bounds_top_left', 'final_position': [0, 53],
                                      'sync': 'succeeded'},
                           'join': {'status': 'recorded', 'placement': 'usable_bounds_bottom_right', 'final_position': [256, 180],
                                    'sync': 'timed_out'}}, **extra}

    def test_summary_names_phase_tracking_and_window_state_per_round(self):
        tracking = self.phase_epoch()
        late = self.acquiring_epoch(player_id=2, host_samples=25, host_late_samples=25, host_slack_min_micros=-25000,
                                    host_slack_max_micros=-1500)
        measured = self.acquiring_epoch(movement_epoch=3, status='host_samples_absent', host_samples=0,
                                        host_slack_min_micros=None, host_slack_max_micros=None,
                                        connection_quality_failures_max=4)
        rounds = iter([
            {'start_phase': {'create': {'status': 'recorded', 'first_epoch': tracking, 'measured_epoch': 1,
                                        'measured_epoch_record': tracking, 'epoch_count': 1, 'dropped_observations': 0},
                             'join': {'status': 'recorded', 'first_epoch': late, 'measured_epoch': 1, 'measured_epoch_record': late,
                                      'epoch_count': 2, 'status_counts': {'acquiring': 1, 'host_samples_absent': 1}}},
             **self.windows(window_disturbed=False, window_disturbances=[])},
            {'start_phase': {'create': {'status': 'absent', 'reason': 'create-start-phase.json missing', 'measured_epoch': 1},
                             'join': {'status': 'recorded', 'first_epoch': tracking, 'measured_epoch': 3,
                                      'measured_epoch_record': measured, 'epoch_count': 2,
                                      'status_counts': {'tracking': 1, 'host_samples_absent': 1}, 'dropped_observations': 3}},
             **self.windows(window_disturbed=False, window_disturbances=[])},
            {'start_phase': {'create': {'status': 'recorded', 'first_epoch': tracking, 'measured_epoch': 7,
                                        'measured_epoch_record': None,
                                        'measured_epoch_reason': 'no record for measured epoch 7', 'epoch_count': 1},
                             'join': {'status': 'invalid',
                                      'reason': 'join-start-phase.json: first_decision_steady_ns must be an integer'}},
             'window': {'create': {'status': 'recorded', 'placement': 'p', 'final_position': None}, 'join': 'garbage'},
             'window_disturbed': None, 'window_disturbances': ['join: window evidence invalid: truncated']}])
        def measurement(args, output, fps):
            return {'overall_passed': True, 'presentation': next(rounds)}
        self.args.report_only = True
        self.assertEqual(self.run_rounds(measurement), 0)
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('Round 1 phase tracking: create measured epoch 1/life 1 tracking: decided 0.984 s before measurement '
                      'began, first error 1.200 ms, 2 correction(s) (0 late), last state tracking, Host samples 900 (late 0, '
                      'slack 1.200 ms to 3.214 ms); join measured epoch 1/life 1 acquiring: never decided, first error unset, '
                      '0 correction(s) (0 late), last state acquiring, Host samples 25 (late 25, slack -25.000 ms to -1.500 ms); '
                      "2 epochs {'acquiring': 1, 'host_samples_absent': 1}", summary)
        self.assertIn('a stall reseed returns it to acquiring', summary)
        self.assertIn('its state during measurement splits into tracking and acquiring seconds', summary)
        self.assertIn('Window interference note: ', summary)
        self.assertIn('Round 1 window: no window-state interference detected (create usable_bounds_top_left at (0, 53); '
                      'join usable_bounds_bottom_right at (256, 180), sync timed_out)', summary)
        self.assertNotIn('window: clean', summary)
        self.assertNotIn('platform-default placement', summary)
        self.assertIn('Round 2 phase tracking: create absent (create-start-phase.json missing); measured epoch 1; '
                      'join measured epoch 3/life 1 host_samples_absent: never decided, first error unset, 0 correction(s) '
                      '(0 late), last state acquiring, Host samples 0, connection-quality failures up to 4; '
                      'first epoch 1/life 1 tracking: decided 0.984 s before measurement began', summary)
        self.assertIn("; 2 epochs {'tracking': 1, 'host_samples_absent': 1}; dropped observations 3", summary)
        self.assertIn('Round 3 phase tracking: create measured epoch 7 not recorded (no record for measured epoch 7); '
                      'first epoch 1/life 1 tracking', summary)
        self.assertIn('join invalid (join-start-phase.json: first_decision_steady_ns must be an integer)', summary)
        self.assertIn('Round 3 window: unknown (create p at ()): join: window evidence invalid: truncated', summary)
        self.assertNotIn('DISTURBED', summary)
        saved = self.results()['rounds'][0]['evidence']['presentation']
        self.assertEqual(saved['start_phase']['join']['first_epoch']['host_slack_min_micros'], -25000)

    def test_platform_note_names_each_role_and_missing_evidence(self):
        presentation = {'platform': {
            'create': {'status': 'recorded', 'os': 'macOS', 'architecture': 'x64', 'video_driver': 'cocoa',
                       'gpu_driver': 'metal', 'refresh_hz': 60.0, 'input': 'sdl_injected'},
            'join': {'status': 'absent', 'reason': 'join-report.txt has no platform fingerprint (probe predates it)'}}}
        self.assertEqual(run_timing.platform_note(1, presentation),
                         'Round 1 platform: create macOS/x64 video cocoa, GPU metal, 60 Hz display, '
                         'input sdl_injected; join absent')
        presentation['platform']['create']['refresh_hz'] = 0.0
        self.assertIn('GPU metal, unknown display', run_timing.platform_note(1, presentation))
        self.assertEqual(run_timing.platform_note(2, {}),
                         'Round 2 platform: not analysed (no presentation platform evidence)')

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

    def test_summary_names_reacquisition_stall_reseeds_and_tracking_seconds(self):
        # create: a stall reseed (counted in its Client trace) returned it to
        # acquiring inside the window; join decided only after measurement began.
        reseeded = self.phase_epoch(corrections=3, stall_reseeds=1,
                                    measurement_window={'status': 'acquiring_during_measurement',
                                                        'states': ['tracking', 'settling', 'acquiring'],
                                                        'state_seconds': {'tracking': 11.5, 'settling': .5, 'acquiring': 3.5},
                                                        'tracking_seconds': 12., 'acquiring_seconds': 3.5,
                                                        'observed_seconds': 15.5, 'window_seconds': 16.,
                                                        'corrections_in_window': 1, 'reacquisitions_in_window': 1})
        late = self.phase_epoch(player_id=2, decided_seconds_before_measurement=-3.5, first_error_seconds=-.0021,
                                corrections=1, late_corrections=1, host_late_samples=3, host_slack_min_micros=-800,
                                connection_quality_failures_max=2,
                                measurement_window={'status': 'acquiring_during_measurement', 'states': ['acquiring', 'tracking'],
                                                    'state_seconds': {'acquiring': 3.5, 'tracking': 12.5},
                                                    'tracking_seconds': 12.5, 'acquiring_seconds': 3.5,
                                                    'observed_seconds': 16., 'window_seconds': 16.,
                                                    'corrections_in_window': 1, 'reacquisitions_in_window': 0})
        evidence = {'overall_passed': True, 'presentation': {'start_phase': {
            'create': {'status': 'recorded', 'first_epoch': reseeded, 'measured_epoch': 1, 'measured_epoch_record': reseeded,
                       'epoch_count': 1},
            'join': {'status': 'recorded', 'first_epoch': late, 'measured_epoch': 1, 'measured_epoch_record': late,
                     'epoch_count': 1}}}}
        self.args.rounds, self.args.report_only = 1, True
        self.assertEqual(self.run_rounds(lambda *args: evidence), 0)
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('create measured epoch 1/life 1 tracking: decided 0.984 s before measurement began, first error 1.200 ms, '
                      '3 correction(s) (0 late), last state tracking, Host samples 900 (late 0, slack 1.200 ms to 3.214 ms), '
                      '1 stall reseed(s) in the Client trace; during measurement: acquiring_during_measurement; '
                      'tracking 12.000 s, acquiring 3.500 s, 1 correction(s), 1 reacquisition(s) '
                      '(observed 15.500 s of the 16.000 s window)', summary)
        join = run_timing._phase_role('join', evidence['presentation']['start_phase']['join'])
        self.assertEqual(join, 'join measured epoch 1/life 1 tracking: decided 3.500 s after measurement began, first error '
                               '-2.100 ms, 1 correction(s) (1 late), last state tracking, Host samples 900 (late 3, slack '
                               '-0.800 ms to 3.214 ms), connection-quality failures up to 2; during measurement: '
                               'acquiring_during_measurement; tracking 12.500 s, acquiring 3.500 s, 1 correction(s)')
        self.assertIn(join, summary)
        phase = {'status': 'recorded', 'first_epoch': reseeded, 'measured_epoch': 1, 'measured_epoch_record': reseeded,
                 'epoch_count': 1, 'unattributed_epochs': [{'player_id': 0}], 'unattributed_client_frames': 2,
                 'reseed_detection': {'status': 'invalid', 'source': 'create-commands.jsonl', 'reason': 'missing trace_end'}}
        self.assertIn('; 1 record(s) and 2 active Client frame(s) without a player id excluded; stall reseeds not '
                      'counted (invalid: missing trace_end)', run_timing._phase_role('create', phase))
        for status in ('recorded', 'not_checked'):
            phase['reseed_detection'] = {'status': status, 'reason': 'no Client trace was given'}
            self.assertNotIn('not counted', run_timing._phase_role('create', phase))
        older = run_timing._measurement_state({'status': 'not_recorded', 'reason': 'record carries no state history'})
        self.assertEqual(older, '; during measurement: not_recorded (record carries no state history)')
        never = run_timing._phase_epoch(self.acquiring_epoch(host_samples=0, stall_reseeds=2))
        self.assertIn('acquiring: never decided, first error unset', never)
        self.assertIn('Host samples 0, 2 stall reseed(s) in the Client trace', never)

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

    def test_summary_names_settling_acquiring_resets_and_frame_intervals(self):
        settling = self.phase_epoch(corrections=3, late_corrections=1, last_state='settling',
                                    measurement_window={'status': 'tracking', 'states': ['tracking', 'settling'],
                                                        'state_seconds': {'tracking': 14., 'settling': 2.},
                                                        'tracking_seconds': 16., 'acquiring_seconds': 0.,
                                                        'observed_seconds': 16., 'window_seconds': 16.,
                                                        'corrections_in_window': 2, 'reacquisitions_in_window': 0})
        acquiring = self.acquiring_epoch(player_id=2, host_samples=40, host_late_samples=40, host_slack_min_micros=-9000,
                                         host_slack_max_micros=-4000,
                                         measurement_window={'status': 'acquiring_during_measurement', 'states': ['acquiring'],
                                                             'state_seconds': {'acquiring': 16.}, 'tracking_seconds': 0.,
                                                             'acquiring_seconds': 16., 'observed_seconds': 16.,
                                                             'window_seconds': 16., 'corrections_in_window': 0,
                                                             'reacquisitions_in_window': 0})
        frames = {'roles': {'create': {'count': 959, 'median_seconds': .016667, 'p95_seconds': .0171, 'maximum_seconds': .0334,
                                       'over_1_1_tick': 3, 'at_least_2_ticks': 1}}}
        evidence = {'overall_passed': True, 'resets': 2, 'reset_reasons': {'starvation': 1, 'backlog': 1},
                    'presentation_frame_intervals': frames,
                    'presentation': {'start_phase': {
                        'create': {'status': 'recorded', 'first_epoch': settling, 'measured_epoch': 1,
                                   'measured_epoch_record': settling, 'epoch_count': 1},
                        'join': {'status': 'recorded', 'first_epoch': acquiring, 'measured_epoch': 1,
                                 'measured_epoch_record': acquiring, 'epoch_count': 2,
                                 'status_counts': {'acquiring': 1, 'tracking': 1}}},
                        'window': {'create': {'status': 'recorded', 'placement': 'platform_default', 'final_position': [320, 180]},
                                   'join': {'status': 'recorded', 'placement': 'platform_default', 'final_position': [320, 180],
                                            'sync': 'not_requested'}},
                        'window_disturbed': False, 'window_disturbances': []}}
        self.args.rounds, self.args.report_only = 1, True
        self.assertEqual(self.run_rounds(lambda *args: evidence), 0)
        summary = (self.args.output/'summary.md').read_text()
        # Settling is tracked time; a full observed window names no observed seconds.
        self.assertIn('create measured epoch 1/life 1 tracking: decided 0.984 s before measurement began, first error 1.200 ms, '
                      '3 correction(s) (1 late), last state settling, Host samples 900 (late 0, slack 1.200 ms to 3.214 ms); '
                      'during measurement: tracking; tracking 16.000 s, acquiring 0.000 s, 2 correction(s); join', summary)
        self.assertIn('join measured epoch 1/life 1 acquiring: never decided, first error unset, 0 correction(s) (0 late), '
                      'last state acquiring, Host samples 40 (late 40, slack -9.000 ms to -4.000 ms); during measurement: '
                      "acquiring_during_measurement; tracking 0.000 s, acquiring 16.000 s, 0 correction(s); "
                      "2 epochs {'acquiring': 1, 'tracking': 1}", summary)
        self.assertIn('settling while a correction slews', summary)
        self.assertIn('Round 1 movement: Host movement resets during measurement: 2 (backlog 1, starvation 1); '
                      'frame intervals during measurement: create 959 intervals: median 16.667 ms, p95 17.100 ms, '
                      'max 33.400 ms, >1.1 tick 3, >=2 ticks 1; join not recorded', summary)
        self.assertIn('Round 1 window: no window-state interference detected (create platform_default at (320, 180); '
                      'join platform_default at (320, 180), sync not_requested) [platform-default placement (the platform did not '
                      'let the probe position its window): the two windows may overlap, and a Wayland compositor may '
                      'report OCCLUDED for the covered one]', summary)
        # Without a plan the decision is named by its frame.
        unplanned = self.phase_epoch(decided_seconds_before_measurement=None, first_decision_frame=40)
        self.assertIn('tracking: decided at frame 40, first error 1.200 ms', run_timing._phase_epoch(unplanned))

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

    def test_not_run_rounds_have_no_phase_tracking_claim(self):
        self.assertEqual(self.run_rounds(lambda *args: {'overall_passed': False}), 1)
        summary = (self.args.output/'summary.md').read_text()
        self.assertIn('Round 1 phase tracking: not analysed', summary)
        self.assertNotIn('Round 2 phase tracking', summary)

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
                        mock.patch.object(run_timing, 'analyze_commands', side_effect=lambda output, enforce=True, fps=None: dict(movement)), \
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

    def test_verified_life_respawns_are_gameplay_not_interference(self):
        resets = [{'player_id': 2, 'life_generation': 2, 'epoch': 2, 'authority_tick': 559, 'time_ns': 1}]
        evidence = {'overall_passed': True, 'passed': True, 'errors': [], 'disturbed': False, 'resets': 1,
                    'unexpected_resets': 0, 'life_respawn_resets': resets, 'combat': {'life_respawns_verified': True}}
        self.assertTrue(run_timing.require_verified_life_resets(evidence))
        self.assertTrue(run_timing.require_clean_gui_round(evidence))
        for combat in ({}, {'life_respawns_verified': False}):
            with self.subTest(combat=combat):
                evidence = {'passed': True, 'errors': [], 'life_respawn_resets': resets, 'combat': combat}
                self.assertFalse(run_timing.require_verified_life_resets(evidence))
                self.assertFalse(evidence['passed'])
                self.assertIn('lack verified deaths', evidence['errors'][0])
        self.assertTrue(run_timing.require_verified_life_resets({'passed': True, 'life_respawn_resets': []}))
        unexpected = {'overall_passed': True, 'errors': [], 'disturbed': False, 'resets': 2, 'unexpected_resets': 1}
        self.assertFalse(run_timing.require_clean_gui_round(unexpected))

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
