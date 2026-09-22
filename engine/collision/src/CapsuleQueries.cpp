#include "engine/collision/Collision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Engine::Collision {
namespace {

// Calculations use doubles to retain accuracy for short sweeps and long rays.
struct Vec3 {
    double x{}, y{}, z{};
};
constexpr double kTolerance = 1.0e-7;
Vec3 V(Float3 v) { return {v.x, v.y, v.z}; }
Float3 F(Vec3 v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, double b) { return {a.x * b, a.y * b, a.z * b}; }
double Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
bool Finite(Float3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec3 Closest(Vec3 p, Vec3 a, Vec3 b) {
    const Vec3 edge = b - a;
    const double squared = Dot(edge, edge);
    return a + edge * (squared > 0.0 ? std::clamp(Dot(p - a, edge) / squared, 0.0, 1.0) : 0.0);
}
Vec3 Clamp(Vec3 p, Vec3 lo, Vec3 hi) {
    return {std::clamp(p.x, lo.x, hi.x), std::clamp(p.y, lo.y, hi.y), std::clamp(p.z, lo.z, hi.z)};
}
void Validate(const VerticalCapsule &c) {
    if (!Finite(c.feet) || !std::isfinite(c.height) || !std::isfinite(c.radius) ||
        c.radius <= 0.0f || c.height < 2.0 * c.radius) {
        throw std::invalid_argument(
            "collision capsule must be finite, positive, and at least two radii high");
    }
}
void Validate(const Capsule &c) {
    if (!Finite(c.segmentStart) || !Finite(c.segmentEnd) || !std::isfinite(c.radius) ||
        c.radius <= 0.0f) {
        throw std::invalid_argument(
            "collision capsule endpoints must be finite and radius positive");
    }
}
void Validate(const Aabb &b) {
    if (!Finite(b.minimum) || !Finite(b.maximum) || b.minimum.x > b.maximum.x ||
        b.minimum.y > b.maximum.y || b.minimum.z > b.maximum.z) {
        throw std::invalid_argument("collision AABB must be finite with ordered bounds");
    }
}
void ValidateSweep(Float3 start, Float3 end, float radius) {
    if (!Finite(start) || !Finite(end) || !std::isfinite(radius) || radius < 0.0f) {
        throw std::invalid_argument(
            "collision sweep endpoints/radius must be finite with non-negative radius");
    }
}

// Solve |offset + velocity*t|^2 == radius^2. Used for both cylinder
// cross-sections and spheres; the caller checks the appropriate surface range.
template <class Accept> void Roots(Vec3 offset, Vec3 velocity, double radius, Accept accept) {
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
    const double root = std::sqrt((std::max)(0.0, discriminant));
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

std::optional<double> RayCapsule(Vec3 origin, Vec3 direction, double maximumDistance, Vec3 start,
                                 Vec3 end, double radius) {
    const Vec3 separation = origin - Closest(origin, start, end);
    if (Dot(separation, separation) <= radius * radius)
        return 0.0;
    double nearest = std::numeric_limits<double>::infinity();
    auto accept = [&](double distance) {
        if (distance >= 0.0 && distance <= maximumDistance)
            nearest = (std::min)(nearest, distance);
    };
    const Vec3 edge = end - start;
    const double edgeLength = Length(edge);
    if (edgeLength > 0.0) {
        const Vec3 axis = edge * (1.0 / edgeLength);
        const Vec3 offset = origin - start;
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
std::optional<double> RayRoundedBox(Vec3 origin, Vec3 displacement, Vec3 lo, Vec3 hi,
                                    double radius) {
    const Vec3 separation = origin - Clamp(origin, lo, hi);
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
                      nearest = (std::min)(nearest, std::clamp(t, first, last));
              });
        if (std::isfinite(nearest))
            return nearest;
    }
    return std::nullopt;
}

struct Upright {
    Vec3 bottom;
    double length;
    double radius;
};
Upright Shape(const VerticalCapsule &c) {
    return {{c.feet.x, static_cast<double>(c.feet.y) + c.radius, c.feet.z},
            static_cast<double>(c.height) - 2.0 * c.radius,
            c.radius};
}
Vec3 BoxContactPoint(Upright c, const Aabb &b, Vec3 normal) {
    const double low = (std::max)(c.bottom.y, static_cast<double>(b.minimum.y));
    const double high = (std::min)(c.bottom.y + c.length, static_cast<double>(b.maximum.y));
    Vec3 point = Clamp({c.bottom.x, (low + high) * 0.5, c.bottom.z}, V(b.minimum), V(b.maximum));
    if (normal.x != 0.0)
        point.x = normal.x > 0.0 ? b.maximum.x : b.minimum.x;
    if (normal.y != 0.0)
        point.y = normal.y > 0.0 ? b.maximum.y : b.minimum.y;
    if (normal.z != 0.0)
        point.z = normal.z > 0.0 ? b.maximum.z : b.minimum.z;
    return point;
}
Contact BoxContact(Upright c, const Aabb &b, double fraction) {
    Vec3 lo = V(b.minimum), hi = V(b.maximum);
    lo.y -= c.length;
    const Vec3 separation = c.bottom - Clamp(c.bottom, lo, hi);
    const double distance = Length(separation);
    Vec3 normal{};
    double depth{};
    if (distance > 0.0) {
        normal = separation * (1.0 / distance);
        depth = (std::max)(0.0, c.radius - distance);
    } else {
        const std::array<double, 6> distances{c.bottom.x - lo.x, hi.x - c.bottom.x,
                                              c.bottom.y - lo.y, hi.y - c.bottom.y,
                                              c.bottom.z - lo.z, hi.z - c.bottom.z};
        constexpr std::array<Vec3, 6> normals{
            {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}}};
        const auto nearest = std::min_element(distances.begin(), distances.end());
        normal = normals[static_cast<std::size_t>(nearest - distances.begin())];
        depth = *nearest + c.radius;
    }
    return {static_cast<float>(fraction), F(BoxContactPoint(c, b, normal)), F(normal),
            static_cast<float>(depth)};
}
Contact CapsuleContact(Upright c, Upright target, double fraction) {
    Vec3 a = c.bottom, b = target.bottom;
    if (a.y > b.y + target.length)
        b.y += target.length;
    else if (a.y + c.length < b.y)
        a.y += c.length;
    else
        a.y = b.y = ((std::max)(a.y, b.y) + (std::min)(a.y + c.length, b.y + target.length)) * 0.5;
    const Vec3 separation = a - b;
    const double distance = Length(separation);
    const Vec3 normal = distance > 0.0 ? separation * (1.0 / distance) : Vec3{1, 0, 0};
    return {static_cast<float>(fraction), F(b + normal * target.radius), F(normal),
            static_cast<float>((std::max)(0.0, c.radius + target.radius - distance))};
}
bool SeparatingContact(const Contact &c, Vec3 displacement) {
    return c.penetrationDepth <= kTolerance && Dot(V(c.normal), displacement) >= 0.0;
}
} // namespace

