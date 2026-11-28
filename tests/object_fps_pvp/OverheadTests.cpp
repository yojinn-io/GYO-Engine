#include <doctest/doctest.h>

#include "object_fps_pvp/present/Overhead.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace object_fps_pvp::present;

std::string ProductSettingsText() {
    std::ifstream file(std::string(PVP_CONTENT_ROOT) + "/ui/overhead.json");
    REQUIRE(file);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

OverheadSettings ProductSettings() {
    std::string error;
    const auto settings = ParseOverheadSettings(ProductSettingsText(), error);
    REQUIRE_MESSAGE(settings, error);
    return *settings;
}
} // namespace

TEST_CASE("Overhead settings come from the product file and reject invalid values, naming the member") {
    const OverheadSettings settings = ProductSettings();
    CHECK(settings.headClearanceMetres == doctest::Approx(0.35));
    CHECK(settings.textPixels == doctest::Approx(18));
    CHECK(settings.lowHealth == doctest::Approx(0.3));
    CHECK(settings.holdSeconds == doctest::Approx(3.0));
    CHECK(settings.fadeSeconds == doctest::Approx(1.0));
    CHECK(settings.fillColor != settings.lowColor);
    const std::string text = ProductSettingsText();
    for (const auto& [from, to, named] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"\"version\": 1", "\"version\": 2", "version"},
             {"\"head_clearance_metres\": 0.35", "\"head_clearance_metres\": -0.1", "head_clearance_metres"},
             {"\"text_pixels\": 18", "\"text_pixels\": 0", "text_pixels"},
             {"\"color\": [1.0, 1.0, 1.0]", "\"color\": [1.0, 1.0]", "name.color"},
             {"\"width_pixels\": 72", "\"width_pixels\": -72", "width_pixels"},
             {"\"back_alpha\": 0.55", "\"back_alpha\": 1.5", "back_alpha"},
             {"\"low_color\": [0.95, 0.25, 0.2]", "\"low_color\": [1.95, 0.25, 0.2]", "bar.low_color"},
             {"\"low_health\": 0.3", "\"low_health\": 1", "low_health"},
             {"\"low_health\": 0.3", "\"low_health\": 0", "low_health"},
             {"\"hold_seconds\": 3.0", "\"hold_seconds\": -1", "hold_seconds"},
             {"\"fade_seconds\": 1.0", "\"fade\": 1.0", "fade_seconds"},
             {"\"engaged\"", "\"engagement\"", "engaged"}}) {
        std::string changed = text;
        const auto at = changed.find(from);
        REQUIRE_MESSAGE(at != std::string::npos, from);
        changed.replace(at, from.size(), to);
        std::string error;
        CHECK_MESSAGE(!ParseOverheadSettings(changed, error).has_value(), "[pvp-overhead-settings] refused: " << to);
        CHECK_MESSAGE(error.find(named) != std::string::npos, "[pvp-overhead-settings-named] " << error << " names " << named);
    }
}

TEST_CASE("A health bar's fraction is the whole points a player reads: any health left is never an empty bar") {
    CHECK(HealthFraction(100'000, 100'000) == doctest::Approx(1.0));
    CHECK(HealthFraction(50'000, 100'000) == doctest::Approx(0.5));
    // 1 milli-point reads as 1 of 100 points, not 0 (sim::DisplayPoints rounds up).
    CHECK_MESSAGE(HealthFraction(1, 100'000) == doctest::Approx(0.01), "[pvp-overhead-fraction-points]");
    CHECK(HealthFraction(0, 100'000) == 0.0F);
    CHECK(HealthFraction(99'001, 100'000) == doctest::Approx(1.0));
    CHECK(HealthFraction(5, 0) == 0.0F);
}

TEST_CASE("A health bar shows always, or while engaged: a recent own hit holds it, then it fades; low health keeps it") {
    const OverheadSettings s = ProductSettings();
    const double hit = 10.0;
    CHECK_MESSAGE(OverheadBarAlpha(OverheadPolicy::Always, 1.0F, std::nullopt, 0.0, s) == 1.0F, "[pvp-overhead-always]");
    CHECK_MESSAGE(OverheadBarAlpha(OverheadPolicy::Engaged, 1.0F, std::nullopt, 50.0, s) == 0.0F, "[pvp-overhead-engaged-unhit]");
    CHECK_MESSAGE(OverheadBarAlpha(OverheadPolicy::Engaged, 0.8F, hit, hit, s) == 1.0F, "[pvp-overhead-hold] at the hit");
    CHECK_MESSAGE(OverheadBarAlpha(OverheadPolicy::Engaged, 0.8F, hit, hit + s.holdSeconds, s) == 1.0F, "[pvp-overhead-hold]");
    CHECK_MESSAGE(OverheadBarAlpha(OverheadPolicy::Engaged, 0.8F, hit, hit + s.holdSeconds + s.fadeSeconds / 2, s) ==
                      doctest::Approx(0.5),
                  "[pvp-overhead-fade]");
    CHECK(OverheadBarAlpha(OverheadPolicy::Engaged, 0.8F, hit, hit + s.holdSeconds + s.fadeSeconds, s) == 0.0F);
    CHECK(OverheadBarAlpha(OverheadPolicy::Engaged, 0.8F, hit, hit + 100.0, s) == 0.0F);
    // Hit again: shown again.
    CHECK(OverheadBarAlpha(OverheadPolicy::Engaged, 0.8F, hit + 100.0, hit + 100.0, s) == 1.0F);
    // At or below the low mark it shows, hit or not; just above it does not.
    CHECK_MESSAGE(OverheadBarAlpha(OverheadPolicy::Engaged, s.lowHealth, std::nullopt, 0.0, s) == 1.0F, "[pvp-overhead-low]");
    CHECK(OverheadBarAlpha(OverheadPolicy::Engaged, 0.2F, std::nullopt, 0.0, s) == 1.0F);
    CHECK(OverheadBarAlpha(OverheadPolicy::Engaged, s.lowHealth + 0.01F, std::nullopt, 0.0, s) == 0.0F);
    // No bar (a player's label): never.
    CHECK_MESSAGE(OverheadBarAlpha(OverheadPolicy::Always, std::nullopt, hit, hit, s) == 0.0F, "[pvp-overhead-no-bar]");
}
