#include <doctest/doctest.h>

#include "RetroFPS/Pvp/HitFeedback.hpp"

#include <cmath>
#include <fstream>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace fps::pvp;
using Engine::Math::Vec3;

std::string ProductSettingsText() {
    std::ifstream file(PVP_HIT_FEEDBACK_PATH);
    REQUIRE(file);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

HitFeedbackSettings ProductSettings() {
    std::string error;
    const auto settings = ParseHitFeedbackSettings(ProductSettingsText(), error);
    REQUIRE_MESSAGE(settings, error);
    return *settings;
}

CombatState Own(std::uint32_t damageCount, std::uint64_t lifeGeneration = 1, PlayerId attacker = 2) {
    CombatState combat;
    combat.playerId = 1;
    combat.lifeGeneration = lifeGeneration;
    combat.damageCount = damageCount;
    combat.lastAttackerId = damageCount ? attacker : 0;
    combat.lastDamageTick = damageCount ? 100 : 0;
    return combat;
}

constexpr Vec3 Self{5, 0, 5};
constexpr Vec3 EastAttacker{9, 0, 5};
} // namespace

TEST_CASE("Hit feedback settings come from the product file and reject invalid values") {
    const auto settings = ProductSettings();
    CHECK(settings.flash.peakAlpha == doctest::Approx(0.35));
    CHECK(settings.flash.seconds == doctest::Approx(0.3));
    CHECK(settings.hpHighlight.seconds == doctest::Approx(0.6));
    CHECK(settings.direction.seconds == doctest::Approx(1.0));
    CHECK(settings.direction.dots == 7);
    CHECK(settings.shake.pitchRadians == doctest::Approx(std::numbers::pi / 180));
    CHECK(settings.shake.seconds == doctest::Approx(0.25));
    const auto text = ProductSettingsText();
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"\"version\": 1", "\"version\": 2"}, {"\"peak_alpha\": 0.35", "\"peak_alpha\": 0"},
             {"\"peak_alpha\": 0.35", "\"peak_alpha\": 1.5"}, {"\"dots\": 7", "\"dots\": 1"},
             {"\"seconds\": 0.25", "\"seconds\": -0.25"}, {"[0.85, 0.05, 0.05]", "[1.85, 0.05, 0.05]"},
             {"\"shake\"", "\"wobble\""}}) {
        CAPTURE(to);
        auto changed = text;
        const auto at = changed.find(from);
        REQUIRE(at != std::string::npos);
        changed.replace(at, from.size(), to);
        std::string error;
        CHECK_FALSE(ParseHitFeedbackSettings(changed, error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("Hit feedback starts once per new hit and never replays old or late snapshots") {
    const auto settings = ProductSettings();
    LocalHitFeedback feedback;
    // The first state seen (a join) never plays, even with earlier hits in it.
    feedback.Observe(Own(2), LifeState::Alive, Self, EastAttacker, 10.0);
    CHECK(feedback.Sample(10.0, 0, settings).flashAlpha == 0);
    // A new hit starts the effect at its observation time.
    feedback.Observe(Own(3), LifeState::Alive, Self, EastAttacker, 11.0);
    CHECK(feedback.Sample(11.0, 0, settings).flashAlpha == doctest::Approx(settings.flash.peakAlpha));
    CHECK(feedback.Sample(11.15, 0, settings).flashAlpha == doctest::Approx(settings.flash.peakAlpha / 2));
    CHECK(feedback.Sample(11.31, 0, settings).flashAlpha == 0);
    // A duplicate or late snapshot with the same or an older count does not restart it.
    feedback.Observe(Own(3), LifeState::Alive, Self, EastAttacker, 11.2);
    feedback.Observe(Own(2), LifeState::Alive, Self, EastAttacker, 11.25);
    CHECK(feedback.Sample(11.31, 0, settings).flashAlpha == 0);
    // Several hits at once start one effect.
    feedback.Observe(Own(6), LifeState::Alive, Self, EastAttacker, 12.0);
    CHECK(feedback.Sample(12.0, 0, settings).flashAlpha == doctest::Approx(settings.flash.peakAlpha));
    CHECK(feedback.Sample(12.5, 0, settings).hpHighlighted);
    CHECK_FALSE(feedback.Sample(12.61, 0, settings).hpHighlighted);
}

TEST_CASE("Hit feedback is isolated per life and suppressed by death") {
    const auto settings = ProductSettings();
    LocalHitFeedback feedback;
    feedback.Observe(Own(0, 1), LifeState::Alive, Self, std::nullopt, 1.0);
    // The first state of a new life, already hit, does not replay.
    feedback.Observe(Own(1, 2), LifeState::Alive, Self, EastAttacker, 2.0);
    CHECK(feedback.Sample(2.0, 0, settings).flashAlpha == 0);
    CHECK_FALSE(feedback.Sample(2.0, 0, settings).directionRadians);
    // The fatal hit shows death only.
    feedback.Observe(Own(2, 2), LifeState::Dead, Self, EastAttacker, 3.0);
    const auto dead = feedback.Sample(3.0, 0, settings);
    CHECK(dead.flashAlpha == 0);
    CHECK_FALSE(dead.hpHighlighted);
    CHECK_FALSE(dead.directionRadians);
    CHECK(dead.shakePitchRadians == 0);
}

TEST_CASE("Hit direction points at the attacker from the same snapshot and turns with the view") {
    const auto settings = ProductSettings();
    LocalHitFeedback feedback;
    feedback.Observe(Own(0), LifeState::Alive, Self, std::nullopt, 0.0);
    feedback.Observe(Own(1), LifeState::Alive, Self, EastAttacker, 1.0);
    // Yaw turns +Z toward +X: an attacker at +X is to the right while facing +Z.
    const auto ahead = feedback.Sample(1.0, 0, settings);
    REQUIRE(ahead.directionRadians);
    CHECK(*ahead.directionRadians == doctest::Approx(std::numbers::pi / 2));
    CHECK(ahead.directionAlpha == doctest::Approx(1));
    // Turning toward the attacker puts it ahead.
    const auto turned = feedback.Sample(1.5, static_cast<float>(std::numbers::pi / 2), settings);
    REQUIRE(turned.directionRadians);
    CHECK(std::abs(*turned.directionRadians) < 1e-6F);
    CHECK(turned.directionAlpha == doctest::Approx(0.5));
    CHECK_FALSE(feedback.Sample(2.01, 0, settings).directionRadians);

    LocalHitFeedback absent, same;
    absent.Observe(Own(0), LifeState::Alive, Self, std::nullopt, 0.0);
    absent.Observe(Own(1), LifeState::Alive, Self, std::nullopt, 1.0);
    CHECK_FALSE(absent.Sample(1.0, 0, settings).directionRadians);
    CHECK(absent.Sample(1.0, 0, settings).flashAlpha > 0);
    same.Observe(Own(0), LifeState::Alive, Self, std::nullopt, 0.0);
    same.Observe(Own(1), LifeState::Alive, Self, Self, 1.0);
    CHECK_FALSE(same.Sample(1.0, 0, settings).directionRadians);
}

TEST_CASE("Hit shake reaches only the camera: sent view angles are bit-identical with and without it") {
    const auto settings = ProductSettings();
    LocalHitFeedback hits;
    hits.Observe(Own(0), LifeState::Alive, Self, std::nullopt, 0.0);
    ClientView shaken, still;
    shaken.Reset(0.3F, -0.2F);
    still.Reset(0.3F, -0.2F);
    const std::vector<std::pair<float, float>> mouse{{12, -4}, {0, 0}, {-30, 9}, {7, 7}, {500, -900}, {-3, 2}, {0, 1}};
    double now = 1.0;
    bool shook = false;
    for (std::size_t frame = 0; frame < mouse.size(); ++frame, now += 1.0 / 60) {
        if (frame == 1) hits.Observe(Own(1), LifeState::Alive, Self, EastAttacker, now);
        shaken.Turn(mouse[frame].first, mouse[frame].second);
        still.Turn(mouse[frame].first, mouse[frame].second);
        const auto sample = hits.Sample(now, shaken.Input().yaw, settings);
        shaken.Present(sample);
        still.Present({});
        CAPTURE(frame);
        // Bit-identical inputs: what Advance and SubmitAction send.
        CHECK(shaken.Input().yaw == still.Input().yaw);
        CHECK(shaken.Input().pitch == still.Input().pitch);
        // The camera alone carries the shake.
        CHECK(shaken.Camera().yaw == still.Camera().yaw);
        CHECK(shaken.Camera().pitch == shaken.Input().pitch + sample.shakePitchRadians);
        CHECK(shaken.Camera().roll == sample.shakeRollRadians);
        shook = shook || sample.shakePitchRadians != 0;
    }
    CHECK(shook);
    // After the shake fades the camera equals the view again.
    shaken.Present(hits.Sample(now + 1.0, shaken.Input().yaw, settings));
    CHECK(shaken.Camera().pitch == shaken.Input().pitch);
    CHECK(shaken.Camera().roll == 0);
}

TEST_CASE("Client view turns by mouse counts, wraps yaw and clamps pitch") {
    ClientView view;
    view.Reset(0, 0);
    view.Turn(100, -40);
    CHECK(view.Input().yaw == doctest::Approx(0.25));
    CHECK(view.Input().pitch == doctest::Approx(-0.1));
    view.Turn(0, 1e6F);
    CHECK(view.Input().pitch == MovementMaximumPitch);
    view.Turn(0, -1e6F);
    CHECK(view.Input().pitch == -MovementMaximumPitch);
    view.Reset(3.1F, 0);
    view.Turn(40, 0);
    CHECK(view.Input().yaw < 0);
    CHECK(view.Input().yaw == doctest::Approx(3.2 - 2 * std::numbers::pi).epsilon(1e-5));
}
