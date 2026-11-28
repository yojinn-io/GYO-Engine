// The door's picture (present/DoorPicture.hpp; interact-door declaration section 8): the shipped settings, what the
// file may not say, and the swing's pace.
#include <doctest/doctest.h>

#include "object_fps_pvp/present/DoorPicture.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace {
using namespace object_fps_pvp::present;

std::string ShippedText() {
    std::ifstream file(std::string(PVP_CONTENT_ROOT) + "/ui/doors.json");
    REQUIRE(file);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}
} // namespace

TEST_CASE("PvP door picture: the shipped settings read; another version or a swing outside [0, 10] s does not") {
    std::string error;
    const auto shipped = ParseDoorPictureSettings(ShippedText(), error);
    REQUIRE_MESSAGE(shipped.has_value(), error);
    CHECK(shipped->swingSeconds > 0.0F);
    CHECK_FALSE(ParseDoorPictureSettings(R"({"version": 2, "swing_seconds": 0.3})", error).has_value());
    CHECK(error.find("unsupported version") != std::string::npos);
    CHECK_FALSE_MESSAGE(ParseDoorPictureSettings(R"({"version": 1, "swing_seconds": -1})", error).has_value(), "[pvp-door-swing-read]");
    CHECK_FALSE(ParseDoorPictureSettings(R"({"version": 1, "swing_seconds": 11})", error).has_value());
    CHECK_FALSE(ParseDoorPictureSettings(R"({"version": 1})", error).has_value());
}

TEST_CASE("PvP door picture: a swing goes toward the state at the settings' pace and stops there") {
    const DoorPictureSettings settings{.swingSeconds = 0.5F};
    CHECK_MESSAGE(SwingToward(0.0F, true, 0.25, settings) == doctest::Approx(0.5F), "[pvp-door-swing-pace] half the time, half way");
    CHECK_MESSAGE(SwingToward(0.5F, true, 1.0, settings) == 1.0F, "[pvp-door-swing-pace] never past open");
    CHECK(SwingToward(1.0F, false, 0.125, settings) == doctest::Approx(0.75F));
    CHECK_MESSAGE(SwingToward(0.25F, false, 1.0, settings) == 0.0F, "[pvp-door-swing-pace] never past shut");
    CHECK(SwingToward(1.0F, true, 0.1, settings) == 1.0F);
    CHECK(SwingToward(0.4F, true, -1.0, settings) == 0.4F);
    // No swing time: at once.
    CHECK(SwingToward(0.0F, true, 0.0, DoorPictureSettings{.swingSeconds = 0.0F}) == 1.0F);
}
