#pragma once

#include "engine/collision/Collision.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fps::pvp {

struct SpawnPoint final {
    Engine::Math::Vec3 position{};
    float yaw{};
};

// Product-owned versioned content shared by runtime collision and rendering.
struct Arena final {
    std::uint32_t version{1};
    std::string id;
    float width{};
    float depth{};
    float cellSize{1.0F};
    float movementSpeed{3.0F};
    float bodyHeight{1.8F};
    float radius{0.25F};
    float eyeHeight{1.6F};
    std::vector<Engine::Math::Aabb> walls;
    std::vector<SpawnPoint> spawns;
    float jumpHeight{0.6F};
    float gravity{18.0F};

    [[nodiscard]] bool Validate(std::string& error) const;
    [[nodiscard]] static std::optional<Arena> Load(
        const std::filesystem::path& path, std::string& error);
};

// Content digest of a loaded arena (pv6 contract §2): 64-bit FNV-1a over the
// parsed members in declaration order, big-endian, floats as their binary32
// bits. Line endings, whitespace, key order and number spelling do not matter.
// Zero is reserved for "missing": a caller treats it as a load failure.
[[nodiscard]] std::uint64_t ArenaContentDigest(const Arena& arena);

// Arena v1 spawn counts (pv6 contract §2, 2026-10-07 revision).
inline constexpr std::size_t MinimumArenaSpawns = 2;
inline constexpr std::size_t MaximumArenaSpawns = 64;

// The Match hosts only an arena with at least MaxPlayers spawns. Binaries from
// before the revision reject any arena whose spawn count is not two, so mixing
// them with a hostable arena always fails at the arena digest.
[[nodiscard]] bool ArenaHostsRoom(const Arena& arena, std::string& error);

// Client content: the arenas a Client installs (arenas.json, version 1), plain
// file names next to the list, in order. The Match chooses the arena; a
// Client selects the installed one with the Match's id (ClientConnection), and
// shows the first before it joins. Ids are unique and every arena shares the
// first one's body height, which the character presentation is built for.
[[nodiscard]] std::optional<std::vector<Arena>> LoadInstalledArenas(
    const std::filesystem::path& list, std::string& error);

} // namespace fps::pvp
