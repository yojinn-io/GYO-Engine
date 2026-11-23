#include "engine/physics_3d/Queries.hpp"

// The pure queries (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, sections 4.2 to 4.4). Closed
// forms after Ericson, Real-Time Collision Detection: ray-sphere and ray-capsule (5.3.2, 5.3.7), the slab test
// (5.3.3), closest points of two segments (5.1.9), the moving sphere against a box (5.5.7). Everything is computed
// in double and each result is rounded to float once; only + - * /, std::sqrt, comparisons and abs are used.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Vec3d.hpp"

namespace GYO::Engine::Physics3d {

namespace {

using Math::Aabbd;
using Math::Vec3d;

// ---- Small double helpers (no assert: callers keep lo <= hi or accept the result as it falls).

double Component(const Vec3d& v, const int axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }

void SetComponent(Vec3d& v, const int axis, const double value) {
    if (axis == 0) {
        v.x = value;
    } else if (axis == 1) {
        v.y = value;
    } else {
        v.z = value;
    }
}

Vec3d UnitAxis(const int axis, const double sign) {
    Vec3d v{};
    SetComponent(v, axis, sign);
    return v;
}

double ClampD(const double v, const double lo, const double hi) { return v < lo ? lo : (hi < v ? hi : v); }

Vec3d ClampToBox(const Vec3d& p, const Aabbd& box) {
    return {ClampD(p.x, box.minimum.x, box.maximum.x), ClampD(p.y, box.minimum.y, box.maximum.y),
            ClampD(p.z, box.minimum.z, box.maximum.z)};
}

Vec3d Normalized(const Vec3d& v) { return v / Math::Length(v); }

bool IsZero(const Vec3d& v) { return v.x == 0.0 && v.y == 0.0 && v.z == 0.0; }

// ---- Poses: world = R * local + t, R from the normalized quaternion (columns c0, c1, c2).

struct Frame final {
    Vec3d c0, c1, c2;
    Vec3d t;
};

Frame FrameOf(const Pose& pose) {
    double x = pose.rotation.x;
    double y = pose.rotation.y;
    double z = pose.rotation.z;
    double w = pose.rotation.w;
    const double n = std::sqrt(x * x + y * y + z * z + w * w);
    x /= n;
    y /= n;
    z /= n;
    w /= n;
    Frame f;
    f.c0 = {1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + w * z), 2.0 * (x * z - w * y)};
    f.c1 = {2.0 * (x * y - w * z), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + w * x)};
    f.c2 = {2.0 * (x * z + w * y), 2.0 * (y * z - w * x), 1.0 - 2.0 * (x * x + y * y)};
    f.t = Math::ToVec3d(pose.translation);
    return f;
}

Vec3d Rotate(const Frame& f, const Vec3d& v) { return f.c0 * v.x + f.c1 * v.y + f.c2 * v.z; }
Vec3d Unrotate(const Frame& f, const Vec3d& v) { return {Math::Dot(f.c0, v), Math::Dot(f.c1, v), Math::Dot(f.c2, v)}; }
Vec3d ToWorld(const Frame& f, const Vec3d& p) { return Rotate(f, p) + f.t; }
Vec3d ToLocal(const Frame& f, const Vec3d& p) { return Unrotate(f, p - f.t); }

// ---- Rays

struct RayD final {
    Vec3d origin;
    Vec3d direction;
};

struct HitD final {
    double t = 0.0;
    Vec3d normal{};  // zero when the origin is inside
};

RayHit ToRayHit(const HitD& hit) { return RayHit{static_cast<float>(hit.t), Math::ToVec3(hit.normal)}; }

// The ray from outside or inside a ball. Lagrange's identity keeps the discriminant accurate far away.
std::optional<HitD> RaySphere(const RayD& ray, const Vec3d& centre, const double radius, const double maxT) {
    const Vec3d offset = ray.origin - centre;
    const double radiusSquared = radius * radius;
    if (Math::Dot(offset, offset) <= radiusSquared) return HitD{};
    const double a = Math::Dot(ray.direction, ray.direction);
    const double halfB = Math::Dot(offset, ray.direction);
    if (halfB >= 0.0) return std::nullopt;
    const Vec3d perpendicular = Math::Cross(offset, ray.direction);
    const double discriminant = a * radiusSquared - Math::Dot(perpendicular, perpendicular);
    if (discriminant < 0.0) return std::nullopt;
    const double t = (-halfB - std::sqrt(discriminant)) / a;
    if (!(t >= 0.0) || t > maxT) return std::nullopt;
    return HitD{t, Normalized(offset + ray.direction * t)};
}

double SegmentDistanceSquared(const Vec3d& p, const Vec3d& a, const Vec3d& b) {
    const Vec3d ab = b - a;
    const double dd = Math::Dot(ab, ab);
    const double s = dd > 0.0 ? ClampD(Math::Dot(p - a, ab) / dd, 0.0, 1.0) : 0.0;
    const Vec3d offset = p - (a + ab * s);
    return Math::Dot(offset, offset);
}

