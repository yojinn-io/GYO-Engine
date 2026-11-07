#include "engine/collision/StaticAabbGrid.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace Engine::Collision {
namespace {

// How far beyond a query's exact extent candidates are taken. The exact
// queries compute in double with a 1e-7 tolerance that only removes contacts,
// and rounding guards far below this; extra candidates cost time, never
// results (they are tested exactly).
double Margin(const double largestMagnitude) { return 1e-3 + 1e-5 * largestMagnitude; }

double LargestMagnitude(const double minimum[3], const double maximum[3]) {
    double largest = 0.0;
    for (std::size_t axis = 0; axis < 3; ++axis)
        largest = std::max({largest, std::abs(minimum[axis]), std::abs(maximum[axis])});
    return largest;
}

void Inflate(double minimum[3], double maximum[3]) {
    const double margin = Margin(LargestMagnitude(minimum, maximum));
    for (std::size_t axis = 0; axis < 3; ++axis) {
        minimum[axis] -= margin;
        maximum[axis] += margin;
    }
}

bool Touches(const Math::Aabb& box, const Math::Aabb& bounds) {
    return box.minimum.x <= bounds.maximum.x && bounds.minimum.x <= box.maximum.x &&
           box.minimum.y <= bounds.maximum.y && bounds.minimum.y <= box.maximum.y &&
           box.minimum.z <= bounds.maximum.z && bounds.minimum.z <= box.maximum.z;
}

// Whether candidate (value, index) replaces best under the tie rule, as the
// linear scan would: the scan keeps the lowest index among equal values with
// "<" and the highest with "<=", so the choice does not depend on the order
// candidates are visited in.
bool Replaces(const float value, const std::uint32_t index, const float bestValue, const std::uint32_t bestIndex,
              const TieBreak tie) {
    if (value != bestValue) return value < bestValue;
    return tie == TieBreak::LowestIndex ? index < bestIndex : index > bestIndex;
}

} // namespace

StaticAabbGrid::StaticAabbGrid(const std::span<const Math::Aabb> boxes, const float cellLength) {
    const auto failure = Initialize(boxes, cellLength);
    GYO_ASSERT(!failure.has_value());
}

Base::Result<StaticAabbGrid, CollisionError> StaticAabbGrid::Build(const std::span<const Math::Aabb> boxes,
                                                                     const float cellLength) {
    StaticAabbGrid grid;
    if (auto failure = grid.Initialize(boxes, cellLength)) return Base::Err(std::move(*failure));
    return grid;
}

