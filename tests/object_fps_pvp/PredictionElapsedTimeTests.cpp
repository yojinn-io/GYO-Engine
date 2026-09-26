#include <doctest/doctest.h>

#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/PredictionElapsedTime.hpp"

#include <chrono>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;

Arena TimingArena() {
    Arena arena;
    arena.id = "prediction_callsite_timing";
    arena.width = arena.depth = 30;
    arena.spawns = {{{2, 0, 2}, 0}, {{20, 0, 2}, 0}};
    return arena;
}
}

TEST_CASE("Prediction call-site time does not replay a stall in the following runtime frame") {
    // GUI trace: the runtime stamped frame 656 before its blocking work, then
    // frame 657 carried that old work in delta despite arriving only 1.16 ms
    // after Advance. Reconciliation in 656 had already covered the interval.
    PredictionElapsedTime clock;
    LocalPlayerPrediction prediction(TimingArena());
    LocalPlayerPrediction oldFrameDelta(TimingArena());
    PlayerState authority{.playerId = 1, .position = {2, 0, 2}, .lastResolvedCommand = 656};
    const PredictionElapsedTime::Clock::time_point start{};
    prediction.Reconcile(authority, 700);
    oldFrameDelta.Reconcile(authority, 700);
    CHECK(prediction.Advance(clock.Sample(start), 1, 0, 0, 0));
    CHECK(oldFrameDelta.Advance(0, 1, 0, 0, 0));
    REQUIRE(prediction.Observation().latestCommand == 658);
    authority.lastResolvedCommand = 659;
    prediction.Reconcile(authority, 704);
    oldFrameDelta.Reconcile(authority, 704);
    CHECK(prediction.Advance(clock.Sample(start + 64268us), 1, 0, 0, 0));
    CHECK(oldFrameDelta.Advance(.016734184, 1, 0, 0, 0));
    REQUIRE(prediction.Observation().latestCommand == 662);
    REQUIRE(oldFrameDelta.Observation().latestCommand == 662);
    authority.lastResolvedCommand = 659;
    prediction.Reconcile(authority, 705);
    oldFrameDelta.Reconcile(authority, 705);
    CHECK_FALSE(prediction.Advance(clock.Sample(start + 65428us), 1, 0, 0, 0));
    CHECK(oldFrameDelta.Advance(.048389225, 1, 0, 0, 0));
    CHECK(prediction.Observation().latestCommand == 662);
    CHECK(prediction.Observation().pendingCommands == InitialCommandLead + 1);
    CHECK(oldFrameDelta.Observation().latestCommand == 664);
    CHECK(prediction.Observation().interpolationAlpha == doctest::Approx(.00116 / MovementTickSeconds));
}

TEST_CASE("Prediction lifecycle starts a new movement sampling interval") {
    PredictionElapsedTime clock;
    const PredictionElapsedTime::Clock::time_point start{};
    CHECK(clock.Sample(start) == 0);
    CHECK(clock.Sample(start + 17ms) == doctest::Approx(.017));
    clock.Reset();
    CHECK(clock.Sample(start + 20s) == 0);
    CHECK(clock.Sample(start + 20s + 7ms) == doctest::Approx(.007));
}

TEST_CASE("Prediction call-site samples preserve regular 30 60 and 144 FPS command rates") {
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        PredictionElapsedTime clock;
        LocalPlayerPrediction prediction(TimingArena());
        PlayerState authority{.playerId = 1, .position = {2, 0, 2}};
        prediction.Reconcile(authority, 1);
        const PredictionElapsedTime::Clock::time_point start{};
        bool published = prediction.Advance(clock.Sample(start), 0, 0, 0, 0);
        CHECK_FALSE(published); // The neutral-only bootstrap is not yet published.
        for (int frame = 1; frame <= fps * 5; ++frame) {
            if (published) {
                for (const auto& command : prediction.PendingInput().commands)
                    if (command.sequence > authority.lastResolvedCommand)
                        authority = StepMovement(TimingArena(), authority, command);
            }
            prediction.Reconcile(authority, static_cast<std::uint64_t>(frame + 1));
            const auto now = start + std::chrono::duration_cast<PredictionElapsedTime::Clock::duration>(
                std::chrono::duration<double>(static_cast<double>(frame) / fps));
            published = prediction.Advance(clock.Sample(now), 0, 0, 0, 0);
        }
        CHECK(prediction.Observation().latestCommand == InitialCommandLead + 300);
        CHECK_FALSE(prediction.Observation().frozen);
    }
}