// The capsule is the union of the finite cylinder and the two end balls; a ray from outside enters the union first
// where it enters one of them first, and the cylinder's end discs lie inside the balls, so the side and the balls
// are enough. Ties keep the first piece in the order side, ball a, ball b.
std::optional<HitD> RayCapsule(const RayD& ray, const Vec3d& a, const Vec3d& b, const double radius, const double maxT) {
    if (SegmentDistanceSquared(ray.origin, a, b) <= radius * radius) return HitD{};
    std::optional<HitD> best;
    const Vec3d ab = b - a;
    const double dd = Math::Dot(ab, ab);
    if (dd > 0.0) {
        const Vec3d m = ray.origin - a;
        const Vec3d mxab = Math::Cross(m, ab);
        const Vec3d nxab = Math::Cross(ray.direction, ab);
        const double qa = Math::Dot(nxab, nxab);
        if (qa > 0.0) {
            const double qb = Math::Dot(mxab, nxab);
            const double qc = Math::Dot(mxab, mxab) - radius * radius * dd;
            const double discriminant = qb * qb - qa * qc;
            if (discriminant >= 0.0) {
                const double t = (-qb - std::sqrt(discriminant)) / qa;
                const double axial = Math::Dot(m, ab) + t * Math::Dot(ray.direction, ab);
                if (t >= 0.0 && t <= maxT && axial >= 0.0 && axial <= dd) {
                    const Vec3d p = m + ray.direction * t;
                    best = HitD{t, Normalized(p - ab * (axial / dd))};
                }
            }
        }
    }
    for (const Vec3d& centre : {a, b}) {
        const std::optional<HitD> cap = RaySphere(ray, centre, radius, maxT);
        if (cap && (!best || cap->t < best->t)) best = cap;
    }
    return best;
}

// The slab test, one function for the hierarchy's nodes and its leaves (P3-3): with the same ray, a box that
// contains another has an entry no later and an exit no earlier, because subtracting and multiplying by the same
// number are monotonic in IEEE arithmetic. axis is the slab that set the entry, -1 when the ray starts inside (or on
// a face it enters through). Faces are closed.
struct SlabHit final {
    double enter = 0.0;
    int axis = -1;
};

std::optional<SlabHit> RaySlab(const RayD& ray, const Aabbd& box, const double maxT) {
    SlabHit hit;
    double exit = maxT;
    for (int axis = 0; axis < 3; ++axis) {
        const double origin = Component(ray.origin, axis);
        const double direction = Component(ray.direction, axis);
        const double lo = Component(box.minimum, axis);
        const double hi = Component(box.maximum, axis);
        if (direction == 0.0) {
            if (origin < lo || origin > hi) return std::nullopt;
            continue;
        }
        const double inverse = 1.0 / direction;
        // Not named near/far: <windows.h> defines those as macros.
        double entry = (lo - origin) * inverse;
        double leave = (hi - origin) * inverse;
        if (entry > leave) std::swap(entry, leave);
        if (entry > hit.enter) {
            hit.enter = entry;
            hit.axis = axis;
        }
        if (leave < exit) exit = leave;
        if (hit.enter > exit) return std::nullopt;
    }
    return hit;
}

Vec3d SlabNormal(const RayD& ray, const SlabHit& hit) {
    if (hit.axis < 0) return {};
    return UnitAxis(hit.axis, Component(ray.direction, hit.axis) > 0.0 ? -1.0 : 1.0);
}

Aabbd BoxOf(const Math::Aabb& box) { return Math::ToAabbd(box); }

Aabbd CuboidBox(const Cuboid& cuboid) {
    const Vec3d h = Math::ToVec3d(cuboid.halfExtents);
    return {-h, h};
}

// ---- BoxSet traversal

// Calls visit(index) for every box whose node path overlaps query (doubles; the boxes' floats are exact in double).
// A set that is a single leaf visits every box without looking at query: the single-box query (a BoxSet of one
// box), the reference of the contract, then does not depend on the candidate box, so a wrong candidate box shows
// up as a difference from it.
template <class Visit>
void ForEachCandidate(const Detail::BoxSetData& data, const Aabbd& query, Visit&& visit) {
    if (data.nodes.empty()) return;
    if (data.nodes[0].count > 0) {
        for (std::uint32_t k = 0; k < data.nodes[0].count; ++k) visit(data.order[k]);
        return;
    }
    std::vector<std::uint32_t> stack{0};
    const auto overlaps = [&query](const Math::Aabb& box) {
        return box.minimum.x <= query.maximum.x && query.minimum.x <= box.maximum.x &&
               box.minimum.y <= query.maximum.y && query.minimum.y <= box.maximum.y &&
               box.minimum.z <= query.maximum.z && query.minimum.z <= box.maximum.z;
    };
    while (!stack.empty()) {
        const Detail::BoxNode& node = data.nodes[stack.back()];
        const std::uint32_t index = stack.back();
        stack.pop_back();
        if (!overlaps(node.bounds)) continue;
        if (node.count > 0) {
            for (std::uint32_t k = node.first; k < node.first + node.count; ++k) {
                if (overlaps(data.boxes[data.order[k]])) visit(data.order[k]);
            }
        } else {
            stack.push_back(node.right);
            stack.push_back(index + 1);
        }
    }
}

