#include "render/Renderer.hpp"
#include "render/ShaderAbi.hpp"
#include "render/ColorTransform.hpp"
#include "render/PrimitiveMesh.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <numbers>
#include <tuple>
#include <utility>
namespace Engine::Render {
namespace {
using namespace ShaderAbi;
[[nodiscard]] Matrix4 Identity() noexcept {
    Matrix4 result{};
    result.values[0][0] = 1.0F;
    result.values[1][1] = 1.0F;
    result.values[2][2] = 1.0F;
    result.values[3][3] = 1.0F;
    return result;
}

[[nodiscard]] Matrix4 Multiply(const Matrix4& left, const Matrix4& right) noexcept {
    Matrix4 result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result.values[row][column] +=
                    left.values[row][inner] * right.values[inner][column];
            }
        }
    }
    return result;
}

[[nodiscard]] Matrix4 Translation(Float3 value) noexcept {
    Matrix4 result = Identity();
    result.values[3][0] = value.x;
    result.values[3][1] = value.y;
    result.values[3][2] = value.z;
    return result;
}

[[nodiscard]] Matrix4 Scale(Float3 value) noexcept {
    Matrix4 result = Identity();
    result.values[0][0] = value.x;
    result.values[1][1] = value.y;
    result.values[2][2] = value.z;
    return result;
}

[[nodiscard]] Matrix4 RotationX(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[1][1] = cosine;
    result.values[1][2] = sine;
    result.values[2][1] = -sine;
    result.values[2][2] = cosine;
    return result;
}

[[nodiscard]] Matrix4 RotationY(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0][0] = cosine;
    result.values[0][2] = -sine;
    result.values[2][0] = sine;
    result.values[2][2] = cosine;
    return result;
}

[[nodiscard]] Matrix4 RotationZ(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0][0] = cosine;
    result.values[0][1] = sine;
    result.values[1][0] = -sine;
    result.values[1][1] = cosine;
    return result;
}

[[nodiscard]] Matrix4 WorldMatrix(const Transform3D& transform) noexcept {
    Matrix4 result = Scale(transform.scale);
    result = Multiply(result, RotationX(transform.rotationRadians.x));
    result = Multiply(result, RotationY(transform.rotationRadians.y));
    result = Multiply(result, RotationZ(transform.rotationRadians.z));
    return Multiply(result, Translation(transform.translation));
}

[[nodiscard]] Matrix4 ViewMatrix(const PerspectiveCamera3D& camera) noexcept {
    Matrix4 result = Translation({
        -camera.position.x,
        -camera.position.y,
        -camera.position.z,
    });
    result = Multiply(result, RotationZ(-camera.rotationRadians.z));
    result = Multiply(result, RotationY(-camera.rotationRadians.y));
    return Multiply(result, RotationX(-camera.rotationRadians.x));
}

[[nodiscard]] Matrix4 ProjectionMatrix(
    const PerspectiveCamera3D& camera,
    float aspectRatio) noexcept {
    Matrix4 result{};
    const float yScale = 1.0F /
                         std::tan(camera.verticalFieldOfViewRadians * 0.5F);
    const float xScale = yScale / aspectRatio;
    const float depthRange = camera.farClip - camera.nearClip;
    result.values[0][0] = xScale;
    result.values[1][1] = yScale;
    result.values[2][2] = camera.farClip / depthRange;
    result.values[2][3] = 1.0F;
    result.values[3][2] =
        -(camera.nearClip * camera.farClip) / depthRange;
    return result;
}

[[nodiscard]] Matrix4 SpriteWorldMatrix(
    const SpriteSubmission& sprite) noexcept {
    const Float3 localPivotTranslation{
        0.5F - sprite.pivotNormalized.x,
        0.5F - sprite.pivotNormalized.y,
        0.0F,
    };
    const Float3 anchor{
        sprite.destinationPixels.x +
            sprite.pivotNormalized.x * sprite.destinationPixels.width,
        sprite.destinationPixels.y +
            sprite.pivotNormalized.y * sprite.destinationPixels.height,
        0.0F,
    };

    Matrix4 result = Translation(localPivotTranslation);
    result = Multiply(result, Scale({
        sprite.destinationPixels.width,
        sprite.destinationPixels.height,
        1.0F,
    }));
    result = Multiply(result, RotationZ(sprite.rotationRadians));
    return Multiply(result, Translation(anchor));
}

