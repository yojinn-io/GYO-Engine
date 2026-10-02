"""Small synthetic regressions for the v5 GUI combat evidence gate."""

import json
from pathlib import Path
import shutil
import tempfile
import unittest

from combat_gui_evidence import RULES, SUBMITTING, analyze_combat_gui, schedule
from gameplay_evidence import combat_expected

SHOOTER, TARGET = 1, 2
FIRST_TICK, SLOT_TICKS, LATENCY_TICKS = 100, 48, 2


def simulate(duration):
    """A consistent run: decisions, the target's life boundaries and per-tick state."""
    slots = schedule(duration)
    submissions, decisions, replay, deaths = [], [], [], []
    life, hits = 1, 0
    for entry in slots:
        if entry["step"] not in SUBMITTING:
            continue
        frame = FIRST_TICK + SLOT_TICKS * entry["slot"]
        tick = frame + LATENCY_TICKS
        action_id = len(submissions) + 1
        generated = 100 + (frame + 1) / 60
        kind = SUBMITTING[entry["step"]]
        hit = entry["step"] == "hit"
        submissions.append({"action_id": action_id, "generated_seconds": generated, "submission_frame_id": frame,
                            "action_kind": kind, "slot": entry["slot"]})
        decisions.append({"action_id": action_id, "resolved_tick": tick, "accepted": True, "rejection": 0,
                          "hit_kind": 2 if hit else (1 if kind == 0 else 0), "target_id": TARGET if hit else 0,
                          "damage": 25 if hit else 0, "action_kind": kind, "received_seconds": generated + .05})
        replay.append((SHOOTER, {"action_id": action_id, "resolved_tick": tick, "accepted": True, "action_kind": kind,
                                 "life_generation": 1, "damage": 25 if hit else 0, "target_id": TARGET if hit else 0,
                                 "target_life_generation": life if hit else 0}))
        if hit:
            hits += 1
            if hits == 4:
                deaths.append(tick)
                life, hits = life + 1, 0
    return slots, submissions, decisions, replay, deaths


def life_state(player, tick, deaths):
    if player == SHOOTER:
        return {"player_id": player, "life_generation": 1, "life_state": 0, "life_state_tick": 0, "respawn_tick": 0, "epoch": 1}
    life, start = 1, 0
    for death in deaths:
        if tick < death:
            break
        if tick < death + 180:
            return {"player_id": player, "life_generation": life, "life_state": 1, "life_state_tick": death,
                    "respawn_tick": death + 180, "epoch": life}
        life, start = life + 1, death + 180
    return {"player_id": player, "life_generation": life, "life_state": 0, "life_state_tick": start,
            "respawn_tick": 0, "epoch": life}


