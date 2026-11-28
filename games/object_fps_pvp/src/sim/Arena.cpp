#include "object_fps_pvp/sim/Arena.hpp"

#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::sim {

namespace Json = GYO::Engine::Json;
namespace Math = GYO::Engine::Math;

namespace {

float Finite(const Json::Value& value, const std::string_view what) {
    const auto number = static_cast<float>(data::Number(value, what));
    if (!std::isfinite(number)) throw std::runtime_error(std::string{what} + " is not finite");
    return number;
}

Math::Vec3 Position(const Json::Value& value) {
    const auto array = value.AsArray();
    if (!array || array.value().Size() != 3) throw std::runtime_error("a position is three numbers");
    float v[3]{};
    std::size_t i = 0;
    for (const auto item : array.value()) {
        v[i] = static_cast<float>(data::Number(item, "a position component"));
        if (!std::isfinite(v[i])) throw std::runtime_error("a position component is not finite");
        ++i;
    }
    return {v[0], v[1], v[2]};
}

// A plain file name next to the file that names it.
std::string PlainName(const Json::Value& value, const std::string_view what) {
    std::string name = data::String(value, what);
    if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\:") != std::string::npos) {
        throw std::runtime_error(std::string{what} + " is not a plain file name: " + name);
    }
    return name;
}

} // namespace

std::optional<ArenaContent> ParseArena(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the arena");
        if (data::Integer(data::Member(json, "version"), "version") != 5) throw std::runtime_error("unsupported arena version");
        ArenaContent arena;
        arena.id = data::String(data::Member(json, "id"), "id");
        if (arena.id.empty() || arena.id.size() > kMaxContentIdBytes) throw std::runtime_error("id is 1 to 64 characters");
        arena.width = data::Positive(data::Member(json, "width"), "width");
        arena.depth = data::Positive(data::Member(json, "depth"), "depth");
        arena.map = data::String(data::Member(json, "map"), "map");
        if (arena.map.empty()) throw std::runtime_error("map is empty");
        if (data::HasMember(json, "scene")) arena.scene = data::ContentPath(data::Member(json, "scene"), "scene");
        if (data::HasMember(json, "floor")) arena.floor = Finite(data::Member(json, "floor"), "floor");
        arena.killHeight = Finite(data::Member(json, "kill_height"), "kill_height");
        if (arena.floor && !(arena.killHeight < *arena.floor)) throw std::runtime_error("the kill height is not below the floor");
        const auto spawns = data::Member(json, "spawns").AsArray();
        if (!spawns) throw std::runtime_error("spawns is not an array");
        for (const auto item : spawns.value()) {
            const auto spawn = data::AsObject(item, "a spawn");
            SpawnPoint point{Position(data::Member(spawn, "position")),
                             static_cast<float>(data::Number(data::Member(spawn, "yaw"), "yaw"))};
            if (!std::isfinite(point.yaw)) throw std::runtime_error("a spawn's yaw is not finite");
            if ((arena.floor && point.position.y < *arena.floor) || !(point.position.y > arena.killHeight) || !SpawnInBounds(arena, point, 0.0F)) {
                throw std::runtime_error("a spawn is below the floor, not above the kill height or outside the arena");
            }
            arena.spawns.push_back(point);
        }
        if (arena.spawns.size() < kMinimumSpawns || arena.spawns.size() > kMaximumSpawns) {
            throw std::runtime_error("an arena has 2 to 64 spawns");
        }
        return arena;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

bool SpawnInBounds(const ArenaContent& arena, const SpawnPoint& spawn, const float radius) noexcept {
    const Math::Vec3& p = spawn.position;
    return p.x >= radius && p.x <= arena.width - radius && p.z >= radius && p.z <= arena.depth - radius;
}

std::optional<std::vector<std::string>> ParseArenaList(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the arena list");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        const auto arenas = data::Member(json, "arenas").AsArray();
        if (!arenas) throw std::runtime_error("arenas is not an array");
        std::vector<std::string> names;
        for (const auto item : arenas.value()) names.push_back(PlainName(item, "an arena file"));
        if (names.empty()) throw std::runtime_error("the list names no arena");
        return names;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

} // namespace object_fps_pvp::sim