[[nodiscard]] Matrix4 PixelProjection(float width, float height) noexcept {
    Matrix4 result{};
    result.values[0][0] = 2.0F / width;
    result.values[1][1] = -2.0F / height;
    result.values[2][2] = 1.0F;
    result.values[3][0] = -1.0F;
    result.values[3][1] = 1.0F;
    result.values[3][3] = 1.0F;
    return result;
}

[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

[[nodiscard]] bool IsFinite(Color value) noexcept {
    return IsFinite(value.red) && IsFinite(value.green) &&
           IsFinite(value.blue) && IsFinite(value.alpha);
}

[[nodiscard]] FragmentUniforms MakeFragmentUniforms(
    Color tint,
    UvTransform uv,
    float alphaCutoff) noexcept {
    FragmentUniforms uniforms{};
    uniforms.tint[0] = tint.red;
    uniforms.tint[1] = tint.green;
    uniforms.tint[2] = tint.blue;
    uniforms.tint[3] = tint.alpha;
    uniforms.uvScaleOffset[0] = uv.scale.x;
    uniforms.uvScaleOffset[1] = uv.scale.y;
    uniforms.uvScaleOffset[2] = uv.offset.x;
    uniforms.uvScaleOffset[3] = uv.offset.y;
    uniforms.alphaCutoff = alphaCutoff;
    return uniforms;
}

[[nodiscard]] float DecodeHalf(std::uint16_t bits) noexcept {
    const unsigned exponent = (bits >> 10U) & 31U;
    const unsigned fraction = bits & 1023U;
    float value;
    if (exponent == 0) {
        value = std::ldexp(static_cast<float>(fraction), -24);
    } else if (exponent == 31) {
        value = fraction == 0 ? std::numeric_limits<float>::infinity()
                              : std::numeric_limits<float>::quiet_NaN();
    } else {
        value = std::ldexp(static_cast<float>(1024U + fraction),
                           static_cast<int>(exponent) - 25);
    }
    return (bits & 0x8000U) != 0 ? -value : value;
}

[[nodiscard]] std::uint8_t CaptureComponent(float value, bool srgb) noexcept {
    if (std::isnan(value)) value = 0.0F;
    value = std::clamp(value, 0.0F, 1.0F);
    if (srgb) value = EncodeSrgbComponent(value);
    return static_cast<std::uint8_t>(std::lround(value * 255.0F));
}


RenderError Error(std::string message) {
    return RenderError::Make(RenderErrorCode::InvalidArgument, std::move(message));
}
template<class T> std::vector<std::byte> Bytes(const T& value) {
    auto bytes = std::as_bytes(std::span{&value, 1});
    return {bytes.begin(), bytes.end()};
}
} // namespace

struct Renderer::Impl final {
    IRenderDevice* device{};
    const ShaderLibrary* library{};
    ShaderFormat format{ShaderFormat::SPIRV};
    TextureHandle white{}, scene{}, depth{};
    MeshHandle quad{};
    std::uint32_t width{}, height{};
    using PipelineKey = std::tuple<std::string, TextureFormat, CullMode, bool, bool, bool, bool>;
    std::map<PipelineKey, PipelineHandle> pipelines;
    std::map<const ShaderArtifact*, ShaderHandle> shaders;
    bool captureRequested{};
    std::optional<SceneCapture> capture;

