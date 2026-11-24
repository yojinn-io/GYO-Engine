#include "engine/model/Retarget.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <string_view>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Model {
namespace {

using Failure = RetargetFailure;

Base::Err<Failure> Fail(const RetargetCode code, const char* message, std::string detail) {
    return Base::Err(Failure::Make(code, message, std::move(detail)));
}

// How far apart the components of a rest scale may be and still count as
// uniform, relative to the largest.
constexpr float kUniformTolerance = 1.0e-4F;

// A node's rest frame in model space: rotation and uniform scale. Valid only
// when every rest scale on the way from the root is uniform and above 0.
struct Frame final {
    Math::Quaternion rotation{};
    float scale = 1.0F;
    bool valid = true;
};

bool UniformPositive(const Math::Vec3 s) noexcept {
    const float largest = std::max({s.x, s.y, s.z});
    const float smallest = std::min({s.x, s.y, s.z});
    return Math::IsFinite(s) && smallest > 0.0F && largest - smallest <= largest * kUniformTolerance;
}

// The reference pose: the one given, or the nodes' rest.
std::vector<LocalTransform> ReferencePose(const std::span<const Node> nodes,
                                          const std::span<const LocalTransform> given) {
    if (!given.empty()) {
        GYO_ASSERT(given.size() == nodes.size());
        return {given.begin(), given.end()};
    }
    std::vector<LocalTransform> pose(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) pose[i] = nodes[i].rest;
    return pose;
}

// frames[i] is node i's own reference frame (its parent's times its own).
std::vector<Frame> ReferenceFrames(const std::span<const Node> nodes, const std::vector<LocalTransform>& pose) {
    std::vector<Frame> frames(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const LocalTransform& own = pose[i];
        const Frame parent = nodes[i].parent == kNoParent ? Frame{} : frames[nodes[i].parent];
        Frame& frame = frames[i];
        frame.rotation = Math::NormalizeOrIdentity(Math::Multiply(parent.rotation, own.rotation));
        frame.scale = parent.scale * own.scale.x;
        frame.valid = parent.valid && UniformPositive(own.scale) && std::isfinite(frame.scale) && frame.scale > 0;
    }
    return frames;
}

Frame ParentFrame(const std::span<const Node> nodes, const std::vector<Frame>& frames, const std::uint32_t node) {
    const std::uint32_t parent = nodes[node].parent;
    return parent == kNoParent ? Frame{} : frames[parent];
}

Math::Vec3 Divide(const Math::Vec3 a, const Math::Vec3 b) noexcept { return {a.x / b.x, a.y / b.y, a.z / b.z}; }
Math::Vec3 Multiply(const Math::Vec3 a, const Math::Vec3 b) noexcept { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

} // namespace

Base::Result<NameBinding, RetargetFailure> BindByName(const std::span<const Node> source,
                                                      const std::span<const Node> target) {
    // Name -> index, or kAmbiguous when the name is on more than one node.
    constexpr std::uint32_t kAmbiguous = std::numeric_limits<std::uint32_t>::max();
    const auto index = [](const std::span<const Node> nodes) {
        std::map<std::string_view, std::uint32_t> byName;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto [it, inserted] = byName.emplace(nodes[i].name, static_cast<std::uint32_t>(i));
            if (!inserted) it->second = kAmbiguous;
        }
        return byName;
    };
    const auto sourceByName = index(source);
    const auto targetByName = index(target);

    NameBinding result;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto found = targetByName.find(source[i].name);
        if (found == targetByName.end()) {
            result.unmatchedSource.push_back(static_cast<std::uint32_t>(i));
            continue;
        }
        if (found->second == kAmbiguous || sourceByName.at(source[i].name) == kAmbiguous) {
            return Fail(RetargetCode::AmbiguousName, "a bound name is on one node of each list",
                        "\"" + source[i].name + "\"");
        }
        result.bindings.push_back({static_cast<std::uint32_t>(i), found->second});
    }
    for (std::size_t i = 0; i < target.size(); ++i) {
        if (!sourceByName.contains(target[i].name)) result.unmatchedTarget.push_back(static_cast<std::uint32_t>(i));
    }
    return result;
}

