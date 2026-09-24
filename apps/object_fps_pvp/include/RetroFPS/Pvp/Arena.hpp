#pragma once

#include "RetroFPS/Math/Vector.hpp"
#include "engine/collision/Collision.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fps::pvp {

struct SpawnPoint final {
    Float3 position{};
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
    std::vector<Engine::Collision::Aabb> walls;
    std::vector<SpawnPoint> spawns;

    [[nodiscard]] bool Validate(std::string& error) const;
    [[nodiscard]] static std::optional<Arena> Load(
        const std::filesystem::path& path, std::string& error);
};

} // namespace fps::pvp