    void Reset() noexcept {
        if (device) {
            for (const auto& [key, pipeline] : pipelines) {
                static_cast<void>(key);
                static_cast<void>(device->ReleasePipeline(pipeline));
            }
            for (const auto& [artifact, shader] : shaders) {
                static_cast<void>(artifact); static_cast<void>(device->ReleaseShader(shader));
            }
            for (auto texture : {white, scene, depth}) if (texture)
                static_cast<void>(device->ReleaseTexture(texture));
            if (quad) static_cast<void>(device->ReleaseMesh(quad));
        }
        pipelines.clear(); shaders.clear(); white = {}; scene = {}; depth = {}; quad = {};
        width = height = 0; device = nullptr; library = nullptr;
        captureRequested = false; capture.reset();
    }
    Base::Result<PipelineHandle, RenderError> Pipeline(
        const std::string& shader, TextureFormat target, CullMode cull,
        bool blend, bool depthTest, bool depthWrite, bool vertexInput = true) {
        using Result = Base::Result<PipelineHandle, RenderError>;
        PipelineKey key{shader, target, cull, blend, depthTest, depthWrite, vertexInput};
        if (auto found = pipelines.find(key); found != pipelines.end()) return Result::Ok(found->second);
        auto program = library->FindProgram(shader, format);
        if (!program) return Result::Err(program.error());
        if (program.value().interfaceName != (vertexInput ? "unlit" : "scene_post"))
            return Result::Err(Error("Renderer: shader interface is incompatible with this draw: " + shader));
        auto createShader = [&](const std::shared_ptr<const ShaderArtifact>& artifact) -> Base::Result<ShaderHandle, RenderError> {
            if (auto found = shaders.find(artifact.get()); found != shaders.end())
                return Base::Result<ShaderHandle, RenderError>::Ok(found->second);
            auto created = device->CreateShader(*artifact);
            if (created) shaders.emplace(artifact.get(), created.value());
            return created;
        };
        auto vertex = createShader(program.value().vertex); if (!vertex) return Result::Err(vertex.error());
        auto fragment = createShader(program.value().fragment); if (!fragment) return Result::Err(fragment.error());
        auto pipeline = device->CreatePipeline({vertex.value(), fragment.value(), target, vertexInput,
            cull, blend, depthTest, depthWrite});
        if (!pipeline) return pipeline;
        pipelines.emplace(std::move(key), pipeline.value());
        return pipeline;
    }
    Base::Result<void, RenderError> Targets(const AcquiredFrame& frame) {
        using Result = Base::Result<void, RenderError>;
        if (scene && width == frame.width && height == frame.height) return Result::Ok();
        auto nextScene = device->CreateTexture({frame.width, frame.height,
            TextureFormat::Rgba16Float, true, true, false});
        if (!nextScene) return Result::Err(nextScene.error());
        auto nextDepth = device->CreateTexture({frame.width, frame.height,
            TextureFormat::Depth32Float, false, false, true});
        if (!nextDepth) {
            static_cast<void>(device->ReleaseTexture(nextScene.value()));
            return Result::Err(nextDepth.error());
        }
        if (scene) static_cast<void>(device->ReleaseTexture(scene));
        if (depth) static_cast<void>(device->ReleaseTexture(depth));
        scene = nextScene.value(); depth = nextDepth.value();
        width = frame.width; height = frame.height;
        return Result::Ok();
    }
    PreparedDraw Draw(const MaterialDesc& material, MeshHandle mesh, PipelineHandle pipeline,
                      const Matrix4& matrix, UvTransform uv, float cutoff) const {
        PreparedDraw draw;
        draw.pipeline = pipeline; draw.mesh = mesh;
        draw.fragmentTextures.push_back({material.texture ? material.texture : white, material.sampler});
        draw.vertexUniforms.push_back(Bytes(VertexUniforms{matrix}));
        draw.fragmentUniforms.push_back(Bytes(MakeFragmentUniforms(material.tint, uv, cutoff)));
        return draw;
    }
    Base::Result<void, RenderError> Meshes(PreparedPass& pass, const RenderQueue& queue,
        MeshLayer layer, const PerspectiveCamera3D& camera) {
        using Result = Base::Result<void, RenderError>;
        const Matrix4 viewProjection = Multiply(ViewMatrix(camera),
            ProjectionMatrix(camera, static_cast<float>(width) / static_cast<float>(height)));
        for (const auto& mesh : queue.Meshes()) {
            if (mesh.layer != layer) continue;
            const bool sky = mesh.surface == SurfaceMode::Sky;
            const bool masked = mesh.surface == SurfaceMode::AlphaMasked;
            const bool overlay = layer == MeshLayer::WorldOverlay;
            const CullMode cull = sky ? CullMode::Front : mesh.doubleSided ? CullMode::None : CullMode::Back;
            auto pipeline = Pipeline(mesh.material.shader, TextureFormat::Rgba16Float,
                cull, masked, !overlay, !overlay && !sky);
            if (!pipeline) return Result::Err(pipeline.error());
            pass.draws.push_back(Draw(mesh.material, mesh.mesh, pipeline.value(),
                Multiply(WorldMatrix(mesh.transform), viewProjection), mesh.uv, masked ? 0.01F : -1.0F));
        }
        return Result::Ok();
    }
    Base::Result<void, RenderError> Sprites(PreparedPass& pass, const RenderQueue& queue,
        CompositeLayer layer, TextureFormat target) {
        using Result = Base::Result<void, RenderError>;
        const Matrix4 projection = PixelProjection(static_cast<float>(width), static_cast<float>(height));
        for (const auto& sprite : queue.Sprites()) {
            if (sprite.layer != layer || sprite.destinationPixels.width == 0 ||
                sprite.destinationPixels.height == 0 || sprite.sourceUv.width == 0 || sprite.sourceUv.height == 0) continue;
            auto pipeline = Pipeline(sprite.material.shader, target, CullMode::None, true, false, false);
            if (!pipeline) return Result::Err(pipeline.error());
            pass.draws.push_back(Draw(sprite.material, quad, pipeline.value(),
                Multiply(SpriteWorldMatrix(sprite), projection), MakeSpriteUvTransform(sprite.sourceUv), -1.0F));
        }
        return Result::Ok();
    }
    Base::Result<void, RenderError> Capture() {
        using Result = Base::Result<void, RenderError>;
        auto read = device->ReadTexture(scene);
        if (!read) return Result::Err(read.error());
        const auto& raw = read.value();
        if (raw.format != TextureFormat::Rgba16Float || raw.rowPitch < raw.width * 8U ||
            raw.bytes.size() < static_cast<std::size_t>(raw.rowPitch) * raw.height)
            return Result::Err(Error("Renderer: invalid scene readback format or size"));
        SceneCapture result{raw.width, raw.height, {}};
        result.rgba8.resize(static_cast<std::size_t>(raw.width) * raw.height * 4U);
        for (std::uint32_t y = 0; y < raw.height; ++y) {
            for (std::uint32_t x = 0; x < raw.width; ++x) {
                std::array<std::uint16_t, 4> channels{};
                std::memcpy(channels.data(), raw.bytes.data() + y * raw.rowPitch + x * 8U, 8);
                for (std::size_t c = 0; c < 4; ++c)
                    result.rgba8[(static_cast<std::size_t>(y) * raw.width + x) * 4U + c] =
                        CaptureComponent(DecodeHalf(channels[c]), c < 3);
            }
        }
        capture = std::move(result);
        return Result::Ok();
    }
};

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() { Reset(); }
void Renderer::Reset() noexcept { impl_->Reset(); }
std::optional<ShaderFormat> Renderer::ActiveShaderFormat() const noexcept {
    return impl_->device ? std::optional{impl_->format} : std::nullopt;
}

