#pragma once

// The shapes of the physics_3d Module and the pose they are placed with
// (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, section 2.1 items 2 and 3, section 4.4).
// Shapes are in their local space; a Pose puts them in the world. Only Math is used: the shapes and the pure
// queries (Queries.hpp) need no World.

#include <cstdint>
#include <memory>
#include <utility>
#include <span>
#include <variant>

#include "engine/base/Result.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/physics_3d/Physics3dError.hpp"

namespace GYO::Engine::Physics3d {

// A ball around the local origin (parry's Ball; not Math::Sphere, which is a world-space centre and radius).
struct Ball final {
    float radius = 0.5F;
};

// The points within radius of the local segment [a, b] (parry's Capsule: a segment and a radius). Any orientation;
// coincident ends are a ball. CapsuleY builds the upright one along local Y.
struct Capsule final {
    Math::Vec3 a{0.0F, -0.5F, 0.0F};
    Math::Vec3 b{0.0F, 0.5F, 0.0F};
    float radius = 0.5F;
};

// The capsule from (0, -halfHeight, 0) to (0, halfHeight, 0).
[[nodiscard]] constexpr Capsule CapsuleY(const float halfHeight, const float radius) noexcept {
    return Capsule{{0.0F, -halfHeight, 0.0F}, {0.0F, halfHeight, 0.0F}, radius};
}

// The box from -halfExtents to halfExtents.
struct Cuboid final {
    Math::Vec3 halfExtents{0.5F, 0.5F, 0.5F};
};

namespace Detail {

// The body of a BoxSet (the boxes, the hierarchy, the digest) and the accessor the Module's sources read it with:
// both live in the Module's src/ (BoxSetData.hpp), so the layout of the hierarchy is not part of the interface.
struct BoxSetData;
struct BoxSetAccess;

} // namespace Detail

// Many static boxes in local space (a voxel map's walls), with a static bounding volume hierarchy. Immutable and
// shared: copies share one body (Bevy/parry's SharedShape).
//
// The contract of every query on it (P3-3): the hierarchy never changes a result. Every query returns what the
// same single-box query over boxes 0..n-1 in index order returns (the nearest, ties to the lowest index; or all,
// by increasing index). The hierarchy only decides which boxes are tested.
class BoxSet final {
public:
    // The empty set.
    BoxSet() = default;

    // Checks every box (finite, minimum <= maximum on each axis: InvalidBox with the index) and the count (at most
    // 2^32 - 1: TooManyBoxes), builds the hierarchy and the content digest.
    [[nodiscard]] static Base::Result<BoxSet, Physics3dError> Build(std::span<const Math::Aabb> boxes);

    [[nodiscard]] std::span<const Math::Aabb> Boxes() const noexcept;
    [[nodiscard]] std::uint32_t Size() const noexcept;
    // 64-bit FNV-1a over each box's six floats (minimum x, y, z, maximum x, y, z) bit by bit, little-endian, in
    // index order; computed once by Build. The empty set has the FNV-1a offset basis.
    [[nodiscard]] std::uint64_t Digest() const noexcept;

    // The same body, or the same boxes bit by bit (checked only when the counts and digests agree): the equality
    // Collider's HashValue hashes (CCR-13).
    friend bool operator==(const BoxSet& a, const BoxSet& b) noexcept;

private:
    friend struct Detail::BoxSetAccess;

    explicit BoxSet(std::shared_ptr<const Detail::BoxSetData> data) : data_(std::move(data)) {}

    std::shared_ptr<const Detail::BoxSetData> data_;
};

using Shape = std::variant<Ball, Capsule, Cuboid, BoxSet>;

// Transform without the scale. The rotation need not be exactly unit length: the queries normalize it (in double).
struct Pose final {
    Math::Vec3 translation{};
    Math::Quaternion rotation{};
};

// Content checks (section 4.2). A pure query given content that fails them is a programmer error (GYO_ASSERT);
// data that comes from content is checked with these first.
// radius finite and > 0.
[[nodiscard]] bool IsValid(const Ball& ball) noexcept;
// Finite ends (coincident ends allowed), radius finite and > 0.
[[nodiscard]] bool IsValid(const Capsule& capsule) noexcept;
// Every half extent finite and >= 0.
[[nodiscard]] bool IsValid(const Cuboid& cuboid) noexcept;
// Always: Build checked the boxes.
[[nodiscard]] bool IsValid(const BoxSet& boxes) noexcept;
[[nodiscard]] bool IsValid(const Shape& shape) noexcept;
// Finite translation and rotation, rotation not zero.
[[nodiscard]] bool IsValid(const Pose& pose) noexcept;

} // namespace GYO::Engine::Physics3d
