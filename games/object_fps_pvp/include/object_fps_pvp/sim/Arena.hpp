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

// An arena file (maps/*.json, version 5): one map and, optionally, one scene played together, with what the ground
// decides -- the arena's size, the spawns, the map it plays on (the catalog id of the tool's chunked map, read as an
// asset when a World is built; world/ArenaLoad.hpp), the scene that furnishes it (doors and the like, another file),
// and its bounds. The rules a match plays with are the mode's (world/Modes.hpp), the body the character's
// (sim/BodySize.hpp): a map adapts to the rules, it does not decide them. The bounds are what the ground decides, not
// colliders: no body goes outside [radius, width - radius] x [radius, depth - radius] (an invisible wall, with the
// character's radius), a floor plane may stand at a height (an arena whose map has no ground of its own), and a body
// whose feet fall below the kill height dies.
struct ArenaContent final {
    std::string id;
    float width = 0.0F;
    float depth = 0.0F;
    // The map's index in the content's catalog (an asset id, e.g. "object_fps_pvp.map.pvp_arena").
    std::string map;
    // The scene file, relative to the content root (scene; absent: the arena is not furnished).
    std::optional<std::string> scene;
    std::vector<SpawnPoint> spawns;
    // The floor plane's height (floor; absent: the map's own ground holds the bodies).
    std::optional<float> floor;
    // Below this height a body dies (kill_height).
    float killHeight = 0.0F;
};

// The longest id an arena or a mode has (the bytes MatchInfo carries for each).
inline constexpr std::size_t kMaxContentIdBytes = 64;

inline constexpr std::size_t kMinimumSpawns = 2;
inline constexpr std::size_t kMaximumSpawns = 64;

// Reads an arena file. nullopt with error when a member is missing or of the wrong kind, the version is not 5, the id
// is not 1 to 64 characters, a size is not finite and positive, the map is empty, the scene is not a path inside the
// content, the kill height is not finite or not below the floor, there are not 2 to 64 spawns, or a spawn is below the
// floor, not above the kill height or outside [0, width] x [0, depth]. Whether a spawn leaves room for the character's
// radius to the bounds is LoadGameContent's to say (it has the character); whether it is free of the walls and of the
// other spawns needs the geometry: the World checks it when it builds the arena.
[[nodiscard]] std::optional<ArenaContent> ParseArena(std::string_view text, std::string& error);

// Whether a body whose feet are at this height has fallen out of the arena (below its kill height).
[[nodiscard]] constexpr bool BelowKillHeight(const ArenaContent& arena, const float feetY) noexcept { return feetY < arena.killHeight; }

// Whether a body of this radius standing at a spawn is inside the bounds: [radius, width - radius] x [radius,
// depth - radius].
[[nodiscard]] bool SpawnInBounds(const ArenaContent& arena, const SpawnPoint& spawn, float radius) noexcept;

// The arena list (maps/arenas.json, version 1): the arena files, plain names next to it, in order; the first is
// the one a Client shows before it joins.
[[nodiscard]] std::optional<std::vector<std::string>> ParseArenaList(std::string_view text, std::string& error);

} // namespace object_fps_pvp::sim