class CombatGuiEvidenceTests(unittest.TestCase):
    templates = {}

    @classmethod
    def tearDownClass(cls):
        for temporary, _ in cls.templates.values():
            temporary.cleanup()
        cls.templates.clear()

    def fixture(self, duration=16):
        """A fresh copy of the consistent run; each duration is generated once per class."""
        if duration not in self.templates:
            temporary = tempfile.TemporaryDirectory(prefix="pvp-combat-gui-template-")
            self.templates[duration] = temporary, self.generate(Path(temporary.name), duration)
        template, resets = self.templates[duration]
        copy = tempfile.TemporaryDirectory(prefix="pvp-combat-gui-evidence-")
        self.addCleanup(copy.cleanup)
        directory = Path(copy.name) / "run"
        shutil.copytree(template.name, directory)
        return directory, [dict(reset) for reset in resets]

    @staticmethod
    def generate(directory, duration):
        slots, submissions, decisions, replay, deaths = simulate(duration)
        shots = [row for row in submissions if row["action_kind"] == 0]
        feedback = [{"action_id": row["action_id"], "submission_frame_id": row["submission_frame_id"],
                     "frame_id": row["submission_frame_id"], "submitted_seconds": row["generated_seconds"],
                     "presented_seconds": row["generated_seconds"] + .002,
                     "shooting": True, "presented_action_id": row["action_id"]} for row in shots]
        local_clicks = sum(entry["step"] in ("empty_click", "reloading_click") for entry in slots)
        frame_count = int((duration + 4) * 60)
        groups = {}
        for owner, d in replay:
            groups.setdefault((owner, d["life_generation"]), []).append((owner, d))
            if d["target_id"]:
                groups.setdefault((d["target_id"], d["target_life_generation"]), []).append((owner, d))
        def state(player, life, tick):
            return combat_expected(player, life, tick, groups.get((player, life), []))
        for role, local in (("create", SHOOTER), ("join", TARGET)):
            mover = local == SHOOTER
            count = lambda value: value if mover else 0
            report = {"schema_version": 2, "protocol": 5, "enabled": True, "role": role,
                      "duration_seconds": duration, "local_id": local, "expected_target_id": 3 - local, **RULES,
                      "planned_slots": count(len(slots)), "dispatched_slots": count(len(slots)),
                      "planned_actions": count(len(submissions)), "submitted_actions": count(len(submissions)),
                      "decision_count": count(len(submissions)), "accepted": count(len(submissions)), "rejected": 0,
                      "submitted_shots": count(len(shots)), "animations": count(len(shots)),
                      "planned_local_clicks": count(local_clicks), "suppressed_clicks": count(local_clicks),
                      "schedule": slots if mover else [], "skipped_presented": 0, "errors": [],
                      "hp_frame_count": frame_count, "submissions": submissions if mover else [],
                      "decisions": decisions if mover else [], "first_feedback": feedback if mover else [],
                      "last_action_transport": {"allocated": count(len(submissions)), "acknowledged": count(len(submissions)),
                          "retired": count(len(submissions)), "pending_requests": 0, "retained_decisions": 0,
                          "unconsumed": 0, "protocol_errors": 0,
                          "max_datagram_bytes": 160 if mover else 0, "max_batch_shots": 1 if mover else 0}}
            (directory / f"{role}-combat.json").write_text(json.dumps(report), encoding="utf-8")
            with (directory / f"{role}-combat-hp.jsonl").open("w", encoding="utf-8") as target:
                for frame in range(frame_count):
                    tick = frame + 1
                    players = [life_state(player, tick, deaths) for player in (SHOOTER, TARGET)]
                    combat = [{"player_id": p["player_id"], "life_generation": p["life_generation"],
                               **state(p["player_id"], p["life_generation"], tick)} for p in players]
                    own = next(c for c in combat if c["player_id"] == local)
                    target.write(json.dumps({"frame_id": frame, "host_seconds": 100 + tick / 60 + .003,
                        "snapshot_tick": tick, "local_id": local, "presented": True, "hud_tick": tick,
                        "hud_hp": own["hp"], "hud_ammo": own["ammo"], "hud_life": own["life_generation"],
                        "hud_dead": own["hp"] == 0, "combat": combat, "players": players}) + "\n")
        return [{"player_id": TARGET, "life_generation": index + 2, "authority_tick": death + 180}
                for index, death in enumerate(deaths)]

    @staticmethod
    def edit_report(directory, edit, role="create"):
        path = directory / f"{role}-combat.json"
        value = json.loads(path.read_text())
        edit(value)
        path.write_text(json.dumps(value))

    @staticmethod
    def edit_hp(directory, edit, role="join"):
        path = directory / f"{role}-combat-hp.jsonl"
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        edit(rows)
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))

    def assert_error(self, result, text):
        self.assertFalse(result["passed"])
        self.assertTrue(any(text in error for error in result["errors"]), result["errors"])

    def test_clean_short_and_integration_keep_scope(self):
        for duration, actions, deaths in ((16, 16, 2), (120, 116, 18)):
            with self.subTest(duration=duration):
                directory, resets = self.fixture(duration)
                result = analyze_combat_gui(directory, resets)
                self.assertTrue(result["passed"], result["errors"][:5])
                self.assertEqual((result["planned_actions"], result["deaths"]), (actions, deaths))
                self.assertTrue(result["life_respawns_verified"])
                self.assertEqual(result["integration_120_seconds"], duration == 120)
                self.assertFalse(result["full_acceptance"])

    def test_missing_outcome_keeps_planned_denominator(self):
        directory, _ = self.fixture()
        self.edit_report(directory, lambda report: report["decisions"].pop())
        result = analyze_combat_gui(directory)
        self.assertFalse(result["passed"])
        self.assertEqual(result["planned_actions"], 16)
        self.assertEqual(result["roles"]["create"]["infinite_arrival_count"], 1)

    def test_duplicate_decision_cannot_hide_behind_summary(self):
        directory, _ = self.fixture()
        self.edit_report(directory, lambda report: report["decisions"].append(report["decisions"][0]))
        self.assert_error(analyze_combat_gui(directory), "repeat an ActionId")

    def test_corpse_damage_or_rejected_reload_is_a_wrong_verdict(self):
        for index, change in ((4, {"damage": 25, "hit_kind": 2, "target_id": TARGET}),
                              (12, {"accepted": False, "rejection": 6})):
            with self.subTest(index=index):
                directory, _ = self.fixture()
                self.edit_report(directory, lambda report: report["decisions"][index].update(change))
                self.assert_error(analyze_combat_gui(directory), "wrong v5 verdict")

    def test_empty_or_reloading_click_must_stay_local(self):
        directory, _ = self.fixture()
        self.edit_report(directory, lambda report: report.update(suppressed_clicks=1))
        self.assert_error(analyze_combat_gui(directory), "suppressed_clicks must equal 2")

    def test_probe_schedule_must_match_the_declaration(self):
        directory, _ = self.fixture()
        self.edit_report(directory, lambda report: report["schedule"][13].update(step="hit"))
        self.assert_error(analyze_combat_gui(directory), "differs from the predeclared v5 schedule")

    def test_hit_on_the_wrong_life_is_rejected(self):
        directory, _ = self.fixture()
        # Slot 8's hit resolved before the target's respawn would land on the corpse of life 1.
        self.edit_report(directory, lambda report: report["decisions"][7].update(
            resolved_tick=report["decisions"][7]["resolved_tick"] - 100))
        self.assert_error(analyze_combat_gui(directory), "outside its predeclared target life")

    def test_snapshot_ammo_and_hud_are_replayed_per_life(self):
        directory, _ = self.fixture()
        self.edit_hp(directory, lambda rows: rows[600]["combat"][0].update(ammo=12), "create")
        self.assert_error(analyze_combat_gui(directory), "snapshot combat state differs")
        directory, _ = self.fixture()
        self.edit_hp(directory, lambda rows: rows[600].update(hud_ammo=rows[600]["hud_ammo"] + 1), "create")
        self.assert_error(analyze_combat_gui(directory), "HUD HP/ammo/life mismatch")
        directory, _ = self.fixture()
        self.edit_hp(directory, lambda rows: rows[700].update(hud_life=1))
        self.assert_error(analyze_combat_gui(directory), "HUD HP/ammo/life mismatch")

    def test_presented_hud_may_lag_the_newest_snapshot(self):
        directory, _ = self.fixture()
        def lag(rows):
            first = next(i for i, row in enumerate(rows) if row["combat"][1]["hp"] < 100)
            previous = rows[first - 1]
            rows[first].update(hud_tick=previous["snapshot_tick"], hud_hp=previous["hud_hp"], hud_ammo=previous["hud_ammo"])
        self.edit_hp(directory, lag)
        self.assertTrue(analyze_combat_gui(directory)["passed"])

    def test_death_wait_and_respawn_resets_are_checked(self):
        directory, resets = self.fixture()
        self.edit_hp(directory, lambda rows: [p.update(respawn_tick=p["respawn_tick"] - 1)
                                              for row in rows for p in row["players"] if p["life_state"] == 1])
        self.assert_error(analyze_combat_gui(directory, resets), "death wait is not 180")
        directory, resets = self.fixture()
        result = analyze_combat_gui(directory, resets[:1])
        self.assert_error(result, "LifeRespawn resets differ")
        self.assertFalse(result["life_respawns_verified"])

    def test_shooter_death_is_rejected(self):
        directory, _ = self.fixture()
        self.edit_hp(directory, lambda rows: rows[50]["players"][0].update(life_state=1, respawn_tick=180), "create")
        self.assert_error(analyze_combat_gui(directory), "shooter changed life or died")

    def test_next_successful_present_is_required_but_skips_are_valid(self):
        directory, _ = self.fixture()
        def postpone(report):
            report["first_feedback"][0]["frame_id"] += 1
            report["first_feedback"][0]["presented_seconds"] += 1 / 60
        self.edit_report(directory, postpone)
        self.assertFalse(analyze_combat_gui(directory)["passed"])
        directory, _ = self.fixture()
        frame = FIRST_TICK
        self.edit_hp(directory, lambda rows: rows[frame].update(presented=False), "create")
        def skip(report):
            report["skipped_presented"] = 1
            first = report["first_feedback"][0]
            first.update(frame_id=frame + 1, presented_seconds=first["presented_seconds"] + 1 / 60)
        self.edit_report(directory, skip)
        result = analyze_combat_gui(directory)
        self.assertTrue(result["passed"], result["errors"])

    def test_truncated_or_corrupt_hp_fails(self):
        directory, _ = self.fixture()
        self.edit_hp(directory, lambda rows: rows.pop())
        self.assertFalse(analyze_combat_gui(directory)["passed"])
        path = directory / "join-combat-hp.jsonl"
        path.write_text(path.read_text() + '{"frame_id":')
        self.assertFalse(analyze_combat_gui(directory)["passed"])

    def test_slow_decisions_and_unsettled_transport_fail(self):
        directory, _ = self.fixture()
        def slow(report):
            for row in report["decisions"]:
                row["received_seconds"] += .2
            report["last_action_transport"]["retired"] -= 1
        self.edit_report(directory, slow)
        result = analyze_combat_gui(directory)
        self.assertFalse(result["passed"])
        self.assertGreater(result["roles"]["create"]["decision_arrival_p95_seconds"], .150)
        self.assertTrue(any("retired" in error for error in result["errors"]))

    def test_nonfinite_data_changed_identity_and_v4_reports_fail(self):
        directory, _ = self.fixture()
        self.edit_hp(directory, lambda rows: rows[10].update(local_id=99))
        self.assertFalse(analyze_combat_gui(directory)["passed"])
        self.edit_report(directory, lambda report: report["decisions"][0].update(received_seconds=float("nan")))
        self.assertFalse(analyze_combat_gui(directory)["passed"])
        directory, _ = self.fixture()
        self.edit_report(directory, lambda report: report.update(schema_version=1))
        self.assert_error(analyze_combat_gui(directory), "unsupported schema")


if __name__ == "__main__":
    unittest.main()
