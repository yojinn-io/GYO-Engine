#pragma once

#include "engine/collision/Collision.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Engine::Collision {

// Which box a nearest query returns when several are equally near: the one
// with the lowest index (a linear scan that replaces on "<") or the highest
// (a scan that replaces on "<=").
enum class TieBreak : std::uint8_t {
    LowestIndex,
    HighestIndex,
};

struct RaycastHit final {
    float distance{};
    std::uint32_t index{}; // into the boxes the grid was built from
};

struct ContactHit final {
    Contact contact{};
    std::uint32_t index{}; // into the boxes the grid was built from
};

// A static broadphase over axis-aligned boxes: a uniform grid built once and
// never changed. Its contract is equivalence (docs/architecture/collision.md):
// every query returns, bit for bit, what a linear scan of all boxes in index
// order 0, 1, ..., n - 1 returns with this module's exact queries:
//
//   Overlapping(b)                 indices i whose box intersects b (closed,
//                                  touching counts), increasing
//   RaycastNearest(r, d, tie)      h = RaycastAabb(r, d, box i); keep (h, i)
//                                  when there is none yet, or h < best
//                                  (LowestIndex) / h <= best (HighestIndex)
//   SweepVerticalCapsuleNearest    the same on SweepVerticalCapsuleAgainstAabb's
//                                  fraction
//   SweepVerticalCapsuleAll        every (SweepVerticalCapsuleAgainstAabb, i)
//                                  that has a value, increasing i
//   OverlapVerticalCapsuleAll      every (OverlapVerticalCapsuleAabb, i) that
//                                  has a value, increasing i
//
// The grid only chooses which boxes to test; the cell length changes speed,
// never results. Queries are const and keep no state.
class StaticAabbGrid final {
public:
    // At most 2^22 cells: a longer cell is used (doubled until they fit) when
    // cellLength would make more. Boxes are copied. Every box must be valid
    // (IsValid), cellLength finite and positive, there must be fewer than 2^32
    // boxes, and the registrations (each box counted once per cell it covers)
    // must number fewer than 2^32 (all asserted).
    static constexpr std::size_t MaxCells = std::size_t{1} << 22;

    StaticAabbGrid(std::span<const Math::Aabb> boxes, float cellLength);

    [[nodiscard]] std::size_t Size() const noexcept { return boxes_.size(); }
    [[nodiscard]] const Math::Aabb& Box(std::size_t index) const;
    // The cell length in use (cellLength, or a doubling of it).
    [[nodiscard]] double CellLength() const noexcept { return cell_; }

    // bounds must be valid (IsValid).
    [[nodiscard]] std::vector<std::uint32_t> Overlapping(const Math::Aabb& bounds) const;
    // Inputs as RaycastAabb.
    [[nodiscard]] std::optional<RaycastHit> RaycastNearest(const Math::Ray& ray, float maximumDistance,
                                                           TieBreak tie) const;
    // Inputs as SweepVerticalCapsuleAgainstAabb.
    [[nodiscard]] std::optional<ContactHit> SweepVerticalCapsuleNearest(const VerticalCapsule& capsule,
                                                                        Math::Vec3 displacement, TieBreak tie) const;
    [[nodiscard]] std::vector<ContactHit> SweepVerticalCapsuleAll(const VerticalCapsule& capsule,
                                                                  Math::Vec3 displacement) const;
    // Inputs as OverlapVerticalCapsuleAabb.
    [[nodiscard]] std::vector<ContactHit> OverlapVerticalCapsuleAll(const VerticalCapsule& capsule) const;

private:
    struct CellRange final {
        std::int64_t low[3]{};
        std::int64_t high[3]{};
        bool empty{true};
    };

    [[nodiscard]] std::int64_t CellIndex(double coordinate, std::size_t axis) const;
    [[nodiscard]] CellRange Cells(const double minimum[3], const double maximum[3]) const;
    // Every index registered in a cell of the range, sorted and unique.
    void Collect(const CellRange& range, std::vector<std::uint32_t>& out) const;
    // Candidates for a capsule moving by displacement: a superset of every box
    // the exact capsule queries can touch.
    [[nodiscard]] std::vector<std::uint32_t> CapsuleCandidates(const VerticalCapsule& capsule,
                                                               Math::Vec3 displacement) const;

    std::vector<Math::Aabb> boxes_;
    double origin_[3]{}; // the minimum corner of all boxes
    double extent_[3]{}; // the maximum corner of all boxes
    double cell_{1.0};
    std::int64_t counts_[3]{};
    std::vector<std::uint32_t> offsets_; // per cell, into items_; size cells + 1
    std::vector<std::uint32_t> items_;   // box indices, increasing within a cell
};

} // namespace Engine::Collision
