#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"

#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace fps::pvp {
namespace {
bool Finite(const Engine::Collision::Float3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
Float3 ReadPosition(const nlohmann::json& value) {
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
        !positive(eyeHeight) || bodyHeight < radius * 2 || eyeHeight > bodyHeight ||
        spawns.size() != 2 || walls.size() > 1024) {
        error = "Arena v1 requires an id, valid dimensions/movement settings and exactly two spawns";
        return false;
    }
    for (const auto& wall : walls) {
        if (!Finite(wall.minimum) || !Finite(wall.maximum) ||
            wall.minimum.x >= wall.maximum.x || wall.minimum.y >= wall.maximum.y ||
            wall.minimum.z >= wall.maximum.z) {
            error = "Arena wall must be a finite non-empty AABB";
            return false;
        }
    }
    std::vector<Engine::Collision::VerticalCapsule> bodies;
    for (const auto& spawn : spawns) {
        const Engine::Collision::VerticalCapsule body{
            {spawn.position.x, spawn.position.y, spawn.position.z}, bodyHeight, radius};
        if (!Finite(body.feet) || spawn.position.y != 0 || !std::isfinite(spawn.yaw) ||
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
        for (const auto& wall : json.at("walls")) {
            const auto minimum = ReadPosition(wall.at("min"));
            const auto maximum = ReadPosition(wall.at("max"));
            arena.walls.push_back({{minimum.x, minimum.y, minimum.z}, {maximum.x, maximum.y, maximum.z}});
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

} // namespace fps::pvp
