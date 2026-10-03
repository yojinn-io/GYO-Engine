"""Regression tests for real-clock command evidence and recovery attribution."""

import csv
import json
from pathlib import Path
import tempfile
import unittest

import command_evidence as evidence


START = 1_000_000_000
SECOND = 1_000_000_000
DURATION = 4


def event(kind, timestamp, player=1, sequence=1, epoch=1, **values):
    result = {"kind": kind, "time_ns": int(timestamp), "player_id": player,
              "epoch": epoch, "sequence": sequence, "authority_tick": sequence,
              "source": "none", "queued": 2, "pending": 3, "seeded_neutral": False,
              "frame_seconds": 0, "dropped_seconds": 0, "count": 0, "age_seconds": 0}
    result.update(values)
    return result


class CommandEvidenceTests(unittest.TestCase):
    def make_run(self, *, rate=60, players=(1, 2), source=None, release=None,
                 epoch_change=None, recovery_client_passed=True):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-command-evidence-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        timing = {"start_ns": START, "end_ns": START + DURATION * SECOND,
                  "duration": DURATION, "fps": 60, "player_ids": [1, 2],
                  "gaps_100ms": 0, "history_overflows": 0, "injected": release is not None,
                  "release_ns": release or 0, "recovery_client_passed": recovery_client_passed}
        client, match = [], []
        for player in players:
            current_epoch, sequence = 1, 0
            for index in range(DURATION * rate):
                generated = START + index * SECOND // rate + 1_000_000
                epoch = 2 if epoch_change is not None and generated >= epoch_change else 1
                if epoch != current_epoch:
                    current_epoch, sequence = epoch, 0
                    match.append(event("reset", generated + 25_000_000, player, 0, epoch))
                sequence += 1
                client.append(event("generated", generated, player, sequence, epoch))
                client.append(event("sent", generated + 1_000_000, player, sequence, epoch))
                match.append(event("host_accepted", generated + 5_000_000, player, sequence, epoch))
                resolved = generated + 30_000_000
                selected = source(generated, resolved, epoch, index) if source else "actual"
                match.append(event("resolved", resolved, player, sequence, epoch, source=selected))
            for index in range(DURATION * 60):
                client.append(event("snapshot_received", START + index * SECOND // 60 + 40_000_000,
                                    player, index + 1))
        self.write(directory, client, match, timing)
        return directory, client, match, timing

    @staticmethod
    def write(directory, client, match, timing, *, missing_end=None, dropped=None):
        (directory / "timing.json").write_text(json.dumps(timing), encoding="utf-8")
        for name, records in (("clients-commands.jsonl", client), ("match-commands.jsonl", match)):
            ordered = sorted(records, key=lambda record: record["time_ns"])
            with (directory / name).open("w", encoding="utf-8") as output:
                for record in ordered:
                    output.write(json.dumps(record) + "\n")
                if missing_end != name:
                    tail = {"kind": "trace_end", "events": len(ordered),
                            "dropped": 1 if dropped == name else 0}
                    if ordered and 'schema_version' in ordered[0]:
                        tail['schema_version'] = ordered[0]['schema_version']
                    output.write(json.dumps(tail) + "\n")

    @staticmethod
    def with_send_intervals(client, match, end_ms=12):
        for record in client + match:
            record['schema_version'] = 2
            record['started_ns'] = 0
            if record['kind'] == 'sent':
                record['started_ns'] = record['time_ns']
                record['time_ns'] += (end_ms - 1) * 1_000_000

    def test_host_can_accept_during_successful_send_call(self):
        directory, client, match, timing = self.make_run()
        self.with_send_intervals(client, match)
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assertTrue(result['passed'], result['errors'])
        self.assertEqual(result['first_send_p95_ms'], 12)

    def test_send_end_remains_conservative_when_return_observation_is_delayed(self):
        directory, client, match, timing = self.make_run()
        self.with_send_intervals(client, match, end_ms=40)
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory, enforce=False)
        self.assertTrue(result['passed'], result['errors'])
        self.assertEqual(result['first_send_p95_ms'], 40)
        self.assert_error(evidence.analyze_commands(directory), 'first send P95')

    def test_send_interval_preserves_both_causal_boundaries(self):
        for offset, message in [(-2_000_000, 'Generated <= Sent start'),
                                (5_000_000, 'Sent start <= HostAccepted'),
                                (20_000_000, 'invalid successful send-call interval')]:
            with self.subTest(offset=offset):
                directory, client, match, timing = self.make_run()
                self.with_send_intervals(client, match)
                next(r for r in client if r['kind'] == 'sent')['started_ns'] += offset
                self.write(directory, client, match, timing)
                self.assert_error(evidence.analyze_commands(directory), message)

    def test_schema_two_requires_interval_fields_and_consistent_schema(self):
        for damage in ('missing', 'mixed'):
            with self.subTest(damage=damage):
                directory, client, match, timing = self.make_run()
                self.with_send_intervals(client, match)
                record = next(r for r in client if r['kind'] == 'sent')
                if damage == 'missing':
                    del record['started_ns']
                else:
                    record['schema_version'] = 1
                self.write(directory, client, match, timing)
                self.assert_error(evidence.analyze_commands(directory), 'corrupt diagnostic record')

    def assert_error(self, result, text):
        self.assertFalse(result["passed"])
        self.assertTrue(any(text in message for message in result["errors"]), result["errors"])

    def test_complete_full_rate_both_players_pass(self):
        directory, *_ = self.make_run()
        result = evidence.analyze_commands(directory)
        self.assertTrue(result["passed"], result["errors"])
        self.assertFalse(result["disturbed"])
        self.assertEqual(result["commands"], 480)
        self.assertEqual(result["actual_fraction"], 1)
        self.assertTrue(result["production_60hz_passed"])

    def test_missing_trace_end_is_not_complete_evidence(self):
        directory, client, match, timing = self.make_run()
        self.write(directory, client, match, timing, missing_end="clients-commands.jsonl")
        self.assert_error(evidence.analyze_commands(directory), "missing trace_end")

    def test_diagnostic_loss_fails_even_if_all_recorded_commands_are_actual(self):
        directory, client, match, timing = self.make_run()
        self.write(directory, client, match, timing, dropped="match-commands.jsonl")
        result = evidence.analyze_commands(directory)
        self.assertEqual(result["actual_fraction"], 1)
        self.assert_error(result, "diagnostic count mismatch or loss")

    def test_missing_client_trace_fails(self):
        directory, *_ = self.make_run()
        (directory / "clients-commands.jsonl").unlink()
        self.assert_error(evidence.analyze_commands(directory), "Missing Match or Client trace")

    def test_half_rate_does_not_pass_using_only_existing_commands(self):
        directory, *_ = self.make_run(rate=30)
        result = evidence.analyze_commands(directory)
        self.assertEqual(result["actual_fraction"], 1)
        self.assertEqual(result["commands"], 240)
        self.assert_error(result, "60 Hz")

    def test_one_missing_producer_does_not_pass(self):
        directory, *_ = self.make_run(players=(1,))
        result = evidence.analyze_commands(directory)
        self.assertEqual(result["actual_fraction"], 1)
        self.assert_error(result, "both players")

    def test_held_commands_stay_in_denominator_and_are_not_actual(self):
        directory, *_ = self.make_run(source=lambda *_: "held")
        result = evidence.analyze_commands(directory)
        self.assertEqual(result["commands"], 480)
        self.assertEqual(result["actual"], 0)
        self.assertEqual(result["substituted"], 480)
        self.assertEqual(result["actual_fraction"], 0)
        self.assertIsNone(result["actual_p50_ms"])
        self.assertIsNone(result["actual_p95_ms"])
        self.assert_error(result, "execution rate")

    def test_one_held_step_is_counted_without_failing_the_99_percent_budget(self):
        directory, *_ = self.make_run(source=lambda _g, _r, _e, index: "held" if index == 10 else "actual")
        result = evidence.analyze_commands(directory)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["substituted"], 2)
        self.assertEqual(result["commands"], 480)
        self.assertAlmostEqual(result["actual_fraction"], 478 / 480)

    def test_old_epoch_actual_cannot_recover_new_epoch(self):
        release = START + SECOND
        switch = release + 80_000_000
        def source(generated, _resolved, epoch, _index):
            return "actual" if generated < release or epoch == 1 else "held"
        directory, *_ = self.make_run(source=source, release=release, epoch_change=switch)
        result = evidence.analyze_commands(directory, enforce=False)
        self.assertTrue(result["recovery"]["actual_within_1_5s"])
        self.assert_error(result, "current-epoch actual execution")

    def test_recovery_onset_before_1_5_s_can_finish_250_ms_validation_after_deadline(self):
        release = START + SECOND
        directory, *_ = self.make_run(
            release=release,
            source=lambda _g, resolved, _e, _i: "actual" if resolved < release or resolved >= release + 1_450_000_000 else "held")
        result = evidence.analyze_commands(directory, enforce=False)
        self.assertTrue(result["passed"], result["errors"])
        self.assertTrue(result["disturbed"])
        resumes = result["recovery"]["post_release_first_actual_ns"]
        self.assertTrue(all(release + 1_250_000_000 < timestamp <= release + 1_500_000_000 for timestamp in resumes.values()))

    def test_recovery_start_after_1_5_s_fails(self):
        release = START + SECOND
        directory, *_ = self.make_run(
            release=release,
            source=lambda _g, resolved, _e, _i: "actual" if resolved < release or resolved >= release + 1_550_000_000 else "held")
        self.assert_error(evidence.analyze_commands(directory, enforce=False), "within 1.5 s")

    def test_isolated_actual_does_not_replace_250_ms_stability(self):
        release = START + SECOND
        directory, *_ = self.make_run(
            release=release,
            source=lambda _g, resolved, _e, _i: "actual" if resolved < release or
                release + 1_450_000_000 <= resolved < release + 1_500_000_000 else "held")
        result = evidence.analyze_commands(directory, enforce=False)
        self.assertTrue(result["recovery"]["actual_within_1_5s"])
        self.assert_error(result, "stabilize for 250 ms")

    def test_every_fifth_actual_cannot_hide_intervening_held_commands(self):
        release = START + SECOND
        directory, *_ = self.make_run(release=release, source=lambda generated, _r, _e, index:
                                     "actual" if generated < release or index % 5 == 0 else "held")
        self.assert_error(evidence.analyze_commands(directory, enforce=False), "stabilize for 250 ms")

    def test_missing_authority_steps_break_recovery_interval(self):
        release = START + SECOND
        directory, client, match, timing = self.make_run(release=release)
        match[:] = [record for record in match if not (record['kind'] == 'resolved' and
                    record['time_ns'] >= release and record['sequence'] % 5 == 0)]
        self.write(directory, client, match, timing)
        self.assert_error(evidence.analyze_commands(directory, enforce=False), "stabilize for 250 ms")

    def test_actual_requires_host_acceptance_even_in_report_only_mode(self):
        directory, client, match, timing = self.make_run()
        match[:] = [record for record in match if record['kind'] != 'host_accepted']
        self.write(directory, client, match, timing)
        self.assert_error(evidence.analyze_commands(directory, enforce=False), "missing Sent or HostAccepted")

    def test_actual_requires_sent_before_host_accepted(self):
        directory, client, match, timing = self.make_run()
        for record in client:
            if record['kind'] == 'sent':
                record['time_ns'] += 10_000_000
        self.write(directory, client, match, timing)
        self.assert_error(evidence.analyze_commands(directory, enforce=False), "Generated <= Sent start <= HostAccepted")

    def test_actual_outside_measurement_still_requires_complete_stage_evidence(self):
        directory, client, match, timing = self.make_run()
        match.append(event('resolved', START-1, sequence=9999, source='actual'))
        self.write(directory, client, match, timing)
        self.assert_error(evidence.analyze_commands(directory, enforce=False), "missing Generated evidence")

    def test_recovery_helper_rejects_generation_after_resolution(self):
        release = START + SECOND
        records = [event('resolved', release+i*20_000_000, sequence=i+1, source='actual') for i in range(30)]
        future = {(1, 1, i+1): record['time_ns']+SECOND for i, record in enumerate(records)}
        intervals = evidence.recovery_actual_intervals(records, future, [1], release)
        self.assertEqual(intervals[1], [])

    def test_pending_and_future_bounds_are_hard_failures(self):
        for field, invalid in (('queued', 33), ('pending', 13)):
            with self.subTest(field=field):
                directory, client, match, timing = self.make_run()
                match[-1][field] = invalid
                self.write(directory, client, match, timing)
                self.assert_error(evidence.analyze_commands(directory, enforce=False), "command window exceeds")

    def test_clean_backlog_threshold_does_not_require_a_recorded_reset_to_fail(self):
        directory, client, match, timing = self.make_run()
        for record in match:
            if record['kind'] == 'resolved':
                record['queued'] = 4
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assertFalse(result['disturbed'])
        self.assertEqual(result['resets'], 0)
        self.assert_error(result, "backlog reset threshold")

    def test_reset_consequence_does_not_explain_its_own_cause(self):
        directory, client, match, timing = self.make_run()
        reset_at = START + SECOND
        match.append(event('reset', reset_at, epoch=2, sequence=0))
        client.append(event('runtime_gap', reset_at+2_500_000, epoch=2,
                            frame_seconds=.016754, dropped_seconds=.000088))
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assertTrue(result['disturbed'])
        self.assert_error(result, "no preceding substantive interference")
        # Schema 1 reset: the error names that no reason was recorded instead of guessing one.
        self.assert_error(result, "Unexplained epoch reset for player 1 (reason unrecorded): ")

    @staticmethod
    def life_respawn(match, client, *, clamp_epoch=2, reset_life=2, cancel=True):
        """Player 2 respawns at tick 70 one second in: its LifeRespawn, fresh-seed clamp and one cancelled command."""
        at = START + SECOND
        match.append(event('reset', at, player=2, epoch=2, sequence=0, authority_tick=70,
                           reset_reason='life_respawn', life_generation=reset_life))
        frame = .0168
        client.append(event('runtime_gap', at + 10_000_000, player=2, epoch=clamp_epoch, sequence=3, authority_tick=71,
                            frame_seconds=frame, dropped_seconds=frame - evidence.TICK_SECONDS))
        if cancel:
            resolved = next(r for r in match if r['kind'] == 'resolved' and r['player_id'] == 2 and r['sequence'] == 100)
            match.remove(resolved)
            client.append(event('lifecycle_cancelled', at + 20_000_000, player=2, epoch=1, sequence=100,
                                life_generation=1))

    def test_life_respawn_is_a_contract_epoch_with_its_clamp_and_cancellations(self):
        directory, client, match, timing = self.make_run()
        self.life_respawn(match, client)
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assertTrue(result['passed'], result['errors'])
        self.assertFalse(result['disturbed'])
        self.assertEqual((result['resets'], result['unexpected_resets'], len(result['life_seed_clamps'])), (1, 0, 1))
        self.assertEqual((result['lifecycle_cancelled'], result['actual_fraction']), (1, 1.0))
        self.assertEqual(result['commands_per_player'][2], DURATION * 60)
        self.assertEqual(result['life_respawn_resets'][0]['life_generation'], 2)

    def test_each_life_respawn_may_move_production_by_its_fresh_phase_and_first_correction(self):
        for missing, passed in ((5, True), (6, False)):
            with self.subTest(missing=missing):
                directory, client, match, timing = self.make_run()
                self.life_respawn(match, client, cancel=False)
                late = [r for r in client if r['kind'] == 'generated' and r['player_id'] == 2][-missing:]
                for record in late:
                    client.remove(record)
                self.write(directory, client, match, timing)
                result = evidence.analyze_commands(directory, enforce=False)
                self.assertEqual(result['production_allowance_steps']['2'], 5)
                self.assertEqual(result['production_60hz_passed'], passed)

    def test_life_seed_clamp_is_structural_not_a_host_timer_tolerance(self):
        reset = event('reset', START, player=2, epoch=2, sequence=0, authority_tick=70,
                      reset_reason='life_respawn', life_generation=2)
        def gap(frame, dropped=None, **values):
            fields = dict(player=2, epoch=2, sequence=3, authority_tick=71, frame_seconds=frame,
                          dropped_seconds=frame - evidence.TICK_SECONDS if dropped is None else dropped)
            fields.update(values)
            return event('runtime_gap', START + 10_000_000, **fields)
        # The 2026-10-03 soak: a macOS 60 Hz loop overslept the respawn frame to 23.1 ms.
        for frame in (.0168, .0231, 1 / 30, .099):
            with self.subTest(frame=frame):
                self.assertTrue(evidence.is_life_seed_clamp(gap(frame), reset))
        for name, candidate in (('100 ms frame', gap(.1)), ('inexact drop', gap(.0231, dropped=.005)),
                                ('nothing dropped', gap(.0160, dropped=0)), ('other epoch', gap(.0231, epoch=1)),
                                ('other player', gap(.0231, player=1)), ('three ticks later', gap(.0231, authority_tick=73))):
            with self.subTest(case=name):
                self.assertFalse(evidence.is_life_seed_clamp(candidate, reset))
        late = dict(gap(.0231), time_ns=START + 100_000_000)
        self.assertFalse(evidence.is_life_seed_clamp(late, reset))
        clamps, unmatched = evidence.match_life_seed_clamps([gap(.0231), gap(.0231)], [reset])
        self.assertEqual((len(clamps), len(unmatched)), (1, 1))

    def test_clamp_in_another_epoch_remains_interference(self):
        directory, client, match, timing = self.make_run()
        self.life_respawn(match, client, clamp_epoch=1)
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assertTrue(result['disturbed'])
        self.assertEqual(result['life_seed_clamps'], [])

    def test_cancellation_without_its_life_respawn_is_an_error(self):
        directory, client, match, timing = self.make_run()
        self.life_respawn(match, client)
        match[:] = [r for r in match if r['kind'] != 'reset']
        self.write(directory, client, match, timing)
        self.assert_error(evidence.analyze_commands(directory), 'lacks a matching LifeRespawn reset')

    def test_life_respawn_without_new_life_is_an_error(self):
        directory, client, match, timing = self.make_run()
        self.life_respawn(match, client, reset_life=1, cancel=False)
        self.write(directory, client, match, timing)
        self.assert_error(evidence.analyze_commands(directory), 'lacks a new life generation')

    def test_unexplained_reset_error_names_the_reset_reason(self):
        directory, client, match, timing = self.make_run()
        match.append(event('reset', START+SECOND, player=2, epoch=2, sequence=0, reset_reason='backlog'))
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assert_error(result, "Unexplained epoch reset for player 2 (reason backlog): no preceding substantive interference")
        self.assertEqual(result['reset_causality'][0]['reason'], 'backlog')

    def test_unrelated_later_stall_cannot_explain_an_earlier_reset(self):
        directory, client, match, timing = self.make_run()
        match.append(event('reset', START+SECOND, epoch=2, sequence=0))
        client.append(event('runtime_gap', START+2*SECOND, frame_seconds=.2, dropped_seconds=.1))
        self.write(directory, client, match, timing)
        self.assert_error(evidence.analyze_commands(directory), "no preceding substantive interference")

    def test_long_stall_logged_on_resume_can_explain_reset_during_stall(self):
        directory, client, match, timing = self.make_run()
        match.append(event('reset', START+SECOND, epoch=2, sequence=0))
        client.append(event('runtime_gap', START+2*SECOND, frame_seconds=1.5, dropped_seconds=1.4))
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assertTrue(result['passed'], result['errors'])
        self.assertEqual(result['reset_causality'][0]['preceding_interference'][0]['kind'], 'runtime_gap')

    def test_resets_keep_their_reason_and_gui_frame_intervals_are_per_role(self):
        directory, client, match, timing = self.make_run()
        match.append(event('reset', START+SECOND, player=2, epoch=2, sequence=0, reset_reason='starvation'))
        match.append(event('reset', START+3*SECOND, epoch=2, sequence=0))  # Schema 1: no reason recorded.
        client.append(event('runtime_gap', START+2*SECOND, player=2, frame_seconds=1.5, dropped_seconds=1.4))
        client.append(event('runtime_gap', START+3*SECOND+100_000_000, player=1, frame_seconds=1.5, dropped_seconds=1.4))
        self.write(directory, client, match, timing)
        tick = 1 / 60
        intervals = [tick] * 56 + [.019, .02, .03, .04]
        presented, stamp = [], START + 5_000_000
        for seconds in intervals:
            stamp += int(seconds * SECOND)
            presented.append(event('presentation', stamp, frame_seconds=seconds))
        presented.append(event('presentation', START + DURATION * SECOND + 1, frame_seconds=.5))  # After the window.
        with (directory / 'create-commands.jsonl').open('w', encoding='utf-8') as output:
            for record in presented:
                output.write(json.dumps(record) + '\n')
            output.write(json.dumps({'kind': 'trace_end', 'events': len(presented), 'dropped': 0}) + '\n')
        result = evidence.analyze_commands(directory)
        self.assertTrue(result['passed'], result['errors'])
        self.assertEqual(result['resets'], 2)
        self.assertEqual(result['reset_reasons'], {'starvation': 1, 'unrecorded': 1})
        self.assertEqual([item['reason'] for item in result['reset_causality']], ['starvation', 'unrecorded'])
        roles = result['presentation_frame_intervals']['roles']
        self.assertEqual(sorted(roles), ['create'])
        create = roles['create']
        self.assertEqual(create['count'], 60)
        self.assertAlmostEqual(create['median_seconds'], tick)
        self.assertEqual(create['maximum_seconds'], .04)
        self.assertEqual(create['p95_seconds'], .019)  # Nearest rank 57 of 60.
        self.assertEqual((create['over_1_1_tick'], create['at_least_2_ticks']), (4, 1))  # 30 ms is under two ticks.
        self.assertAlmostEqual(result['presentation_frame_intervals']['cut_seconds'], tick * 1.1)

    def test_client_recovery_failure_cannot_be_overridden_by_actual_trace(self):
        directory, *_ = self.make_run(release=START + SECOND, recovery_client_passed=False)
        self.assert_error(evidence.analyze_commands(directory, enforce=False), "Client prediction/presentation recovery")

    def test_snapshot_receipt_gap_marks_otherwise_good_run_disturbed(self):
        directory, client, match, timing = self.make_run()
        client[:] = [record for record in client if not (record["kind"] == "snapshot_received" and
                     record["player_id"] == 1 and START + SECOND < record["time_ns"] < START + SECOND + 180_000_000)]
        self.write(directory, client, match, timing)
        result = evidence.analyze_commands(directory)
        self.assertTrue(result["passed"], result["errors"])
        self.assertTrue(result["disturbed"])
        self.assertGreaterEqual(len(result["snapshot_receipt_gaps"]), 1)

    def test_remote_age_spike_marks_otherwise_good_run_disturbed(self):
        directory, *_ = self.make_run()
        with (directory / "frames.csv").open("w", newline="") as output:
            writer = csv.writer(output)
            writer.writerow(["time_ns", "remote_age_a", "remote_age_b"])
            writer.writerow([START + SECOND, .02, .03])
            writer.writerow([START + SECOND + 16_666_667, .101, .03])
        result = evidence.analyze_commands(directory)
        self.assertTrue(result["passed"], result["errors"])
        self.assertTrue(result["disturbed"])
        self.assertEqual(result["remote_stale_frames"], 1)

    def test_unread_history_overflow_is_not_clean(self):
        directory, client, match, timing = self.make_run()
        timing["history_overflows"] = 1
        self.write(directory, client, match, timing)
        self.assertTrue(evidence.analyze_commands(directory)["disturbed"])


if __name__ == "__main__":
    unittest.main()