Capsule ToCapsule(const VerticalCapsule &capsule) {
    Validate(capsule);
    const auto c = Shape(capsule);
    return {F(c.bottom), F(c.bottom + Vec3{0, c.length, 0}), capsule.radius};
}

std::optional<float> RaycastCapsule(Float3 origin, Float3 direction, float maximumDistance,
                                    const Capsule &capsule, float sweepRadius) {
    Validate(capsule);
    ValidateSweep(origin, direction, sweepRadius);
    const double length = Length(V(direction));
    if (!std::isfinite(maximumDistance) || maximumDistance < 0.0f || length <= 1.0e-6) {
        throw std::invalid_argument(
            "collision ray needs a non-zero direction and finite non-negative distance");
    }
    const auto hit = RayCapsule(V(origin), V(direction) * (1.0 / length), maximumDistance,
                                V(capsule.segmentStart), V(capsule.segmentEnd),
                                static_cast<double>(capsule.radius) + sweepRadius);
    return hit ? std::optional<float>{static_cast<float>(*hit)} : std::nullopt;
}

std::optional<float> SweepSphereAgainstCapsule(Float3 start, Float3 end, float sweepRadius,
                                               const Capsule &capsule) {
    Validate(capsule);
    ValidateSweep(start, end, sweepRadius);
    const Vec3 delta = V(end) - V(start);
    // Parametric t is directly the sweep fraction, including a stationary query.
    const auto hit =
        RayCapsule(V(start), delta, 1.0, V(capsule.segmentStart), V(capsule.segmentEnd),
                   static_cast<double>(capsule.radius) + sweepRadius);
    return hit ? std::optional<float>{static_cast<float>(*hit)} : std::nullopt;
}

