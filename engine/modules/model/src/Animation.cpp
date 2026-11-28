#include "engine/model/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Model {
namespace {

// Clamp needs no change of the time: every key lies in [0, duration], so a
// time outside it already takes a channel's first or last value.
float WrapTime(const float seconds, const float duration, const Wrap wrap) noexcept {
    if (wrap == Wrap::Clamp) return seconds;
    if (!(duration > 0.0F)) return 0.0F;
    float t = std::fmod(seconds, duration);
    if (t < 0.0F) t += duration;
    // A negative time just below a multiple rounds t + duration up to
    // duration itself; that is the same instant as 0.
    return t < duration ? t : 0.0F;
}

Math::Vec3 Interpolate(const Math::Vec3 a, const Math::Vec3 b, const float s) noexcept { return Math::Lerp(a, b, s); }
Math::Quaternion Interpolate(const Math::Quaternion a, const Math::Quaternion b, const float s) noexcept {
    return Math::Slerp(a, b, s);
}

// The channel's value at time t (Jolt SkeletalAnimation::Sample, binary
// search; Bevy's keyframe curves give the first and last values outside).
template <class T>
T SampleKeys(const Keys<T>& keys, const float t) noexcept {
    const auto& times = keys.times;
    // The first key later than t.
    const std::size_t high = static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), t) - times.begin());
    if (high == 0) return keys.values.front();
    if (high == times.size()) return keys.values.back();
    const std::size_t low = high - 1;
    const float s = (t - times[low]) / (times[high] - times[low]);
    return Interpolate(keys.values[low], keys.values[high], s);
}

// One node of Blend: translation and scale linearly, rotation by slerp. By
// value, so out may be either input.
LocalTransform Mix(const LocalTransform from, const LocalTransform to, const float weight) noexcept {
    return LocalTransform{Math::Lerp(from.translation, to.translation, weight),
                          Math::Slerp(from.rotation, to.rotation, weight),
                          Math::Lerp(from.scale, to.scale, weight)};
}

// Squared lengths at or under this are treated as 0 (a length of 1e-6).
constexpr float kDegenerate = 1.0e-12F;

// A unit vector at right angles to v (v not 0).
Math::Vec3 AnyPerpendicular(const Math::Vec3 v) noexcept {
    const Math::Vec3 axis = std::fabs(v.x) < 0.9F * Math::Length(v) ? Math::Vec3{1, 0, 0} : Math::Vec3{0, 1, 0};
    return Math::Normalize(Math::Cross(v, axis));
}

// The shortest rotation taking the direction of from to that of to (neither 0).
Math::Quaternion Arc(const Math::Vec3 from, const Math::Vec3 to) noexcept {
    const Math::Vec3 f = Math::Normalize(from);
    const Math::Vec3 t = Math::Normalize(to);
    const float cosine = Math::Dot(f, t);
    if (cosine < -1.0F + 1.0e-6F) {
        // Opposite: half a turn about any axis at right angles.
        const Math::Vec3 axis = AnyPerpendicular(f);
        return Math::Quaternion{axis.x, axis.y, axis.z, 0.0F};
    }
    const Math::Vec3 c = Math::Cross(f, t);
    return Math::Normalize(Math::Quaternion{c.x, c.y, c.z, 1.0F + cosine});
}

// A node's position and rotation in model space: the matrices down from its
// root (as ComputeModelSpace), the rotations without their scale.
struct ModelFrame final {
    Math::Vec3 position;
    Math::Quaternion rotation;
};

ModelFrame FrameOf(const std::span<const Node> nodes, const std::span<const LocalTransform> pose,
                   const std::uint32_t node) noexcept {
    Math::Matrix4 matrix = ToMatrix(pose[node]);
    Math::Quaternion rotation = Math::NormalizeOrIdentity(pose[node].rotation);
    for (std::uint32_t p = nodes[node].parent; p != kNoParent; p = nodes[p].parent) {
        matrix = Math::Multiply(ToMatrix(pose[p]), matrix);
        rotation = Math::NormalizeOrIdentity(Math::Multiply(Math::NormalizeOrIdentity(pose[p].rotation), rotation));
    }
    return ModelFrame{Math::TransformPoint(matrix, {0, 0, 0}), rotation};
}

// The rotation in model space of a node's parent (identity for a root node).
Math::Quaternion ParentRotationOf(const std::span<const Node> nodes, const std::span<const LocalTransform> pose,
                                  const std::uint32_t node) noexcept {
    const std::uint32_t parent = nodes[node].parent;
    return parent == kNoParent ? Math::Quaternion{} : FrameOf(nodes, pose, parent).rotation;
}

bool IsBelow(const std::span<const Node> nodes, const std::uint32_t node, const std::uint32_t ancestor) noexcept {
    for (std::uint32_t p = nodes[node].parent; p != kNoParent; p = nodes[p].parent) {
        if (p == ancestor) return true;
    }
    return false;
}

} // namespace