// The candidates in increasing index (the order "all" results are reported in).
std::vector<std::uint32_t> Candidates(const Detail::BoxSetData& data, const Aabbd& query) {
    std::vector<std::uint32_t> found;
    ForEachCandidate(data, query, [&found](const std::uint32_t index) { found.push_back(index); });
    std::sort(found.begin(), found.end());
    return found;
}

// A query box that holds every point within reach of the segment [a, b], with a margin far above the rounding of
// the per-box computations. It only picks candidates: the results come from the per-box query (P3-3).
Aabbd ReachBox(const Vec3d& a, const Vec3d& b, const double reach) {
    const double scale = Math::Max(Math::Max(Math::Max(std::abs(a.x), std::abs(a.y)), Math::Max(std::abs(a.z), std::abs(b.x))),
                                   Math::Max(Math::Max(std::abs(b.y), std::abs(b.z)), reach));
    const double margin = (scale + 1.0) * 1.0e-9;
    const double r = reach + margin;
    return {{Math::Min(a.x, b.x) - r, Math::Min(a.y, b.y) - r, Math::Min(a.z, b.z) - r},
            {Math::Max(a.x, b.x) + r, Math::Max(a.y, b.y) + r, Math::Max(a.z, b.z) + r}};
}

// The nearest box the local ray hits: (float t, index) compared lexicographically, so the result is the linear
// scan's (nearest, ties to the lowest index) whatever order the hierarchy visits the boxes in. A node is skipped
// when its entry, rounded to float, is beyond the best: every box under it enters no earlier.
struct BoxRayHit final {
    float t = 0.0F;
    std::uint32_t index = 0;
    SlabHit slab;
};

std::optional<BoxRayHit> RayBoxSet(const Detail::BoxSetData& data, const RayD& ray, const double maxT) {
    if (data.nodes.empty()) return std::nullopt;
    std::optional<BoxRayHit> best;
    std::vector<std::uint32_t> stack{0};
    while (!stack.empty()) {
        const std::uint32_t index = stack.back();
        stack.pop_back();
        const Detail::BoxNode& node = data.nodes[index];
        const std::optional<SlabHit> reach = RaySlab(ray, BoxOf(node.bounds), maxT);
        if (!reach) continue;
        if (best && static_cast<float>(reach->enter) > best->t) continue;
        if (node.count > 0) {
            for (std::uint32_t k = node.first; k < node.first + node.count; ++k) {
                const std::uint32_t box = data.order[k];
                const std::optional<SlabHit> hit = RaySlab(ray, BoxOf(data.boxes[box]), maxT);
                if (!hit) continue;
                const auto t = static_cast<float>(hit->enter);
                if (!best || t < best->t || (t == best->t && box < best->index)) best = BoxRayHit{t, box, *hit};
            }
        } else {
            stack.push_back(node.right);
            stack.push_back(index + 1);
        }
    }
    return best;
}

// ---- Cores: shape 1 of FindContact and CastShape is a segment (a point for a sphere) and a radius.

struct Core final {
    Vec3d a, b;
    double radius = 0.0;
};

std::optional<Core> CoreOf(const Shape& shape, const Frame& f) {
    if (const auto* sphere = std::get_if<Sphere>(&shape)) return Core{f.t, f.t, sphere->radius};
    if (const auto* capsule = std::get_if<Capsule>(&shape)) {
        const Vec3d half = f.c1 * static_cast<double>(capsule->halfHeight);
        return Core{f.t - half, f.t + half, capsule->radius};
    }
    return std::nullopt;
}

