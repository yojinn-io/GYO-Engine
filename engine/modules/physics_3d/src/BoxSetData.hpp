#pragma once

// The body of a BoxSet, internal to the physics_3d Module (Shapes.hpp only declares it): the boxes, the static
// bounding volume hierarchy and the content digest, and the accessor Shapes.cpp and Queries.cpp read it through.

#include <cstdint>
#include <vector>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/physics_3d/Shapes.hpp"

namespace GYO::Engine::Physics3d::Detail {

// A node of the hierarchy, in depth-first order: the left child of a branch is the next node, the right child is
// at `right`. A leaf names the range [first, first + count) of BoxSetData::order. The bounds are the union of the
// children's (float min and max are exact).
struct BoxNode final {
    Math::Aabb bounds{};
    std::uint32_t first = 0;
    std::uint32_t count = 0;  // 0: a branch
    std::uint32_t right = 0;
};

struct BoxSetData final {
    std::vector<Math::Aabb> boxes;
    std::vector<BoxNode> nodes;
    std::vector<std::uint32_t> order;
    std::uint64_t digest = 0;
    BoxSetFlags flags = BoxSetFlags::None;
    // FixInternalEdges only (empty otherwise): the boxes flush against each face. Face f of box i (f = axis * 2 +
    // side, side 0 for the minimum and 1 for the maximum) has the neighbours faceNeighbours[faceStart[i * 6 + f] ..
    // faceStart[i * 6 + f + 1]), by ascending index. Per box and independent of the hierarchy, so the single-box
    // query of the contract (P3-3) and the accelerated one read the same data.
    std::vector<std::uint32_t> faceStart;
    std::vector<std::uint32_t> faceNeighbours;
};

struct BoxSetAccess final {
    // nullptr for the empty set.
    [[nodiscard]] static const BoxSetData* Data(const BoxSet& set) noexcept { return set.data_.get(); }
};

} // namespace GYO::Engine::Physics3d::Detail
