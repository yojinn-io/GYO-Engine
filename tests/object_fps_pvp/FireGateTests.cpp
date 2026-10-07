#include <doctest/doctest.h>

#include "RetroFPS/Pvp/FireGate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace {
using namespace fps::pvp;

constexpr double StepMs = 1000.0 / AuthorityTickRate;

FireGateTiming Timing(std::uint64_t authorityTick, std::uint64_t ahead, double secondsSinceStep,
                      double phaseShiftSeconds = 0) {
    return {.authorityTick = authorityTick, .lastResolvedCommand = 50, .latestCommand = 50 + ahead,
            .secondsSinceStep = secondsSinceStep, .phaseShiftSeconds = phaseShiftSeconds, .phaseDecided = true};
}

CombatState Combat(std::uint64_t nextAllowedShotTick, ActionId lastShotActionId = 0) {
    CombatState combat;
    combat.playerId = 1;
    combat.nextAllowedShotTick = nextAllowedShotTick;
    combat.lastShotActionId = lastShotActionId;
    return combat;
}

ShotDecision Decision(ActionId id, bool accepted, std::uint64_t resolvedTick, ActionKind kind = ActionKind::Shot) {
    ShotDecision decision;
    decision.actionId = id;
    decision.accepted = accepted;
    decision.resolvedTick = resolvedTick;
    decision.kind = kind;
    decision.rejection = accepted ? ShotRejection::None : ShotRejection::Cooldown;
    return decision;
}

} // namespace

TEST_CASE("Earliest shot resolve tick follows the newest command less the lead") {
    // newest = 100 + 4; a shot sent at the step boundary reaches the tick two
    // steps before it, one step later once the step is older than 4 + 2 ms.
    CHECK(EarliestShotResolveTick(Timing(100, 4, 0.0)) == 102);
    CHECK(EarliestShotResolveTick(Timing(100, 4, 0.006)) == 102);
    CHECK(EarliestShotResolveTick(Timing(100, 4, 0.0061)) == 103);
    CHECK(EarliestShotResolveTick(Timing(100, 4, 0.0166)) == 103);
    // A correction still delaying the phase widens the guard; one advancing it does not.
    CHECK(EarliestShotResolveTick(Timing(100, 4, 0.010, 0.010)) == 102);
    CHECK(EarliestShotResolveTick(Timing(100, 4, 0.010, 0.030)) == 101);
    CHECK(EarliestShotResolveTick(Timing(100, 4, 0.010, -0.030)) == 103);
    // Never before the tick after the snapshot.
    CHECK(EarliestShotResolveTick(Timing(100, 0, 0.0)) == 101);
    CHECK(EarliestShotResolveTick(Timing(0, 0, 0.0)) == 1);
}

TEST_CASE("Earliest shot resolve tick is unknown without a decided phase or valid commands") {
    auto timing = Timing(100, 4, 0.0);
    timing.phaseDecided = false;
    CHECK_FALSE(EarliestShotResolveTick(timing));
    timing = Timing(100, 4, 0.0);
    timing.latestCommand = timing.lastResolvedCommand - 1;
    CHECK_FALSE(EarliestShotResolveTick(timing));
    timing = Timing(100, 4, std::nan(""));
    CHECK_FALSE(EarliestShotResolveTick(timing));
    timing = Timing(100, 4, 0.0, INFINITY);
    CHECK_FALSE(EarliestShotResolveTick(timing));
}

TEST_CASE("Local fire gate waits for every known cooldown") {
    LocalFireGate gate;
    gate.ObserveSnapshot(100, Combat(105));
    CHECK(gate.RemainingTicks(Timing(100, 4, 0.0)) == 3);
    CHECK(gate.Allows(Timing(103, 4, 0.0)));
    // Without a bounded timing the snapshot tick stands in.
    CHECK(gate.RemainingTicks(std::nullopt) == 5);

    // An accepted decision sets the cooldown before a snapshot shows it;
    // rejections and reloads do not.
    gate.ObserveDecision(Decision(3, true, 104), 10);
    CHECK(gate.RemainingTicks(Timing(103, 4, 0.0)) == 9);
    gate.ObserveDecision(Decision(4, false, 120), 10);
    gate.ObserveDecision(Decision(5, true, 130, ActionKind::Reload), 10);
    CHECK(gate.RemainingTicks(Timing(103, 4, 0.0)) == 9);

    gate.Reset();
    CHECK(gate.Allows(std::nullopt));
}