// Closest points of segments [p1, q1] and [p2, q2] (Ericson 5.1.9; exact zero tests for degenerate segments).
std::pair<Vec3d, Vec3d> ClosestSegmentSegment(const Vec3d& p1, const Vec3d& q1, const Vec3d& p2, const Vec3d& q2) {
    const Vec3d d1 = q1 - p1;
    const Vec3d d2 = q2 - p2;
    const Vec3d r = p1 - p2;
    const double a = Math::Dot(d1, d1);
    const double e = Math::Dot(d2, d2);
    const double f = Math::Dot(d2, r);
    double s = 0.0;
    double t = 0.0;
    if (a == 0.0 && e == 0.0) {
        // both points
    } else if (a == 0.0) {
        t = ClampD(f / e, 0.0, 1.0);
    } else {
        const double c = Math::Dot(d1, r);
        if (e == 0.0) {
            s = ClampD(-c / a, 0.0, 1.0);
        } else {
            const double b = Math::Dot(d1, d2);
            const double denominator = a * e - b * b;
            s = denominator > 0.0 ? ClampD((b * f - c * e) / denominator, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = ClampD(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = ClampD((b - c) / a, 0.0, 1.0);
            }
        }
    }
    return {p1 + d1 * s, p2 + d2 * t};
}

struct ContactD final {
    Vec3d point1, point2, normal1;
    double distance = 0.0;
};

Contact ToContact(const ContactD& c) {
    const Math::Vec3 normal = Math::ToVec3(c.normal1);
    return Contact{Math::ToVec3(c.point1), Math::ToVec3(c.point2), normal, -normal, static_cast<float>(c.distance)};
}

ContactD ToWorld(const Frame& f, const ContactD& local) {
    return {ToWorld(f, local.point1), ToWorld(f, local.point2), Rotate(f, local.normal1), local.distance};
}

std::optional<ContactD> CoreCore(const Core& one, const Core& two, const double prediction) {
    const auto [c1, c2] = ClosestSegmentSegment(one.a, one.b, two.a, two.b);
    const Vec3d delta = c2 - c1;
    const double length = Math::Length(delta);
    const double distance = length - one.radius - two.radius;
    if (distance > prediction) return std::nullopt;
    const Vec3d normal = length > 0.0 ? delta / length : Vec3d{1.0, 0.0, 0.0};
    return ContactD{c1 + normal * one.radius, c2 - normal * two.radius, normal, distance};
}

// The face a core touching or inside a box leaves through: the smallest push of the segment's projection out of
// the box along a face normal, ties in the order -X, +X, -Y, +Y, -Z, +Z ("-X": the core moves toward -X and leaves
// through the minimum X face). normal is shape 1's outward normal toward the box (opposite to the push); onBox is
// the deepest core point carried to that face.
struct FaceExit final {
    double push = 0.0;
    Vec3d normal{};
    Vec3d core{};
    Vec3d onBox{};
};

FaceExit LeaveThroughFace(const Vec3d& a, const Vec3d& b, const Aabbd& box) {
    FaceExit best;
    bool found = false;
    int bestAxis = 0;
    bool bestMinus = true;
    for (int axis = 0; axis < 3; ++axis) {
        const double lo = Component(box.minimum, axis);
        const double hi = Component(box.maximum, axis);
        const double low = Math::Min(Component(a, axis), Component(b, axis));
        const double high = Math::Max(Component(a, axis), Component(b, axis));
        for (const bool minus : {true, false}) {
            const double push = Math::Max(minus ? high - lo : hi - low, 0.0);
            if (!found || push < best.push) {
                found = true;
                best.push = push;
                bestAxis = axis;
                bestMinus = minus;
            }
        }
    }
    // The deepest core point in the push direction; a segment parallel to the face gives its point nearest the
    // box's centre.
    const double ca = Component(a, bestAxis);
    const double cb = Component(b, bestAxis);
    Vec3d deepest;
    if (ca == cb) {
        const Vec3d centre = (box.minimum + box.maximum) * 0.5;
        const Vec3d ab = b - a;
        const double dd = Math::Dot(ab, ab);
        deepest = a + ab * (dd > 0.0 ? ClampD(Math::Dot(centre - a, ab) / dd, 0.0, 1.0) : 0.0);
    } else if (bestMinus) {
        deepest = ca > cb ? a : b;
    } else {
        deepest = ca < cb ? a : b;
    }
    best.normal = UnitAxis(bestAxis, bestMinus ? 1.0 : -1.0);
    best.core = deepest;
    best.onBox = ClampToBox(deepest, box);
    SetComponent(best.onBox, bestAxis, bestMinus ? Component(box.minimum, bestAxis) : Component(box.maximum, bestAxis));
    return best;
}

double BoxDistanceSquared(const Vec3d& p, const Aabbd& box) {
    const Vec3d offset = ClampToBox(p, box) - p;
    return Math::Dot(offset, offset);
}

// The point of segment [a, b] nearest the box. The squared distance along the segment is convex and piecewise
// quadratic between the parameters where a coordinate crosses a face: its minimum is at an end, a crossing or the
// stationary point of a piece, so those candidates are evaluated (ties to the smaller parameter).
Vec3d ClosestSegmentPointToBox(const Vec3d& a, const Vec3d& b, const Aabbd& box) {
    const Vec3d d = b - a;
    if (IsZero(d)) return a;
    std::vector<double> breaks{0.0, 1.0};
    for (int axis = 0; axis < 3; ++axis) {
        const double direction = Component(d, axis);
        if (direction == 0.0) continue;
        for (const double bound : {Component(box.minimum, axis), Component(box.maximum, axis)}) {
            const double s = (bound - Component(a, axis)) / direction;
            if (s > 0.0 && s < 1.0) breaks.push_back(s);
        }
    }
    std::sort(breaks.begin(), breaks.end());
    std::vector<double> candidates = breaks;
    for (std::size_t i = 0; i + 1 < breaks.size(); ++i) {
        const double s0 = breaks[i];
        const double s1 = breaks[i + 1];
        if (!(s1 > s0)) continue;
        const double middle = (s0 + s1) * 0.5;
        const Vec3d p = a + d * middle;
        double numerator = 0.0;
        double denominator = 0.0;
        for (int axis = 0; axis < 3; ++axis) {
            const double coordinate = Component(p, axis);
            const double direction = Component(d, axis);
            if (coordinate < Component(box.minimum, axis)) {
                numerator += (Component(a, axis) - Component(box.minimum, axis)) * direction;
                denominator += direction * direction;
            } else if (coordinate > Component(box.maximum, axis)) {
                numerator += (Component(a, axis) - Component(box.maximum, axis)) * direction;
                denominator += direction * direction;
            }
        }
        candidates.push_back(denominator > 0.0 ? ClampD(-numerator / denominator, s0, s1) : middle);
    }
    double bestS = 0.0;
    double bestDistance = BoxDistanceSquared(a, box);
    for (const double s : candidates) {
        const double distance = BoxDistanceSquared(a + d * s, box);
        if (distance < bestDistance || (distance == bestDistance && s < bestS)) {
            bestDistance = distance;
            bestS = s;
        }
    }
    return a + d * bestS;
}

// A core (local to the box's frame) against one box: the single-box contact query.
std::optional<ContactD> CoreBox(const Core& core, const Aabbd& box, const double prediction) {
    const Vec3d p = ClosestSegmentPointToBox(core.a, core.b, box);
    const Vec3d q = ClampToBox(p, box);
    const Vec3d delta = q - p;
    const double squared = Math::Dot(delta, delta);
    if (squared > 0.0) {
        const double length = std::sqrt(squared);
        const double distance = length - core.radius;
        if (distance > prediction) return std::nullopt;
        const Vec3d normal = delta / length;
        return ContactD{p + normal * core.radius, q, normal, distance};
    }
    const FaceExit exit = LeaveThroughFace(core.a, core.b, box);
    return ContactD{exit.core + exit.normal * core.radius, exit.onBox, exit.normal, -(exit.push + core.radius)};
}

Core CoreToLocal(const Frame& f, const Core& core) { return Core{ToLocal(f, core.a), ToLocal(f, core.b), core.radius}; }

// The deepest box contact (float distance, then index), or every contact by increasing index.
struct BoxContactD final {
    ContactD contact;
    float distance = 0.0F;
    std::uint32_t index = 0;
};

std::vector<BoxContactD> CoreBoxSet(const Detail::BoxSetData* data, const Core& local, const double prediction,
                                    const bool deepestOnly) {
    std::vector<BoxContactD> out;
    if (data == nullptr) return out;
    for (const std::uint32_t index : Candidates(*data, ReachBox(local.a, local.b, local.radius + prediction))) {
        const std::optional<ContactD> contact = CoreBox(local, BoxOf(data->boxes[index]), prediction);
        if (!contact) continue;
        const auto distance = static_cast<float>(contact->distance);
        if (!deepestOnly) {
            out.push_back({*contact, distance, index});
        } else if (out.empty() || distance < out[0].distance) {  // increasing index: a tie keeps the lower one
            out.assign(1, {*contact, distance, index});
        }
    }
    return out;
}

const char* NameOf(const Shape& shape) {
    switch (shape.index()) {
    case 0: return "Sphere";
    case 1: return "Capsule";
    case 2: return "Cuboid";
    default: return "BoxSet";
    }
}

Physics3dError UnsupportedPair(const char* query, const Shape& one, const Shape& two, const char* why = nullptr) {
    std::string detail = std::string(query) + " " + NameOf(one) + " -> " + NameOf(two);
    if (why != nullptr) {
        detail += ": ";
        detail += why;
    }
    return Physics3dError::Make(Physics3dErrorCode::Unsupported, "the query is not implemented for this pair of shapes",
                                std::move(detail));
}

// ---- CastShape: shape 1 is a ball (radius) whose centre moves from c by m, against a box extended along `axis` by
// `extend` (the capsule's core; -1: none). The capsule meets the box exactly when its centre meets the box grown by
// the core segment, which is again a box because the segment is parallel to an axis of the box.

struct Sweep final {
    Vec3d centre;
    Vec3d motion;
    double radius = 0.0;    // the shape's own radius (witness1 is on this surface)
    double inflated = 0.0;  // radius + targetDistance (what the cast stops at)
    int axis = -1;
    double extend = 0.0;
};

struct SweepHit final {
    double time = 0.0;
    Vec3d witness1, witness2, normal1;
    bool penetrating = false;
};

// Carries a point of the extended box and the ball centre there back to the real box point and core point: shift
// both along the axis by o in [-extend, extend] so that the box point lies in the real box, o closest to 0.
SweepHit Witnesses(const Sweep& s, const Aabbd& box, const Vec3d& centre, const Vec3d& onExtended, const Vec3d& normal,
                   const double time, const bool penetrating) {
    Vec3d core = centre;
    Vec3d onBox = onExtended;
    if (s.axis >= 0) {
        const double coordinate = Component(onExtended, s.axis);
        const double lower = Math::Max(-s.extend, Component(box.minimum, s.axis) - coordinate);
        const double upper = Math::Min(s.extend, Component(box.maximum, s.axis) - coordinate);
        const double shift = lower > 0.0 ? lower : (upper < 0.0 ? upper : 0.0);
        SetComponent(core, s.axis, Component(core, s.axis) + shift);
        SetComponent(onBox, s.axis, coordinate + shift);
    }
    return SweepHit{time, core + normal * s.radius, onBox, normal, penetrating};
}

std::optional<SweepHit> SweepBox(const Sweep& s, const Aabbd& box, const ShapeCastOptions& options) {
    Aabbd grown = box;
    if (s.axis >= 0) {
        SetComponent(grown.minimum, s.axis, Component(box.minimum, s.axis) - s.extend);
        SetComponent(grown.maximum, s.axis, Component(box.maximum, s.axis) + s.extend);
    }
    // Time 0: overlapping or within targetDistance.
    const Vec3d nearest = ClampToBox(s.centre, grown);
    const Vec3d gap = nearest - s.centre;
    const double gapSquared = Math::Dot(gap, gap);
    if (gapSquared <= s.inflated * s.inflated) {
        Vec3d normal;
        Vec3d onExtended;
        if (gapSquared > 0.0) {
            normal = gap / std::sqrt(gapSquared);
            onExtended = nearest;
        } else {
            const FaceExit exit = LeaveThroughFace(s.centre, s.centre, grown);
            normal = exit.normal;
            onExtended = exit.onBox;
        }
        if (!options.stopAtPenetration && Math::Dot(s.motion, normal) <= 0.0) return std::nullopt;
        return Witnesses(s, box, s.centre, onExtended, normal, 0.0, true);
    }
    if (IsZero(s.motion)) return std::nullopt;
    // The ray of the centre against the rounded box (Ericson 5.5.7): the box expanded by the radius first; a hit in
    // an edge or corner region is then tested against the rounded edges.
    const RayD ray{s.centre, s.motion};
    const double maxTime = options.maxTime;
    const Vec3d r{s.inflated, s.inflated, s.inflated};
    const std::optional<SlabHit> expanded = RaySlab(ray, Aabbd{grown.minimum - r, grown.maximum + r}, maxTime);
    if (!expanded) return std::nullopt;
    double time = expanded->enter;
    const Vec3d entry = s.centre + s.motion * time;
    int below = 0;
    int above = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (Component(entry, axis) < Component(grown.minimum, axis)) below |= 1 << axis;
        if (Component(entry, axis) > Component(grown.maximum, axis)) above |= 1 << axis;
    }
    const int outside = below | above;
    const auto corner = [&grown](const int bits) {
        return Vec3d{(bits & 1) != 0 ? grown.maximum.x : grown.minimum.x, (bits & 2) != 0 ? grown.maximum.y : grown.minimum.y,
                     (bits & 4) != 0 ? grown.maximum.z : grown.minimum.z};
    };
    if (outside == 7) {
        // A corner region: the three rounded edges that meet at the corner.
        std::optional<HitD> best;
        for (const int flip : {1, 2, 4}) {
            const std::optional<HitD> edge = RayCapsule(ray, corner(above), corner(above ^ flip), s.inflated, maxTime);
            if (edge && (!best || edge->t < best->t)) best = edge;
        }
        if (!best) return std::nullopt;
        time = best->t;
    } else if ((outside & (outside - 1)) != 0) {
        // An edge region: the rounded edge between the two corners that share the outside coordinates.
        const std::optional<HitD> edge = RayCapsule(ray, corner(below ^ 7), corner(above), s.inflated, maxTime);
        if (!edge) return std::nullopt;
        time = edge->t;
    }
    const Vec3d centre = s.centre + s.motion * time;
    const Vec3d onExtended = ClampToBox(centre, grown);
    const Vec3d toBox = onExtended - centre;
    const double length = Math::Length(toBox);
    Vec3d normal;
    Vec3d onFace = onExtended;
    if (length > 0.0) {
        normal = toBox / length;
    } else {
        const FaceExit exit = LeaveThroughFace(centre, centre, grown);
        normal = exit.normal;
        onFace = exit.onBox;
    }
    return Witnesses(s, box, centre, onFace, normal, time, false);
}

