#pragma once

#include "model/Animation.hpp"
#include "render/RenderTypes.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>

namespace Engine::Render {
class IRenderDevice;
class RenderQueue;
}

namespace Engine::ModelRenderer {

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
    [[nodiscard]] static Base::Result<std::shared_ptr<ModelResource>, std::string> Create(
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
    [[nodiscard]] static Base::Result<std::unique_ptr<ModelInstance>, std::string> Create(
        std::shared_ptr<ModelResource> resource, const Model::Pose& pose,
        Model::Vec3 modelSpaceOffset = {});
    ~ModelInstance();
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance& operator=(const ModelInstance&) = delete;

    [[nodiscard]] Base::Result<void, std::string> UpdatePose(const Model::Pose& pose);
    [[nodiscard]] Base::Result<void, std::string> Submit(
        Render::RenderQueue& queue, const Render::Transform3D& transform,
        Render::MeshLayer layer = Render::MeshLayer::World,
        Render::Color tint = {}) const;

private:
    struct Impl;
    explicit ModelInstance(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::ModelRenderer
