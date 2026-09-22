#include "render/PrimitiveMesh.hpp"

#include <cmath>
#include <array>
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

[[nodiscard]] Float3 Add(Float3 a, Float3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] Float3 Subtract(Float3 a, Float3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] Float3 Scale(Float3 a, float scale) noexcept {
    return {a.x * scale, a.y * scale, a.z * scale};
}

[[nodiscard]] Float3 Cross(Float3 a, Float3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

[[nodiscard]] float Length(Float3 value) noexcept {
    return std::hypot(value.x, value.y, value.z);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

[[nodiscard]] Float3 Perpendicular(Float3 axis) noexcept {
    const Float3 reference = std::abs(axis.y) < 0.9F
        ? Float3{0.0F, 1.0F, 0.0F} : Float3{1.0F, 0.0F, 0.0F};
    const Float3 cross = Cross(axis, reference);
    return Scale(cross, 1.0F / Length(cross));
}

void AppendWireSegment(MeshData& mesh, Float3 start, Float3 end, float thickness) {
    const Float3 delta = Subtract(end, start);
    const float length = Length(delta);
    if (length <= 0.000001F) return;
    const Float3 axis = Scale(delta, 1.0F / length);
    const Float3 u = Scale(Perpendicular(axis), thickness * 0.5F);
    const Float3 v = Scale(Cross(axis, Perpendicular(axis)), thickness * 0.5F);
    const std::array<Float3, 4> offsets{
        Add(u, v), Subtract(v, u), Scale(Add(u, v), -1.0F), Subtract(u, v)};
    for (std::size_t side = 0; side < offsets.size(); ++side) {
        const auto next = (side + 1) % offsets.size();
        AppendFace(mesh, Add(start, offsets[side]), Add(start, offsets[next]),
                   Add(end, offsets[next]), Add(end, offsets[side]));
    }
    AppendFace(mesh, Add(start, offsets[3]), Add(start, offsets[2]),
               Add(start, offsets[1]), Add(start, offsets[0]));
    AppendFace(mesh, Add(end, offsets[0]), Add(end, offsets[1]),
               Add(end, offsets[2]), Add(end, offsets[3]));
}

void AppendWireArc(MeshData& mesh, Float3 center, Float3 u, Float3 v,
                   float radius, float startAngle, float endAngle,
                   std::uint32_t segments, float thickness) {
    const auto point = [&](float angle) {
        return Add(center, Scale(Add(Scale(u, std::cos(angle)),
                                     Scale(v, std::sin(angle))), radius));
    };
    Float3 previous = point(startAngle);
    for (std::uint32_t index = 1; index <= segments; ++index) {
        const float angle = startAngle + (endAngle - startAngle) *
            static_cast<float>(index) / static_cast<float>(segments);
        const Float3 current = point(angle);
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

Base::Result<MeshData, RenderError> MakeWireBox(
    Float3 minimum, Float3 maximum, float lineThickness) {
    using Result = Base::Result<MeshData, RenderError>;
    if (!IsFinite(minimum) || !IsFinite(maximum) ||
        !IsFinite(Subtract(maximum, minimum)) ||
        minimum.x >= maximum.x || minimum.y >= maximum.y || minimum.z >= maximum.z ||
        !std::isfinite(lineThickness) || lineThickness <= 0.0F) {
        return Result::Err(RenderError::Make(RenderErrorCode::InvalidArgument,
            "MakeWireBox: finite ordered bounds and positive line thickness required"));
    }
    std::array<Float3, 8> corners{};
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
    return Result::Ok(std::move(mesh));
}

Base::Result<MeshData, RenderError> MakeWireCapsule(
    Float3 segmentStart, Float3 segmentEnd, float radius,
    float lineThickness, std::uint32_t segments) {
    using Result = Base::Result<MeshData, RenderError>;
    const Float3 delta = Subtract(segmentEnd, segmentStart);
    const float length = Length(delta);
    if (!IsFinite(segmentStart) || !IsFinite(segmentEnd) || !std::isfinite(length) ||
        !std::isfinite(radius) || radius <= 0.0F ||
        !std::isfinite(lineThickness) || lineThickness <= 0.0F ||
        segments < 8 || segments > 128) {
        return Result::Err(RenderError::Make(RenderErrorCode::InvalidArgument,
            "MakeWireCapsule: finite endpoints, positive radius/thickness and 8..128 segments required"));
    }
    const Float3 axis = length > 0.000001F
        ? Scale(delta, 1.0F / length) : Float3{0.0F, 1.0F, 0.0F};
    const Float3 u = Perpendicular(axis);
    const Float3 v = Cross(axis, u);
    constexpr float pi = std::numbers::pi_v<float>;
    MeshData mesh;
    AppendWireArc(mesh, segmentStart, u, v, radius, 0.0F, 2.0F * pi,
                  segments, lineThickness);
    if (length <= 0.000001F) {
        AppendWireArc(mesh, segmentStart, u, axis, radius, 0.0F, 2.0F * pi,
                      segments, lineThickness);
        AppendWireArc(mesh, segmentStart, v, axis, radius, 0.0F, 2.0F * pi,
                      segments, lineThickness);
    } else {
        AppendWireArc(mesh, segmentEnd, u, v, radius, 0.0F, 2.0F * pi,
                      segments, lineThickness);
        for (const Float3 radial : {u, v}) {
            AppendWireArc(mesh, segmentStart, radial, axis, radius, pi, 2.0F * pi,
                          segments / 2, lineThickness);
            AppendWireArc(mesh, segmentEnd, radial, axis, radius, 0.0F, pi,
                          segments / 2, lineThickness);
            for (const float side : {-1.0F, 1.0F}) {
                const Float3 offset = Scale(radial, radius * side);
                AppendWireSegment(mesh, Add(segmentStart, offset),
                                  Add(segmentEnd, offset), lineThickness);
            }
        }
    }
    return Result::Ok(std::move(mesh));
}

} // namespace Engine::Render
