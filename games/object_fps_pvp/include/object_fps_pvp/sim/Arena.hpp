#pragma once

#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
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

// An arena file (maps/*.json, version 3): the arena's size, the players' body, how fast they move and jump, the
// spawns, and the map it plays on (the catalog id of the tool's chunked map, read as an asset when a World is built;
// world/ArenaLoad.hpp). The floor is the plane y = 0 inside [0, width] x [0, depth]: a game rule, not a collider.
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
    // The map's index in the content's catalog (an asset id, e.g. "object_fps_pvp.map.pvp_arena").
    std::string map;
    std::vector<SpawnPoint> spawns;
};

inline constexpr std::size_t kMinimumSpawns = 2;
inline constexpr std::size_t kMaximumSpawns = 64;

// Reads an arena file. nullopt with error when a member is missing or of the wrong kind, the version is not 3, a
// size or speed is not finite and positive, the eye is above the top of the body, the body is shorter than its
// two caps (2 x radius), the map is empty, there are not 2 to 64 spawns, or a spawn is below
// the floor or outside [radius, width - radius] x [radius, depth - radius]. Whether a spawn is free of the walls
// and of the other spawns needs the geometry: the World checks it when it builds the arena.
[[nodiscard]] std::optional<ArenaContent> ParseArena(std::string_view text, std::string& error);

// The arena list (maps/arenas.json, version 1): the arena files, plain names next to it, in order; the first is
// the one a Client shows before it joins.
[[nodiscard]] std::optional<std::vector<std::string>> ParseArenaList(std::string_view text, std::string& error);

} // namespace object_fps_pvp::sim