TEST_CASE("Local fire gate holds a pending shot at its latest possible cooldown") {
    LocalFireGate gate;
    gate.ObserveSnapshot(100, Combat(0));
    const auto timing = Timing(100, 4, 0.0); // earliest 102
    REQUIRE(gate.Allows(timing));
    gate.Submitted(7, timing, 10);
    // 102 + 7 spread + 10 cooldown.
    CHECK(gate.RemainingTicks(Timing(110, 4, 0.0)) == 7);
    CHECK(gate.Allows(Timing(117, 4, 0.0)));
    // An older decision does not clear it; its own decision does.
    gate.ObserveDecision(Decision(6, true, 95), 10);
    CHECK(gate.RemainingTicks(Timing(110, 4, 0.0)) == 7);
    gate.ObserveDecision(Decision(7, true, 103), 10);
    CHECK(gate.RemainingTicks(Timing(108, 4, 0.0)) == 3);

    // A snapshot that includes the shot clears it as well.
    LocalFireGate other;
    other.Submitted(7, timing, 10);
    other.ObserveSnapshot(104, Combat(113, 6));
    CHECK(other.RemainingTicks(Timing(110, 4, 0.0)) == 7);
    other.ObserveSnapshot(105, Combat(113, 7));
    CHECK(other.RemainingTicks(Timing(110, 4, 0.0)) == 1);
}

TEST_CASE("A shot submitted without a bounded timing blocks until it is decided") {
    LocalFireGate gate;
    gate.ObserveSnapshot(100, Combat(0));
    gate.Submitted(1, std::nullopt, 10);
    CHECK(gate.RemainingTicks(Timing(500, 4, 0.0)) >= 1);
    CHECK(gate.RemainingTicks(std::nullopt) >= 1);
    gate.ObserveDecision(Decision(1, false, 101), 10);
    CHECK(gate.Allows(std::nullopt));
}

TEST_CASE("The gate is never stricter than the snapshot tick gate it replaces") {
    // Whenever the newest snapshot already reached its cooldown, nothing else
    // pending, a shot is allowed with or without a bounded timing.
    for (std::uint64_t snapshot = 0; snapshot < 40; ++snapshot) {
        for (std::uint64_t next = 0; next <= snapshot; ++next) {
            for (std::uint64_t ahead = 0; ahead < 6; ++ahead) {
                LocalFireGate gate;
                gate.ObserveSnapshot(snapshot, Combat(next));
                CHECK(gate.Allows(std::nullopt));
                CHECK(gate.Allows(Timing(snapshot, ahead, 0.0)));
                CHECK(gate.Allows(Timing(snapshot, ahead, 0.016)));
            }
        }
    }
}

namespace {

// Deterministic on every standard library: splitmix64 and sums of uniforms.
struct Random final {
    std::uint64_t state;
    double Uniform() {
        state += 0x9e3779b97f4a7c15ULL;
        auto z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return static_cast<double>((z ^ (z >> 31)) >> 11) * 0x1.0p-53;
    }
    double Uniform(double low, double high) { return low + (high - low) * Uniform(); }
    double Bell(double mean, double spread) { return mean + (Uniform() + Uniform() + Uniform() - 1.5) * spread; }
};

struct TimelineResult final {
    int clicks{}, blocked{}, accepted{}, cooldownRejected{};
    // Resolve tick less the gate's earliest tick, over submitted shots.
    std::int64_t minimumLead{(std::numeric_limits<std::int64_t>::max)()};
    std::int64_t maximumLead{(std::numeric_limits<std::int64_t>::min)()};