Base::Result<void, RenderError> Renderer::Initialize(IRenderDevice& device, const ShaderLibrary& library) {
    using Result = Base::Result<void, RenderError>;
    Reset();
    const auto formats = device.GetInfo().shaderFormats & library.CompleteFormats();
    if (!formats) return Result::Err(Error("Renderer: device and complete shader bundle share no shader format"));
    ShaderFormat selected = ShaderFormat::Metallib;
    for (auto candidate : {ShaderFormat::DXIL, ShaderFormat::SPIRV, ShaderFormat::Metallib})
        if (formats & FormatBit(candidate)) { selected = candidate; break; }
    for (const auto& id : {"builtin/unlit", "builtin/scene_post"}) {
        auto program = library.FindProgram(id, selected);
        if (!program) return Result::Err(program.error());
    }
    impl_->device = &device; impl_->library = &library; impl_->format = selected;
    const std::array<std::uint8_t, 4> pixels{255,255,255,255};
    auto white = device.CreateTexture(ImageView{1,1,4,std::as_bytes(std::span{pixels}),TextureColorSpace::Linear});
    if (!white) { Reset(); return Result::Err(white.error()); }
    impl_->white = white.value();
    auto quad = device.CreateMesh(MakeUnitQuadXY().View());
    if (!quad) { Reset(); return Result::Err(quad.error()); }
    impl_->quad = quad.value();
    return Result::Ok();
}