std::optional<Contact> OverlapVerticalCapsuleAabb(const VerticalCapsule &capsule,
                                                  const Aabb &bounds) {
    Validate(capsule);
    Validate(bounds);
    const auto c = Shape(capsule);
    Vec3 lo = V(bounds.minimum);
    lo.y -= c.length;
    if (Length(c.bottom - Clamp(c.bottom, lo, V(bounds.maximum))) > c.radius)
        return std::nullopt;
    return BoxContact(c, bounds, 0.0);
}

std::optional<Contact> OverlapVerticalCapsules(const VerticalCapsule &capsule,
                                               const VerticalCapsule &target) {
    Validate(capsule);
    Validate(target);
    const auto c = Shape(capsule), t = Shape(target);
    if (Length(c.bottom - Closest(c.bottom, t.bottom - Vec3{0, c.length, 0},
                                  t.bottom + Vec3{0, t.length, 0})) > c.radius + t.radius)
        return std::nullopt;
    return CapsuleContact(c, t, 0.0);
}

std::optional<Contact> SweepVerticalCapsuleAgainstAabb(const VerticalCapsule &capsule,
                                                       Float3 displacement, const Aabb &bounds) {
    Validate(capsule);
    Validate(bounds);
    if (!Finite(displacement))
        throw std::invalid_argument("collision displacement must be finite");
    auto c = Shape(capsule);
    const Vec3 delta = V(displacement);
    if (Dot(delta, delta) == 0.0)
        return OverlapVerticalCapsuleAabb(capsule, bounds);
    if (const auto overlap = OverlapVerticalCapsuleAabb(capsule, bounds)) {
        return SeparatingContact(*overlap, delta) ? std::nullopt : overlap;
    }
    Vec3 lo = V(bounds.minimum);
    lo.y -= c.length;
    const auto hit = RayRoundedBox(c.bottom, delta, lo, V(bounds.maximum), c.radius);
    if (!hit)
        return std::nullopt;
    c.bottom = c.bottom + delta * *hit;
    auto result = BoxContact(c, bounds, *hit);
    result.penetrationDepth = 0.0f;
    return result;
}

std::optional<Contact> SweepVerticalCapsuleAgainstCapsule(const VerticalCapsule &capsule,
                                                          Float3 displacement,
                                                          const VerticalCapsule &target) {
    Validate(capsule);
    Validate(target);
    if (!Finite(displacement))
        throw std::invalid_argument("collision displacement must be finite");
    auto c = Shape(capsule);
    const auto t = Shape(target);
    const Vec3 delta = V(displacement);
    if (Dot(delta, delta) == 0.0)
        return OverlapVerticalCapsules(capsule, target);
    if (const auto overlap = OverlapVerticalCapsules(capsule, target)) {
        return SeparatingContact(*overlap, delta) ? std::nullopt : overlap;
    }
    const auto hit = RayCapsule(c.bottom, delta, 1.0, t.bottom - Vec3{0, c.length, 0},
                                t.bottom + Vec3{0, t.length, 0}, c.radius + t.radius);
    if (!hit)
        return std::nullopt;
    c.bottom = c.bottom + delta * *hit;
    auto result = CapsuleContact(c, t, *hit);
    result.penetrationDepth = 0.0f;
    return result;
}

} // namespace Engine::Collision
