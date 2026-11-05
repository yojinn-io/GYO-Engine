#include "gyo/model_viewer/PreviewRenderer.hpp"

#include "engine/base/Error.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/render/PrimitiveMesh.hpp"
#include "engine/render/RenderQueue.hpp"
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

namespace {

ModelViewerError PreviewError(std::string message, std::string detail = {}) {
    return ModelViewerError::Make(ModelViewerErrorCode::PreviewFailed, std::move(message), std::move(detail));
}

Math::Vec3 Translation(const Math::Matrix4& matrix) noexcept { return Math::TransformPoint(matrix, {}); }

// The preview's background, linear.
constexpr Render::Color kBackground{0.18F, 0.19F, 0.21F, 1.0F};

} // namespace

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
    std::unique_ptr<Framework::OffscreenPreview> preview;
    std::shared_ptr<const Engine::Model::ModelAsset> model;
    std::unique_ptr<Engine::ModelRenderer::ModelInstance> instance;
    Render::MeshHandle bone{};
    bool skeleton{true};
    float boneThickness{0.01F};

    ~Impl() {
        // Model resources release device objects before the preview's device goes.
        instance.reset();
        if (preview && bone) static_cast<void>(preview->Device().ReleaseMesh(bone));
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
    Framework::PreviewOptions preview;
    preview.shaderBundle = options.shaderBundle;
    preview.width = options.width;
    preview.height = options.height;
    preview.driver = options.driver;
    auto created = Framework::OffscreenPreview::Create(platform, preview);
    if (!created) {
        return Engine::Base::Err(PreviewError("the preview cannot start", Engine::Base::CauseDetail(created.error())));
    }
    impl->preview = std::move(created.value());
    const Render::MeshData cube = Render::MakeUnitCube();
    auto bone = impl->preview->Device().CreateMesh(cube.View());
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
        auto resource = Engine::ModelRenderer::ModelResource::Create(impl_->preview->Device(), model, materials);
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

void PreviewRenderer::SetSkeleton(const bool shown) noexcept { impl_->skeleton = shown; }

Result<Render::TextureReadback, ModelViewerError> PreviewRenderer::Render(const Engine::Model::Pose& pose,
                                                                          const OrbitCamera& camera,
                                                                          const std::uint32_t width,
                                                                          const std::uint32_t height) {
    if (impl_->instance) {
        auto updated = impl_->instance->UpdatePose(pose);
        if (!updated) return Engine::Base::Err(PreviewError("Engine cannot skin the pose", Engine::Base::CauseDetail(updated.error())));
    }
    auto queue = impl_->preview->BeginFrame(width, height, camera.Camera(), kBackground);
    if (!queue) return Engine::Base::Err(PreviewError("the preview cannot start the frame", Engine::Base::CauseDetail(queue.error())));
    // From here the frame is open: every path ends it, so that a failure leaves the preview ready for the next frame.
    const auto fail = [&](std::string message, std::string detail) {
        static_cast<void>(impl_->preview->EndFrame());
        return Engine::Base::Err(PreviewError(std::move(message), std::move(detail)));
    };
    if (impl_->instance) {
        auto submitted = impl_->instance->Submit(*queue.value(), {});
        if (!submitted) return fail("the model cannot be drawn", Engine::Base::CauseDetail(submitted.error()));
    }
    if (impl_->model && impl_->skeleton) {
        auto skeleton = impl_->SubmitSkeleton(*queue.value(), pose);
        if (!skeleton) return fail(skeleton.error().message, skeleton.error().detail);
    }
    auto frame = impl_->preview->EndFrame();
    if (!frame) return Engine::Base::Err(PreviewError("the preview cannot draw the frame", Engine::Base::CauseDetail(frame.error())));
    return std::move(frame.value());
}

Framework::OffscreenPreview& PreviewRenderer::Preview() noexcept { return *impl_->preview; }

} // namespace Gyo::Tools::ModelViewer