ShapeHit ToShapeHit(const Frame& f, const SweepHit& hit) {
    const Math::Vec3 normal = Math::ToVec3(Rotate(f, hit.normal1));
    return ShapeHit{static_cast<float>(hit.time),
                    Math::ToVec3(ToWorld(f, hit.witness1)),
                    Math::ToVec3(ToWorld(f, hit.witness2)),
                    normal,
                    -normal,
                    hit.penetrating ? ShapeHitStatus::PenetratingOrWithinTargetDistance : ShapeHitStatus::Converged};
}

void CheckRay(const Math::Ray& ray, const float maxT) {
    GYO_ASSERT(IsValid(ray));
    GYO_ASSERT(maxT >= 0.0F);
}

bool IsValidRadius(const float radius) { return std::isfinite(radius) && radius > 0.0F; }

} // namespace

bool IsValid(const Math::Ray& ray) noexcept {
    return Math::IsFinite(ray.origin) && Math::IsFinite(ray.direction) &&
           (ray.direction.x != 0.0F || ray.direction.y != 0.0F || ray.direction.z != 0.0F);
}

bool IsValid(const ShapeCastOptions& options) noexcept {
    return std::isfinite(options.maxTime) && options.maxTime >= 0.0F && std::isfinite(options.targetDistance) &&
           options.targetDistance >= 0.0F;
}

