#include "engine/physics_3d/Shapes.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "BoxSetData.hpp"

namespace GYO::Engine::Physics3d {

namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
// Boxes per leaf of the hierarchy.
constexpr std::uint32_t kLeafSize = 4;

// FNV-1a over a float's bits, little-endian (Base::Fnv1a64's byte loop, fed without building a string).
void DigestFloat(std::uint64_t& hash, const float value) {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint64_t>((bits >> shift) & 0xFFU);
        hash *= kFnvPrime;
    }
}

bool IsValidBox(const Math::Aabb& box) {
    return Math::IsFinite(box) && box.minimum.x <= box.maximum.x && box.minimum.y <= box.maximum.y &&
           box.minimum.z <= box.maximum.z;
}

double CentreTwice(const Math::Aabb& box, const int axis) {
    switch (axis) {
    case 0: return static_cast<double>(box.minimum.x) + static_cast<double>(box.maximum.x);
    case 1: return static_cast<double>(box.minimum.y) + static_cast<double>(box.maximum.y);
    default: return static_cast<double>(box.minimum.z) + static_cast<double>(box.maximum.z);
    }
}

// Top-down median split on the longest axis of the centres. The sort uses a total order (centre, then index), so
// the tree is the same on every standard library; the queries' results do not depend on the tree anyway (P3-3).
void BuildNode(Detail::BoxSetData& data, const std::uint32_t first, const std::uint32_t last) {
    const auto index = static_cast<std::uint32_t>(data.nodes.size());
    data.nodes.push_back({});
    Math::Aabb bounds = data.boxes[data.order[first]];
    double lowest[3]{CentreTwice(bounds, 0), CentreTwice(bounds, 1), CentreTwice(bounds, 2)};
    double highest[3]{lowest[0], lowest[1], lowest[2]};
    for (std::uint32_t i = first; i < last; ++i) {
        const Math::Aabb& box = data.boxes[data.order[i]];
        bounds = Math::Merge(bounds, box);
        for (int axis = 0; axis < 3; ++axis) {
            const double centre = CentreTwice(box, axis);
            lowest[axis] = Math::Min(lowest[axis], centre);
            highest[axis] = Math::Max(highest[axis], centre);
        }
    }
    data.nodes[index].bounds = bounds;
    const std::uint32_t count = last - first;
    if (count <= kLeafSize) {
        data.nodes[index].first = first;
        data.nodes[index].count = count;
        return;
    }
    int axis = 0;
    for (int candidate = 1; candidate < 3; ++candidate) {
        if (highest[candidate] - lowest[candidate] > highest[axis] - lowest[axis]) axis = candidate;
    }
    const std::vector<Math::Aabb>& boxes = data.boxes;
    std::sort(data.order.begin() + first, data.order.begin() + last,
              [&boxes, axis](const std::uint32_t a, const std::uint32_t b) {
                  const double ca = CentreTwice(boxes[a], axis);
                  const double cb = CentreTwice(boxes[b], axis);
                  return ca < cb || (ca == cb && a < b);
              });
    const std::uint32_t middle = first + count / 2;
    BuildNode(data, first, middle);
    data.nodes[index].right = static_cast<std::uint32_t>(data.nodes.size());
    BuildNode(data, middle, last);
}

bool FiniteNonNegative(const float value) { return std::isfinite(value) && value >= 0.0F; }
bool FinitePositive(const float value) { return std::isfinite(value) && value > 0.0F; }

} // namespace

Base::Result<BoxSet, Physics3dError> BoxSet::Build(const std::span<const Math::Aabb> boxes) {
    if (boxes.size() > std::numeric_limits<std::uint32_t>::max()) {
        return Base::Err(Physics3dError::Make(Physics3dErrorCode::TooManyBoxes,
                                              "a BoxSet holds at most 2^32 - 1 boxes",
                                              std::to_string(boxes.size()) + " boxes"));
    }
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        if (!IsValidBox(boxes[i])) {
            return Base::Err(Physics3dError::Make(Physics3dErrorCode::InvalidBox,
                                                  "a box is not finite or has minimum > maximum",
                                                  "box " + std::to_string(i)));
        }
    }
    auto data = std::make_shared<Detail::BoxSetData>();
    data->boxes.assign(boxes.begin(), boxes.end());
    std::uint64_t digest = kFnvOffsetBasis;
    for (const Math::Aabb& box : data->boxes) {
        DigestFloat(digest, box.minimum.x);
        DigestFloat(digest, box.minimum.y);
        DigestFloat(digest, box.minimum.z);
        DigestFloat(digest, box.maximum.x);
        DigestFloat(digest, box.maximum.y);
        DigestFloat(digest, box.maximum.z);
    }
    data->digest = digest;
    const auto count = static_cast<std::uint32_t>(data->boxes.size());
    data->order.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) data->order[i] = i;
    if (count > 0) BuildNode(*data, 0, count);
    return BoxSet(std::shared_ptr<const Detail::BoxSetData>(std::move(data)));
}

std::span<const Math::Aabb> BoxSet::Boxes() const noexcept {
    if (!data_) return {};
    return data_->boxes;
}

std::uint32_t BoxSet::Size() const noexcept {
    return data_ ? static_cast<std::uint32_t>(data_->boxes.size()) : 0U;
}

std::uint64_t BoxSet::Digest() const noexcept { return data_ ? data_->digest : kFnvOffsetBasis; }

bool operator==(const BoxSet& a, const BoxSet& b) noexcept {
    if (a.data_ == b.data_) return true;
    if (a.Size() != b.Size() || a.Digest() != b.Digest()) return false;
    const std::span<const Math::Aabb> left = a.Boxes();
    const std::span<const Math::Aabb> right = b.Boxes();
    const auto same = [](const float x, const float y) {
        return std::bit_cast<std::uint32_t>(x) == std::bit_cast<std::uint32_t>(y);
    };
    for (std::size_t i = 0; i < left.size(); ++i) {
        const Math::Aabb& l = left[i];
        const Math::Aabb& r = right[i];
        if (!same(l.minimum.x, r.minimum.x) || !same(l.minimum.y, r.minimum.y) || !same(l.minimum.z, r.minimum.z) ||
            !same(l.maximum.x, r.maximum.x) || !same(l.maximum.y, r.maximum.y) || !same(l.maximum.z, r.maximum.z)) {
            return false;
        }
    }
    return true;
}

bool IsValid(const Ball& ball) noexcept { return FinitePositive(ball.radius); }

bool IsValid(const Capsule& capsule) noexcept {
    return Math::IsFinite(capsule.a) && Math::IsFinite(capsule.b) && FinitePositive(capsule.radius);
}

bool IsValid(const Cuboid& cuboid) noexcept {
    return FiniteNonNegative(cuboid.halfExtents.x) && FiniteNonNegative(cuboid.halfExtents.y) &&
           FiniteNonNegative(cuboid.halfExtents.z);
}

bool IsValid(const BoxSet&) noexcept { return true; }

bool IsValid(const Shape& shape) noexcept {
    return std::visit([](const auto& s) { return IsValid(s); }, shape);
}

bool IsValid(const Pose& pose) noexcept {
    const Math::Quaternion& q = pose.rotation;
    if (!Math::IsFinite(pose.translation)) return false;
    if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w)) return false;
    return q.x != 0.0F || q.y != 0.0F || q.z != 0.0F || q.w != 0.0F;
}

} // namespace GYO::Engine::Physics3d