    void Add(const TimelineResult& run) {
        clicks += run.clicks;
        blocked += run.blocked;
        accepted += run.accepted;
        cooldownRejected += run.cooldownRejected;
        minimumLead = std::min(minimumLead, run.minimumLead);
        maximumLead = std::max(maximumLead, run.maximumLead);
    }
};

// How long a shot waits for the action pacing after its frame, beyond the
// command's own one-way delay. Worst: anywhere up to one Client and one
// Gateway send interval, a Gateway wake and a late worker wake, for every shot.
// Measured: mostly an idle 30 Hz deadline, sometimes one interval; the batch 10
// L2 clean cases measured 4.5-28 ms from submission to the resolving tick.
enum class Pacing { Worst, Measured };

// A synthetic match of one shooter, in milliseconds. The host executes tick k
// at k * step plus its wake lateness, in the about 4 ms or about 8 ms state of
// the batch 07 A/B investigation. Phase tracking aligns the 90th percentile of
// the commands' slack with the lead and the 4 ms target, within the 2 ms
// deadband: the shooter's step boundaries follow the 90th percentile of the
// lateness plus a fixed error within the deadband. A shot leaves with the
// frame's command and reaches the Match after the same one-way delay plus the
// pacing. Snapshots arrive every tick, results on a 30 Hz ticker.
TimelineResult RunTimeline(std::uint64_t seed, double fps, bool lateHost, std::uint64_t intervalTicks, int clicks,
                           Pacing pacing) {
    Random random{seed};
    const double oneWay = 0.3;
    const auto late = [&] {
        return lateHost ? std::clamp(random.Bell(7.2, 2.4), 4.0, 9.5) : std::clamp(random.Bell(3.0, 2.0), 0.5, 5.5);
    };
    const double start = 2000.0;
    const double end = start + (clicks + 2) * static_cast<double>(intervalTicks) * StepMs + 500.0;
    const auto ticks = static_cast<std::size_t>(end / StepMs) + 32;
    std::vector<double> lateness(ticks);
    for (auto& value : lateness) value = late();
    std::vector<double> sorted = lateness;
    std::sort(sorted.begin(), sorted.end());
    const double trackedLate = sorted[sorted.size() * 9 / 10];
    std::vector<double> execute(ticks);
    for (std::size_t k = 0; k < ticks; ++k) execute[k] = static_cast<double>(k) * StepMs + lateness[k];
    const double phaseError = random.Uniform(-MovementPhaseDeadbandSeconds, MovementPhaseDeadbandSeconds) * 1000.0;
    // Command L's step boundary: its tracked execution less one way, the lead and the target.
    const double boundaryOffset = trackedLate + phaseError - oneWay -
        (static_cast<double>(InitialCommandLead) * StepMs + MovementPhaseTargetSeconds * 1000.0);
    const double worstPacing = 2 * 1000.0 / ActionSendRate + StepMs + (lateHost ? 9.5 : 5.5);

    struct Accepted final { std::uint64_t tick; ActionId id; };
    std::vector<Accepted> acceptedShots;
    struct Pending final { double at; ShotDecision decision; };
    std::vector<Pending> decisions;
    std::uint64_t nextAllowed = 0;
    ActionId nextId = 1;

    LocalFireGate gate;
    TimelineResult result;
    std::uint64_t drainedTick = 0;
    double nextClick = start + random.Uniform(0, StepMs);
    double frame = random.Uniform(0, 1000.0 / fps);
    while (result.clicks < clicks) {
        frame += 1000.0 / fps + std::clamp(random.Bell(1.0, 1.2), 0.2, 2.2);
        REQUIRE(frame < end);
        while (drainedTick + 1 < ticks && execute[drainedTick + 1] + oneWay * 2 <= frame) ++drainedTick;
        std::uint64_t snapshotNext = 0;
        ActionId snapshotShot = 0;
        for (const auto& shot : acceptedShots) {
            if (shot.tick > drainedTick) continue;
            snapshotNext = std::max(snapshotNext, shot.tick + PvpCombatRules.cooldownTicks);
            snapshotShot = std::max(snapshotShot, shot.id);
        }
        gate.ObserveSnapshot(drainedTick, Combat(snapshotNext, snapshotShot));
        for (auto it = decisions.begin(); it != decisions.end();) {
            if (it->at > frame) { ++it; continue; }
            gate.ObserveDecision(it->decision, PvpCombatRules.cooldownTicks);
            it = decisions.erase(it);
        }
        if (frame < nextClick) continue;
        ++result.clicks;
        nextClick += static_cast<double>(intervalTicks) * StepMs;

        const auto latest = static_cast<std::uint64_t>(std::floor((frame - boundaryOffset) / StepMs));
        const FireGateTiming timing{.authorityTick = drainedTick, .lastResolvedCommand = drainedTick,
            .latestCommand = latest,
            .secondsSinceStep = (frame - boundaryOffset - static_cast<double>(latest) * StepMs) / 1000.0,
            .phaseDecided = true};
        if (!gate.Allows(timing)) {
            ++result.blocked;
            continue;
        }
        const ActionId id = nextId++;
        gate.Submitted(id, timing, PvpCombatRules.cooldownTicks);
        double wait = random.Uniform(0, worstPacing);
        if (pacing == Pacing::Measured)
            wait = random.Uniform() < 0.9 ? random.Uniform(0, 2.5) : random.Uniform(0, 1000.0 / ActionSendRate);
        const double arrival = frame + oneWay + wait;
        std::uint64_t resolved = drainedTick + 1;
        while (execute[resolved] < arrival) ++resolved;
        const auto lead = static_cast<std::int64_t>(resolved) - static_cast<std::int64_t>(*EarliestShotResolveTick(timing));
        result.minimumLead = std::min(result.minimumLead, lead);
        result.maximumLead = std::max(result.maximumLead, lead);
        ShotDecision decision;
        decision.actionId = id;
        decision.kind = ActionKind::Shot;
        decision.resolvedTick = resolved;
        if (resolved < nextAllowed) {
            decision.rejection = ShotRejection::Cooldown;
            ++result.cooldownRejected;
        } else {
            decision.accepted = true;
            nextAllowed = resolved + PvpCombatRules.cooldownTicks;
            acceptedShots.push_back({resolved, id});
            ++result.accepted;
        }
        decisions.push_back({execute[resolved] + oneWay * 2 + random.Uniform(0, 1000.0 / ActionSendRate), decision});
    }
    return result;
}

constexpr int TimelineSeeds = 12;
constexpr int TimelineClicks = 150;

TimelineResult RunTimelines(double fps, bool lateHost, std::uint64_t intervalTicks, Pacing pacing) {
    TimelineResult total;
    for (std::uint64_t seed = 1; seed <= TimelineSeeds; ++seed)
        total.Add(RunTimeline(seed * 7919 + static_cast<std::uint64_t>(fps) * 31 + intervalTicks * 3 + (lateHost ? 5 : 0),
                              fps, lateHost, intervalTicks, TimelineClicks, pacing));
    return total;
}

} // namespace

