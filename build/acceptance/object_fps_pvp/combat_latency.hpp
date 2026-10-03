// Product acceptance only: concurrent SDL firing and bounded read-only evidence.
// Included after weapon_short.hpp; no production controls or additional Drain.
//
// v5 schedule: one SDL input slot every 0.8 s from 0.2 s, repeating a 17-slot
// cycle (13.6 s). The creator kills the joiner with four hits, shoots its corpse
// through the 180-tick death wait, kills the respawned life with four more hits,
// spends the twelfth round, clicks once on the empty magazine and once while
// reloading (the Client must not submit either), and reloads with R. Every slot
// keeps at least 0.6 s from the death, respawn and reload boundaries it depends on.
enum class CombatStep : std::uint8_t { Hit, DeadTargetShot, Idle, EmptyClick, Reload, ReloadingClick };
constexpr std::array<CombatStep, 17> CombatCycle{
    CombatStep::Hit, CombatStep::Hit, CombatStep::Hit, CombatStep::Hit,
    CombatStep::DeadTargetShot, CombatStep::DeadTargetShot, CombatStep::DeadTargetShot, CombatStep::Idle,
    CombatStep::Hit, CombatStep::Hit, CombatStep::Hit, CombatStep::Hit,
    CombatStep::DeadTargetShot, CombatStep::EmptyClick, CombatStep::Reload, CombatStep::ReloadingClick, CombatStep::Idle};
