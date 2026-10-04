"""Counterexamples for the repeated-plan soak evidence and runner options; no services are started."""
import contextlib
import io
from pathlib import Path
import unittest

from gameplay_soak_evidence import (JumpTally, LifeTally, PhaseTally, cycle_of, drift_windows,
                                    expected_target_life)
import run_gameplay_soak as runner

A, B = 11, 22
CYCLE_NS = 16_000_000_000
LIVES = [0, 1]


def player(identity, life, *, dead=False, state_tick=0, epoch=1, x=0.0, y=0.0, grounded=True):
    return {'player_id': identity, 'life_generation': life, 'life_state': 1 if dead else 0,
            'life_state_tick': state_tick, 'respawn_tick': state_tick+180 if dead else 0, 'epoch': epoch,
            'position': [x, y, 0.0], 'grounded': grounded}


def samples(cycles=2):
    """A never dies; B dies at 6.8 s of each cycle and respawns 180 ticks later in a new epoch."""
    out = []
    for step in range(cycles*32):
        seconds = step*.5
        time_ns, tick = round(seconds*1e9), round(seconds*60)+1
        out.append((time_ns, tick, player(A, 1)))
        cycle, offset = divmod(seconds, 16)
        cycle = int(cycle)
        death_tick = round((cycle*16+6.8)*60)+1
        if offset < 6.8:
            life, alive_since = cycle+1, 0 if cycle == 0 else round(((cycle-1)*16+6.8)*60)+181
            out.append((time_ns, tick, player(B, life, state_tick=alive_since, epoch=2*cycle+1, x=offset)))
        elif offset < 9.8:
            out.append((time_ns, tick, player(B, cycle+1, dead=True, state_tick=death_tick, epoch=2*cycle+1, x=6.8)))
        else:
            out.append((time_ns, tick, player(B, cycle+2, state_tick=death_tick+180, epoch=2*cycle+2, x=offset)))
    return out


def finish(values, cycles=2):
    tally = LifeTally()
    for time_ns, tick, sample in values:
        tally.observe(time_ns, tick, sample)
    return tally.finish([A, B], LIVES, cycles, 0, CYCLE_NS)


class LifeTests(unittest.TestCase):
    def test_one_death_and_respawn_per_cycle_passes(self):
        errors, deaths, respawns = finish(samples())
        self.assertEqual(errors, [])
        self.assertEqual([(d['life'], d['cycle']) for d in deaths], [(1, 0), (2, 1)])
        self.assertEqual([r['life'] for r in respawns], [2, 3])

    def test_both_clients_reporting_the_same_ticks_is_idempotent(self):
        values = samples()
        self.assertEqual(finish(values+list(reversed(values)))[0], [])

    def test_missing_life_is_rejected(self):
        values = [item for item in samples() if item[2]['life_generation'] != 2 or item[2]['player_id'] != B]
        self.assertTrue(any('lives' in error for error in finish(values)[0]))

    def test_survivor_death_is_rejected(self):
        values = samples()+[(1, 1, player(A, 1, dead=True, state_tick=5))]
        self.assertTrue(any('final life died' in error for error in finish(values)[0]))

    def test_dead_player_moving_is_rejected(self):
        values = samples()
        time_ns, tick, sample = next(item for item in values if item[2]['life_state'] == 1)
        values.append((time_ns+1, tick+1, {**sample, 'position': [7.5, 0, 0]}))
        self.assertIn('Dead player moved horizontally', finish(values)[0])

    def test_wrong_death_wait_is_rejected(self):
        values = [(t, k, {**p, 'respawn_tick': p['respawn_tick']-1} if p['life_state'] == 1 else p)
                  for t, k, p in samples()]
        self.assertIn('Death wait is not180 authority ticks', finish(values)[0])

    def test_respawn_in_old_epoch_is_rejected(self):
        values = [(t, k, {**p, 'epoch': 1} if p['player_id'] == B and p['life_generation'] == 2 else p)
                  for t, k, p in samples()]
        self.assertIn('Respawn did not establish new movement epoch', finish(values)[0])

    def test_respawn_before_deadline_is_rejected(self):
        values = [(t, k, {**p, 'life_state_tick': p['life_state_tick']-1}
                   if p['player_id'] == B and p['life_generation'] == 2 else p) for t, k, p in samples()]
        self.assertIn('Respawn occurred before deadline', finish(values)[0])

    def test_death_outside_its_cycle_is_rejected(self):
        values = [(t+CYCLE_NS if p['life_state'] == 1 and p['life_generation'] == 1 else t, k, p) for t, k, p in samples()]
        self.assertIn('Death did not occur inside its planned cycle', finish(values)[0])

    def test_second_death_in_one_life_is_rejected(self):
        values = samples()
        time_ns, tick, sample = next(item for item in values if item[2]['life_state'] == 1)
        values.append((time_ns+1, tick+1, {**sample, 'life_state_tick': sample['life_state_tick']+1,
                                           'respawn_tick': sample['respawn_tick']+1}))
        self.assertIn('Death occurred more than once within life', finish(values)[0])


