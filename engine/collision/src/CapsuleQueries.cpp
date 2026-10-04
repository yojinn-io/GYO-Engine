#include "engine/collision/Collision.hpp"

#include "engine/base/Assert.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Engine::Collision {
namespace {

// Calculations use doubles to retain accuracy for short sweeps and long rays.
using Math::Vec3d;
constexpr double kTolerance = 1.0e-7;
void Validate(const VerticalCapsule &c) {
    // Finite, positive and at least two radii high.
    GYO_ASSERT(!(!Math::IsFinite(c.feet) || !std::isfinite(c.height) || !std::isfinite(c.radius) ||
                 c.radius <= 0.0f || c.height < 2.0 * c.radius));
}
void Validate(const Math::Capsule &c) {
    // Finite endpoints and a positive radius.
    GYO_ASSERT(!(!Math::IsFinite(c.segmentStart) || !Math::IsFinite(c.segmentEnd) ||
                 !std::isfinite(c.radius) || c.radius <= 0.0f));
}
void Validate(const Math::Aabb &b) {
    // Finite with ordered bounds.
    GYO_ASSERT(!(!Math::IsFinite(b.minimum) || !Math::IsFinite(b.maximum) || b.minimum.x > b.maximum.x ||
                 b.minimum.y > b.maximum.y || b.minimum.z > b.maximum.z));
}
void ValidateSweep(Math::Vec3 start, Math::Vec3 end, float radius) {
    GYO_ASSERT(Math::IsFinite(start) && Math::IsFinite(end) && std::isfinite(radius) && !(radius < 0.0f));
}

// Solve |offset + velocity*t|^2 == radius^2. Used for both cylinder
// cross-sections and spheres; the caller checks the appropriate surface range.
template <class Accept> void Roots(Vec3d offset, Vec3d velocity, double radius, Accept accept) {
    const double a = Dot(velocity, velocity);
    if (a <= 0.0)
        return;
    const double b = Dot(offset, velocity);
    const double c = Dot(offset, offset) - radius * radius;
    const double discriminant = b * b - a * c;
    const double roundoff =
        16.0 * std::numeric_limits<double>::epsilon() * (std::abs(b * b) + std::abs(a * c));
    if (discriminant < -roundoff)
        return;
    const double root = std::sqrt(Math::Max(0.0, discriminant));
    // Stable quadratic roots avoid cancellation close to a surface.
    const double q = -b - std::copysign(root, b);
    if (q == 0.0) {
        accept(0.0);
        return;
    }
    double first = q / a;
    double second = c / q;
    if (first > second)
        std::swap(first, second);
    accept(first);
    accept(second);
}

std::optional<double> RayCapsule(Vec3d origin, Vec3d direction, double maximumDistance, Vec3d start,
                                 Vec3d end, double radius) {
    const Vec3d separation = origin - Math::ClosestPoint(origin, Math::Segmentd{start, end});
    if (Dot(separation, separation) <= radius * radius)
        return 0.0;
    double nearest = std::numeric_limits<double>::infinity();
    auto accept = [&](double distance) {
        if (distance >= 0.0 && distance <= maximumDistance)
            nearest = Math::Min(nearest, distance);
    };
    const Vec3d edge = end - start;
    const double edgeLength = Length(edge);
    if (edgeLength > 0.0) {
        const Vec3d axis = edge * (1.0 / edgeLength);
        const Vec3d offset = origin - start;
        const double along = Dot(offset, axis);
        const double directionAlong = Dot(direction, axis);
        Roots(offset - axis * along, direction - axis * directionAlong, radius,
              [&](double distance) {
                  const double height = along + directionAlong * distance;
                  if (height >= 0.0 && height <= edgeLength)
                      accept(distance);
              });
    }
    Roots(origin - start, direction, radius, accept);
    Roots(origin - end, direction, radius, accept);
    return std::isfinite(nearest) ? std::optional<double>{nearest} : std::nullopt;
}

// The Minkowski difference of an upright segment and AABB is another AABB.
// Its rounded boundary is queried exactly by solving the squared point-to-box
// distance in each interval between the ray's six slab crossings.
std::optional<double> RayRoundedBox(Vec3d origin, Vec3d displacement, Vec3d lo, Vec3d hi,
                                    double radius) {
    const Vec3d separation = origin - Math::ClosestPoint(origin, Math::Aabbd{lo, hi});
    if (Dot(separation, separation) <= radius * radius)
        return 0.0;
    const std::array<double, 3> p{origin.x, origin.y, origin.z};
    const std::array<double, 3> d{displacement.x, displacement.y, displacement.z};
    const std::array<double, 3> minima{lo.x, lo.y, lo.z};
    const std::array<double, 3> maxima{hi.x, hi.y, hi.z};
    std::array<double, 8> breaks{0.0, 1.0};
    std::size_t count = 2;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (d[axis] == 0.0)
            continue;
        for (const double plane : {minima[axis], maxima[axis]}) {
            const double t = (plane - p[axis]) / d[axis];
            if (t > 0.0 && t < 1.0)
                breaks[count++] = t;
        }
    }
    std::sort(breaks.begin(), breaks.begin() + static_cast<std::ptrdiff_t>(count));
    for (std::size_t interval = 1; interval < count; ++interval) {
        const double first = breaks[interval - 1], last = breaks[interval];
        const double middle = (first + last) * 0.5;
        std::array<double, 3> offset{}, velocity{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const double value = p[axis] + d[axis] * middle;
            if (value < minima[axis] || value > maxima[axis]) {
                offset[axis] = p[axis] - (value < minima[axis] ? minima[axis] : maxima[axis]);
                velocity[axis] = d[axis];
            }
        }
        double nearest = std::numeric_limits<double>::infinity();
        Roots({offset[0], offset[1], offset[2]}, {velocity[0], velocity[1], velocity[2]}, radius,
              [&](double t) {
                  if (t >= first - 1.0e-12 && t <= last + 1.0e-12)
                      nearest = Math::Min(nearest, Math::Clamp(t, first, last));
              });
        if (std::isfinite(nearest))
            return nearest;
    }
    return std::nullopt;
}

