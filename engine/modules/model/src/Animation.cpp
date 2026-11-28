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
