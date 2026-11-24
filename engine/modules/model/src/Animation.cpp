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
    for (std::size_t i = 0; i < out.size(); ++i) {
        // Read both inputs before writing: out may be a or b.
        const LocalTransform from = a[i];
        const LocalTransform to = b[i];
        out[i] = LocalTransform{Math::Lerp(from.translation, to.translation, weight),
                                Math::Slerp(from.rotation, to.rotation, weight),
                                Math::Lerp(from.scale, to.scale, weight)};
    }
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