std::optional<CollisionError> StaticAabbGrid::Initialize(const std::span<const Math::Aabb> boxes,
                                                         const float cellLength) {
    if (!(std::isfinite(cellLength) && cellLength > 0.0F))
        return CollisionError::Make(CollisionErrorCode::InvalidCellLength, "the cell length must be finite and positive",
                                    std::to_string(cellLength));
    if (boxes.size() >= std::size_t{std::numeric_limits<std::uint32_t>::max()})
        return CollisionError::Make(CollisionErrorCode::TooManyBoxes, "boxes are indexed with 32 bits",
                                    std::to_string(boxes.size()));
    for (std::size_t i = 0; i < boxes.size(); ++i)
        if (!IsValid(boxes[i]))
            return CollisionError::Make(CollisionErrorCode::InvalidBox, "a box is not valid (IsValid)",
                                        std::to_string(i));
    boxes_.assign(boxes.begin(), boxes.end());
    cell_ = cellLength;
    if (boxes_.empty()) {
        offsets_.assign(1, 0);
        return std::nullopt;
    }
    double low[3]{boxes_[0].minimum.x, boxes_[0].minimum.y, boxes_[0].minimum.z};
    double high[3]{boxes_[0].maximum.x, boxes_[0].maximum.y, boxes_[0].maximum.z};
    for (const auto& box : boxes_) {
        low[0] = std::min<double>(low[0], box.minimum.x);
        low[1] = std::min<double>(low[1], box.minimum.y);
        low[2] = std::min<double>(low[2], box.minimum.z);
        high[0] = std::max<double>(high[0], box.maximum.x);
        high[1] = std::max<double>(high[1], box.maximum.y);
        high[2] = std::max<double>(high[2], box.maximum.z);
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        origin_[axis] = low[axis];
        extent_[axis] = high[axis];
    }
    // Double the cell until the grid has at most MaxCells cells.
    while (true) {
        double total = 1.0;
        for (std::size_t axis = 0; axis < 3; ++axis) total *= std::floor((high[axis] - low[axis]) / cell_) + 1.0;
        if (total <= static_cast<double>(MaxCells)) break;
        cell_ *= 2.0;
    }
    for (std::size_t axis = 0; axis < 3; ++axis)
        counts_[axis] = static_cast<std::int64_t>(std::floor((high[axis] - low[axis]) / cell_)) + 1;
    const auto cells = static_cast<std::size_t>(counts_[0] * counts_[1] * counts_[2]);

    // Every (box, cell) registration is an entry of items_, indexed by uint32:
    // the total must stay below 2^32. It is summed from the cell ranges in 64
    // bits before anything of its size is allocated, so a huge input is
    // rejected cheaply and the count cannot wrap silently.
    std::vector<CellRange> ranges(boxes_.size());
    std::uint64_t registrations = 0;
    for (std::size_t i = 0; i < boxes_.size(); ++i) {
        const double minimum[3]{boxes_[i].minimum.x, boxes_[i].minimum.y, boxes_[i].minimum.z};
        const double maximum[3]{boxes_[i].maximum.x, boxes_[i].maximum.y, boxes_[i].maximum.z};
        ranges[i] = Cells(minimum, maximum);
        GYO_ASSERT(!ranges[i].empty);
        const auto& r = ranges[i];
        registrations += static_cast<std::uint64_t>(r.high[0] - r.low[0] + 1) *
                         static_cast<std::uint64_t>(r.high[1] - r.low[1] + 1) *
                         static_cast<std::uint64_t>(r.high[2] - r.low[2] + 1);
    }
    if (registrations >= (std::uint64_t{1} << 32U))
        return CollisionError::Make(CollisionErrorCode::TooManyRegistrations,
                                    "the (box, cell) registrations are indexed with 32 bits",
                                    std::to_string(registrations));

    // Two passes: count per cell, then fill (boxes in increasing index, so
    // every cell's list is increasing).
    std::vector<std::uint32_t> counts(cells + 1, 0);
    for (const auto& r : ranges)
        for (auto z = r.low[2]; z <= r.high[2]; ++z)
            for (auto y = r.low[1]; y <= r.high[1]; ++y)
                for (auto x = r.low[0]; x <= r.high[0]; ++x)
                    ++counts[static_cast<std::size_t>((z * counts_[1] + y) * counts_[0] + x)];
    offsets_.assign(cells + 1, 0);
    for (std::size_t c = 0; c < cells; ++c) offsets_[c + 1] = offsets_[c] + counts[c];
    items_.resize(offsets_[cells]);
    std::vector<std::uint32_t> fill(offsets_.begin(), offsets_.end() - 1);
    for (std::size_t i = 0; i < boxes_.size(); ++i) {
        const auto& r = ranges[i];
        for (auto z = r.low[2]; z <= r.high[2]; ++z)
            for (auto y = r.low[1]; y <= r.high[1]; ++y)
                for (auto x = r.low[0]; x <= r.high[0]; ++x)
                    items_[fill[static_cast<std::size_t>((z * counts_[1] + y) * counts_[0] + x)]++] =
                        static_cast<std::uint32_t>(i);
    }
    return std::nullopt;
}

const Math::Aabb& StaticAabbGrid::Box(const std::size_t index) const {
    GYO_ASSERT(index < boxes_.size());
    return boxes_[index];
}

