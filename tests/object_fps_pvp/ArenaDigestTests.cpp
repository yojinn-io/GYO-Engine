#include <doctest/doctest.h>

#include "RetroFPS/Pvp/Arena.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
using namespace fps::pvp;

// Synthetic arena with decimals that are not exact binary fractions, so the
// golden value also pins strtod on every CI platform.
constexpr const char* Fixture = R"({"version": 1, "id": "digest_fixture_arena", "width": 20.1, "depth": 12.3, "cell_size": 0.7,
 "movement_speed": 3.3, "body_height": 1.85, "radius": 0.27, "eye_height": 1.62, "jump_height": 0.65, "gravity": 17.9,
 "walls": [{"min": [-1, 0, -1], "max": [0, 3.3, 13.3]}, {"min": [10.1, 0, 4.4], "max": [11.2, 2.5, 5.6]}],
 "spawns": [{"position": [2.2, 0, 2.3], "yaw": 0.3}, {"position": [17.7, 0, 9.9], "yaw": 3.14159265}]})";
// Computed independently of the product code (Python struct and FNV-1a).
constexpr std::uint64_t FixtureDigest = 0x60d800e09a796f00ULL;

Arena LoadText(const std::string& text) {
    static std::atomic<unsigned> counter{};
    const auto path = std::filesystem::temp_directory_path() /
        ("gyo_pvp_arena_digest_" + std::to_string(++counter) + ".json");
    { std::ofstream(path, std::ios::binary) << text; }
    std::string error;
    auto arena = Arena::Load(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(arena, error);
    return *arena;
}

std::uint64_t Digest(const std::string& text) { return ArenaContentDigest(LoadText(text)); }

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}
}

TEST_CASE("PvP arena content digest matches the independent golden value") {
    CHECK(Digest(Fixture) == FixtureDigest);
}

TEST_CASE("PvP arena content digest ignores the JSON spelling of the same content") {
    const auto json = nlohmann::json::parse(Fixture);
    std::string crlf = json.dump(2);
    for (std::size_t at = 0; (at = crlf.find('\n', at)) != std::string::npos; at += 2) crlf.replace(at, 1, "\r\n");
    CHECK(Digest(crlf) == FixtureDigest);
    CHECK(Digest(json.dump()) == FixtureDigest);                       // whitespace and (sorted) key order
    CHECK(Digest(Replace(Fixture, "\"width\": 20.1", "\"width\": 2.01e1")) == FixtureDigest);
    CHECK(Digest(Replace(Fixture, "\"gravity\": 17.9", "\"gravity\": 17.900")) == FixtureDigest);
    CHECK(Digest(Replace(Fixture, "\"version\": 1,", "\"version\": 1, \"unknown\": [1, 2],")) == FixtureDigest);
    auto defaults = json;
    defaults["cell_size"] = 1.0;
    auto omitted = defaults;
    omitted.erase("cell_size");
    CHECK(Digest(defaults.dump()) == Digest(omitted.dump()));
}

TEST_CASE("PvP arena content digest changes with every member, order and signed zero") {
    const auto json = nlohmann::json::parse(Fixture);
    const auto changed = [&](auto edit) {
        auto copy = json;
        edit(copy);
        return Digest(copy.dump()) != FixtureDigest;
    };
    CHECK(changed([](auto& j) { j["id"] = "digest_fixture_arenb"; }));
    for (const char* key : {"width", "depth", "cell_size", "movement_speed", "body_height", "radius", "eye_height",
                            "jump_height", "gravity"}) {
        CAPTURE(key);
        CHECK(changed([&](auto& j) { j[key] = j[key].template get<double>() * 1.01; }));
    }
    CHECK(changed([](auto& j) { j["walls"][1]["max"][1] = 2.6; }));
    CHECK(changed([](auto& j) { j["spawns"][0]["yaw"] = 0.31; }));
    CHECK(changed([](auto& j) { std::swap(j["walls"][0], j["walls"][1]); }));
    CHECK(changed([](auto& j) { std::swap(j["spawns"][0], j["spawns"][1]); }));
    CHECK(changed([](auto& j) { j["walls"][0]["min"][1] = -0.0; }));  // JSON -0.0, not -0 (an integer +0)
    CHECK(changed([](auto& j) { j["walls"].erase(1); }));
    // Version 1 is the only loadable format, so its member is checked directly.
    auto arena = LoadText(Fixture);
    arena.version = 2;
    CHECK(ArenaContentDigest(arena) != FixtureDigest);
}
