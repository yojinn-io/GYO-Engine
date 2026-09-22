#include "model_renderer/ModelRenderer.hpp"

#include "render/IRenderDevice.hpp"
#include "render/RenderQueue.hpp"

#include <cmath>
#include <utility>
#include <vector>

namespace Engine::ModelRenderer {
namespace {
using Result = Base::Result<void, std::string>;
bool Finite(const Render::Color color) {
    return std::isfinite(color.red) && std::isfinite(color.green) &&
           std::isfinite(color.blue) && std::isfinite(color.alpha);
}
}

struct ModelResource::Impl final {
    Render::IRenderDevice* device{};
    std::shared_ptr<const Model::ModelAsset> model;
    std::vector<Render::MaterialDesc> materials;

    ~Impl() {
        if (device) for (const auto& material : materials) {
            if (material.texture.IsValid()) static_cast<void>(device->ReleaseTexture(material.texture));
        }
    }
};

ModelResource::ModelResource(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ModelResource::~ModelResource() = default;

Base::Result<std::shared_ptr<ModelResource>, std::string> ModelResource::Create(
    Render::IRenderDevice& device, std::shared_ptr<const Model::ModelAsset> model,
    const std::span<const ModelMaterial> materials) {
    using CreateResult = Base::Result<std::shared_ptr<ModelResource>, std::string>;
    if (!model) return CreateResult::Err("Model resource requires a model.");
    const auto valid = Model::ValidateModel(*model);
    if (!valid) return CreateResult::Err(valid.error());
    if (materials.size() != model->materials.size())
        return CreateResult::Err("Model resource requires one material per source material.");
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->model = std::move(model);
    impl->materials.reserve(materials.size());
    for (const auto& source : materials) {
        if (!Finite(source.tint)) return CreateResult::Err("Model material tint must be finite.");
        Render::MaterialDesc material;
        material.tint = source.tint;
        material.sampler = source.sampler;
        if (source.texture) {
            const auto created = device.CreateTexture(*source.texture);
            if (!created) return CreateResult::Err("Model texture upload failed: " + created.error().message);
            material.texture = created.value();
        }
        impl->materials.push_back(material);
    }
    return CreateResult::Ok(std::shared_ptr<ModelResource>(new ModelResource(std::move(impl))));
}

struct ModelInstance::Impl final {
    std::shared_ptr<ModelResource> resource;
    Model::Vec3 offset{};
    std::vector<Render::MeshHandle> meshes;
    std::vector<Model::SkinnedVertex> skinned;
    std::vector<Render::Vertex3D> vertices;

    ~Impl() {
        if (resource) for (const auto handle : meshes)
            static_cast<void>(resource->impl_->device->ReleaseMesh(handle));
    }

    Result Skin(const std::size_t mesh, const Model::Pose& pose) {
        const auto result = Model::SkinMesh(*resource->impl_->model, mesh, pose, skinned);
        if (!result) return result;
        vertices.resize(skinned.size());
        for (std::size_t index = 0; index < skinned.size(); ++index) {
            const auto& vertex = skinned[index];
            vertices[index] = {{vertex.position.x + offset.x, vertex.position.y + offset.y,
                                vertex.position.z + offset.z}, {vertex.uv.x, vertex.uv.y}};
        }
        return Result::Ok();
    }
};

ModelInstance::ModelInstance(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ModelInstance::~ModelInstance() = default;

Base::Result<std::unique_ptr<ModelInstance>, std::string> ModelInstance::Create(
    std::shared_ptr<ModelResource> resource, const Model::Pose& pose,
    const Model::Vec3 modelSpaceOffset) {
    using CreateResult = Base::Result<std::unique_ptr<ModelInstance>, std::string>;
    if (!resource) return CreateResult::Err("Model instance requires a resource.");
    if (!std::isfinite(modelSpaceOffset.x) || !std::isfinite(modelSpaceOffset.y) ||
        !std::isfinite(modelSpaceOffset.z)) return CreateResult::Err("Model offset must be finite.");
    auto impl = std::make_unique<Impl>();
    impl->resource = std::move(resource);
    impl->offset = modelSpaceOffset;
    const auto& model = *impl->resource->impl_->model;
    impl->meshes.reserve(model.meshes.size());
    for (std::size_t mesh = 0; mesh < model.meshes.size(); ++mesh) {
        const auto skinned = impl->Skin(mesh, pose);
        if (!skinned) return CreateResult::Err(skinned.error());
        const auto created = impl->resource->impl_->device->CreateMesh({impl->vertices, model.meshes[mesh].indices});
        if (!created) return CreateResult::Err("Model mesh creation failed: " + created.error().message);
        impl->meshes.push_back(created.value());
    }
    return CreateResult::Ok(std::unique_ptr<ModelInstance>(new ModelInstance(std::move(impl))));
}

Result ModelInstance::UpdatePose(const Model::Pose& pose) {
    for (std::size_t mesh = 0; mesh < impl_->meshes.size(); ++mesh) {
        const auto skinned = impl_->Skin(mesh, pose);
        if (!skinned) return skinned;
        const auto updated = impl_->resource->impl_->device->UpdateMeshVertices(impl_->meshes[mesh], impl_->vertices);
        if (!updated) return Result::Err("Model mesh update failed: " + updated.error().message);
    }
    return Result::Ok();
}

Result ModelInstance::Submit(Render::RenderQueue& queue, const Render::Transform3D& transform,
                              const Render::MeshLayer layer, const Render::Color tint) const {
    if (!Finite(tint)) return Result::Err("Model instance tint must be finite.");
    const auto& resource = *impl_->resource->impl_;
    for (std::size_t mesh = 0; mesh < impl_->meshes.size(); ++mesh) {
        Render::MeshSubmission submission;
        submission.mesh = impl_->meshes[mesh];
        submission.material = resource.materials[resource.model->meshes[mesh].materialIndex];
        submission.material.tint.red *= tint.red;
        submission.material.tint.green *= tint.green;
        submission.material.tint.blue *= tint.blue;
        submission.material.tint.alpha *= tint.alpha;
        submission.transform = transform;
        submission.layer = layer;
        const auto submitted = queue.Submit(submission);
        if (!submitted) return Result::Err("Model submission failed: " + submitted.error().message);
    }
    return Result::Ok();
}

} // namespace Engine::ModelRenderer
