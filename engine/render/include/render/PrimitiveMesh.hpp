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

// World-space wire outlines made from closed thin triangle prisms. They use
// the ordinary unlit triangle pipeline, including on backends without lines.
[[nodiscard]] Base::Result<MeshData, RenderError> MakeWireBox(
    Float3 minimum,
    Float3 maximum,
    float lineThickness = 0.012F);
// Endpoints are the centers of the hemispheres, not the outer capsule tips.
// Coincident endpoints produce a sphere outline.
[[nodiscard]] Base::Result<MeshData, RenderError> MakeWireCapsule(
    Float3 segmentStart,
    Float3 segmentEnd,
    float radius,
    float lineThickness = 0.012F,
    std::uint32_t segments = 16);

} // namespace Engine::Render