Base::Result<AnimationClip, RetargetFailure> Retarget(const std::span<const Node> source,
                                                      const std::span<const Node> target, const AnimationClip& clip,
                                                      const std::span<const JointBinding> bindings,
                                                      const RetargetSettings& settings,
                                                      const std::span<const LocalTransform> sourceReference,
                                                      const std::span<const LocalTransform> targetReference) {
    GYO_ASSERT(clip.nodeCount == source.size());
    // 1
    if (!std::isfinite(settings.translationScale) || !(settings.translationScale > 0.0F)) {
        return Fail(RetargetCode::BadTranslationScale, "the translation scale is finite and above 0",
                    std::to_string(settings.translationScale));
    }
    // 2
    constexpr std::uint32_t kUnbound = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> targetOf(source.size(), kUnbound);
    std::vector<bool> targetBound(target.size(), false);
    for (const JointBinding& binding : bindings) {
        const std::string where = std::to_string(binding.source) + " -> " + std::to_string(binding.target);
        if (binding.source >= source.size() || binding.target >= target.size()) {
            return Fail(RetargetCode::BadBinding, "a binding names a node of each list", where);
        }
        if (targetOf[binding.source] != kUnbound || targetBound[binding.target]) {
            return Fail(RetargetCode::BadBinding, "a node is bound at most once", where);
        }
        targetOf[binding.source] = binding.target;
        targetBound[binding.target] = true;
    }
    // 3
    const std::vector<LocalTransform> sourcePose = ReferencePose(source, sourceReference);
    const std::vector<LocalTransform> targetPose = ReferencePose(target, targetReference);
    const std::vector<Frame> sourceFrames = ReferenceFrames(source, sourcePose);
    const std::vector<Frame> targetFrames = ReferenceFrames(target, targetPose);
    for (const JointBinding& binding : bindings) {
        const std::string where = std::to_string(binding.source) + " \"" + source[binding.source].name + "\" -> " +
                                  std::to_string(binding.target) + " \"" + target[binding.target].name + "\"";
        if (!ParentFrame(source, sourceFrames, binding.source).valid ||
            !ParentFrame(target, targetFrames, binding.target).valid) {
            return Fail(RetargetCode::BadRestScale, "the rest scales above a bound node are uniform and above 0",
                        where);
        }
        const Math::Vec3 own = sourcePose[binding.source].scale;
        if (own.x == 0.0F || own.y == 0.0F || own.z == 0.0F) {
            return Fail(RetargetCode::BadRestScale, "a bound source node's rest scale has no component of 0", where);
        }
    }

    AnimationClip out;
    out.name = clip.name;
    out.duration = clip.duration;
    out.nodeCount = static_cast<std::uint32_t>(target.size());
    for (const Track& track : clip.tracks) {
        const std::uint32_t t = targetOf[track.node];
        if (t == kUnbound) continue;
        const LocalTransform& restS = sourcePose[track.node];
        const LocalTransform& restT = targetPose[t];
        const Frame parentS = ParentFrame(source, sourceFrames, track.node);
        const Frame parentT = ParentFrame(target, targetFrames, t);
        const Math::Quaternion c = Math::NormalizeOrIdentity(Math::Multiply(Math::Conjugate(parentT.rotation),
                                                                            parentS.rotation));
        const Math::Quaternion cInverse = Math::Conjugate(c);
        const Math::Quaternion restSInverse = Math::Conjugate(Math::NormalizeOrIdentity(restS.rotation));
        const Math::Quaternion restTRotation = Math::NormalizeOrIdentity(restT.rotation);
        const float k = settings.translationScale * parentS.scale / parentT.scale;

        Track mapped;
        mapped.node = t;
        mapped.translation.times = track.translation.times;
        mapped.translation.values.reserve(track.translation.values.size());
        for (const Math::Vec3 v : track.translation.values) {
            mapped.translation.values.push_back(restT.translation + Math::Rotate(c, v - restS.translation) * k);
        }
        mapped.rotation.times = track.rotation.times;
        mapped.rotation.values.reserve(track.rotation.values.size());
        for (const Math::Quaternion q : track.rotation.values) {
            const Math::Quaternion delta = Math::Multiply(Math::NormalizeOrIdentity(q), restSInverse);
            const Math::Quaternion moved = Math::Multiply(Math::Multiply(c, delta), cInverse);
            mapped.rotation.values.push_back(Math::NormalizeOrIdentity(Math::Multiply(moved, restTRotation)));
        }
        mapped.scale.times = track.scale.times;
        mapped.scale.values.reserve(track.scale.values.size());
        for (const Math::Vec3 s : track.scale.values) {
            mapped.scale.values.push_back(Multiply(restT.scale, Divide(s, restS.scale)));
        }
        out.tracks.push_back(std::move(mapped));
    }
    // In the target's node order.
    std::sort(out.tracks.begin(), out.tracks.end(), [](const Track& a, const Track& b) { return a.node < b.node; });
    return out;
}

} // namespace GYO::Engine::Model