// The cell of a coordinate, unclamped except to [-1, count] so that it fits an
// integer. floor((x - origin) / cell) is monotonic in x (subtraction and
// division by a positive constant round monotonically), which is what makes
// the grid exact: two closed intervals that intersect have cell ranges that
// intersect.
std::int64_t StaticAabbGrid::CellIndex(const double coordinate, const std::size_t axis) const {
    const double cell = std::floor((coordinate - origin_[axis]) / cell_);
    if (cell < -1.0) return -1;
    if (cell > static_cast<double>(counts_[axis])) return counts_[axis];
    return static_cast<std::int64_t>(cell);
}

StaticAabbGrid::CellRange StaticAabbGrid::Cells(const double minimum[3], const double maximum[3]) const {
    CellRange range;
    if (boxes_.empty()) return range;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto low = CellIndex(minimum[axis], axis);
        const auto high = CellIndex(maximum[axis], axis);
        if (high < 0 || low >= counts_[axis]) return range;
        range.low[axis] = std::max<std::int64_t>(low, 0);
        range.high[axis] = std::min<std::int64_t>(high, counts_[axis] - 1);
    }
    range.empty = false;
    return range;
}

void StaticAabbGrid::Collect(const CellRange& range, std::vector<std::uint32_t>& out) const {
    out.clear();
    if (range.empty) return;
    for (auto z = range.low[2]; z <= range.high[2]; ++z)
        for (auto y = range.low[1]; y <= range.high[1]; ++y)
            for (auto x = range.low[0]; x <= range.high[0]; ++x) {
                const auto c = static_cast<std::size_t>((z * counts_[1] + y) * counts_[0] + x);
                out.insert(out.end(), items_.begin() + offsets_[c], items_.begin() + offsets_[c + 1]);
            }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

std::vector<std::uint32_t> StaticAabbGrid::Overlapping(const Math::Aabb& bounds) const {
    GYO_ASSERT(IsValid(bounds));
    const double minimum[3]{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z};
    const double maximum[3]{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};
    std::vector<std::uint32_t> candidates;
    Collect(Cells(minimum, maximum), candidates);
    std::vector<std::uint32_t> result;
    for (const auto index : candidates)
        if (Touches(boxes_[index], bounds)) result.push_back(index);
    return result;
}

std::vector<std::uint32_t> StaticAabbGrid::CapsuleCandidates(const VerticalCapsule& capsule,
                                                             const Math::Vec3 displacement) const {
    GYO_ASSERT(IsValid(capsule) && Math::IsFinite(displacement));
    const double r = capsule.radius;
    const double start[3]{capsule.feet.x, capsule.feet.y, capsule.feet.z};
    const double end[3]{start[0] + displacement.x, start[1] + displacement.y, start[2] + displacement.z};
    double minimum[3]{std::min(start[0], end[0]) - r, std::min(start[1], end[1]), std::min(start[2], end[2]) - r};
    double maximum[3]{std::max(start[0], end[0]) + r, std::max(start[1], end[1]) + capsule.height,
                      std::max(start[2], end[2]) + r};
    Inflate(minimum, maximum);
    std::vector<std::uint32_t> candidates;
    Collect(Cells(minimum, maximum), candidates);
    return candidates;
}

std::optional<RaycastHit> StaticAabbGrid::RaycastNearest(const Math::Ray& ray, const float maximumDistance,
                                                         const TieBreak tie) const {
    GYO_ASSERT(IsValid(ray));
    GYO_ASSERT(std::isfinite(maximumDistance) && !(maximumDistance < 0.0F));
    std::optional<RaycastHit> best;
    if (boxes_.empty()) return best;
    // The ray in double, along the direction normalized as RaycastAabb does.
    const double dx = ray.direction.x, dy = ray.direction.y, dz = ray.direction.z;
    const double inverse = 1.0 / std::sqrt(dx * dx + dy * dy + dz * dz);
    const double origin[3]{ray.origin.x, ray.origin.y, ray.origin.z};
    const double unit[3]{dx * inverse, dy * inverse, dz * inverse};
    // Every hit lies in the hull of all boxes, so only the part of [0,
    // maximumDistance] inside the (inflated) hull is walked.
    double first = 0.0;
    double last = maximumDistance;
    {
        double hullLow[3]{origin_[0], origin_[1], origin_[2]};
        double hullHigh[3]{extent_[0], extent_[1], extent_[2]};
        Inflate(hullLow, hullHigh);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            if (unit[axis] == 0.0) {
                if (origin[axis] < hullLow[axis] || origin[axis] > hullHigh[axis]) return best;
                continue;
            }
            const double a = (hullLow[axis] - origin[axis]) / unit[axis];
            const double b = (hullHigh[axis] - origin[axis]) / unit[axis];
            first = std::max(first, std::min(a, b));
            last = std::min(last, std::max(a, b));
        }
        const double slack = Margin(std::max(std::abs(first), std::abs(last)));
        first = std::max(0.0, first - slack);
        last = std::min(static_cast<double>(maximumDistance), last + slack);
        if (first > last) return best;
    }
    // Walk [first, last] in pieces one cell long; a box whose entry lies in a
    // piece meets that piece's (inflated) bounds. Once a hit is known, pieces
    // that start beyond it (plus the margin) cannot hold a nearer or equal one.
    std::vector<std::uint32_t> candidates;
    std::vector<std::uint32_t> tested;
    std::vector<std::uint32_t> fresh;
    const auto pieces = static_cast<std::int64_t>(std::floor((last - first) / cell_)) + 1;
    for (std::int64_t piece = 0; piece < pieces; ++piece) {
        const double t0 = first + static_cast<double>(piece) * cell_;
        const double t1 = std::min(last, t0 + cell_);
        if (best && t0 > static_cast<double>(best->distance) + Margin(t0 + LargestMagnitude(origin, origin)))
            break;
        double minimum[3]{};
        double maximum[3]{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const double a = origin[axis] + unit[axis] * t0;
            const double b = origin[axis] + unit[axis] * t1;
            minimum[axis] = std::min(a, b);
            maximum[axis] = std::max(a, b);
        }
        Inflate(minimum, maximum);
        Collect(Cells(minimum, maximum), candidates);
        fresh.clear();
        std::set_difference(candidates.begin(), candidates.end(), tested.begin(), tested.end(),
                            std::back_inserter(fresh));
        for (const auto index : fresh) {
            const auto distance = RaycastAabb(ray, maximumDistance, boxes_[index]);
            if (distance && (!best || Replaces(*distance, index, best->distance, best->index, tie)))
                best = RaycastHit{*distance, index};
        }
        const auto middle = tested.size();
        tested.insert(tested.end(), fresh.begin(), fresh.end());
        std::inplace_merge(tested.begin(), tested.begin() + static_cast<std::ptrdiff_t>(middle), tested.end());
    }
    return best;
}