std::optional<RayHit> CastRay(const Shape& shape, const Pose& pose, const Math::Ray& ray, const float maxT) {
    GYO_ASSERT(IsValid(shape));
    GYO_ASSERT(IsValid(pose));
    CheckRay(ray, maxT);
    const Frame f = FrameOf(pose);
    const RayD world{Math::ToVec3d(ray.origin), Math::ToVec3d(ray.direction)};
    const double limit = maxT;
    if (const auto* sphere = std::get_if<Sphere>(&shape)) {
        const auto hit = RaySphere(world, f.t, sphere->radius, limit);
        return hit ? std::optional<RayHit>(ToRayHit(*hit)) : std::nullopt;
    }
    if (const auto* capsule = std::get_if<Capsule>(&shape)) {
        const Vec3d half = f.c1 * static_cast<double>(capsule->halfHeight);
        const auto hit = RayCapsule(world, f.t - half, f.t + half, capsule->radius, limit);
        return hit ? std::optional<RayHit>(ToRayHit(*hit)) : std::nullopt;
    }
    const RayD local{ToLocal(f, world.origin), Unrotate(f, world.direction)};
    if (const auto* cuboid = std::get_if<Cuboid>(&shape)) {
        const auto hit = RaySlab(local, CuboidBox(*cuboid), limit);
        if (!hit) return std::nullopt;
        return RayHit{static_cast<float>(hit->enter), Math::ToVec3(Rotate(f, SlabNormal(local, *hit)))};
    }
    const Detail::BoxSetData* data = std::get_if<BoxSet>(&shape)->Data();
    if (data == nullptr) return std::nullopt;
    const auto hit = RayBoxSet(*data, local, limit);
    if (!hit) return std::nullopt;
    return RayHit{hit->t, Math::ToVec3(Rotate(f, SlabNormal(local, hit->slab)))};
}

