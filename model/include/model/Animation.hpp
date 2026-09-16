#pragma once

#include "model/ModelAsset.hpp"
#include "engine/base/Result.hpp"

#include <span>

namespace Engine::Model {

enum class PlaybackMode { Clamp, Loop };

struct Pose final {
    std::vector<Transform> localTransforms;
    std::vector<Matrix4> globalTransforms;
};

struct SkinnedVertex final {
    Vec3 position{};
    Vec3 normal{};
    Vec2 uv{};
};

// Call once when constructing a model outside a provided asset loader.
[[nodiscard]] Base::Result<void, std::string> ValidateModel(const ModelAsset& model);

// Validated model input. Sampling is deterministic and never advances a clock.
// Output storage is reused; all animation policy remains with the caller.
[[nodiscard]] Base::Result<void, std::string> SamplePose(
    const ModelAsset& model, std::size_t clipIndex, double seconds,
    PlaybackMode mode, Pose& output);
[[nodiscard]] Base::Result<void, std::string> MakeDefaultPose(
    const ModelAsset& model, Pose& output);

// Produces complete model-space vertices; apply the instance transform once
// when rendering. Topology and UVs remain fixed while the pose changes.
[[nodiscard]] Base::Result<void, std::string> SkinMesh(
    const ModelAsset& model, std::size_t meshIndex, const Pose& pose,
    std::vector<SkinnedVertex>& output);

} // namespace Engine::Model
