#include "RetroFPS/Collision/GridWorldCollision.hpp"
#include "RetroFPS/World/GridMap.hpp"
#include "RetroFPS/World/WorldSettings.hpp"

#include <cmath>
#include <stdexcept>

namespace fps {

std::vector<Engine::Collision::Aabb> BuildWorldCollisionBoxes(
    const GridMap& map, const WorldSettings& settings) {
    if (!std::isfinite(settings.cellSize) || settings.cellSize <= 0 ||
        !std::isfinite(settings.wallHeight) || settings.wallHeight <= 0)
        throw std::invalid_argument(
            "Character world cell size and wall height must be finite and positive");
    std::vector<Engine::Collision::Aabb> result;
    for (std::size_t row = 0; row < map.GetHeight(); ++row)
        for (std::size_t col = 0; col < map.GetWidth(); ++col)
            if (map.IsSolid(static_cast<std::ptrdiff_t>(row), static_cast<std::ptrdiff_t>(col)))
                result.push_back({{col * settings.cellSize, 0, row * settings.cellSize},
                                  {(col + 1) * settings.cellSize, settings.wallHeight,
                                   (row + 1) * settings.cellSize}});
    // Campaign map composition remains outside the pure character solver.
    const float width = static_cast<float>(map.GetWidth()) * settings.cellSize;
    const float depth = static_cast<float>(map.GetHeight()) * settings.cellSize;
    const float border = settings.cellSize, height = settings.wallHeight;
    result.push_back({{-border, 0, -border}, {0, height, depth + border}});
    result.push_back({{width, 0, -border}, {width + border, height, depth + border}});
    result.push_back({{0, 0, -border}, {width, height, 0}});
    result.push_back({{0, 0, depth}, {width, height, depth + border}});
    return result;
}

} // namespace fps
