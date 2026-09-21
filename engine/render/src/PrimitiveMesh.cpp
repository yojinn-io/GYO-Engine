#include "render/PrimitiveMesh.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace Engine::Render {
namespace {

void AppendFace(
    MeshData& mesh,
    Float3 bottomLeft,
    Float3 bottomRight,
    Float3 topRight,
    Float3 topLeft) {
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({bottomLeft, {0.0F, 1.0F}});
    mesh.vertices.push_back({bottomRight, {1.0F, 1.0F}});
    mesh.vertices.push_back({topRight, {1.0F, 0.0F}});
    mesh.vertices.push_back({topLeft, {0.0F, 0.0F}});
    mesh.indices.insert(mesh.indices.end(), {
        first, first + 1, first + 2,
        first, first + 2, first + 3,
    });
}

} // namespace

MeshData MakeUnitQuadXY() {
    MeshData mesh;
    AppendFace(
        mesh,
        {-0.5F, -0.5F, 0.0F},
        {0.5F, -0.5F, 0.0F},
        {0.5F, 0.5F, 0.0F},
        {-0.5F, 0.5F, 0.0F});
    return mesh;
}

MeshData MakeUnitQuadXZ() {
    MeshData mesh;
    AppendFace(
        mesh,
        {-0.5F, 0.0F, -0.5F},
        {-0.5F, 0.0F, 0.5F},
        {0.5F, 0.0F, 0.5F},
        {0.5F, 0.0F, -0.5F});
    return mesh;
}

MeshData MakeUnitCube() {
    MeshData mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    AppendFace(mesh, {-0.5F, -0.5F, 0.5F}, {0.5F, -0.5F, 0.5F},
               {0.5F, 0.5F, 0.5F}, {-0.5F, 0.5F, 0.5F});
    AppendFace(mesh, {0.5F, -0.5F, -0.5F}, {-0.5F, -0.5F, -0.5F},
               {-0.5F, 0.5F, -0.5F}, {0.5F, 0.5F, -0.5F});
    AppendFace(mesh, {0.5F, -0.5F, 0.5F}, {0.5F, -0.5F, -0.5F},
               {0.5F, 0.5F, -0.5F}, {0.5F, 0.5F, 0.5F});
    AppendFace(mesh, {-0.5F, -0.5F, -0.5F}, {-0.5F, -0.5F, 0.5F},
               {-0.5F, 0.5F, 0.5F}, {-0.5F, 0.5F, -0.5F});
    AppendFace(mesh, {-0.5F, 0.5F, 0.5F}, {0.5F, 0.5F, 0.5F},
               {0.5F, 0.5F, -0.5F}, {-0.5F, 0.5F, -0.5F});
    AppendFace(mesh, {-0.5F, -0.5F, -0.5F}, {0.5F, -0.5F, -0.5F},
               {0.5F, -0.5F, 0.5F}, {-0.5F, -0.5F, 0.5F});
    return mesh;
}

Base::Result<MeshData, RenderError> MakeUvSphere(
    std::uint32_t verticalSegments,
    std::uint32_t horizontalSegments) {
    using Result = Base::Result<MeshData, RenderError>;

    constexpr std::uint32_t maximumSegments = 512;
    if (verticalSegments < 3 || horizontalSegments < 3 ||
        verticalSegments > maximumSegments || horizontalSegments > maximumSegments) {
        return Result::Err(RenderError::Make(
            RenderErrorCode::InvalidArgument,
            "MakeUvSphere: segment counts must be in the range [3, 512]"));
    }

    const std::uint64_t vertexCount =
        static_cast<std::uint64_t>(verticalSegments + 1) *
        static_cast<std::uint64_t>(horizontalSegments + 1);
    if (vertexCount > std::numeric_limits<std::uint32_t>::max()) {
        return Result::Err(RenderError::Make(
            RenderErrorCode::InvalidArgument,
            "MakeUvSphere: vertex count exceeds the 32-bit index range"));
    }

    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(vertexCount));
    mesh.indices.reserve(
        static_cast<std::size_t>(verticalSegments) * horizontalSegments * 6U);

    constexpr float pi = std::numbers::pi_v<float>;
    for (std::uint32_t vertical = 0; vertical <= verticalSegments; ++vertical) {
        const float v = static_cast<float>(vertical) /
                        static_cast<float>(verticalSegments);
        const float latitude = v * pi;
        const float sinLatitude = std::sin(latitude);
        const float cosLatitude = std::cos(latitude);

        for (std::uint32_t horizontal = 0; horizontal <= horizontalSegments;
             ++horizontal) {
            const float u = static_cast<float>(horizontal) /
                            static_cast<float>(horizontalSegments);
            const float longitude = u * 2.0F * pi;
            mesh.vertices.push_back({
                {
                    sinLatitude * std::sin(longitude),
                    cosLatitude,
                    sinLatitude * std::cos(longitude),
                },
                {u, v},
            });
        }
    }

    const std::uint32_t rowLength = horizontalSegments + 1;
    for (std::uint32_t vertical = 0; vertical < verticalSegments; ++vertical) {
        for (std::uint32_t horizontal = 0; horizontal < horizontalSegments;
             ++horizontal) {
            const std::uint32_t topLeft = vertical * rowLength + horizontal;
            const std::uint32_t topRight = topLeft + 1;
            const std::uint32_t bottomLeft = topLeft + rowLength;
            const std::uint32_t bottomRight = bottomLeft + 1;
            mesh.indices.insert(mesh.indices.end(), {
                topLeft, bottomLeft, topRight,
                topRight, bottomLeft, bottomRight,
            });
        }
    }

    return Result::Ok(std::move(mesh));
}

} // namespace Engine::Render