std::optional<RayHit> CastRay(const Math::Ray& ray, const float maxT, const Math::Sphere& sphere) {
    CheckRay(ray, maxT);
    GYO_ASSERT(Math::IsFinite(sphere.center) && IsValidRadius(sphere.radius));
    const auto hit = RaySphere({Math::ToVec3d(ray.origin), Math::ToVec3d(ray.direction)}, Math::ToVec3d(sphere.center),
                               sphere.radius, maxT);
    return hit ? std::optional<RayHit>(ToRayHit(*hit)) : std::nullopt;
}

std::optional<RayHit> CastRay(const Math::Ray& ray, const float maxT, const Math::Capsule& capsule) {
    CheckRay(ray, maxT);
    GYO_ASSERT(Math::IsFinite(capsule.segmentStart) && Math::IsFinite(capsule.segmentEnd) &&
               IsValidRadius(capsule.radius));
    const auto hit = RayCapsule({Math::ToVec3d(ray.origin), Math::ToVec3d(ray.direction)},
                                Math::ToVec3d(capsule.segmentStart), Math::ToVec3d(capsule.segmentEnd), capsule.radius,
                                maxT);
    return hit ? std::optional<RayHit>(ToRayHit(*hit)) : std::nullopt;
}

namespace {

void CheckContact(const Shape& shape1, const Pose& pose1, const Shape& shape2, const Pose& pose2, const float prediction) {
    GYO_ASSERT(IsValid(shape1));
    GYO_ASSERT(IsValid(pose1));
    GYO_ASSERT(IsValid(shape2));
    GYO_ASSERT(IsValid(pose2));
    GYO_ASSERT(std::isfinite(prediction) && prediction >= 0.0F);
}

Base::Result<std::vector<BoxContactD>, Physics3dError> BoxSetContacts(const Shape& shape1, const Pose& pose1,
                                                                      const BoxSet& boxes, const Pose& pose2,
                                                                      const float prediction, const bool deepestOnly) {
    const Shape asShape = boxes;
    CheckContact(shape1, pose1, asShape, pose2, prediction);
    const std::optional<Core> core = CoreOf(shape1, FrameOf(pose1));
    if (!core) return Base::Err(UnsupportedPair("FindContact", shape1, asShape));
    const Frame f = FrameOf(pose2);
    std::vector<BoxContactD> found = CoreBoxSet(boxes.Data(), CoreToLocal(f, *core), prediction, deepestOnly);
    for (BoxContactD& c : found) c.contact = ToWorld(f, c.contact);
    return found;
}

} // namespace

Base::Result<std::optional<Contact>, Physics3dError> FindContact(const Shape& shape1, const Pose& pose1,
                                                                 const Shape& shape2, const Pose& pose2,
                                                                 const float prediction) {
    CheckContact(shape1, pose1, shape2, pose2, prediction);
    const std::optional<Core> core = CoreOf(shape1, FrameOf(pose1));
    if (!core) return Base::Err(UnsupportedPair("FindContact", shape1, shape2));
    if (const auto* boxes = std::get_if<BoxSet>(&shape2)) {
        auto deepest = FindDeepestBoxContact(shape1, pose1, *boxes, pose2, prediction);
        if (!deepest) return Base::Err(std::move(deepest).error());
        if (!*deepest) return std::optional<Contact>{};
        return std::optional<Contact>((*deepest)->contact);
    }
    const Frame f = FrameOf(pose2);
    std::optional<ContactD> contact;
    if (const std::optional<Core> other = CoreOf(shape2, f)) {
        contact = CoreCore(*core, *other, prediction);
    } else {
        contact = CoreBox(CoreToLocal(f, *core), CuboidBox(*std::get_if<Cuboid>(&shape2)), prediction);
        if (contact) contact = ToWorld(f, *contact);
    }
    if (!contact) return std::optional<Contact>{};
    return std::optional<Contact>(ToContact(*contact));
}

