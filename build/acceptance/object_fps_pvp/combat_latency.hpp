// Product acceptance only: concurrent SDL firing and bounded read-only evidence.
// Included after weapon_short.hpp; no production controls or additional Drain.
class CombatLatencyEvidence {
    struct Submitted { std::uint64_t id{}, frame{}; double seconds{}; };
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
        std::uint32_t hud{};
        bool presented{};
    };
    const Options& options_;
    bool mover_{}, releaseMouse_{}, expectedShot_{};
    unsigned planned_{}, dispatched_{};
    std::size_t bound_{};
    fps::pvp::PlayerId local_{}, target_{};
    std::uint32_t maximumHp_{}, damage_{};
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
        if (mover_) while (.2 + planned_ * .8 < options.duration) ++planned_;
        bound_ = static_cast<std::size_t>(std::ceil((options.duration + 45) * options.fps)) + 1024;
        frames_.reserve(bound_);
        submitted_.reserve(planned_ + 16);
        decisions_.reserve(planned_ + 16);
        feedback_.reserve(planned_ + 16);
    }
    void BeforeUpdate(double elapsed, fps::pvp::PvpApplication& application) {
        expectedShot_ = false;
        auto* window = application.Platform().NativeWindow();
        if (releaseMouse_) { PushMouse(window, false); releaseMouse_ = false; }
        if (!mover_ || dispatched_ >= planned_ || elapsed < .2 + dispatched_ * .8) return;
        const double due = .2 + dispatched_ * .8;
        ++dispatched_;
        Check(elapsed < due + .2, "A predeclared shot missed its 200 ms dispatch window");
        if (elapsed >= due + .2) return; // Never catch up missed shots or remove them from the denominator.
        PushMouse(window, true);
        releaseMouse_ = expectedShot_ = true;
    }
    void AfterUpdate(std::uint64_t frame, const fps::pvp::WeaponFeedbackObservation& before,
                     fps::pvp::PvpApplication& application) {
        const auto after = application.WeaponFeedback();
        const auto state = application.Connection().State();
        if (state.combatRules) {
            maximumHp_ = state.combatRules->maximumHp;
            damage_ = state.combatRules->shotDamage;
        }
        if (state.snapshot && state.snapshot->combat.size() == 2) {
            local_ = state.playerId;
            for (const auto& entry : state.snapshot->combat) if (entry.playerId != local_) target_ = entry.playerId;
        }
        Check(after.animationStarts == after.submittedActions, "Animation count differs from submitted SDL actions");
        if (expectedShot_)
            Check(after.submittedActions == before.submittedActions + 1 && after.shooting,
                  "A predeclared SDL edge did not submit one immediate local shot");
        else Check(after.submittedActions == before.submittedActions, "Unexpected shot outside the predeclared schedule");
        if (after.submittedActions != before.submittedActions) {
            Check(after.submittedActions == before.submittedActions + 1, "Multiple submissions cannot be fully observed in one frame");
            Require(submitted_.size() < planned_ + 16, "Combat submission buffer exhausted");
            Submitted entry{after.lastActionId, frame, after.lastSubmittedSeconds};
            submitted_.push_back(entry);
            Check(!awaiting_, "Another shot arrived before the previous successful Presented sample");
            awaiting_ = entry;
        }
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
        sample.presented = presented.has_value();
        if (presented) { sample.hudTick = presented->local.authorityTick; sample.hud = presented->weapon.hp; }
        frames_.push_back(sample);
    }
    void Save(fps::pvp::PvpApplication& application) {
        const auto w = application.WeaponFeedback();
        const auto t = application.Connection().State().actionTransport;
        Check(dispatched_ == planned_, "Not all predeclared shot events were dispatched");
        Check(w.submittedActions == planned_ && submitted_.size() == planned_, "Not all predeclared shots were submitted");
        Check(w.decisionCount == planned_ && decisions_.size() == planned_, "Some shot outcomes were not fully observed");
        Check(feedback_.size() == planned_ && !awaiting_, "Some shots lack next-Presented feedback");
        WeaponJson report{{"schema_version", 1}, {"enabled", true}, {"role", options_.role},
            {"duration_seconds", options_.duration}, {"local_id", local_}, {"expected_target_id", target_},
            {"maximum_hp", maximumHp_}, {"damage_per_hit", damage_}, {"planned_shots", planned_},
            {"shot_period_seconds", .8}, {"shot_start_seconds", .2}, {"dispatched_shots", dispatched_},
            {"submitted_shots", w.submittedActions}, {"decision_count", w.decisionCount},
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
            {"action_id", entry.id}, {"generated_seconds", entry.seconds}, {"submission_frame_id", entry.frame}});
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
                {"received_seconds", decision.lastDecisionSeconds}});
        }
        std::ofstream out(options_.output / (options_.role + "-combat.json"));
        out << report.dump(2) << '\n';
        out.flush(); Require(bool(out), "Cannot write combat summary");
        std::ofstream hp(options_.output / (options_.role + "-combat-hp.jsonl"));
        for (const auto& entry : frames_) {
            WeaponJson players = WeaponJson::array();
            for (const auto& player : entry.combat) players.push_back({{"player_id", player.playerId}, {"hp", player.hp}});
            hp << WeaponJson{{"frame_id", entry.frame}, {"host_seconds", entry.seconds},
                {"snapshot_tick", entry.tick}, {"local_id", entry.local}, {"presented", entry.presented},
                {"hud_tick", entry.hudTick}, {"hud_hp", entry.hud}, {"combat", players}}.dump() << '\n';
        }
        hp.flush(); Require(bool(hp), "Cannot write complete combat HP trace");
    }
};