constexpr double CombatSlotStart = .2, CombatSlotPeriod = .8;
inline const char* CombatStepName(CombatStep step) {
    switch (step) {
    case CombatStep::Hit: return "hit";
    case CombatStep::DeadTargetShot: return "dead_target_shot";
    case CombatStep::Idle: return "idle";
    case CombatStep::EmptyClick: return "empty_click";
    case CombatStep::Reload: return "reload";
    case CombatStep::ReloadingClick: return "reloading_click";
    }
    return "unknown";
}
class CombatLatencyEvidence {
    struct Submitted { std::uint64_t id{}, frame{}; double seconds{}; fps::pvp::ActionKind kind{}; unsigned slot{}; };
    struct Decision {
        fps::pvp::WeaponFeedbackObservation weapon;
        bool accepted{};
    };
    struct Feedback {
        Submitted submitted;
        std::uint64_t frame{}, action{};
        double seconds{};
        bool shooting{};
    };
    struct HpFrame {
        std::uint64_t frame{}, tick{}, hudTick{};
        double seconds{};
        fps::pvp::PlayerId local{};
        std::array<fps::pvp::CombatState, 2> combat{};
        std::array<fps::pvp::PlayerState, 2> players{};
        std::uint32_t hud{}, hudAmmo{};
        std::uint64_t hudLife{};
        bool presented{}, hudDead{};
    };
    const Options& options_;
    bool mover_{}, releaseMouse_{}, releaseReload_{};
    std::optional<CombatStep> expected_;
    unsigned planned_{}, dispatched_{}, suppressed_{};
    std::uint64_t submittedShots_{};
    std::size_t bound_{};
    fps::pvp::PlayerId local_{}, target_{};
    std::uint32_t maximumHp_{}, damage_{}, capacity_{}, cooldownTicks_{}, reloadTicks_{}, respawnTicks_{};
    std::uint64_t decisionsSeen_{};
    std::optional<Submitted> awaiting_;
    std::vector<Submitted> submitted_;
    std::vector<Decision> decisions_;
    std::vector<Feedback> feedback_;
    std::vector<HpFrame> frames_;
    std::vector<std::string> errors_;
    void Check(bool condition, const std::string& reason) {
        if (!condition) {
            Require(errors_.size() < 1024, "Combat diagnostic error buffer exhausted");
            errors_.push_back(reason);
        }
    }
public:
    explicit CombatLatencyEvidence(const Options& options) : options_(options), mover_(options.role == "create") {
        if (!options.combat) return;
        if (mover_) while (CombatSlotStart + planned_ * CombatSlotPeriod < options.duration) ++planned_;
        bound_ = static_cast<std::size_t>(std::ceil((options.duration + 45) * options.fps)) + 1024;
        frames_.reserve(bound_);
        submitted_.reserve(planned_ + 16);
        decisions_.reserve(planned_ + 16);
        feedback_.reserve(planned_ + 16);
    }
    static CombatStep StepAt(unsigned slot) { return CombatCycle[slot % CombatCycle.size()]; }
    void BeforeUpdate(double elapsed, fps::pvp::PvpApplication& application) {
        expected_.reset();
        auto* window = application.Platform().NativeWindow();
        if (releaseMouse_) { PushMouse(window, false); releaseMouse_ = false; }
        if (releaseReload_) { PushKey(window, SDL_SCANCODE_R, false); releaseReload_ = false; }
        if (!mover_ || dispatched_ >= planned_ || elapsed < CombatSlotStart + dispatched_ * CombatSlotPeriod) return;
        const double due = CombatSlotStart + dispatched_ * CombatSlotPeriod;
        const auto step = StepAt(dispatched_++);
        Check(elapsed < due + .2, "A predeclared combat slot missed its 200 ms dispatch window");
        if (elapsed >= due + .2) return; // Never catch up missed slots or remove them from the denominator.
        if (step == CombatStep::Idle) return;
        if (step == CombatStep::Reload) { PushKey(window, SDL_SCANCODE_R, true); releaseReload_ = true; }
        else { PushMouse(window, true); releaseMouse_ = true; }
        expected_ = step;
    }
    void AfterUpdate(std::uint64_t frame, const fps::pvp::WeaponFeedbackObservation& before,
                     fps::pvp::PvpApplication& application) {
        const auto after = application.WeaponFeedback();
        const auto state = application.Connection().State();
        if (state.combatRules) {
            maximumHp_ = state.combatRules->maximumHp;
            damage_ = state.combatRules->shotDamage;
            capacity_ = state.combatRules->magazineCapacity;
            cooldownTicks_ = state.combatRules->cooldownTicks;
            reloadTicks_ = state.combatRules->reloadTicks;
            respawnTicks_ = state.combatRules->respawnTicks;
        }
        if (state.snapshot && state.snapshot->combat.size() == 2) {
            local_ = state.playerId;
            for (const auto& entry : state.snapshot->combat) if (entry.playerId != local_) target_ = entry.playerId;
        }
        const bool shot = expected_ == CombatStep::Hit || expected_ == CombatStep::DeadTargetShot;
        if (shot)
            Check(after.submittedActions == before.submittedActions + 1 && after.lastActionKind == fps::pvp::ActionKind::Shot &&
                  after.animationStarts == before.animationStarts + 1 && after.shooting,
                  "A predeclared SDL shot did not submit one immediate local shot");
        else if (expected_ == CombatStep::Reload)
            Check(after.submittedActions == before.submittedActions + 1 && after.lastActionKind == fps::pvp::ActionKind::Reload &&
                  after.animationStarts == before.animationStarts, "A predeclared R edge did not submit one reload");
        else if (expected_) {
            // The Client must keep an empty or reloading click local: no request, no shot animation.
            const bool local = expected_ == CombatStep::EmptyClick ? before.magazineAmmo == 0 && !before.reloading
                                                                   : before.reloading || before.reloadPending;
            Check(local, std::string("Local state did not match the predeclared ") + CombatStepName(*expected_));
            Check(after.submittedActions == before.submittedActions && after.animationStarts == before.animationStarts,
                  std::string("The Client submitted or animated a ") + CombatStepName(*expected_));
            ++suppressed_;
        } else Check(after.submittedActions == before.submittedActions, "Unexpected action outside the predeclared schedule");
        if (after.submittedActions != before.submittedActions) {
            Check(after.submittedActions == before.submittedActions + 1, "Multiple submissions cannot be fully observed in one frame");
            Require(submitted_.size() < planned_ + 16, "Combat submission buffer exhausted");
            Submitted entry{after.lastActionId, frame, after.lastSubmittedSeconds, after.lastActionKind, dispatched_ - 1};
            submitted_.push_back(entry);
            if (entry.kind == fps::pvp::ActionKind::Shot) {
                ++submittedShots_;
                Check(!awaiting_, "Another shot arrived before the previous successful Presented sample");
                awaiting_ = entry;
            }
        }
        Check(after.animationStarts == submittedShots_, "Animation count differs from submitted SDL shots");
        if (after.decisionCount != decisionsSeen_) {
            Check(after.decisionCount == decisionsSeen_ + 1, "More than one consumed decision in a frame: diagnostic detail incomplete");
            Require(decisions_.size() < planned_ + 16, "Combat decision buffer exhausted");
            decisions_.push_back({after, after.acceptedDecisions == before.acceptedDecisions + 1});
            decisionsSeen_ = after.decisionCount;
        }
    }
    void AfterRender(std::uint64_t frame, fps::pvp::PvpApplication& application) {
        const auto& presented = application.PresentedMovement();
        if (presented && awaiting_) {
            Require(feedback_.size() < planned_ + 16, "Combat feedback buffer exhausted");
            feedback_.push_back({*awaiting_, frame, presented->weapon.lastActionId,
                                 presented->hostSteadySeconds, presented->weapon.shooting});
            Check(presented->weapon.shooting && presented->weapon.lastActionId == awaiting_->id,
                  "Shot animation absent from the next successful Presented frame");
            awaiting_.reset();
        }
        const auto state = application.Connection().State();
        if (!state.snapshot || state.snapshot->combat.size() != 2) return;
        Require(frames_.size() < bound_, "Combat HP frame buffer exhausted");
        HpFrame sample;
        sample.frame = frame;
        sample.seconds = std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
        sample.tick = state.snapshot->tick;
        sample.local = state.playerId;
        std::copy_n(state.snapshot->combat.begin(), 2, sample.combat.begin());
        if (state.snapshot->players.size() == 2) std::copy_n(state.snapshot->players.begin(), 2, sample.players.begin());
        sample.presented = presented.has_value();
        if (presented) {
            sample.hudTick = presented->local.authorityTick; sample.hud = presented->weapon.hp;
            sample.hudAmmo = presented->weapon.magazineAmmo; sample.hudLife = presented->weapon.lifeGeneration;
            sample.hudDead = presented->weapon.dead;
        }
        frames_.push_back(sample);
    }
    void Save(fps::pvp::PvpApplication& application) {
        const auto w = application.WeaponFeedback();
        const auto t = application.Connection().State().actionTransport;
        unsigned submitting{}, local{};
        WeaponJson schedule = WeaponJson::array();
        for (unsigned slot = 0; slot < planned_; ++slot) {
            const auto step = StepAt(slot);
            submitting += step == CombatStep::Hit || step == CombatStep::DeadTargetShot || step == CombatStep::Reload;
            local += step == CombatStep::EmptyClick || step == CombatStep::ReloadingClick;
            schedule.push_back({{"slot", slot}, {"cycle", slot / CombatCycle.size()}, {"cycle_slot", slot % CombatCycle.size()},
                {"due_seconds", CombatSlotStart + slot * CombatSlotPeriod}, {"step", CombatStepName(step)}});
        }
        Check(dispatched_ == planned_, "Not all predeclared combat slots were dispatched");
        Check(w.submittedActions == submitting && submitted_.size() == submitting, "Not all predeclared actions were submitted");
        Check(suppressed_ == local, "Not every predeclared local-only click was observed");
        Check(w.decisionCount == submitting && decisions_.size() == submitting, "Some action outcomes were not fully observed");
        Check(feedback_.size() == submittedShots_ && !awaiting_, "Some shots lack next-Presented feedback");
        WeaponJson report{{"schema_version", 2}, {"protocol", 5}, {"enabled", true}, {"role", options_.role},
            {"duration_seconds", options_.duration}, {"local_id", local_}, {"expected_target_id", target_},
            {"maximum_hp", maximumHp_}, {"damage_per_hit", damage_}, {"magazine_capacity", capacity_},
            {"cooldown_ticks", cooldownTicks_}, {"reload_ticks", reloadTicks_}, {"respawn_ticks", respawnTicks_},
            {"planned_slots", planned_}, {"planned_actions", mover_ ? submitting : 0u},
            {"planned_local_clicks", mover_ ? local : 0u}, {"schedule", mover_ ? schedule : WeaponJson::array()},
            {"slot_period_seconds", CombatSlotPeriod}, {"slot_start_seconds", CombatSlotStart}, {"dispatched_slots", dispatched_},
            {"suppressed_clicks", suppressed_}, {"submitted_shots", submittedShots_},
            {"submitted_actions", w.submittedActions}, {"decision_count", w.decisionCount},
            {"accepted", w.acceptedDecisions}, {"rejected", w.rejectedDecisions},
            {"animations", w.animationStarts}, {"hp_at_end", w.hp},
            {"hp_frame_count", frames_.size()},
            {"skipped_presented", application.SkippedPresentationFrames()}, {"errors", errors_},
            {"first_feedback", WeaponJson::array()}, {"submissions", WeaponJson::array()},
            {"decisions", WeaponJson::array()},
            {"last_action_transport", {{"allocated", t.allocatedThrough}, {"acknowledged", t.acknowledgedThrough},
                {"retired", t.retiredThrough}, {"pending_requests", t.pending}, {"retained_decisions", t.retained},
                {"unconsumed", t.unconsumed}, {"protocol_errors", t.rejectedResultBatches},
                {"max_datagram_bytes", t.maxDatagramBytes}, {"max_batch_shots", t.maxBatchShots}}}};
        for (const auto& entry : submitted_) report["submissions"].push_back({
            {"action_id", entry.id}, {"generated_seconds", entry.seconds}, {"submission_frame_id", entry.frame},
            {"action_kind", static_cast<int>(entry.kind)}, {"slot", entry.slot}});
        for (const auto& entry : feedback_) report["first_feedback"].push_back({
            {"action_id", entry.submitted.id}, {"submission_frame_id", entry.submitted.frame},
            {"frame_id", entry.frame}, {"submitted_seconds", entry.submitted.seconds},
            {"presented_seconds", entry.seconds}, {"shooting", entry.shooting}, {"presented_action_id", entry.action}});
        for (const auto& entry : decisions_) {
            const auto& decision = entry.weapon;
            report["decisions"].push_back({{"action_id", decision.lastDecisionActionId},
                {"resolved_tick", decision.lastDecisionTick}, {"accepted", entry.accepted},
                {"rejection", static_cast<int>(decision.lastRejection)}, {"hit_kind", static_cast<int>(decision.lastHitKind)},
                {"target_id", decision.lastTargetId}, {"damage", decision.lastDamage},
                {"action_kind", static_cast<int>(decision.lastDecisionKind)},
                {"received_seconds", decision.lastDecisionSeconds}});
        }
        std::ofstream out(options_.output / (options_.role + "-combat.json"));
        out << report.dump(2) << '\n';
        out.flush(); Require(bool(out), "Cannot write combat summary");
        std::ofstream hp(options_.output / (options_.role + "-combat-hp.jsonl"));
        for (const auto& entry : frames_) {
            WeaponJson players = WeaponJson::array();
            for (const auto& c : entry.combat) players.push_back({{"player_id", c.playerId}, {"life_generation", c.lifeGeneration},
                {"hp", c.hp}, {"ammo", c.magazineAmmo}, {"reload_action_id", c.reloadActionId},
                {"reload_start_tick", c.reloadStartTick}, {"reload_end_tick", c.reloadEndTick},
                {"last_shot_id", c.lastShotActionId}, {"last_shot_tick", c.lastShotTick}});
            WeaponJson lives = WeaponJson::array();
            for (const auto& p : entry.players) lives.push_back({{"player_id", p.playerId}, {"life_generation", p.lifeGeneration},
                {"life_state", static_cast<int>(p.lifeState)}, {"life_state_tick", p.lifeStateTick}, {"respawn_tick", p.respawnTick},
                {"epoch", p.movementEpoch}});
            hp << WeaponJson{{"frame_id", entry.frame}, {"host_seconds", entry.seconds},
                {"snapshot_tick", entry.tick}, {"local_id", entry.local}, {"presented", entry.presented},
                {"hud_tick", entry.hudTick}, {"hud_hp", entry.hud}, {"hud_ammo", entry.hudAmmo},
                {"hud_life", entry.hudLife}, {"hud_dead", entry.hudDead}, {"combat", players}, {"players", lives}}.dump() << '\n';
        }
        hp.flush(); Require(bool(hp), "Cannot write complete combat HP trace");
    }
};