TEST_CASE("Synthetic frame and host jitter: the gate's bounds hold and the authority never rejects") {
    // Worst pacing for every shot, clicks from the cooldown itself upwards.
    for (const bool lateHost : {false, true}) {
        for (const double fps : {30.0, 60.0, 144.0}) {
            for (const std::uint64_t interval : {10U, 11U, 12U, 13U}) {
                const auto result = RunTimelines(fps, lateHost, interval, Pacing::Worst);
                CAPTURE(lateHost);
                CAPTURE(fps);
                CAPTURE(interval);
                CAPTURE(result.minimumLead);
                CAPTURE(result.maximumLead);
                CAPTURE(result.blocked);
                CAPTURE(result.cooldownRejected);
                CHECK(result.clicks == TimelineSeeds * TimelineClicks);
                CHECK(result.minimumLead >= 0);
                CHECK(result.maximumLead <= static_cast<std::int64_t>(FireGatePendingSpreadTicks));
                CHECK(result.cooldownRejected == 0);
                CHECK(result.blocked + result.accepted == result.clicks);
            }
        }
    }
}

TEST_CASE("Synthetic frame and host jitter: twelve-tick clicks are not eaten locally") {
    for (const bool lateHost : {false, true}) {
        for (const double fps : {30.0, 60.0, 144.0}) {
            const auto result = RunTimelines(fps, lateHost, 12, Pacing::Measured);
            CAPTURE(lateHost);
            CAPTURE(fps);
            CAPTURE(result.blocked);
            CAPTURE(result.cooldownRejected);
            CHECK(result.cooldownRejected == 0);
            CHECK(result.blocked * 100 <= result.clicks * (fps < 60 ? 5 : 1));
        }
    }
}