Base::Result<PresentStatus, RenderError> Renderer::Render(const RenderQueue& queue) {
    using Result = Base::Result<PresentStatus, RenderError>;
    auto& state = *impl_;
    if (!state.device) return Result::Err(Error("Renderer: not initialized"));
    if (!IsFinite(queue.Frame().clearColor) || !IsValidSceneColorTransform(queue.Frame().sceneColorTransform))
        return Result::Err(Error("Renderer: invalid frame description"));
    // Revalidate cameras in case a caller changed them after submissions.
    RenderQueue validation(queue.Frame());
    if (queue.Camera()) validation.SetCamera(*queue.Camera());
    if (queue.ViewModelCamera()) validation.SetViewModelCamera(*queue.ViewModelCamera());
    for (const auto& mesh : queue.Meshes()) { auto r = validation.Submit(mesh); if (!r) return Result::Err(r.error()); }
    for (const auto& sprite : queue.Sprites()) { auto r = validation.Submit(sprite); if (!r) return Result::Err(r.error()); }
    auto acquired = state.device->AcquireFrame();
    if (!acquired) return Result::Err(acquired.error());
    if (!acquired.value()) return Result::Ok(PresentStatus::Skipped);
    const AcquiredFrame frame = *acquired.value();
    struct Guard { IRenderDevice& device; AcquiredFrame frame; bool active{true};
        ~Guard() { if (active) device.AbandonFrame(frame); } } guard{*state.device, frame};
    auto targets = state.Targets(frame);
    if (!targets) return Result::Err(targets.error());
    PreparedFrame prepared;
    PreparedPass world;
    world.color = state.scene; world.depth = state.depth;
    world.colorLoad = AttachmentLoad::Clear; world.clearColor = queue.Frame().clearColor;
    if (queue.Camera()) {
        auto r = state.Meshes(world, queue, MeshLayer::World, *queue.Camera());
        if (!r) return Result::Err(r.error());
    }
    prepared.passes.push_back(std::move(world));
    PreparedPass sceneSprites; sceneSprites.color = state.scene;
    auto r = state.Sprites(sceneSprites, queue, CompositeLayer::Scene, TextureFormat::Rgba16Float);
    if (!r) return Result::Err(r.error());
    if (!sceneSprites.draws.empty()) prepared.passes.push_back(std::move(sceneSprites));
    if (std::any_of(queue.Meshes().begin(), queue.Meshes().end(), [](const auto& m) { return m.layer == MeshLayer::WorldOverlay; })) {
        PreparedPass worldOverlay; worldOverlay.color = state.scene;
        r = state.Meshes(worldOverlay, queue, MeshLayer::WorldOverlay, *queue.Camera());
        if (!r) return Result::Err(r.error());
        prepared.passes.push_back(std::move(worldOverlay));
    }
    if (std::any_of(queue.Meshes().begin(), queue.Meshes().end(), [](const auto& m) { return m.layer == MeshLayer::ViewModel; })) {
        PreparedPass viewmodel; viewmodel.color = state.scene; viewmodel.depth = state.depth;
        r = state.Meshes(viewmodel, queue, MeshLayer::ViewModel, *queue.ViewModelCamera());
        if (!r) return Result::Err(r.error());
        prepared.passes.push_back(std::move(viewmodel));
    }
    PreparedPass post; post.colorLoad = AttachmentLoad::DontCare;
    auto postPipeline = state.Pipeline("builtin/scene_post", frame.colorFormat, CullMode::None, false, false, false, false);
    if (!postPipeline) return Result::Err(postPipeline.error());
    PreparedDraw postDraw; postDraw.pipeline = postPipeline.value();
    postDraw.fragmentTextures.push_back({state.scene, SamplerMode::LinearClamp});
    const auto color = queue.Frame().sceneColorTransform;
    postDraw.fragmentUniforms.push_back(Bytes(SceneColorUniforms{color.exposureEv, color.gammaAdjustment,{}}));
    post.draws.push_back(std::move(postDraw)); prepared.passes.push_back(std::move(post));
    PreparedPass overlay;
    r = state.Sprites(overlay, queue, CompositeLayer::Overlay, frame.colorFormat);
    if (!r) return Result::Err(r.error());
    if (!overlay.draws.empty()) prepared.passes.push_back(std::move(overlay));
    guard.active = false;
    auto submitted = state.device->SubmitFrame(frame, prepared);
    if (!submitted) return submitted;
    if (submitted.value() == PresentStatus::Presented && state.captureRequested) {
        state.captureRequested = false;
        auto capture = state.Capture();
        if (!capture) return Result::Err(capture.error());
    }
    return submitted;
}
void Renderer::RequestSceneCapture() noexcept { impl_->captureRequested = true; impl_->capture.reset(); }
std::optional<SceneCapture> Renderer::TakeSceneCapture() { return std::exchange(impl_->capture, std::nullopt); }
} // namespace Engine::Render
