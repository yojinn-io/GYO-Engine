#pragma once

#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <vector>

namespace fps {

enum class SurfaceType {
    Floor,
    Wall,
    Door,
};

struct SurfaceTransform {
    Engine::Math::Vec3 translation{};
    Engine::Math::Vec3 rotationRadians{};
    Engine::Math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct SurfaceInstance {
    SurfaceType type = SurfaceType::Floor;
    SurfaceTransform transform{};
    Engine::Math::Vec3 normal{0.0f, 1.0f, 0.0f};
    std::size_t row = 0;
    std::size_t column = 0;
};

// 静的グリッドマップ用のCPU側描画データ。
// ビルボードなど他の描画形式は、独立したレンダラーとジオメトリ形式を使用できる。
struct MapGeometry {
    std::vector<SurfaceInstance> surfaces;
};

} // namespace fps
