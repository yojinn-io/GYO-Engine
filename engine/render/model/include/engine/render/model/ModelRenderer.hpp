#pragma once

#include "engine/model/Animation.hpp"
#include "engine/render/RenderTypes.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace Engine::Render {
class IRenderDevice;
class RenderQueue;
}

namespace Engine::ModelRenderer {

// Failure classes of the model renderer. Zero is not a valid code; the numeric
// values are not a data contract.
enum class ModelRendererErrorCode : std::uint8_t {
    // The model failed Model validation or skinning (the Model code is in detail).
    InvalidModel = 1,
    // A caller argument is invalid (material count, tint, offset).
    InvalidArgument,
    // The device could not create or update a texture or mesh (the Render code
    // is in detail).
    ResourceCreationFailed,
    // The render queue rejected a submission (the Render code is in detail).
    SubmissionFailed,
};

[[nodiscard]] constexpr const char* ToString(const ModelRendererErrorCode code) noexcept {
    switch (code) {
    case ModelRendererErrorCode::InvalidModel: return "InvalidModel";
    case ModelRendererErrorCode::InvalidArgument: return "InvalidArgument";
    case ModelRendererErrorCode::ResourceCreationFailed: return "ResourceCreationFailed";
    case ModelRendererErrorCode::SubmissionFailed: return "SubmissionFailed";
    }
    return "Unknown";
}

using ModelRendererError = Base::Error<ModelRendererErrorCode>;
static_assert(Base::CodedError<ModelRendererError>);

// One entry per model material. Pixels are consumed during Create and need not
// outlive that call. Absent textures use the renderer's white fallback. Tint
// is explicit; source model colors are not multiplied a second time.
struct ModelMaterial final {
    std::optional<Render::ImageView> texture;
    Render::Color tint{};
    Render::SamplerMode sampler{Render::SamplerMode::LinearClamp};
};

// Share this object across instances of the same model/material combination.
// The device must outlive resources and their instances; construction, update,
// submission, and destruction follow the device's thread requirements.
class ModelResource final {
public:
    [[nodiscard]] static Base::Result<std::shared_ptr<ModelResource>, ModelRendererError> Create(
        Render::IRenderDevice& device, std::shared_ptr<const Model::ModelAsset> model,
        std::span<const ModelMaterial> materials);
    ~ModelResource();
    ModelResource(const ModelResource&) = delete;
    ModelResource& operator=(const ModelResource&) = delete;

private:
    struct Impl;
    explicit ModelResource(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    friend class ModelInstance;
};

// Every instance owns its mesh handles and skinning buffers. Pose evaluation
// remains external so simulation and rendering can consume the same snapshot.
class ModelInstance final {
public:
    [[nodiscard]] static Base::Result<std::unique_ptr<ModelInstance>, ModelRendererError> Create(
        std::shared_ptr<ModelResource> resource, const Model::Pose& pose,
        Math::Vec3 modelSpaceOffset = {});
    ~ModelInstance();
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance& operator=(const ModelInstance&) = delete;

    [[nodiscard]] Base::Result<void, ModelRendererError> UpdatePose(const Model::Pose& pose);
    [[nodiscard]] Base::Result<void, ModelRendererError> Submit(
        Render::RenderQueue& queue, const Render::Transform3D& transform,
        Render::MeshLayer layer = Render::MeshLayer::World,
        Render::Color tint = {}) const;

private:
    struct Impl;
    explicit ModelInstance(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::ModelRenderer
