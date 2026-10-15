"""Analyzer v7 HUD rule (D83): v6 runs still pass, a HUD lagging hud_tick by the simulation role's delay passes only
inside [hud_tick, snapshot_tick], and everything else the frozen analyzer rejects is still rejected."""

import json
import unittest

import combat_gui_evidence_v7 as v7
import presentation_evidence
import test_combat_gui_evidence as reference

TARGET = reference.TARGET


def state_change_frames(rows, own_index, count):
    """Frames where the local player's snapshot HP/ammo changes, after the first few frames."""
    found = []
    for index in range(10, len(rows)):
        now, before = rows[index]["combat"][own_index], rows[index - 1]["combat"][own_index]
        if (now["hp"], now["ammo"]) != (before["hp"], before["ammo"]) and rows[index]["players"][own_index]["life_state"] == 0:
            found.append(index)
    return found[:count]


def v7_lag(rows, indices, lag):
    """v7 batch 04: hud_tick is the simulation role's older snapshot; HP/ammo/life are the newest snapshot's."""
    for index in indices:
        rows[index]["hud_tick"] = rows[index - lag]["snapshot_tick"]


class CombatGuiEvidenceV7Tests(unittest.TestCase):
    templates = {}
    fixture = reference.CombatGuiEvidenceTests.fixture
    generate = staticmethod(reference.CombatGuiEvidenceTests.generate)
    edit_hp = staticmethod(reference.CombatGuiEvidenceTests.edit_hp)
    edit_report = staticmethod(reference.CombatGuiEvidenceTests.edit_report)

    @classmethod
    def tearDownClass(cls):
        for temporary, _ in cls.templates.values():
            temporary.cleanup()
        cls.templates.clear()

    def lagged(self, role="join", lag=1, count=4):
        directory, resets = self.fixture()
        own = 1 if role == "join" else 0
        chosen = []
        def edit(rows):
            chosen.extend(state_change_frames(rows, own, count))
            v7_lag(rows, chosen, lag)
        self.edit_hp(directory, edit, role)
        return directory, resets, chosen

    def assert_v7_error(self, result, text):
        self.assertFalse(result["passed"])
        self.assertTrue(any(text in error for error in result["errors"]), result["errors"])

    def test_v6_runs_still_pass_unchanged(self):
        for duration in (16, 120):
            with self.subTest(duration=duration):
                directory, resets = self.fixture(duration)
                before = sorted(p.name for p in directory.iterdir())
                result = v7.analyze_combat_gui_v7(directory, resets, write=False)
                self.assertTrue(result["passed"], result["errors"][:5])
                self.assertTrue(result["frozen"]["passed"])
                self.assertEqual(result["hud"]["interval_only_frames"], [])
                self.assertEqual(result["hud"]["max_width_ticks"], 0)
                self.assertTrue(result["life_respawns_verified"])
                # The evidence directory is never rewritten (no combat-gui-evidence.json from the frozen copy).
                self.assertEqual(sorted(p.name for p in directory.iterdir()), before)

    def test_lag_of_one_or_two_ticks_passes_only_through_the_interval(self):
        for role in ("create", "join"):
            for lag in (1, 2):
                with self.subTest(role=role, lag=lag):
                    directory, resets, chosen = self.lagged(role, lag)
                    self.assertEqual(len(chosen), 4)
                    result = v7.analyze_combat_gui_v7(directory, resets, write=False)
                    self.assertFalse(result["frozen"]["passed"])
                    self.assertEqual(len(result["frozen_hud_errors"]), 4)
                    self.assertEqual(result["other_errors"], [])
                    self.assertTrue(result["passed"], result["errors"])
                    only = result["hud"]["interval_only_frames"]
                    self.assertEqual(sorted(f["frame"] for f in only), chosen)
                    for frame in only:
                        # Not at hud_tick (that is the v6 rule), but inside (hud_tick, snapshot_tick], and not below
                        # the previous row's snapshot_tick (one tick per row here, so the interval is one tick wide).
                        self.assertEqual(frame["hud_lag_ticks"], lag)
                        self.assertEqual(frame["lower_tick"], max(frame["hud_tick"], frame["previous_snapshot_tick"]))
                        self.assertEqual(frame["width_ticks"], frame["snapshot_tick"] - frame["lower_tick"])
                        self.assertNotIn(0, frame["matched_offsets"])
                        self.assertTrue(all(0 < offset <= lag for offset in frame["matched_offsets"]))
                    self.assertEqual(result["flags"], [])
                    self.assertFalse(result["suspected_lower_bound_false_failure"])

    def test_hud_value_matching_no_tick_of_the_interval_fails(self):
        directory, resets, chosen = self.lagged("join", 1, 1)
        self.edit_hp(directory, lambda rows: rows[chosen[0]].update(hud_ammo=rows[chosen[0]]["hud_ammo"] + 1), "join")
        result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        self.assert_v7_error(result, "matches no tick")
        self.assertFalse(result["suspected_lower_bound_false_failure"])
        # A HUD one tick older than hud_tick: the state before the change, with hud_tick at the change.
        directory, resets = self.fixture()
        def older(rows):
            index = state_change_frames(rows, 1, 1)[0]
            rows[index].update(hud_hp=rows[index - 1]["hud_hp"], hud_ammo=rows[index - 1]["hud_ammo"])
        self.edit_hp(directory, older)
        result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        self.assert_v7_error(result, "matches no tick")
        # It matches at the previous row's snapshot_tick, below hud_tick: reported (still a failure), U6.
        self.assertTrue(result["suspected_lower_bound_false_failure"])
        self.assertEqual(len(result["hud"]["failed_frames"][0]["below_hud_tick_matches"]), 1)

    def test_hud_newer_than_its_snapshot_fails(self):
        directory, resets = self.fixture()
        def newer(rows):
            index = state_change_frames(rows, 1, 1)[0]
            rows[index - 1].update(hud_hp=rows[index]["hud_hp"], hud_ammo=rows[index]["hud_ammo"],
                                   hud_tick=rows[index - 2]["snapshot_tick"])
        self.edit_hp(directory, newer)
        self.assert_v7_error(v7.analyze_combat_gui_v7(directory, resets, write=False), "matches no tick")

    def test_life_mismatch_fails_even_inside_the_interval(self):
        directory, resets, chosen = self.lagged("join", 1, 1)
        self.edit_hp(directory, lambda rows: rows[700].update(hud_life=1, hud_tick=rows[699]["snapshot_tick"]))
        self.assert_v7_error(v7.analyze_combat_gui_v7(directory, resets, write=False), "matches no tick")
        # Dead flag inconsistent with the matched HP.
        directory, resets, chosen = self.lagged("join", 1, 1)
        self.edit_hp(directory, lambda rows: rows[chosen[0]].update(hud_dead=True))
        self.assert_v7_error(v7.analyze_combat_gui_v7(directory, resets, write=False), "matches no tick")

    def test_life_must_hold_at_the_matched_tick_across_a_respawn(self):
        directory, resets = self.fixture()
        def straddle(rows):
            # The first frame of life 2: HUD shows life 1's last state, interval reaching back into life 1's death.
            index = next(i for i, row in enumerate(rows) if row["players"][1]["life_generation"] == 2)
            dead = rows[index - 1]
            rows[index].update(hud_tick=dead["snapshot_tick"], hud_life=2, hud_hp=0, hud_ammo=dead["hud_ammo"], hud_dead=True)
        self.edit_hp(directory, straddle)
        self.assert_v7_error(v7.analyze_combat_gui_v7(directory, resets, write=False), "matches no tick")

    def test_hud_tick_after_snapshot_tick_fails(self):
        directory, resets = self.fixture()
        self.edit_hp(directory, lambda rows: rows[300].update(hud_tick=rows[300]["snapshot_tick"] + 1))
        self.assert_v7_error(v7.analyze_combat_gui_v7(directory, resets, write=False), "hud_tick after snapshot_tick")

    def test_every_other_frozen_error_still_decides(self):
        cases = (("snapshot combat state differs", lambda d: self.edit_hp(d, lambda rows: rows[600]["combat"][0].update(ammo=12), "create")),
                 ("wrong v5 verdict", lambda d: self.edit_report(d, lambda report: report["decisions"][4].update(
                     damage=25, hit_kind=2, target_id=TARGET))),
                 ("LifeRespawn resets differ", None))
        for text, change in cases:
            with self.subTest(text=text):
                directory, resets, _ = self.lagged("join", 1, 2)
                if change is None:
                    resets = resets[:1]
                else:
                    change(directory)
                result = v7.analyze_combat_gui_v7(directory, resets, write=False)
                self.assertTrue(any(text in error for error in result["other_errors"]), result["other_errors"])
                self.assertFalse(result["passed"])

    def test_disagreement_with_the_frozen_hud_verdict_fails(self):
        directory, resets, _ = self.lagged("join", 1, 2)
        original = v7.frozen_result
        def drop_one(*args):
            result = original(*args)
            first = next(e for e in result["errors"] if v7.HUD_ERROR.match(e))
            result["errors"].remove(first)
            return result
        v7.frozen_result = drop_one
        try:
            result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        finally:
            v7.frozen_result = original
        self.assert_v7_error(result, "does not reproduce the frozen analyzer's HUD verdict")

    def test_written_result_and_missing_input(self):
        directory, resets, _ = self.lagged("join", 1, 1)
        self.assertTrue(v7.analyze_combat_gui_v7(directory, resets)["passed"])
        self.assertFalse((directory / "combat-gui-evidence-v7.json").exists())   # writes only when asked
        self.assertTrue(v7.analyze_combat_gui_v7(directory, resets, write=True)["passed"])
        self.assertTrue((directory / "combat-gui-evidence-v7.json").exists())
        (directory.parent / "files.sha256").write_text("listed\n")              # inside a session: refused
        with self.assertRaises(ValueError):
            v7.analyze_combat_gui_v7(directory, resets, write=True)
        self.assertFalse((directory / "combat-gui-evidence.json").exists())
        (directory / "join-combat-hp.jsonl").unlink()
        result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        self.assertFalse(result["passed"])
        self.assertTrue(result["other_errors"])

    def test_hud_older_than_the_previous_snapshot_fails_even_at_hud_tick(self):
        # v6 accepts it (the HUD equals the state at hud_tick); the previous row already recorded a newer snapshot.
        directory, resets = self.fixture()
        def stale(rows):
            index = state_change_frames(rows, 1, 1)[0]
            rows[index + 1].update(hud_tick=rows[index - 1]["snapshot_tick"], hud_hp=rows[index - 1]["hud_hp"],
                                   hud_ammo=rows[index - 1]["hud_ammo"])
        self.edit_hp(directory, stale)
        result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        self.assertTrue(result["frozen"]["passed"])
        self.assert_v7_error(result, "matches no tick")
        self.assertEqual(result["hud"]["hud_tick_below_previous_snapshot"], 1)

    def test_not_presented_rows_are_not_hud_frames(self):
        directory, resets = self.fixture()
        def hide(rows):
            rows[400].update(presented=False, hud_tick=0, hud_hp=0, hud_ammo=0, hud_life=0, hud_dead=False)
        self.edit_hp(directory, hide)
        result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        self.assertTrue(result["passed"], result["errors"])
        directory2, _ = self.fixture()
        self.assertEqual(result["hud"]["presented_frames"] + 1,
                         v7.analyze_combat_gui_v7(directory2, resets, write=False)["hud"]["presented_frames"])

    def test_same_count_of_different_frames_is_a_disagreement(self):
        directory, resets, chosen = self.lagged("join", 1, 2)
        original = v7.frozen_result
        def move_one(*args):
            result = original(*args)
            first = next(e for e in result["errors"] if v7.HUD_ERROR.match(e))
            result["errors"][result["errors"].index(first)] = first.rsplit(" ", 1)[0] + f" {chosen[0] + 1}"
            return result
        v7.frozen_result = move_one
        try:
            result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        finally:
            v7.frozen_result = original
        self.assert_v7_error(result, "does not reproduce the frozen analyzer's HUD verdict")

    def test_source_tick_order_is_greedy_per_role(self):
        frame = lambda role, ticks: {"role": role, "frame": len(ticks), "matched_ticks": ticks}
        self.assertEqual(v7.source_order_violations([frame("create", [10, 11]), frame("create", [10])]), [])
        self.assertEqual(len(v7.source_order_violations([frame("create", [10]), frame("create", [9])])), 1)
        self.assertEqual(v7.source_order_violations([frame("create", [10]), frame("join", [9]), frame("create", [])]), [])

    def test_wide_interval_only_match_is_a_flag_not_an_error(self):
        self.assertEqual(v7.WIDE_INTERVAL_TICKS, round(presentation_evidence.MAXIMUM_CROSSING_BRACKET_SECONDS * 60))
        directory, resets = self.fixture()
        dropped = 8
        def gap(rows):
            index = state_change_frames(rows, 1, 1)[0]
            del rows[index - dropped:index]              # the change row's previous row is now dropped + 1 ticks older
            rows[index - dropped].update(hud_tick=rows[index - dropped - 1]["snapshot_tick"])
        self.edit_hp(directory, gap)
        self.edit_report(directory, lambda report: report.update(hp_frame_count=report["hp_frame_count"] - dropped), "join")
        result = v7.analyze_combat_gui_v7(directory, resets, write=False)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(len(result["flags"]), 1)
        self.assertGreater(result["hud"]["wide_interval_only_frames"][0]["width_ticks"], v7.WIDE_INTERVAL_TICKS)


if __name__ == "__main__":
    unittest.main()
