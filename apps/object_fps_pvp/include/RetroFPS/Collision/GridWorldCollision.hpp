#pragma once

#include "engine/collision/Collision.hpp"

#include <vector>

namespace fps {

class GridMap;
struct WorldSettings;

// Campaign grid composition is separate from the pure character geometry API.
[[nodiscard]] std::vector<Engine::Collision::Aabb> BuildWorldCollisionBoxes(
    const GridMap& map, const WorldSettings& settings);

} // namespace fps
