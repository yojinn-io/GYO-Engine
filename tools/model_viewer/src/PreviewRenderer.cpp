#include "gyo/model_viewer/PreviewRenderer.hpp"

#include "gyo/model_viewer/ModelContent.hpp"

#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/base/Error.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/render/PrimitiveMesh.hpp"
#include "engine/render/RenderQueue.hpp"
#include "engine/render/Renderer.hpp"
#include "engine/render/ShaderLibrary.hpp"
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "engine/render/model/ModelRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Gyo::Tools::ModelViewer {

namespace Math = Engine::Math;
namespace Render = Engine::Render;
using GpuDevice = Render::Backend::SdlGpu::SdlGpuRenderDevice;

namespace {

ModelViewerError PreviewError(std::string message, std::string detail = {}) {
    return ModelViewerError::Make(ModelViewerErrorCode::PreviewFailed, std::move(message), std::move(detail));
}

Math::Vec3 Translation(const Math::Matrix4& matrix) noexcept { return Math::TransformPoint(matrix, {}); }

} // namespace

Render::PerspectiveCamera3D OrbitCamera::Camera() const noexcept {
    // Engine's camera: rotation Z, then Y, then X; +Z forward. MakeRotationX
    // takes +Z to (0, -sin, cos), so a positive pitch looks down.
    const Math::Matrix4 rotation = Math::Multiply(Math::MakeRotationY(yawRadians), Math::MakeRotationX(pitchRadians));
    const Math::Vec3 forward = Math::TransformPoint(rotation, {0, 0, 1});
    Render::PerspectiveCamera3D camera;
    camera.position = target - forward * distance;
    camera.rotationRadians = {pitchRadians, yawRadians, 0};
    camera.verticalFieldOfViewRadians = verticalFieldOfViewRadians;
    camera.nearClip = std::max(distance * 0.01F, 0.001F);
    camera.farClip = std::max(distance * 10.0F, 1.0F);
    return camera;
}

OrbitCamera FrameModel(const Engine::Model::ModelAsset& model, const Engine::Model::Pose& pose, OrbitCamera camera) noexcept {
    constexpr float kHuge = std::numeric_limits<float>::max();
    Math::Vec3 low{kHuge, kHuge, kHuge}, high{-kHuge, -kHuge, -kHuge};
    const auto add = [&](const Math::Vec3 point) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) return;
        low = {std::min(low.x, point.x), std::min(low.y, point.y), std::min(low.z, point.z)};
        high = {std::max(high.x, point.x), std::max(high.y, point.y), std::max(high.z, point.z)};
    };
    for (const auto& global : pose.globalTransforms) add(Translation(global));
    for (const auto& mesh : model.meshes) {
        if (mesh.nodeIndex >= pose.globalTransforms.size()) continue;
        const Math::Matrix4 place = Math::Multiply(pose.globalTransforms[mesh.nodeIndex], mesh.geometryToNode);
        for (const auto& vertex : mesh.vertices) add(Math::TransformPoint(place, vertex.position));
    }
    if (low.x > high.x) return camera;
    camera.target = (low + high) * 0.5F;
    const float radius = std::max(Math::Length(high - low) * 0.5F, 0.01F);
    camera.distance = radius / std::sin(camera.verticalFieldOfViewRadians * 0.5F) * 1.1F;
    return camera;
}

struct PreviewRenderer::Impl final {
    Render::ShaderLibrary shaders;
    std::unique_ptr<GpuDevice> device;
    Render::Renderer renderer;
    std::shared_ptr<const Engine::Model::ModelAsset> model;
    std::unique_ptr<Engine::ModelRenderer::ModelInstance> instance;
    Render::MeshHandle bone{};
    bool skeleton{true};
    float boneThickness{0.01F};
    Render::TextureReadback frame;

    ~Impl() {
        // Renderer and model resources release device objects before the device goes.
        instance.reset();
        renderer.Reset();
        if (device && bone) static_cast<void>(device->ReleaseMesh(bone));
    }