void RestPose(const std::span<const Node> nodes, const std::span<LocalTransform> out) {
    GYO_ASSERT(out.size() == nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) out[i] = nodes[i].rest;
}

void SampleClip(const std::span<const Node> nodes, const AnimationClip& clip, const float seconds, const Wrap wrap,
                const std::span<LocalTransform> out) {
    GYO_ASSERT(out.size() == nodes.size());
    GYO_ASSERT(clip.nodeCount == nodes.size());
    GYO_ASSERT(std::isfinite(seconds));
    RestPose(nodes, out);
    const float t = WrapTime(seconds, clip.duration, wrap);
    for (const Track& track : clip.tracks) {
        LocalTransform& pose = out[track.node];
        if (!track.translation.Empty()) pose.translation = SampleKeys(track.translation, t);
        if (!track.rotation.Empty()) pose.rotation = SampleKeys(track.rotation, t);
        if (!track.scale.Empty()) pose.scale = SampleKeys(track.scale, t);
    }
}

void Blend(const std::span<const LocalTransform> a, const std::span<const LocalTransform> b, const float weight,
           const std::span<LocalTransform> out) {
    GYO_ASSERT(a.size() == b.size() && out.size() == a.size());
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = Mix(a[i], b[i], weight);
}

void Blend(const std::span<const LocalTransform> a, const std::span<const LocalTransform> b,
           const std::span<const float> weights, const std::span<LocalTransform> out) {
    GYO_ASSERT(a.size() == b.size() && out.size() == a.size() && weights.size() == a.size());
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = Mix(a[i], b[i], weights[i]);
}

void ApplyAdditive(const std::span<const LocalTransform> base, const std::span<const LocalTransform> motion,
                   const std::span<const LocalTransform> reference, const float weight,
                   const std::span<LocalTransform> out) {
    GYO_ASSERT(motion.size() == base.size() && reference.size() == base.size() && out.size() == base.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        // Read every input before writing: out may be any of them.
        const LocalTransform onto = base[i];
        const LocalTransform moved = motion[i];
        const LocalTransform from = reference[i];
        const Math::Quaternion change = Math::Multiply(Math::Conjugate(Math::NormalizeOrIdentity(from.rotation)),
                                                       Math::NormalizeOrIdentity(moved.rotation));
        out[i] = LocalTransform{
            onto.translation + (moved.translation - from.translation) * weight,
            Math::NormalizeOrIdentity(Math::Multiply(Math::NormalizeOrIdentity(onto.rotation),
                                                     Math::Slerp(Math::Quaternion{}, change, weight))),
            onto.scale + (moved.scale - from.scale) * weight};
    }
}

std::size_t CrossedEvents(const std::span<const float> times, const float from, const float to, const float duration,
                          const Wrap wrap, const std::span<std::uint32_t> out) {
    GYO_ASSERT(out.size() >= times.size());
    GYO_ASSERT(std::isfinite(from) && std::isfinite(to) && std::isfinite(duration) && duration >= 0.0F);
    if (from == to || !(duration > 0.0F)) return 0;
    // Bevy's TriggeredEvents::from_animation (bevy_animation lib.rs:1455-1528):
    // forward [from, to), backward (to, from], split in two where a loop
    // crosses its end.
    const std::size_t count = times.size();
    const auto atLeast = [&times](const float t) {
        return static_cast<std::size_t>(std::lower_bound(times.begin(), times.end(), t) - times.begin());
    };
    const auto above = [&times](const float t) {
        return static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), t) - times.begin());
    };
    std::size_t written = 0;
    const auto up = [&](const std::size_t begin, const std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) out[written++] = static_cast<std::uint32_t>(i);
    };
    const auto down = [&](const std::size_t begin, const std::size_t end) {
        for (std::size_t i = end; i > begin; --i) out[written++] = static_cast<std::uint32_t>(i - 1);
    };
    const bool forward = from < to;
    if (wrap == Wrap::Clamp) {
        const float f = std::clamp(from, 0.0F, duration);
        const float g = std::clamp(to, 0.0F, duration);
        if (forward) {
            // The interval that reaches the end also crosses the instants on it.
            up(atLeast(f), g == duration && f < duration ? count : atLeast(g));
        } else {
            down(g == 0.0F && f > 0.0F ? 0 : above(g), above(f));
        }
        return written;
    }
    const float span = forward ? to - from : from - to;
    const float p = WrapTime(from, duration, wrap);
    if (forward) {
        if (span >= duration) {
            // A whole loop or more: each instant once, from where it starts.
            up(atLeast(p), count);
            up(0, atLeast(p));
            return written;
        }
        const float q = p + span;
        up(atLeast(p), atLeast(q));
        if (q > duration) up(0, atLeast(q - duration));
    } else {
        if (span >= duration) {
            down(0, above(p));
            down(above(p), count);
            return written;
        }
        const float q = p - span;
        down(above(q), above(p));
        if (q < 0.0F) down(above(q + duration), count);
    }
    return written;
}

