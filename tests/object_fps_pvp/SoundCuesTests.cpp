#include <doctest/doctest.h>

#include "object_fps_pvp/present/SoundCues.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace object_fps_pvp::present;
using GYO::Engine::Math::Vec3;

std::string ProductText() {
    std::ifstream file(PVP_SOUND_SETTINGS_PATH);
    REQUIRE(file);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

SoundSettings ProductSettings() {
    std::string error;
    const auto settings = ParseSoundSettings(ProductText(), error);
    REQUIRE_MESSAGE(settings, error);
    return *settings;
}

// The product text with one spelling replaced (it must be there exactly once).
std::string Changed(const std::string& from, const std::string& to) {
    auto text = ProductText();
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    REQUIRE(text.find(from, at + 1) == std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}
} // namespace

TEST_CASE("PvP sound settings parse the product content and reject bad values") {
    const auto settings = ProductSettings();
    CHECK(settings.cues[static_cast<std::size_t>(SoundCue::LocalShot)].clip == "object_fps_pvp.audio.gun_shoot");
    CHECK(settings.cues[static_cast<std::size_t>(SoundCue::HitConfirmed)].clip == "object_fps_pvp.audio.hit_flesh");
    CHECK(settings.strideDistance > 0);
    std::string error;
    CHECK_FALSE(ParseSoundSettings("{\"version\":3}", error));
    CHECK_FALSE(error.empty());
    for (const auto& [what, text] : std::vector<std::pair<std::string, std::string>>{
             {"version 2", Changed("\"version\": 3", "\"version\": 2")},
             // Event sounds are the weapon's own data since version 3.
             {"version 2's animation_events",
              Changed("\"version\": 3,",
                      "\"version\": 3, \"animation_events\": {\"magazine_in\": {\"cue\": \"reload\", \"sync_seconds\": 0.33}},")},
             {"a volume above 4", Changed("\"volume\": 0.8", "\"volume\": 9.0")}}) {
        CAPTURE(what);
        CHECK_FALSE(ParseSoundSettings(text, error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("PvP spatial gains pan by bearing with constant power and fall off with distance") {
    SoundSettings settings = ProductSettings();
    settings.referenceDistance = 2;
    settings.minimumGain = 0.1F;
    // Yaw 0 looks along +Z; +X is to the right.
    const auto ahead = SpatialGains({0, 0, 0}, 0, {0, 0, 1}, 1, settings);
    CHECK(ahead.left == doctest::Approx(1));
    CHECK(ahead.right == doctest::Approx(1));
    const auto right = SpatialGains({0, 0, 0}, 0, {1, 0, 0}, 1, settings);
    CHECK(right.left == doctest::Approx(0).epsilon(1e-6));
    CHECK(right.right == doctest::Approx(std::sqrt(2.0F)));
    const auto left = SpatialGains({0, 0, 0}, 0, {-1, 0, 0}, 1, settings);
    CHECK(left.left > left.right);
    // Turning the listener right puts a source ahead on its left.
    const auto turned = SpatialGains({0, 0, 0}, 1.5707964F, {0, 0, 1}, 1, settings);
    CHECK(turned.left > turned.right);
    // Equal power on both sides: left^2 + right^2 is constant around the listener.
    for (const float angle : {0.0F, 0.5F, 1.0F, 2.0F, 3.0F}) {
        const auto g = SpatialGains({0, 0, 0}, 0, {std::sin(angle), 0, std::cos(angle)}, 1, settings);
        CHECK(g.left * g.left + g.right * g.right == doctest::Approx(2));
    }
    const auto distant = SpatialGains({0, 0, 0}, 0, {0, 0, 8}, 1, settings);
    CHECK(distant.left == doctest::Approx(0.25)); // reference 2 over distance 8
    const auto beyond = SpatialGains({0, 0, 0}, 0, {0, 0, 100}, 1, settings);
    CHECK(beyond.left == doctest::Approx(0.1)); // never below the minimum gain
    const auto here = SpatialGains({1, 0, 1}, 0, {1, 2, 1}, 0.5F, settings);
    CHECK(here.left == doctest::Approx(here.right));
}

TEST_CASE("PvP footstep tracker steps every stride on the ground and lands once") {
    SoundSettings settings = ProductSettings();
    settings.strideDistance = 1;
    settings.teleportDistance = 2;
    FootstepTracker steps;
    using Step = FootstepTracker::Step;
    CHECK(steps.Observe({0, 0, 0}, true, 1, settings) == Step::None); // the first sample only starts
    CHECK(steps.Observe({0.6F, 0, 0}, true, 1, settings) == Step::None);
    CHECK(steps.Observe({1.2F, 0, 0}, true, 1, settings) == Step::Footstep);
    CHECK(steps.Observe({1.8F, 0, 0}, true, 1, settings) == Step::None);
    CHECK(steps.Observe({2.4F, 0, 0}, true, 1, settings) == Step::Footstep);
    // In the air no steps, however far; touching down is one landing.
    CHECK(steps.Observe({3.4F, 1, 0}, false, 1, settings) == Step::None);
    CHECK(steps.Observe({4.4F, 1, 0}, false, 1, settings) == Step::None);
    CHECK(steps.Observe({4.5F, 0, 0}, true, 1, settings) == Step::Landing);
    CHECK(steps.Observe({4.6F, 0, 0}, true, 1, settings) == Step::None);
    // Standing still never steps.
    CHECK(steps.Observe({4.6F, 0, 0}, true, 1, settings) == Step::None);
    // A teleport or a new life restarts silently.
    CHECK(steps.Observe({20, 0, 0}, true, 1, settings) == Step::None);
    CHECK(steps.Observe({20.9F, 0, 0}, false, 1, settings) == Step::None);
    CHECK(steps.Observe({20.9F, 0, 0}, true, 2, settings) == Step::None); // new life: no landing replay
}

TEST_CASE("PvP hit taken sounds for a new hit and the killing one, never for a new life's first state") {
    CHECK(HitTakenSounds(HitFeedbackOutcome::Played));
    CHECK(HitTakenSounds(HitFeedbackOutcome::SuppressedDead)); // the killing blow sounds, though it does not flash
    CHECK_FALSE(HitTakenSounds(HitFeedbackOutcome::SuppressedNewLife));
    CHECK_FALSE(HitTakenSounds(HitFeedbackOutcome::None));
}
