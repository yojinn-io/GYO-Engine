#include "engine/render/PrimitiveMesh.hpp"
#include "engine/math/scalar/Constants.hpp"

#include <cmath>
#include <array>
#include <limits>
#include <utility>

namespace Engine::Render {
namespace {

void AppendFace(
    MeshData& mesh,
    Math::Vec3 bottomLeft,
    Math::Vec3 bottomRight,
    Math::Vec3 topRight,
    Math::Vec3 topLeft) {
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

[[nodiscard]] Math::Vec3 Perpendicular(Math::Vec3 axis) noexcept {
    const Math::Vec3 reference = std::abs(axis.y) < 0.9F
        ? Math::Vec3{0.0F, 1.0F, 0.0F} : Math::Vec3{1.0F, 0.0F, 0.0F};
    return Math::Normalize(Math::Cross(axis, reference));
}

// Math::Length does not rescale, so an edge or chord longer than about 1.8e19
// overflows and yields non-finite vertices; such meshes are rejected.
[[nodiscard]] bool HasFiniteVertices(const MeshData& mesh) noexcept {
    for (const auto& vertex : mesh.vertices) {
        if (!IsFinite(vertex.position)) return false;
    }
    return true;
}

void AppendWireSegment(MeshData& mesh, Math::Vec3 start, Math::Vec3 end, float thickness) {
    const Math::Vec3 delta = end - start;
    const float length = Math::Length(delta);
    if (length <= 0.000001F) return;
    const Math::Vec3 axis = delta / length;
    const Math::Vec3 u = Perpendicular(axis) * (thickness * 0.5F);
    const Math::Vec3 v = Math::Cross(axis, Perpendicular(axis)) * (thickness * 0.5F);
    const std::array<Math::Vec3, 4> offsets{u + v, v - u, (u + v) * -1.0F, u - v};
    for (std::size_t side = 0; side < offsets.size(); ++side) {
        const auto next = (side + 1) % offsets.size();
        AppendFace(mesh, start + offsets[side], start + offsets[next],
                   end + offsets[next], end + offsets[side]);
    }
    AppendFace(mesh, start + offsets[3], start + offsets[2],
               start + offsets[1], start + offsets[0]);
    AppendFace(mesh, end + offsets[0], end + offsets[1],
               end + offsets[2], end + offsets[3]);
}

void AppendWireArc(MeshData& mesh, Math::Vec3 center, Math::Vec3 u, Math::Vec3 v,
                   float radius, float startAngle, float endAngle,
                   std::uint32_t segments, float thickness) {
    const auto point = [&](float angle) {
        return center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
    };
    Math::Vec3 previous = point(startAngle);
    for (std::uint32_t index = 1; index <= segments; ++index) {
        const float angle = startAngle + (endAngle - startAngle) *
            static_cast<float>(index) / static_cast<float>(segments);
        const Math::Vec3 current = point(angle);
        AppendWireSegment(mesh, previous, current, thickness);
        previous = current;
    }
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

    constexpr std::uint32_t maximumSegments = 512;
    if (verticalSegments < 3 || horizontalSegments < 3 ||
        verticalSegments > maximumSegments || horizontalSegments > maximumSegments) {
        return Base::Err(RenderError::Make(
            RenderErrorCode::InvalidArgument,
            "MakeUvSphere: segment counts must be in the range [3, 512]"));
    }

    const std::uint64_t vertexCount =
        static_cast<std::uint64_t>(verticalSegments + 1) *
        static_cast<std::uint64_t>(horizontalSegments + 1);
    if (vertexCount > std::numeric_limits<std::uint32_t>::max()) {
        return Base::Err(RenderError::Make(
            RenderErrorCode::InvalidArgument,
            "MakeUvSphere: vertex count exceeds the 32-bit index range"));
    }

    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(vertexCount));
    mesh.indices.reserve(
        static_cast<std::size_t>(verticalSegments) * horizontalSegments * 6U);

    for (std::uint32_t vertical = 0; vertical <= verticalSegments; ++vertical) {
        const float v = static_cast<float>(vertical) /
                        static_cast<float>(verticalSegments);
        const float latitude = v * Math::Pi;
        const float sinLatitude = std::sin(latitude);
        const float cosLatitude = std::cos(latitude);

        for (std::uint32_t horizontal = 0; horizontal <= horizontalSegments;
             ++horizontal) {
            const float u = static_cast<float>(horizontal) /
                            static_cast<float>(horizontalSegments);
            const float longitude = u * 2.0F * Math::Pi;
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

    return std::move(mesh);
}

Base::Result<MeshData, RenderError> MakeWireBox(
    Math::Vec3 minimum, Math::Vec3 maximum, float lineThickness) {
    if (!IsFinite(minimum) || !IsFinite(maximum) ||
        !IsFinite(maximum - minimum) ||
        minimum.x >= maximum.x || minimum.y >= maximum.y || minimum.z >= maximum.z ||
        !std::isfinite(lineThickness) || lineThickness <= 0.0F) {
        return Base::Err(RenderError::Make(RenderErrorCode::InvalidArgument,
            "MakeWireBox: finite ordered bounds and positive line thickness required"));
    }
    std::array<Math::Vec3, 8> corners{};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        corners[index] = {(index & 1U) ? maximum.x : minimum.x,
                          (index & 2U) ? maximum.y : minimum.y,
                          (index & 4U) ? maximum.z : minimum.z};
    }
    MeshData mesh;
    mesh.vertices.reserve(12U * 24U);
    mesh.indices.reserve(12U * 36U);
    for (std::size_t index = 0; index < corners.size(); ++index) {
        for (const std::size_t bit : {1U, 2U, 4U}) {
            if ((index & bit) == 0) {
                AppendWireSegment(mesh, corners[index], corners[index | bit], lineThickness);
            }
        }
    }
    if (!HasFiniteVertices(mesh)) {
        return Base::Err(RenderError::Make(RenderErrorCode::InvalidArgument,
            "MakeWireBox: bounds too large for finite wire geometry"));
    }
    return std::move(mesh);
}

Base::Result<MeshData, RenderError> MakeWireCapsule(
    Math::Vec3 segmentStart, Math::Vec3 segmentEnd, float radius,
    float lineThickness, std::uint32_t segments) {
    const Math::Vec3 delta = segmentEnd - segmentStart;
    const float length = Math::Length(delta);
    if (!IsFinite(segmentStart) || !IsFinite(segmentEnd) || !std::isfinite(length) ||
        !std::isfinite(radius) || radius <= 0.0F ||
        !std::isfinite(lineThickness) || lineThickness <= 0.0F ||
        segments < 8 || segments > 128) {
        return Base::Err(RenderError::Make(RenderErrorCode::InvalidArgument,
            "MakeWireCapsule: finite endpoints, positive radius/thickness and 8..128 segments required"));
    }
    const Math::Vec3 axis = length > 0.000001F
        ? delta / length : Math::Vec3{0.0F, 1.0F, 0.0F};
    const Math::Vec3 u = Perpendicular(axis);
    const Math::Vec3 v = Math::Cross(axis, u);
    MeshData mesh;
    AppendWireArc(mesh, segmentStart, u, v, radius, 0.0F, Math::TwoPi,
                  segments, lineThickness);
    if (length <= 0.000001F) {
        AppendWireArc(mesh, segmentStart, u, axis, radius, 0.0F, Math::TwoPi,
                      segments, lineThickness);
        AppendWireArc(mesh, segmentStart, v, axis, radius, 0.0F, Math::TwoPi,
                      segments, lineThickness);
    } else {
        AppendWireArc(mesh, segmentEnd, u, v, radius, 0.0F, Math::TwoPi,
                      segments, lineThickness);
        for (const Math::Vec3 radial : {u, v}) {
            AppendWireArc(mesh, segmentStart, radial, axis, radius, Math::Pi, Math::TwoPi,
                          segments / 2, lineThickness);
            AppendWireArc(mesh, segmentEnd, radial, axis, radius, 0.0F, Math::Pi,
                          segments / 2, lineThickness);
            for (const float side : {-1.0F, 1.0F}) {
                const Math::Vec3 offset = radial * (radius * side);
                AppendWireSegment(mesh, segmentStart + offset,
                                  segmentEnd + offset, lineThickness);
            }
        }
    }
    if (!HasFiniteVertices(mesh)) {
        return Base::Err(RenderError::Make(RenderErrorCode::InvalidArgument,
            "MakeWireCapsule: capsule too large for finite wire geometry"));
    }
    return std::move(mesh);
}

} // namespace Engine::Render
