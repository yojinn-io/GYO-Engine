#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/base/Fnv1a.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <nlohmann/json.hpp>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace fps::pvp {
namespace {
Engine::Math::Vec3 ReadPosition(const nlohmann::json& value) {
    if (!value.is_array() || value.size() != 3)
        throw std::invalid_argument("Arena vectors require three numbers");
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}
}

bool Arena::Validate(std::string& error) const {
    error.clear();
    const auto positive = [](float value) { return std::isfinite(value) && value > 0; };
    if (version != 1 || id.empty() || id.size() > 64 ||
        !positive(width) || !positive(depth) || !positive(cellSize) ||
        !positive(movementSpeed) || !positive(bodyHeight) || !positive(radius) ||
        !positive(eyeHeight) || !positive(jumpHeight) || !positive(gravity) ||
        !std::isfinite(2 * gravity * jumpHeight) ||
        !Engine::Collision::IsValid(Engine::Collision::VerticalCapsule{{}, bodyHeight, radius}) ||
        eyeHeight > bodyHeight ||
        spawns.size() < MinimumArenaSpawns || spawns.size() > MaximumArenaSpawns || walls.size() > 1024) {
        error = "Arena v1 requires an id, valid dimensions/movement settings and 2-64 spawns";
        return false;
    }
    for (const auto& wall : walls) {
        // Engine validity allows zero thickness; an empty wall is a content error.
        if (!Engine::Collision::IsValid(wall) ||
            wall.minimum.x >= wall.maximum.x || wall.minimum.y >= wall.maximum.y ||
            wall.minimum.z >= wall.maximum.z) {
            error = "Arena wall must be a finite non-empty AABB";
            return false;
        }
    }
    std::vector<Engine::Collision::VerticalCapsule> bodies;
    for (const auto& spawn : spawns) {
        const Engine::Collision::VerticalCapsule body{spawn.position, bodyHeight, radius};
        if (!Engine::Math::IsFinite(body.feet) || spawn.position.y != 0 || !std::isfinite(spawn.yaw) ||
            spawn.position.x < radius || spawn.position.x > width - radius ||
            spawn.position.z < radius || spawn.position.z > depth - radius ||
            !CanPlaceCharacterBody(body, walls, bodies)) {
            error = "Arena spawn must be on the floor, inside the arena and free of walls/other spawns";
            return false;
        }
        bodies.push_back(body);
    }
    return true;
}

std::optional<Arena> Arena::Load(const std::filesystem::path& path, std::string& error) {
    error.clear();
    try {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream) throw std::runtime_error("Cannot open arena: " + path.string());
        if (stream.tellg() < 0 || stream.tellg() > 256 * 1024)
            throw std::runtime_error("Arena content exceeds 256 KiB");
        stream.seekg(0);
        const auto json = nlohmann::json::parse(stream);
        if (!json.at("version").is_number_integer() || json.at("version").get<std::int64_t>() != 1)
            throw std::runtime_error("Unsupported arena version");
        if (!json.at("walls").is_array() || !json.at("spawns").is_array())
            throw std::runtime_error("Arena walls and spawns must be arrays");
        Arena arena;
        arena.id = json.at("id").get<std::string>();
        arena.width = json.at("width").get<float>();
        arena.depth = json.at("depth").get<float>();
        arena.cellSize = json.value("cell_size", 1.0F);
        arena.movementSpeed = json.value("movement_speed", 3.0F);
        arena.bodyHeight = json.value("body_height", 1.8F);
        arena.radius = json.value("radius", 0.25F);
        arena.eyeHeight = json.value("eye_height", 1.6F);
        arena.jumpHeight = json.value("jump_height", 0.6F);
        arena.gravity = json.value("gravity", 18.0F);
        for (const auto& wall : json.at("walls")) {
            const auto minimum = ReadPosition(wall.at("min"));
            const auto maximum = ReadPosition(wall.at("max"));
            arena.walls.push_back({minimum, maximum});
        }
        for (const auto& spawn : json.at("spawns"))
            arena.spawns.push_back({ReadPosition(spawn.at("position")), spawn.at("yaw").get<float>()});
        if (!arena.Validate(error)) return std::nullopt;
        return arena;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

std::uint64_t ArenaContentDigest(const Arena& arena) {
    // Thirteen names: adding, removing or reordering a member of Arena breaks
    // this binding, so the canonical form changes with it (a wire change).
    const auto& [version, id, width, depth, cellSize, movementSpeed, bodyHeight, radius, eyeHeight,
        walls, spawns, jumpHeight, gravity] = arena;
    std::string bytes;
    const auto u32 = [&](std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
    };
    const auto f32 = [&](float value) { u32(std::bit_cast<std::uint32_t>(value)); };
    const auto vec3 = [&](const Engine::Math::Vec3& value) { f32(value.x); f32(value.y); f32(value.z); };
    u32(version);
    u32(static_cast<std::uint32_t>(id.size()));
    bytes += id;
    for (const float value : {width, depth, cellSize, movementSpeed, bodyHeight, radius, eyeHeight}) f32(value);
    u32(static_cast<std::uint32_t>(walls.size()));
    for (const auto& wall : walls) { vec3(wall.minimum); vec3(wall.maximum); }
    u32(static_cast<std::uint32_t>(spawns.size()));
    for (const auto& spawn : spawns) { vec3(spawn.position); f32(spawn.yaw); }
    f32(jumpHeight);
    f32(gravity);
    return Engine::Base::Fnv1a64(bytes);
}

bool ArenaHostsRoom(const Arena& arena, std::string& error) {
    error.clear();
    if (arena.spawns.size() < MaxPlayers) {
        error = "Arena " + arena.id + " has " + std::to_string(arena.spawns.size()) +
            " spawns; a room of " + std::to_string(MaxPlayers) + " players needs at least that many";
        return false;
    }
    return true;
}

} // namespace fps::pvp