std::optional<ContactHit> StaticAabbGrid::SweepVerticalCapsuleNearest(const VerticalCapsule& capsule,
                                                                      const Math::Vec3 displacement,
                                                                      const TieBreak tie) const {
    std::optional<ContactHit> best;
    for (const auto index : CapsuleCandidates(capsule, displacement)) {
        const auto contact = SweepVerticalCapsuleAgainstAabb(capsule, displacement, boxes_[index]);
        if (contact && (!best || Replaces(contact->fraction, index, best->contact.fraction, best->index, tie)))
            best = ContactHit{*contact, index};
    }
    return best;
}

std::vector<ContactHit> StaticAabbGrid::SweepVerticalCapsuleAll(const VerticalCapsule& capsule,
                                                                const Math::Vec3 displacement) const {
    std::vector<ContactHit> result;
    for (const auto index : CapsuleCandidates(capsule, displacement))
        if (const auto contact = SweepVerticalCapsuleAgainstAabb(capsule, displacement, boxes_[index]))
            result.push_back({*contact, index});
    return result;
}

std::vector<ContactHit> StaticAabbGrid::OverlapVerticalCapsuleAll(const VerticalCapsule& capsule) const {
    std::vector<ContactHit> result;
    for (const auto index : CapsuleCandidates(capsule, {}))
        if (const auto contact = OverlapVerticalCapsuleAabb(capsule, boxes_[index]))
            result.push_back({*contact, index});
    return result;
}

} // namespace Engine::Collision
