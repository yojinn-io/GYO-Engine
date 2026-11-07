#include <doctest/doctest.h>

// v8-product-pm-detect-diag: the movement diagnostics only record. D2 compares
// every step with and without them bit for bit; the writer tests count the
// lines against the movement trace (Match: Resolved, Client: Generated).

#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/MovementDiag.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <nlohmann/json.hpp>

#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace fps::pvp;

struct Draw final {
    std::uint64_t state;
    std::uint32_t Next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33);
    }
    int Int(int low, int high) { return low + static_cast<int>(Next() % static_cast<std::uint32_t>(high - low + 1)); }
    float Coordinate(int low, int high) {
        const float lattice = static_cast<float>(Int(low * 4, high * 4)) * 0.25F;
        return Int(0, 3) == 0 ? lattice + static_cast<float>(Int(-99, 99)) * 0.001F : lattice;
    }
};

std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }

bool SameState(const PlayerState& a, const PlayerState& b) {
    return a.playerId == b.playerId && Bits(a.position.x) == Bits(b.position.x) &&
           Bits(a.position.y) == Bits(b.position.y) && Bits(a.position.z) == Bits(b.position.z) &&
           Bits(a.yaw) == Bits(b.yaw) && Bits(a.pitch) == Bits(b.pitch) &&
           a.lastResolvedCommand == b.lastResolvedCommand && a.movementEpoch == b.movementEpoch &&
           a.contiguousPendingCommands == b.contiguousPendingCommands &&
           Bits(a.verticalVelocity) == Bits(b.verticalVelocity) && a.grounded == b.grounded &&
           a.lifeGeneration == b.lifeGeneration && a.lifeState == b.lifeState &&
           a.lifeStateTick == b.lifeStateTick && a.respawnTick == b.respawnTick &&
           a.movementSlackSequence == b.movementSlackSequence && a.movementSlackMicros == b.movementSlackMicros &&
           a.connectionQualityFailures == b.connectionQualityFailures && a.crouched == b.crouched &&
           a.sprinting == b.sprinting;
}

Arena RandomArena(Draw& draw) {
    Arena arena;
    arena.id = "pm_detect_diag_cross";
    arena.width = arena.depth = 16;
    const int count = draw.Int(0, 60);
    for (int i = 0; i < count; ++i) {
        const Engine::Math::Vec3 minimum{draw.Coordinate(0, 15), draw.Coordinate(0, 3), draw.Coordinate(0, 15)};
        Engine::Math::Vec3 size{draw.Coordinate(0, 3), draw.Coordinate(0, 2), draw.Coordinate(0, 3)};
        if (size.x <= 0) size.x = 0.25F;
        if (size.y <= 0) size.y = 0.25F;
        if (size.z <= 0) size.z = 0.25F;
        arena.walls.push_back({minimum, minimum + size});
    }
    arena.BuildWallGrid();
    return arena;
}

Arena Biome() {
    std::string error;
    const auto arena = Arena::Load(std::filesystem::path(PVP_PRODUCT_ARENA_PATH).parent_path() / "biome.json", error);
    REQUIRE_MESSAGE(arena, error);
    return *arena;
}

std::filesystem::path TempFile(const char* name) {
    return std::filesystem::temp_directory_path() /
           (std::string(name) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            ".jsonl");
}

std::vector<nlohmann::json> ReadLines(const std::filesystem::path& path) {
    std::vector<nlohmann::json> lines;
    std::ifstream file(path);
    for (std::string text; std::getline(file, text);)
        if (!text.empty()) lines.push_back(nlohmann::json::parse(text));
    return lines;
}

std::size_t Count(const std::vector<MovementTraceEvent>& events, MovementTraceKind kind) {
    std::size_t count{};
    for (const auto& event : events) count += event.kind == kind ? 1 : 0;
    return count;
}
} // namespace

TEST_CASE("PvP movement diagnostics never change a step (pm-detect-diag D2)") {
    constexpr int Sets = 200;
    constexpr int Steps = 200;
    Draw draw{0xd1a6d1a6ULL};
    const auto biome = Biome();
    int generated = 0, compared = 0, mismatches = 0, contacts = 0, airborne = 0;
    for (int set = 0; set < Sets; ++set) {
        // Every tenth set is biome; the others are random boxes.
        const auto arena = set % 10 == 0 ? biome : RandomArena(draw);
        PlayerState plain;
        plain.playerId = 1;
        plain.position = set % 10 == 0 ? arena.spawns[static_cast<std::size_t>(set / 10) % arena.spawns.size()].position
                                       : Engine::Math::Vec3{draw.Coordinate(1, 14), 0, draw.Coordinate(1, 14)};
        auto diagnosed = plain;
        float forward = 1, right = 0, yaw = 0;
        for (int step = 1; step <= Steps; ++step) {
            ++generated;
            if (step % 20 == 1) {
                forward = static_cast<float>(draw.Int(-1, 1));
                right = static_cast<float>(draw.Int(-1, 1));
                yaw = static_cast<float>(draw.Int(-180, 180)) * 0.0174532925F;
            }
            const MovementCommand command{plain.lastResolvedCommand + 1, forward, right, yaw, 0,
                                          draw.Int(0, 14) == 0, draw.Int(0, 9) == 0};
            plain = StepMovement(arena, DefaultStanceRules, plain, command);
            MovementStepDiagnostics diagnostics;
            diagnosed = StepMovement(arena, DefaultStanceRules, diagnosed, command, &diagnostics);
            ++compared;
            contacts += diagnostics.move.penetrationPushes > 0 || diagnostics.move.sweepHits > 0 ? 1 : 0;
            airborne += diagnosed.grounded ? 0 : 1;
            if (!SameState(plain, diagnosed)) {
                ++mismatches;
                CAPTURE(set);
                CAPTURE(step);
                CHECK(SameState(plain, diagnosed));
                diagnosed = plain;
            }
        }
    }
    MESSAGE("D2 generated " << generated << ", compared " << compared << ", mismatches " << mismatches
                            << ", steps with a contact " << contacts << ", airborne " << airborne);
    CHECK(generated == Sets * Steps);
    CHECK(compared == generated);
    CHECK(mismatches == 0);
    CHECK(contacts > 0);
    CHECK(airborne > 0);
}