struct Upright {
    Vec3d bottom;
    double length;
    double radius;
};
Upright Shape(const VerticalCapsule &c) {
    return {{c.feet.x, static_cast<double>(c.feet.y) + c.radius, c.feet.z},
            static_cast<double>(c.height) - 2.0 * c.radius,
            c.radius};
}
Vec3d BoxContactPoint(Upright c, const Math::Aabb &b, Vec3d normal) {
    const double low = Math::Max(c.bottom.y, static_cast<double>(b.minimum.y));
    const double high = Math::Min(c.bottom.y + c.length, static_cast<double>(b.maximum.y));
    Vec3d point = Math::ClosestPoint(Vec3d{c.bottom.x, (low + high) * 0.5, c.bottom.z}, Math::ToAabbd(b));
    if (normal.x != 0.0)
        point.x = normal.x > 0.0 ? b.maximum.x : b.minimum.x;
    if (normal.y != 0.0)
        point.y = normal.y > 0.0 ? b.maximum.y : b.minimum.y;
    if (normal.z != 0.0)
        point.z = normal.z > 0.0 ? b.maximum.z : b.minimum.z;
    return point;
}
Contact BoxContact(Upright c, const Math::Aabb &b, double fraction) {
    Vec3d lo = Math::ToVec3d(b.minimum), hi = Math::ToVec3d(b.maximum);
    lo.y -= c.length;
    const Vec3d separation = c.bottom - Math::ClosestPoint(c.bottom, Math::Aabbd{lo, hi});
    const double distance = Length(separation);
    Vec3d normal{};
    double depth{};
    if (distance > 0.0) {
        normal = separation * (1.0 / distance);
        depth = Math::Max(0.0, c.radius - distance);
    } else {
        const std::array<double, 6> distances{c.bottom.x - lo.x, hi.x - c.bottom.x,
                                              c.bottom.y - lo.y, hi.y - c.bottom.y,
                                              c.bottom.z - lo.z, hi.z - c.bottom.z};
        constexpr std::array<Vec3d, 6> normals{
            {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}}};
        const auto nearest = std::min_element(distances.begin(), distances.end());
        normal = normals[static_cast<std::size_t>(nearest - distances.begin())];
        depth = *nearest + c.radius;
    }
    return {static_cast<float>(fraction), Math::ToVec3(BoxContactPoint(c, b, normal)), Math::ToVec3(normal),
            static_cast<float>(depth)};
}
Contact CapsuleContact(Upright c, Upright target, double fraction) {
    Vec3d a = c.bottom, b = target.bottom;
    if (a.y > b.y + target.length)
        b.y += target.length;
    else if (a.y + c.length < b.y)
        a.y += c.length;
    else
        a.y = b.y = (Math::Max(a.y, b.y) + Math::Min(a.y + c.length, b.y + target.length)) * 0.5;
    const Vec3d separation = a - b;
    const double distance = Length(separation);
    const Vec3d normal = distance > 0.0 ? separation * (1.0 / distance) : Vec3d{1, 0, 0};
    return {static_cast<float>(fraction), Math::ToVec3(b + normal * target.radius), Math::ToVec3(normal),
            static_cast<float>(Math::Max(0.0, c.radius + target.radius - distance))};
}
bool SeparatingContact(const Contact &c, Vec3d displacement) {
    return c.penetrationDepth <= kTolerance && Dot(Math::ToVec3d(c.normal), displacement) >= 0.0;
}
} // namespace

Math::Capsule ToCapsule(const VerticalCapsule &capsule) {
    Validate(capsule);
    const auto c = Shape(capsule);
    return {Math::ToVec3(c.bottom), Math::ToVec3(c.bottom + Vec3d{0, c.length, 0}), capsule.radius};
}