    // Each bone (a node and its parent) as a thin box from parent to child.
    [[nodiscard]] Result<void, ModelViewerError> SubmitSkeleton(Render::RenderQueue& queue,
                                                                const Engine::Model::Pose& pose) const {
        for (std::size_t node = 0; node < model->nodes.size() && node < pose.globalTransforms.size(); ++node) {
            const auto parent = model->nodes[node].parentIndex;
            if (!parent || *parent >= pose.globalTransforms.size()) continue;
            const Math::Vec3 from = Translation(pose.globalTransforms[*parent]);
            const Math::Vec3 to = Translation(pose.globalTransforms[node]);
            const Math::Vec3 along = to - from;
            const float length = Math::Length(along);
            if (!(length > 1e-6F) || !std::isfinite(length)) continue;
            // Ry(yaw) Rx(pitch) takes +Z to (sin yaw cos pitch, -sin pitch, cos yaw cos pitch).
            const float pitch = -std::asin(std::clamp(along.y / length, -1.0F, 1.0F));
            const float yaw = std::atan2(along.x, along.z);
            Render::MeshSubmission submission;
            submission.mesh = bone;
            submission.layer = Render::MeshLayer::WorldOverlay;
            submission.doubleSided = true;
            submission.material.tint = {1.0F, 0.85F, 0.2F, 1.0F};
            submission.transform = {(from + to) * 0.5F, {pitch, yaw, 0}, {boneThickness, boneThickness, length}};
            auto submitted = queue.Submit(submission);
            if (!submitted) return Engine::Base::Err(PreviewError("the skeleton cannot be drawn", Engine::Base::CauseDetail(submitted.error())));
        }
        return {};
    }
};

PreviewRenderer::PreviewRenderer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
PreviewRenderer::~PreviewRenderer() = default;

Result<std::unique_ptr<PreviewRenderer>, ModelViewerError> PreviewRenderer::Create(
    Engine::Platform::Sdl::SdlPlatform& platform, const PreviewOptions& options) {
    auto impl = std::make_unique<Impl>();
    impl->skeleton = options.skeleton;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Resolver::AssetPathResolver::Options resolver;
    resolver.assetsRoot = Utf8Path(options.shaderBundle);
    auto bundle = impl->shaders.AppendBundle(source, Engine::Asset::Resolver::AssetPathResolver(resolver));
    if (!bundle) {
        return Engine::Base::Err(PreviewError("cannot load Engine's shader bundle " + resolver.assetsRoot,
                                              Engine::Base::CauseDetail(bundle.error())));
    }
    Render::Backend::SdlGpu::SdlGpuOffscreenOptions device;
    device.width = options.width;
    device.height = options.height;
    device.driver = options.driver;
    device.availableShaderFormats = impl->shaders.CompleteFormats();
    auto created = GpuDevice::CreateOffscreen(platform, device);
    if (!created) {
        return Engine::Base::Err(PreviewError("cannot create the offscreen GPU device", Engine::Base::CauseDetail(created.error())));
    }
    impl->device = std::move(created.value());
    auto initialized = impl->renderer.Initialize(*impl->device, impl->shaders);
    if (!initialized) {
        return Engine::Base::Err(PreviewError("cannot start Engine's renderer", Engine::Base::CauseDetail(initialized.error())));
    }
    const Render::MeshData cube = Render::MakeUnitCube();
    auto bone = impl->device->CreateMesh(cube.View());
    if (!bone) {
        return Engine::Base::Err(PreviewError("cannot create the bone mesh", Engine::Base::CauseDetail(bone.error())));
    }
    impl->bone = bone.value();
    return std::unique_ptr<PreviewRenderer>(new PreviewRenderer(std::move(impl)));
}