TEST_CASE("PvP Match movement diagnostics write one line per resolved step") {
    const auto path = TempFile("pvp-match-movement-diag");
    auto trace = std::make_shared<MovementTrace>();
    SetMovementTrace(trace);
    {
        MovementDiagFile diag(path);
        auto arena = Biome();
        PvpMatch match(arena);
        std::string error;
        REQUIRE(match.Join(1, error));
        REQUIRE(match.Join(2, error));
        for (std::uint64_t sequence = 1; sequence <= 60; ++sequence) {
            REQUIRE(match.SubmitInput({1, {{sequence, 1, 0, 0.5F, 0, sequence == 5}}}));
            match.Tick({match.TickCount() + 1, 1.0 / 60});
        }
    }
    SetMovementTrace({});
    const auto resolved = Count(trace->Drain(), MovementTraceKind::Resolved);
    const auto lines = ReadLines(path);
    std::filesystem::remove(path);
    REQUIRE(lines.size() >= 2);
    const auto& end = lines.back();
    CHECK(end.at("kind") == "diag_end");
    CHECK(end.at("dropped") == 0);
    CHECK(end.at("records") == lines.size() - 1);
    CHECK(lines.size() - 1 == resolved);
    CHECK(resolved > 0);
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
        CHECK(lines[i].at("schema") == "gyo.pvp.movement_diag");
        CHECK(lines[i].at("version") == MovementDiagVersion);
        CHECK(lines[i].at("kind") == "step");
        CHECK(lines[i].at("role") == "match");
        CHECK(lines[i].at("authority_tick").get<std::uint64_t>() > 0);
        CHECK(lines[i].at("after").size() == 3);
        CHECK(lines[i].contains("deepest_depth"));
        CHECK(lines[i].contains("sweep_fraction"));
    }
}

TEST_CASE("PvP Client movement diagnostics record first predictions, not replays") {
    const auto path = TempFile("pvp-client-movement-diag");
    auto trace = std::make_shared<MovementTrace>();
    SetMovementTrace(trace);
    std::uint64_t replays{};
    {
        MovementDiagFile diag(path);
        auto arena = Biome();
        PvpMatch match(arena);
        std::string error;
        REQUIRE(match.Join(1, error));
        LocalPlayerPrediction client(arena);
        client.Reconcile(match.Snapshot().players.front(), 0);
        for (int frame = 0; frame < 120; ++frame) {
            if (client.Advance(1.0 / 60, 1, 0, 0.3F, 0, frame == 30)) REQUIRE(match.SubmitInput(client.PendingInput()));
            match.Tick({match.TickCount() + 1, 1.0 / 60});
            if (frame % 4 == 3) {
                // Each Reconcile replays the pending commands: those steps are not recorded.
                replays += client.PendingInput().commands.size();
                const auto snapshot = match.Snapshot();
                client.Reconcile(snapshot.players.front(), snapshot.tick);
            }
        }
    }
    SetMovementTrace({});
    const auto generated = Count(trace->Drain(), MovementTraceKind::Generated);
    const auto lines = ReadLines(path);
    std::filesystem::remove(path);
    REQUIRE(lines.size() >= 2);
    CHECK(lines.back().at("kind") == "diag_end");
    CHECK(lines.back().at("dropped") == 0);
    std::size_t client{};
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) client += lines[i].at("role") == "client" ? 1 : 0;
    MESSAGE("client lines " << client << ", generated " << generated << ", replayed (not recorded) about " << replays);
    CHECK(client == generated);
    CHECK(replays > 0);
}

TEST_CASE("PvP movement diagnostics count what a full queue loses") {
    const auto path = TempFile("pvp-movement-diag-full");
    std::uint64_t recorded{};
    {
        MovementDiagWriter writer(path);
        // More than the queue holds, faster than the writer drains: some are lost and counted.
        for (std::size_t i = 0; i < MovementDiagWriter::Capacity * 4; ++i, ++recorded)
            writer.Record({.role = MovementDiagRole::Match, .timeNs = 1, .sequence = i + 1});
        writer.Finish();
    }
    const auto lines = ReadLines(path);
    std::filesystem::remove(path);
    REQUIRE_FALSE(lines.empty());
    const auto& end = lines.back();
    REQUIRE(end.at("kind") == "diag_end");
    CHECK(end.at("records").get<std::uint64_t>() == lines.size() - 1);
    CHECK(end.at("records").get<std::uint64_t>() + end.at("dropped").get<std::uint64_t>() == recorded);
}