std::optional<float> RaycastCapsule(const Math::Ray &ray, float maximumDistance,
                                    const Math::Capsule &capsule, float sweepRadius) {
    const Math::Vec3 origin = ray.origin;
    const Math::Vec3 direction = ray.direction;
    Validate(capsule);
    ValidateSweep(origin, direction, sweepRadius);
    const double length = Length(Math::ToVec3d(direction));
    // A non-zero direction and a finite, non-negative distance.
    GYO_ASSERT(!(!std::isfinite(maximumDistance) || maximumDistance < 0.0f || length <= 1.0e-6));
    const auto hit = RayCapsule(Math::ToVec3d(origin), Math::ToVec3d(direction) * (1.0 / length), maximumDistance,
                                Math::ToVec3d(capsule.segmentStart), Math::ToVec3d(capsule.segmentEnd),
                                static_cast<double>(capsule.radius) + sweepRadius);
    return hit ? std::optional<float>{static_cast<float>(*hit)} : std::nullopt;
}

std::optional<float> SweepSphereAgainstCapsule(const Math::Segment &path, float sweepRadius,
                                               const Math::Capsule &capsule) {
    const Math::Vec3 start = path.start;
    const Math::Vec3 end = path.end;
    Validate(capsule);
    ValidateSweep(start, end, sweepRadius);
    const Vec3d delta = Math::ToVec3d(end) - Math::ToVec3d(start);
    // Parametric t is directly the sweep fraction, including a stationary query.
    const auto hit =
        RayCapsule(Math::ToVec3d(start), delta, 1.0, Math::ToVec3d(capsule.segmentStart), Math::ToVec3d(capsule.segmentEnd),
                   static_cast<double>(capsule.radius) + sweepRadius);
    return hit ? std::optional<float>{static_cast<float>(*hit)} : std::nullopt;
}

std::optional<Contact> OverlapVerticalCapsuleAabb(const VerticalCapsule &capsule,
                                                  const Math::Aabb &bounds) {
    Validate(capsule);
    Validate(bounds);
    const auto c = Shape(capsule);
    Vec3d lo = Math::ToVec3d(bounds.minimum);
    lo.y -= c.length;
    if (Length(c.bottom - Math::ClosestPoint(c.bottom, Math::Aabbd{lo, Math::ToVec3d(bounds.maximum)})) > c.radius)
        return std::nullopt;
    return BoxContact(c, bounds, 0.0);
}

std::optional<Contact> OverlapVerticalCapsules(const VerticalCapsule &capsule,
                                               const VerticalCapsule &target) {
    Validate(capsule);
    Validate(target);
    const auto c = Shape(capsule), t = Shape(target);
    if (Length(c.bottom - Math::ClosestPoint(c.bottom, Math::Segmentd{t.bottom - Vec3d{0, c.length, 0},
                                                                t.bottom + Vec3d{0, t.length, 0}})) >
        c.radius + t.radius)
        return std::nullopt;
    return CapsuleContact(c, t, 0.0);
}

std::optional<Contact> SweepVerticalCapsuleAgainstAabb(const VerticalCapsule &capsule,
                                                       Math::Vec3 displacement, const Math::Aabb &bounds) {
    Validate(capsule);
    Validate(bounds);
    GYO_ASSERT(Math::IsFinite(displacement));
    auto c = Shape(capsule);
    const Vec3d delta = Math::ToVec3d(displacement);
    if (Dot(delta, delta) == 0.0)
        return OverlapVerticalCapsuleAabb(capsule, bounds);
    if (const auto overlap = OverlapVerticalCapsuleAabb(capsule, bounds)) {
        return SeparatingContact(*overlap, delta) ? std::nullopt : overlap;
    }
    Vec3d lo = Math::ToVec3d(bounds.minimum);
    lo.y -= c.length;
    const auto hit = RayRoundedBox(c.bottom, delta, lo, Math::ToVec3d(bounds.maximum), c.radius);
    if (!hit)
        return std::nullopt;
    c.bottom = c.bottom + delta * *hit;
    auto result = BoxContact(c, bounds, *hit);
    result.penetrationDepth = 0.0f;
    return result;
}

std::optional<Contact> SweepVerticalCapsuleAgainstCapsule(const VerticalCapsule &capsule,
                                                          Math::Vec3 displacement,
                                                          const VerticalCapsule &target) {
    Validate(capsule);
    Validate(target);
    GYO_ASSERT(Math::IsFinite(displacement));
    auto c = Shape(capsule);
    const auto t = Shape(target);
    const Vec3d delta = Math::ToVec3d(displacement);
    if (Dot(delta, delta) == 0.0)
        return OverlapVerticalCapsules(capsule, target);
    if (const auto overlap = OverlapVerticalCapsules(capsule, target)) {
        return SeparatingContact(*overlap, delta) ? std::nullopt : overlap;
    }
    const auto hit = RayCapsule(c.bottom, delta, 1.0, t.bottom - Vec3d{0, c.length, 0},
                                t.bottom + Vec3d{0, t.length, 0}, c.radius + t.radius);
    if (!hit)
        return std::nullopt;
    c.bottom = c.bottom + delta * *hit;
    auto result = CapsuleContact(c, t, *hit);
    result.penetrationDepth = 0.0f;
    return result;
}

} // namespace Engine::Collision
