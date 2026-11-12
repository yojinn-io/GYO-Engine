#pragma once

#include "engine/model/ModelAsset.hpp"
#include "engine/model/ModelError.hpp"

#include <span>
#include <memory>

namespace Engine::Model {

enum class PlaybackMode { Clamp, Loop };

struct Pose final {
    std::vector<Transform> localTransforms;
    std::vector<Math::Matrix4> globalTransforms;
};

struct SkinnedVertex final {
    Math::Vec3 position{};
    Math::Vec3 normal{};
    Math::Vec2 uv{};
};

// Call once when constructing a model outside a provided asset loader.
[[nodiscard]] ModelResult<void> ValidateModel(const ModelAsset& model);

// Validated model input. Sampling is deterministic and never advances a clock.
// Output storage is reused; all animation policy remains with the caller.
[[nodiscard]] ModelResult<void> SamplePose(
    const ModelAsset& model, std::size_t clipIndex, double seconds,
    PlaybackMode mode, Pose& output);
[[nodiscard]] ModelResult<void> MakeDefaultPose(
    const ModelAsset& model, Pose& output);

struct AnimationNodeBinding final {
    std::size_t sourceNodeIndex{};
    std::size_t targetNodeIndex{};
};

// Transfer rest-relative motion between explicitly matched hierarchies. The
// one-to-one binding must include each mapped node's ancestors. Rest frames may
// differ in orientation, proportions and positive uniform scale; non-uniform
// or mirrored rest/animated scale is unsupported. translationScale is an
// explicit positive multiplier of model-space displacement, with parent rest
// scales accounted for when converting back to target local coordinates.
// Every source track must be mapped, except explicitly excluded non-skeletal
// mesh nodes. Unmapped target nodes retain their defaults during sampling.
// The returned clip owns its keys; neither input asset is modified. No names,
// bone roles, axis inference or automatic humanoid matching are involved.
[[nodiscard]] ModelResult<AnimationClip> TransferCompatibleAnimation(
    const ModelAsset& source, const ModelAsset& target, std::size_t sourceClipIndex,
    std::span<const AnimationNodeBinding> bindings, float translationScale,
    std::span<const std::size_t> excludedSourceTrackNodes = {});

// Interpolate local TRS, then resolve the hierarchy. Matrix interpolation is
// deliberately avoided so rotations remain rigid during transitions.
[[nodiscard]] ModelResult<void> BlendPoses(
    const ModelAsset& model, const Pose& from, const Pose& to, float alpha, Pose& output);

struct PlaybackInterval final {
    // Unwrapped playback seconds; a clamped clip never advances past its end.
    double previousSeconds{};
    double currentSeconds{};
    double durationSeconds{};
    PlaybackMode mode{PlaybackMode::Clamp};

    // Event policy stays with the caller. Each Advance reports (previous,
    // current], including intervals which span one or more loop boundaries.
    [[nodiscard]] bool Crossed(double eventSeconds) const noexcept;
};

// Produces complete model-space vertices; apply the instance transform once
// when rendering. Topology and UVs remain fixed while the pose changes.
[[nodiscard]] ModelResult<void> SkinMesh(
    const ModelAsset& model, std::size_t meshIndex, const Pose& pose,
    std::vector<SkinnedVertex>& output);

} // namespace Engine::Model
