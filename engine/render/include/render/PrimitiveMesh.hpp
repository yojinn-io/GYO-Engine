#pragma once

#include <cstdint>
#include <vector>

#include "engine/base/Result.hpp"
#include "render/RenderError.hpp"
#include "render/RenderTypes.hpp"

namespace Engine::Render {

struct MeshData final {
    std::vector<Vertex3D> vertices{};
    std::vector<std::uint32_t> indices{};

    [[nodiscard]] MeshView View() const noexcept {
        return {vertices, indices};
    }
};

[[nodiscard]] MeshData MakeUnitQuadXY();
[[nodiscard]] MeshData MakeUnitQuadXZ();
[[nodiscard]] MeshData MakeUnitCube();
[[nodiscard]] Base::Result<MeshData, RenderError> MakeUvSphere(
    std::uint32_t verticalSegments,
    std::uint32_t horizontalSegments);

} // namespace Engine::Render
