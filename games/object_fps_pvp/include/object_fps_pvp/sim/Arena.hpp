#pragma once

#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace object_fps_pvp::sim {

// Where a player (re)appears: its feet and the yaw it faces.
struct SpawnPoint final {
    GYO::Engine::Math::Vec3 position{};
    float yaw = 0.0F;
};

// How a landing hurts: a fall from higher than the safe height takes the points per metre beyond it.
struct FallDamage final {
    float safeHeight = 0.0F;
    // Milli-points (as HP) per metre fallen beyond the safe height; the file gives points.
    std::uint32_t milliPointsPerMetre = 0;
};

// An arena file (maps/*.json, version 4): the arena's size, the players' body, how fast they move and jump, the
// spawns, the map it plays on (a directory next to it with the tool's chunked map, map.json and its regions;
// data/ArenaMap.hpp), and its bounds. The bounds are game rules, not colliders: no body goes outside
// [radius, width - radius] x [radius, depth - radius] (an invisible wall), a floor plane may stand at a height (an
// arena whose map has no ground of its own), and a body whose feet fall below the kill height dies.
struct ArenaContent final {
    std::string id;
    float width = 0.0F;
    float depth = 0.0F;
    float movementSpeed = 3.0F;
    float bodyHeight = 1.8F;
    float radius = 0.25F;
    float eyeHeight = 1.6F;
    float jumpHeight = 0.6F;
    float gravity = 18.0F;
    // The map's directory, a plain name next to the arena file.
    std::string map;
    std::vector<SpawnPoint> spawns;
    // The floor plane's height (floor; absent: the map's own ground holds the bodies).
    std::optional<float> floor;
    // Below this height a body dies (kill_height).
    float killHeight = 0.0F;
    // fall_damage (absent: a landing never hurts).
    std::optional<FallDamage> fallDamage;
};

inline constexpr std::size_t kMinimumSpawns = 2;
inline constexpr std::size_t kMaximumSpawns = 64;

// Reads an arena file. nullopt with error when a member is missing or of the wrong kind, the version is not 4, a
// size or speed is not finite and positive, the eye is above the top of the body, the body is shorter than its
// two caps (2 x radius), the map is not a plain name, there are not 2 to 64 spawns, a spawn is below the floor,
// not above the kill height or outside [radius, width - radius] x [radius, depth - radius], the kill height is not
// finite or not below the floor, or the fall damage's safe height is not finite and positive or its points per metre
// not a whole number of milli-points from 0 to the HP of a body. Whether a spawn is free of the walls and of the other
// spawns needs the geometry: the World checks it when it builds the arena.
[[nodiscard]] std::optional<ArenaContent> ParseArena(std::string_view text, std::string& error);

// The arena list (maps/arenas.json, version 1): the arena files, plain names next to it, in order; the first is
// the one a Client shows before it joins.
[[nodiscard]] std::optional<std::vector<std::string>> ParseArenaList(std::string_view text, std::string& error);

} // namespace object_fps_pvp::sim
