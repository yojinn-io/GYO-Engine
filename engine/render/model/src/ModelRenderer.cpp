#include "engine/render/model/ModelRenderer.hpp"

#include "engine/render/IRenderDevice.hpp"
#include "engine/render/RenderQueue.hpp"

#include "engine/base/Assert.hpp"

#include <cmath>
#include <utility>
#include <vector>

namespace Engine::ModelRenderer {
namespace {
using Result = Base::Result<void, ModelRendererError>;

ModelRendererError Failure(const ModelRendererErrorCode code, std::string message, std::string detail = {}) {
    return ModelRendererError::Make(code, std::move(message), std::move(detail));
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

Base::Result<std::shared_ptr<ModelResource>, ModelRendererError> ModelResource::Create(
    Render::IRenderDevice& device, std::shared_ptr<const Model::ModelAsset> model,
    const std::span<const ModelMaterial> materials) {
    GYO_ASSERT(model != nullptr);
    const auto valid = Model::ValidateModel(*model);
    if (!valid) {
        return Base::Err(Failure(ModelRendererErrorCode::InvalidModel, valid.error().message,
                                 Base::CauseDetail(valid.error())));
    }
    if (materials.size() != model->materials.size())
        return Base::Err(Failure(ModelRendererErrorCode::InvalidArgument,
                                 "Model resource requires one material per source material."));
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->model = std::move(model);
    impl->materials.reserve(materials.size());
    for (const auto& source : materials) {
        if (!Render::IsFinite(source.tint))
            return Base::Err(Failure(ModelRendererErrorCode::InvalidArgument, "Model material tint must be finite."));
        Render::MaterialDesc material;
        material.tint = source.tint;
        material.sampler = source.sampler;
        if (source.texture) {
            const auto created = device.CreateTexture(*source.texture);
            if (!created) {
                return Base::Err(Failure(ModelRendererErrorCode::ResourceCreationFailed,
                                         "Model texture upload failed: " + created.error().message,
                                         Base::CauseDetail(created.error())));
            }
            material.texture = created.value();
        }
        impl->materials.push_back(material);
    }
    return std::shared_ptr<ModelResource>(new ModelResource(std::move(impl)));
}

struct ModelInstance::Impl final {
    std::shared_ptr<ModelResource> resource;
    Math::Vec3 offset{};
    std::vector<Render::MeshHandle> meshes;
    std::vector<Model::SkinnedVertex> skinned;
    std::vector<Render::Vertex3D> vertices;

    ~Impl() {
        if (resource) for (const auto handle : meshes)
            static_cast<void>(resource->impl_->device->ReleaseMesh(handle));
    }

    Result Skin(const std::size_t mesh, const Model::Pose& pose) {
        const auto result = Model::SkinMesh(*resource->impl_->model, mesh, pose, skinned);
        if (!result) {
            return Base::Err(Failure(ModelRendererErrorCode::InvalidModel, result.error().message,
                                     Base::CauseDetail(result.error())));
        }
        vertices.resize(skinned.size());
        for (std::size_t index = 0; index < skinned.size(); ++index) {
            const auto& vertex = skinned[index];
            vertices[index] = {vertex.position + offset, vertex.uv};
        }
        return {};
    }
};

ModelInstance::ModelInstance(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ModelInstance::~ModelInstance() = default;

Base::Result<std::unique_ptr<ModelInstance>, ModelRendererError> ModelInstance::Create(
    std::shared_ptr<ModelResource> resource, const Model::Pose& pose,
    const Math::Vec3 modelSpaceOffset) {
    GYO_ASSERT(resource != nullptr);
    if (!Math::IsFinite(modelSpaceOffset))
        return Base::Err(Failure(ModelRendererErrorCode::InvalidArgument, "Model offset must be finite."));
    auto impl = std::make_unique<Impl>();
    impl->resource = std::move(resource);
    impl->offset = modelSpaceOffset;
    const auto& model = *impl->resource->impl_->model;
    impl->meshes.reserve(model.meshes.size());
    for (std::size_t mesh = 0; mesh < model.meshes.size(); ++mesh) {
        const auto skinned = impl->Skin(mesh, pose);
        if (!skinned) return Base::Err(skinned.error());
        const auto created = impl->resource->impl_->device->CreateMesh({impl->vertices, model.meshes[mesh].indices});
        if (!created) {
            return Base::Err(Failure(ModelRendererErrorCode::ResourceCreationFailed,
                                     "Model mesh creation failed: " + created.error().message,
                                     Base::CauseDetail(created.error())));
        }
        impl->meshes.push_back(created.value());
    }
    return std::unique_ptr<ModelInstance>(new ModelInstance(std::move(impl)));
}

Result ModelInstance::UpdatePose(const Model::Pose& pose) {
    for (std::size_t mesh = 0; mesh < impl_->meshes.size(); ++mesh) {
        const auto skinned = impl_->Skin(mesh, pose);
        if (!skinned) return skinned;
        const auto updated = impl_->resource->impl_->device->UpdateMeshVertices(impl_->meshes[mesh], impl_->vertices);
        if (!updated) {
            return Base::Err(Failure(ModelRendererErrorCode::ResourceCreationFailed,
                                     "Model mesh update failed: " + updated.error().message,
                                     Base::CauseDetail(updated.error())));
        }
    }
    return {};
}

Result ModelInstance::Submit(Render::RenderQueue& queue, const Render::Transform3D& transform,
                              const Render::MeshLayer layer, const Render::Color tint) const {
    if (!Render::IsFinite(tint))
        return Base::Err(Failure(ModelRendererErrorCode::InvalidArgument, "Model instance tint must be finite."));
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
        if (!submitted) {
            return Base::Err(Failure(ModelRendererErrorCode::SubmissionFailed,
                                     "Model submission failed: " + submitted.error().message,
                                     Base::CauseDetail(submitted.error())));
        }
    }
    return {};
}

} // namespace Engine::ModelRenderer