Result<void, ModelViewerError> PreviewRenderer::Show(std::shared_ptr<const Engine::Model::ModelAsset> model,
                                                     const Engine::Model::Pose& pose) {
    impl_->instance.reset();
    impl_->model.reset();
    if (!model) return {};
    if (!model->meshes.empty()) {
        std::vector<Engine::ModelRenderer::ModelMaterial> materials;
        materials.reserve(model->materials.size());
        for (const auto& material : model->materials) {
            const auto& c = material.baseColorLinear;
            materials.push_back({std::nullopt, {c[0], c[1], c[2], c[3]}, Render::SamplerMode::LinearClamp});
        }
        auto resource = Engine::ModelRenderer::ModelResource::Create(*impl_->device, model, materials);
        if (!resource) {
            return Engine::Base::Err(PreviewError("Engine cannot prepare the model for drawing",
                                                  Engine::Base::CauseDetail(resource.error())));
        }
        auto instance = Engine::ModelRenderer::ModelInstance::Create(std::move(resource.value()), pose);
        if (!instance) {
            return Engine::Base::Err(PreviewError("Engine cannot pose the model for drawing",
                                                  Engine::Base::CauseDetail(instance.error())));
        }
        impl_->instance = std::move(instance.value());
    }
    impl_->model = std::move(model);
    const OrbitCamera framed = FrameModel(*impl_->model, pose, {});
    impl_->boneThickness = std::max(framed.distance * 0.015F, 0.0005F);
    return {};
}

Result<void, ModelViewerError> PreviewRenderer::Resize(const std::uint32_t width, const std::uint32_t height) {
    auto resized = impl_->device->SetFrameSize(width, height);
    if (!resized) {
        return Engine::Base::Err(PreviewError("cannot resize the preview", Engine::Base::CauseDetail(resized.error())));
    }
    return {};
}

void PreviewRenderer::SetSkeleton(const bool shown) noexcept { impl_->skeleton = shown; }

Result<const Render::TextureReadback*, ModelViewerError> PreviewRenderer::Render(const Engine::Model::Pose& pose,
                                                                                 const OrbitCamera& camera) {
    Render::RenderQueue queue({{0.18F, 0.19F, 0.21F, 1.0F}, {}});
    queue.SetCamera(camera.Camera());
    if (impl_->instance) {
        auto updated = impl_->instance->UpdatePose(pose);
        if (!updated) return Engine::Base::Err(PreviewError("Engine cannot skin the pose", Engine::Base::CauseDetail(updated.error())));
        auto submitted = impl_->instance->Submit(queue, {});
        if (!submitted) return Engine::Base::Err(PreviewError("the model cannot be drawn", Engine::Base::CauseDetail(submitted.error())));
    }
    if (impl_->model && impl_->skeleton) {
        auto skeleton = impl_->SubmitSkeleton(queue, pose);
        if (!skeleton) return Engine::Base::Err(skeleton.error());
    }
    auto rendered = impl_->renderer.Render(queue);
    if (!rendered) return Engine::Base::Err(PreviewError("Engine's renderer cannot draw the frame", Engine::Base::CauseDetail(rendered.error())));
    auto read = impl_->device->ReadFrame();
    if (!read) return Engine::Base::Err(PreviewError("cannot read the frame back", Engine::Base::CauseDetail(read.error())));
    impl_->frame = std::move(read.value());
    return &impl_->frame;
}

GpuDevice& PreviewRenderer::Device() noexcept { return *impl_->device; }

Result<void, ModelViewerError> UploadFrame(Engine::Render::Backend::Sdl::SdlRenderer& renderer, PreviewTexture& texture,
                                           const Render::TextureReadback& frame) {
    if (texture.handle && (texture.width != frame.width || texture.height != frame.height)) {
        static_cast<void>(renderer.ReleaseTexture(texture.handle));
        texture = {};
    }
    if (!texture.handle) {
        auto created = renderer.CreateStreamingTexture(frame.width, frame.height);
        if (!created) return Engine::Base::Err(PreviewError("cannot create the preview texture", Engine::Base::CauseDetail(created.error())));
        texture = {created.value(), frame.width, frame.height};
    }
    auto uploaded = renderer.UploadTexture(texture.handle, frame.bytes, frame.rowPitch);
    if (!uploaded) return Engine::Base::Err(PreviewError("cannot upload the preview", Engine::Base::CauseDetail(uploaded.error())));
    return {};
}

} // namespace Gyo::Tools::ModelViewer