Base::Result<std::optional<BoxContact>, Physics3dError> FindDeepestBoxContact(const Shape& shape1, const Pose& pose1,
                                                                             const BoxSet& boxes, const Pose& pose2,
                                                                             const float prediction) {
    auto found = BoxSetContacts(shape1, pose1, boxes, pose2, prediction, true);
    if (!found) return Base::Err(std::move(found).error());
    if (found->empty()) return std::optional<BoxContact>{};
    return std::optional<BoxContact>(BoxContact{ToContact((*found)[0].contact), (*found)[0].index});
}

Base::Result<std::vector<BoxContact>, Physics3dError> FindBoxContacts(const Shape& shape1, const Pose& pose1,
                                                                      const BoxSet& boxes, const Pose& pose2,
                                                                      const float prediction) {
    auto found = BoxSetContacts(shape1, pose1, boxes, pose2, prediction, false);
    if (!found) return Base::Err(std::move(found).error());
    std::vector<BoxContact> out;
    out.reserve(found->size());
    for (const BoxContactD& c : *found) out.push_back({ToContact(c.contact), c.index});
    return out;
}

Base::Result<std::optional<ShapeHit>, Physics3dError> CastShape(const Shape& shape1, const Pose& pose1,
                                                                const Math::Vec3& motion, const Shape& shape2,
                                                                const Pose& pose2, const ShapeCastOptions& options) {
    GYO_ASSERT(IsValid(shape1));
    GYO_ASSERT(IsValid(pose1));
    GYO_ASSERT(Math::IsFinite(motion));
    GYO_ASSERT(IsValid(shape2));
    GYO_ASSERT(IsValid(pose2));
    GYO_ASSERT(IsValid(options));
    const bool sphere = std::holds_alternative<Sphere>(shape1);
    const bool capsule = std::holds_alternative<Capsule>(shape1);
    const bool box = std::holds_alternative<Cuboid>(shape2) || std::holds_alternative<BoxSet>(shape2);
    if (!(sphere || capsule) || !box) return Base::Err(UnsupportedPair("CastShape", shape1, shape2));
    const Frame f1 = FrameOf(pose1);
    const Frame f2 = FrameOf(pose2);
    Sweep s;
    s.centre = ToLocal(f2, f1.t);
    s.motion = Unrotate(f2, Math::ToVec3d(motion));
    if (sphere) {
        s.radius = std::get_if<Sphere>(&shape1)->radius;
    } else {
        const Capsule& c = *std::get_if<Capsule>(&shape1);
        s.radius = c.radius;
        if (c.halfHeight > 0.0F) {
            // The capsule's axis in the box's frame must have exactly two zero components.
            const Vec3d axis = Unrotate(f2, f1.c1);
            int zeros = 0;
            for (int i = 0; i < 3; ++i) {
                if (Component(axis, i) == 0.0) {
                    ++zeros;
                } else {
                    s.axis = i;
                }
            }
            if (zeros != 2) {
                return Base::Err(UnsupportedPair("CastShape", shape1, shape2,
                                                 "the capsule's axis is not parallel to an axis of the box"));
            }
            s.extend = static_cast<double>(c.halfHeight) * std::abs(Component(axis, s.axis));
        }
    }
    s.inflated = s.radius + static_cast<double>(options.targetDistance);
    if (const auto* cuboid = std::get_if<Cuboid>(&shape2)) {
        const std::optional<SweepHit> hit = SweepBox(s, CuboidBox(*cuboid), options);
        if (!hit) return std::optional<ShapeHit>{};
        return std::optional<ShapeHit>(ToShapeHit(f2, *hit));
    }
    const Detail::BoxSetData* data = std::get_if<BoxSet>(&shape2)->Data();
    if (data == nullptr) return std::optional<ShapeHit>{};
    // Candidates: the boxes the swept core (grown by the inflated radius) can reach within maxTime.
    Vec3d half{};
    if (s.axis >= 0) SetComponent(half, s.axis, s.extend);
    const Vec3d end = s.centre + s.motion * static_cast<double>(options.maxTime);
    const Aabbd path{{Math::Min(s.centre.x, end.x) - half.x, Math::Min(s.centre.y, end.y) - half.y,
                      Math::Min(s.centre.z, end.z) - half.z},
                     {Math::Max(s.centre.x, end.x) + half.x, Math::Max(s.centre.y, end.y) + half.y,
                      Math::Max(s.centre.z, end.z) + half.z}};
    std::optional<SweepHit> best;
    float bestTime = 0.0F;
    for (const std::uint32_t index : Candidates(*data, ReachBox(path.minimum, path.maximum, s.inflated))) {
        const std::optional<SweepHit> hit = SweepBox(s, BoxOf(data->boxes[index]), options);
        if (!hit) continue;
        const auto time = static_cast<float>(hit->time);
        if (!best || time < bestTime) {  // increasing index: a tie keeps the lower one
            best = hit;
            bestTime = time;
        }
    }
    if (!best) return std::optional<ShapeHit>{};
    return std::optional<ShapeHit>(ToShapeHit(f2, *best));
}

} // namespace GYO::Engine::Physics3d
