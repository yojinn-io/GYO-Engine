#include "object_fps_pvp/sim/Arena.hpp"

#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::sim {

namespace Json = GYO::Engine::Json;
namespace Math = GYO::Engine::Math;

namespace {

float Positive(const Json::ObjectView& object, const std::string_view key, const float fallback) {
    if (!data::HasMember(object, key)) return fallback;
    const auto value = static_cast<float>(data::Number(data::Member(object, key), key));
    if (!std::isfinite(value) || value <= 0) throw std::runtime_error(std::string{key} + " is not a finite positive number");
    return value;
}

float Finite(const Json::Value& value, const std::string_view what) {
    const auto number = static_cast<float>(data::Number(value, what));
    if (!std::isfinite(number)) throw std::runtime_error(std::string{what} + " is not finite");
    return number;
}

// The milli-points per metre are a std::uint32_t.
constexpr double kMaximumMilliPointsPerMetre = 4'294'967'295.0;

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
        if (data::Integer(data::Member(json, "version"), "version") != 4) throw std::runtime_error("unsupported arena version");
        ArenaContent arena;
        arena.id = data::String(data::Member(json, "id"), "id");
        if (arena.id.empty() || arena.id.size() > 64) throw std::runtime_error("id is 1 to 64 characters");
        arena.width = Positive(json, "width", 0.0F);
        arena.depth = Positive(json, "depth", 0.0F);
        if (arena.width <= 0 || arena.depth <= 0) throw std::runtime_error("width and depth are required");
        arena.movementSpeed = Positive(json, "movement_speed", arena.movementSpeed);
        arena.bodyHeight = Positive(json, "body_height", arena.bodyHeight);
        arena.radius = Positive(json, "radius", arena.radius);
        arena.eyeHeight = Positive(json, "eye_height", arena.eyeHeight);
        arena.jumpHeight = Positive(json, "jump_height", arena.jumpHeight);
        arena.gravity = Positive(json, "gravity", arena.gravity);
        if (arena.eyeHeight > arena.bodyHeight) throw std::runtime_error("the eye is above the top of the body");
        if (arena.bodyHeight < 2 * arena.radius) throw std::runtime_error("the body is shorter than its two caps");
        if (!std::isfinite(2 * arena.gravity * arena.jumpHeight)) throw std::runtime_error("the jump speed is not finite");
        arena.map = PlainName(data::Member(json, "map"), "map");
        if (data::HasMember(json, "floor")) arena.floor = Finite(data::Member(json, "floor"), "floor");
        arena.killHeight = Finite(data::Member(json, "kill_height"), "kill_height");
        if (arena.floor && !(arena.killHeight < *arena.floor)) throw std::runtime_error("the kill height is not below the floor");
        if (data::HasMember(json, "fall_damage")) {
            const auto fall = data::AsObject(data::Member(json, "fall_damage"), "fall_damage");
            FallDamage damage;
            damage.safeHeight = Positive(fall, "safe_height", 0.0F);
            if (!(damage.safeHeight > 0)) throw std::runtime_error("fall_damage names its safe_height");
            const double points = data::Number(data::Member(fall, "points_per_metre"), "points_per_metre");
            const double milli = points * 1000.0;
            if (!std::isfinite(milli) || milli < 0 || milli > kMaximumMilliPointsPerMetre || milli != std::floor(milli)) {
                throw std::runtime_error("points_per_metre is not a whole number of milli-points from 0 to 2^32 - 1");
            }
            damage.milliPointsPerMetre = static_cast<std::uint32_t>(milli);
            arena.fallDamage = damage;
        }
        const auto spawns = data::Member(json, "spawns").AsArray();
        if (!spawns) throw std::runtime_error("spawns is not an array");
        for (const auto item : spawns.value()) {
            const auto spawn = data::AsObject(item, "a spawn");
            SpawnPoint point{Position(data::Member(spawn, "position")),
                             static_cast<float>(data::Number(data::Member(spawn, "yaw"), "yaw"))};
            if (!std::isfinite(point.yaw)) throw std::runtime_error("a spawn's yaw is not finite");
            if ((arena.floor && point.position.y < *arena.floor) || !(point.position.y > arena.killHeight) ||
                point.position.x < arena.radius || point.position.x > arena.width - arena.radius ||
                point.position.z < arena.radius || point.position.z > arena.depth - arena.radius) {
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