void SolveTwoBoneIK(const std::span<const Node> nodes, const TwoBoneChain& chain, const Math::Vec3 target,
                    const Math::Vec3 pole, const float weight, const std::span<LocalTransform> pose) {
    GYO_ASSERT(pose.size() == nodes.size());
    GYO_ASSERT(chain.root < nodes.size() && chain.mid < nodes.size() && chain.tip < nodes.size());
    GYO_ASSERT(IsBelow(nodes, chain.mid, chain.root) && IsBelow(nodes, chain.tip, chain.mid));
    GYO_ASSERT(Math::IsFinite(target) && Math::IsFinite(pole) && std::isfinite(weight));
    if (weight == 0.0F) return;
    const ModelFrame root = FrameOf(nodes, pose, chain.root);
    const ModelFrame mid = FrameOf(nodes, pose, chain.mid);
    const Math::Vec3 tip = FrameOf(nodes, pose, chain.tip).position;
    const Math::Vec3 upper = mid.position - root.position;
    const Math::Vec3 lower = tip - mid.position;
    const Math::Vec3 toTarget = target - root.position;
    if (Math::LengthSquared(upper) <= kDegenerate || Math::LengthSquared(lower) <= kDegenerate ||
        Math::LengthSquared(toTarget) <= kDegenerate)
        return;
    const float a = Math::Length(upper);
    const float b = Math::Length(lower);
    const float distance = Math::Length(toTarget);
    const Math::Vec3 u = toTarget / distance;
    // Where mid and tip go (Godot TwoBoneIK3D::_process_joints,
    // scene/3d/two_bone_ik_3d.cpp:780-831): the tip at the target held
    // between the two lengths' difference and their sum (out of reach the
    // chain is straight toward it, nearer the root it folds), the mid on the
    // circle where the two bones' spheres meet, on the pole's side of the
    // line from root to target; with a pole on the line, on the side the mid
    // is now.
    const float reach = std::clamp(distance, std::fabs(a - b), a + b);
    const auto across = [&u](const Math::Vec3 v) { return v - u * Math::Dot(v, u); };
    Math::Vec3 side = across(pole - root.position);
    if (Math::LengthSquared(side) <= kDegenerate) side = across(upper);
    side = Math::LengthSquared(side) <= kDegenerate ? AnyPerpendicular(u) : Math::Normalize(side);
    const float along = (reach * reach + a * a - b * b) / (2.0F * reach);
    const float height = std::sqrt(std::max(a * a - along * along, 0.0F));
    const Math::Vec3 midTo = root.position + u * along + side * height;
    const Math::Vec3 tipTo = root.position + u * reach;
    // The root's turn in model space, then the mid's after it (Godot's swing,
    // two_bone_ik_3d.h:163,176). A turn s in model space of a node whose
    // parent turns by q is the local turn inverse(q) * s * q.
    const Math::Quaternion rootTurn = Arc(upper, midTo - root.position);
    const Math::Quaternion midTurn = Arc(Math::Rotate(rootTurn, lower), tipTo - midTo);
    const Math::Quaternion rootParent = ParentRotationOf(nodes, pose, chain.root);
    const Math::Quaternion midParent = Math::Multiply(rootTurn, ParentRotationOf(nodes, pose, chain.mid));
    Math::Quaternion rootLocal = Math::NormalizeOrIdentity(
        Math::Multiply(Math::Conjugate(rootParent), Math::Multiply(rootTurn, root.rotation)));
    Math::Quaternion midLocal = Math::NormalizeOrIdentity(Math::Multiply(
        Math::Conjugate(Math::NormalizeOrIdentity(midParent)),
        Math::Multiply(midTurn, Math::Multiply(rootTurn, mid.rotation))));
    if (weight != 1.0F) {
        // Godot's influence: each rotation from its value in pose
        // (scene/3d/skeleton_3d.cpp:1267).
        rootLocal = Math::Slerp(pose[chain.root].rotation, rootLocal, weight);
        midLocal = Math::Slerp(pose[chain.mid].rotation, midLocal, weight);
    }
    pose[chain.root].rotation = rootLocal;
    pose[chain.mid].rotation = midLocal;
}

void ComputeModelSpace(const std::span<const Node> nodes, const std::span<const LocalTransform> local,
                       const std::span<Math::Matrix4> out) {
    GYO_ASSERT(local.size() == nodes.size() && out.size() == nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const Math::Matrix4 own = ToMatrix(local[i]);
        const std::uint32_t parent = nodes[i].parent;
        out[i] = parent == kNoParent ? own : Math::Multiply(out[parent], own);
    }
}

} // namespace GYO::Engine::Model