class CycleTests(unittest.TestCase):
    def test_target_life_advances_only_for_the_player_who_dies(self):
        self.assertEqual([expected_target_life(cycle, 0, LIVES) for cycle in range(3)], [1, 2, 3])
        self.assertEqual([expected_target_life(cycle, 1, LIVES) for cycle in range(3)], [1, 1, 1])

    def test_cycle_index_uses_the_probe_start(self):
        self.assertEqual(cycle_of(100+CYCLE_NS-1, 100, CYCLE_NS), 0)
        self.assertEqual(cycle_of(100+CYCLE_NS, 100, CYCLE_NS), 1)

    def test_every_player_cycle_must_jump_and_land(self):
        jumps = JumpTally(2)
        for cycle in range(2):
            for identity in (A, B):
                jumps.observe(.9, cycle, player(identity, 1, y=.6, grounded=False))
                jumps.observe(3.0, cycle, player(identity, 1))
        self.assertEqual(jumps.errors([A, B]), [])
        missing = JumpTally(2)
        missing.observe(.9, 0, player(A, 1, y=.6, grounded=False))
        missing.observe(3.0, 0, player(A, 1))
        self.assertTrue(missing.errors([A, B]))

    def test_late_samples_do_not_count_as_a_jump(self):
        jumps = JumpTally(1)
        jumps.observe(7.0, 0, player(A, 1, y=.6, grounded=False))
        jumps.observe(3.0, 0, player(A, 1))
        self.assertTrue(jumps.errors([A]))

    def test_phase_counters_restart_per_epoch_without_losing_corrections(self):
        tally = PhaseTally()
        for time_ns, state, corrections, cycle in ((0, 0, 0, 0), (10**9, 2, 2, 0), (2*10**9, 0, 0, 1),
                                                   (3*10**9, 2, 1, 1), (4*10**9, 2, 1, 1)):
            tally.observe(time_ns, state, corrections, 0, cycle)
        summary = tally.summary(2)
        self.assertEqual(summary['corrections'], 3)
        self.assertEqual(summary['corrections_per_cycle'], [2, 1])
        self.assertEqual(summary['state_seconds'], {'acquiring': 2.0, 'tracking': 2.0})

    def test_late_drift_fails_even_when_the_whole_run_passes(self):
        cycles = [[20.0]*100 for _ in range(30)]
        cycles[-1] = [80.0]*100
        windows = drift_windows(cycles, bucket=1)
        self.assertTrue(all(window['passed'] for window in windows[:-1]))
        self.assertFalse(windows[-1]['passed'])
        self.assertTrue(drift_windows([[20.0]*100]*29+[[80.0]*100], bucket=30)[0]['passed'])

    def test_missing_execution_counts_as_infinite_in_its_window(self):
        self.assertFalse(drift_windows([[20.0]*10+[float('inf')]], bucket=1)[0]['passed'])


class RunnerOptionTests(unittest.TestCase):
    BASE = ['--match', 'm', '--gateway', 'g', '--probe', 'p', '--arena', 'a', '--output', 'o']

    def parse(self, *extra):
        return runner.parse(self.BASE+list(extra))[1]

    def test_short_mode_defaults_to_two_cycles(self):
        self.assertEqual(self.parse().cycles, 2)

    def test_soak_always_runs_113_cycles(self):
        args = self.parse('--soak', '--fps', '144')
        self.assertEqual((args.cycles, args.soak), (runner.SOAK_CYCLES, True))
        self.assertGreaterEqual(args.cycles*runner.CYCLE_SECONDS, 1800)
        self.assertLess((args.cycles-1)*runner.CYCLE_SECONDS, 1800)

    def test_long_runs_require_explicit_soak_and_supported_rates(self):
        for extra in (('--cycles', '5'), ('--cycles', '1'), ('--soak', '--cycles', '50'), ('--soak', '--fps', '30')):
            with self.subTest(extra=extra), self.assertRaises(SystemExit), \
                    contextlib.redirect_stderr(io.StringIO()):
                self.parse(*extra)


class TimerBaselineIsNeverAVerdict(unittest.TestCase):
    """The empty-loop timer baseline is interpretation only: the analyzer may pass it through, never read it."""
    def test_source_only_passes_the_timer_baseline_through(self):
        source = (Path(__file__).resolve().parent / 'gameplay_soak_evidence.py').read_text(encoding="utf-8")
        lines = [line.strip() for line in source.splitlines() if "timer" in line.lower()]
        self.assertEqual(lines, ["'timer_baseline': client.get('timer'),  # Interpretation only; no check reads it."], "a check may have started reading the timer baseline")


if __name__ == '__main__':
    unittest.main()
