#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <array>
#include <vector>
#include <string>
#include <numbers>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include "engine/base/Sha256.hpp"
#include "render/Renderer.hpp"
#include "render/ShaderAbi.hpp"

namespace {
using namespace Engine;
using namespace Engine::Render;
using Json = nlohmann::json;
template<class T> using R = Base::Result<T, RenderError>;
using Bytes = Asset::Loading::ByteBuffer;
Bytes ByteString(std::string_view text) {
    Bytes bytes(text.size()); std::memcpy(bytes.data(), text.data(), text.size()); return bytes;
}
struct Source final : Asset::Loading::IAssetSource {
    std::map<std::string, Bytes, std::less<>> files;
    Base::Result<Bytes, Asset::AssetError> ReadAll(std::string_view path) override {
        return files.at(std::string(path));
    }
};
ShaderLibrary Library(bool post = true) {
    Source source;
    const Bytes vertex = ByteString("vertex"), fragment = ByteString("fragment");
    source.files["bundle/v.bin"] = vertex; source.files["bundle/f.bin"] = fragment;
    Json programs = Json::array();
    for (std::string id : {"builtin/unlit", "builtin/scene_post", "game/test_tint"}) {
        const bool isPost = id == "builtin/scene_post";
        if (isPost && !post) continue;
        programs.push_back({{"id", id}, {"interface", isPost ? "scene_post" : "unlit"},
            {"variants", Json::array({{{"format", "spirv"},
                {"vertex", {{"file", "v.bin"}, {"entrypoint", "main"}, {"samplers", 0},
                    {"uniform_sizes", isPost ? Json::array() : Json::array({64})}, {"sha256", Base::Sha256(vertex)}}},
                {"fragment", {{"file", "f.bin"}, {"entrypoint", "main"}, {"samplers", 1},
                    {"uniform_sizes", Json::array({isPost ? 16 : 48})}, {"sha256", Base::Sha256(fragment)}}}}})}});
    }
    source.files["bundle/manifest.json"] = ByteString(Json{{"version",1},{"abi","gyo.raster.v1"},{"programs",programs}}.dump());
    ShaderLibrary library;
    REQUIRE(library.AppendBundle(source, Asset::Resolver::AssetPathResolver(
        Asset::Resolver::AssetPathResolver::Options{"bundle"})));
    return library;
}
struct Device final : IRenderDevice {
    std::uint32_t serial{1};
    unsigned acquisitions{}, submissions{}, abandons{}, reads{}, pipelineCreates{}, shaderCreates{};
    bool skip{}, failSubmit{}, failTargets{};
    std::uint32_t width{100}, height{80};
    std::unordered_set<MeshHandle> meshes;
    std::unordered_map<TextureHandle, TextureDesc> textures;
    std::unordered_map<ShaderHandle, ShaderArtifact> shaders;
    std::unordered_map<PipelineHandle, PipelineDesc> pipelines;
    PreparedFrame captured;
    RenderDeviceInfo GetInfo() const override { return {FormatBit(ShaderFormat::SPIRV), "mock"}; }
    R<MeshHandle> CreateMesh(const MeshView&) override {
        auto handle = MeshHandle::FromParts(serial++,1); meshes.insert(handle); return handle;
    }
    R<void> ReleaseMesh(MeshHandle handle) override { meshes.erase(handle); return {}; }
    R<TextureHandle> CreateTexture(const ImageView& image) override {
        auto handle = TextureHandle::FromParts(serial++,1);
        textures.emplace(handle, TextureDesc{image.width,image.height}); return handle;
    }
    R<TextureHandle> CreateTexture(const TextureDesc& desc) override {
        if (failTargets) return Engine::Base::Err(RenderError::Make(RenderErrorCode::ResourceCreationFailed,"injected target failure"));
        auto handle = TextureHandle::FromParts(serial++,1); textures.emplace(handle,desc); return handle;
    }
    R<void> ReleaseTexture(TextureHandle handle) override { textures.erase(handle); return {}; }
    R<ShaderHandle> CreateShader(const ShaderArtifact& artifact) override {
        auto handle = ShaderHandle::FromParts(serial++,1); shaders.emplace(handle,artifact); ++shaderCreates; return handle;
    }
    R<void> ReleaseShader(ShaderHandle handle) override { shaders.erase(handle); return {}; }
    R<PipelineHandle> CreatePipeline(const PipelineDesc& desc) override {
        auto handle = PipelineHandle::FromParts(serial++,1); pipelines.emplace(handle,desc); ++pipelineCreates; return handle;
    }
    R<void> ReleasePipeline(PipelineHandle handle) override { pipelines.erase(handle); return {}; }
    R<std::optional<AcquiredFrame>> AcquireFrame() override {
        ++acquisitions;
        if (skip) return std::nullopt;
        return AcquiredFrame{acquisitions,width,height,TextureFormat::Bgra8SRgb};
    }
    R<PresentStatus> SubmitFrame(const AcquiredFrame&, const PreparedFrame& frame) override {
        ++submissions; captured = frame;
        if (failSubmit) return Engine::Base::Err(RenderError::Make(RenderErrorCode::SubmissionFailed,"injected submit failure"));
        return PresentStatus::Presented;
    }
    void AbandonFrame(const AcquiredFrame&) noexcept override { ++abandons; }
    R<TextureReadback> ReadTexture(TextureHandle handle) override {
        ++reads; const auto& desc = textures.at(handle);
        TextureReadback result{desc.width, desc.height, desc.width*8+8, TextureFormat::Rgba16Float,{}};
        result.bytes.resize(static_cast<std::size_t>(result.rowPitch)*desc.height);
        const std::array<std::uint16_t,4> half{0x3800,0,0,0x3c00};
        for (unsigned y=0;y<desc.height;++y) for (unsigned x=0;x<desc.width;++x)
            std::memcpy(result.bytes.data()+y*result.rowPitch+x*8,half.data(),8);
        return std::move(result);
    }
};
template<class T> T Uniform(const std::vector<std::byte>& bytes) {
    REQUIRE(bytes.size() == sizeof(T)); T value{}; std::memcpy(&value,bytes.data(),sizeof(T)); return value;
}
RenderQueue AllLayers() {
    RenderQueue queue({{0.1F,0.2F,0.3F,1},{2,1.5F}});
    queue.SetCamera({{0,1,0}}); queue.SetViewModelCamera({{}, {},0.8F});
    MeshSubmission mesh; mesh.mesh = MeshHandle::FromParts(99,1); mesh.transform.translation = {0,2,3};
    mesh.material.tint = {0.25F,0.5F,1,1}; REQUIRE(queue.Submit(mesh));
    mesh.layer = MeshLayer::ViewModel; REQUIRE(queue.Submit(mesh));
    SpriteSubmission sprite; sprite.destinationPixels = {5,10,20,30}; sprite.sourceUv={0.25F,0.125F,0.5F,0.25F};
    sprite.layer=CompositeLayer::Scene; REQUIRE(queue.Submit(sprite));
    sprite.layer=CompositeLayer::Overlay; REQUIRE(queue.Submit(sprite));
    return queue;
}
TEST_CASE("Renderer owns fixed passes, uniform ABI and independent viewmodel depth") {
    auto library = Library(); Device device; Renderer renderer;
    CHECK_FALSE(renderer.ActiveShaderFormat());
    REQUIRE(renderer.Initialize(device,library));
    CHECK(renderer.ActiveShaderFormat()==ShaderFormat::SPIRV);
    auto queue = AllLayers(); REQUIRE(renderer.Render(queue));
    const auto& passes=device.captured.passes; REQUIRE(passes.size()==5);
    CHECK(passes[0].colorLoad==AttachmentLoad::Clear); CHECK(passes[0].depth);
    CHECK(passes[0].clearColor.red==doctest::Approx(0.1F));
    CHECK(passes[1].color==passes[0].color); CHECK_FALSE(passes[1].depth);
    CHECK(passes[2].color==passes[0].color); CHECK(passes[2].depth==passes[0].depth);
    CHECK(passes[2].depthLoad==AttachmentLoad::Clear); CHECK(passes[2].colorLoad==AttachmentLoad::Load);
    CHECK_FALSE(passes[3].color); CHECK_FALSE(passes[3].depth); CHECK(passes[3].draws[0].vertexUniforms.empty());
    CHECK_FALSE(passes[4].color); CHECK_FALSE(passes[4].depth); CHECK(passes[4].colorLoad==AttachmentLoad::Load);
    const auto world=Uniform<ShaderAbi::VertexUniforms>(passes[0].draws[0].vertexUniforms[0]);
    const auto weapon=Uniform<ShaderAbi::VertexUniforms>(passes[2].draws[0].vertexUniforms[0]);
    CHECK(world.worldViewProjection.values[3][3]==doctest::Approx(3));
    CHECK(world.worldViewProjection.values[3][1]==doctest::Approx(1.7320508F));
    CHECK(weapon.worldViewProjection.values[0][0]!=doctest::Approx(world.worldViewProjection.values[0][0]));
    const auto fragment=Uniform<ShaderAbi::FragmentUniforms>(passes[0].draws[0].fragmentUniforms[0]);
    CHECK(fragment.tint[0]==doctest::Approx(0.25F)); CHECK(fragment.alphaCutoff==-1);
    const auto sprite=Uniform<ShaderAbi::FragmentUniforms>(passes[1].draws[0].fragmentUniforms[0]);
    CHECK(sprite.uvScaleOffset[1]==doctest::Approx(-0.25F)); CHECK(sprite.uvScaleOffset[3]==doctest::Approx(0.375F));
    const auto post=Uniform<ShaderAbi::SceneColorUniforms>(passes[3].draws[0].fragmentUniforms[0]);
    CHECK(post.exposureEv==2); CHECK(post.gammaAdjustment==1.5F);
    const auto count=device.pipelineCreates; const auto shaderCount=device.shaderCreates;
    REQUIRE(renderer.Render(queue)); CHECK(device.pipelineCreates==count); CHECK(device.shaderCreates==shaderCount);
    CHECK(device.textures.size()==3); CHECK(device.reads==0);
    renderer.Reset(); CHECK(device.textures.empty()); CHECK(device.meshes.empty()); CHECK(device.pipelines.empty()); CHECK(device.shaders.empty());
    CHECK_FALSE(renderer.ActiveShaderFormat());
}
TEST_CASE("Renderer skips minimized frames and captures only requested successful scene frames") {
    auto library=Library(); Device device; Renderer renderer; REQUIRE(renderer.Initialize(device,library));
    renderer.RequestSceneCapture(); device.skip=true;
    auto result=renderer.Render(RenderQueue{}); REQUIRE(result); CHECK(result.value()==PresentStatus::Skipped);
    CHECK(device.textures.size()==1); CHECK(device.submissions==0); CHECK(device.reads==0); CHECK_FALSE(renderer.TakeSceneCapture());
    device.skip=false; REQUIRE(renderer.Render(RenderQueue{})); CHECK(device.reads==1);
    auto capture=renderer.TakeSceneCapture(); REQUIRE(capture); CHECK(capture->width==100); CHECK(capture->rgba8[0]==188); CHECK(capture->rgba8[3]==255);
    CHECK_FALSE(renderer.TakeSceneCapture()); REQUIRE(renderer.Render(RenderQueue{})); CHECK(device.reads==1);
}
TEST_CASE("World overlay uses world camera without depth between Scene and ViewModel") {
    auto library = Library(); Device device; Renderer renderer;
    REQUIRE(renderer.Initialize(device, library));
    auto queue = AllLayers();
    MeshSubmission debug = queue.Meshes().front();
    debug.layer = MeshLayer::WorldOverlay;
    REQUIRE(queue.Submit(debug));
    REQUIRE(renderer.Render(queue));
    const auto& passes = device.captured.passes;
    REQUIRE(passes.size() == 6);
    REQUIRE(passes[2].draws.size() == 1);
    CHECK(passes[2].color == passes[0].color);
    CHECK(passes[2].colorLoad == AttachmentLoad::Load);
    CHECK_FALSE(passes[2].depth);
    const auto& pipeline = device.pipelines.at(passes[2].draws.front().pipeline);
    CHECK_FALSE(pipeline.depthTest);
    CHECK_FALSE(pipeline.depthWrite);
    CHECK(passes[2].draws.front().vertexUniforms == passes[0].draws.front().vertexUniforms);
    CHECK(passes[3].depth == passes[0].depth);
    CHECK(passes[3].depthLoad == AttachmentLoad::Clear);
    CHECK_FALSE(passes[5].color); // Overlay UI is still the final swapchain pass.

    auto normal = AllLayers();
    REQUIRE(renderer.Render(normal));
    CHECK(device.captured.passes.size() == 5); // No empty debug pass when disabled.
}
TEST_CASE("Renderer abandons acquired frames on preparation failure and consumes failed submissions once") {
    auto library=Library(); Device device; Renderer renderer; REQUIRE(renderer.Initialize(device,library));
    device.failTargets=true; CHECK_FALSE(renderer.Render(RenderQueue{})); CHECK(device.abandons==1); CHECK(device.submissions==0);
    device.failTargets=false;
    RenderQueue queue; SpriteSubmission sprite; sprite.destinationPixels={0,0,10,10}; sprite.material.shader="game/missing"; REQUIRE(queue.Submit(sprite));
    CHECK_FALSE(renderer.Render(queue)); CHECK(device.abandons==2);
    device.failSubmit=true; CHECK_FALSE(renderer.Render(RenderQueue{})); CHECK(device.abandons==2); CHECK(device.submissions==1);
    device.failSubmit=false; REQUIRE(renderer.Render(RenderQueue{})); CHECK(device.submissions==2);
}
TEST_CASE("Renderer selects game material program and rejects incompatible shader interfaces") {
    auto library=Library(); Device device; Renderer renderer; REQUIRE(renderer.Initialize(device,library));
    RenderQueue queue; SpriteSubmission sprite; sprite.destinationPixels={0,0,10,10}; sprite.material.shader="game/test_tint"; REQUIRE(queue.Submit(sprite));
    REQUIRE(renderer.Render(queue)); CHECK(device.shaders.size()==4);
    queue.Reset(); sprite.material.shader="builtin/scene_post"; REQUIRE(queue.Submit(sprite));
    CHECK_FALSE(renderer.Render(queue)); CHECK(device.abandons==1);
}
TEST_CASE("Renderer resize replaces targets without replacing cached shader and pipeline resources") {
    auto library=Library(); Device device; Renderer renderer; REQUIRE(renderer.Initialize(device,library));
    REQUIRE(renderer.Render(RenderQueue{})); const auto old=device.captured.passes[0].color; const auto count=device.pipelineCreates;
    device.width=200; REQUIRE(renderer.Render(RenderQueue{})); CHECK(device.captured.passes[0].color!=old);
    CHECK_FALSE(device.textures.contains(old)); CHECK(device.textures.size()==3); CHECK(device.pipelineCreates==count);
}
TEST_CASE("Renderer requires complete builtins before allocating resources") {
    auto library=Library(false); Device device; Renderer renderer;
    CHECK_FALSE(renderer.Initialize(device,library)); CHECK(device.textures.empty()); CHECK(device.meshes.empty());
}

// Characterization of the uploaded world-view-projection uniforms. The renderer
// moved from its own row-major, row-vector matrix code to Engine::Math's
// column-major, column-vector convention; the 16 floats sent to the GPU must
// stay byte-identical. Namespace Legacy freezes the former code verbatim
// (engine/render/src/Renderer.cpp:18-158 and the WVP composition at :305-331
// at ed7a08a, the master before B4b; Matrix4 mirrors ShaderAbi.hpp:5) as the
// reference. Inputs come from a runtime seed so no expression is constant-folded.
namespace Legacy {

struct Matrix4 final {
    float values[4][4]{};
};

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

[[nodiscard]] Matrix4 Translation(Math::Vec3 value) noexcept {
    Matrix4 result = Identity();
    result.values[3][0] = value.x;
    result.values[3][1] = value.y;
    result.values[3][2] = value.z;
    return result;
}

[[nodiscard]] Matrix4 Scale(Math::Vec3 value) noexcept {
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
    const Math::Vec3 localPivotTranslation{
        0.5F - sprite.pivotNormalized.x,
        0.5F - sprite.pivotNormalized.y,
        0.0F,
    };
    const Math::Vec3 anchor{
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

// Renderer.cpp:305-306 and :317 at master: World * (View * Projection).
[[nodiscard]] Matrix4 MeshUniform(const Transform3D& transform, const PerspectiveCamera3D& camera,
                                  float width, float height) noexcept {
    const Matrix4 viewProjection = Multiply(ViewMatrix(camera), ProjectionMatrix(camera, width / height));
    return Multiply(WorldMatrix(transform), viewProjection);
}

// Renderer.cpp:324 and :331 at master: SpriteWorld * PixelProjection.
[[nodiscard]] Matrix4 SpriteUniform(const SpriteSubmission& sprite, float width, float height) noexcept {
    return Multiply(SpriteWorldMatrix(sprite), PixelProjection(width, height));
}

} // namespace Legacy

static_assert(sizeof(Legacy::Matrix4) == sizeof(ShaderAbi::VertexUniforms));

// Runtime seed: a volatile read keeps every generated input opaque to the optimizer.
volatile std::uint32_t gSeed = 0xB4B5EEDU;

class Lcg final {
public:
    explicit Lcg(std::uint32_t seed) : state_(seed) {}
    float Range(float lo, float hi) {
        state_ = state_ * 1664525U + 1013904223U;
        const float unit = static_cast<float>(state_ >> 8U) / 16777216.0F;
        return lo + (hi - lo) * unit;
    }
    std::uint32_t Next() {
        state_ = state_ * 1664525U + 1013904223U;
        return state_;
    }

private:
    std::uint32_t state_;
};

float Angle(Lcg& random) {
    // Mix ordinary angles with exact multiples of pi/2, pi and large values.
    switch (random.Next() % 6U) {
    case 0: return 0.0F;
    case 1: return std::numbers::pi_v<float> * static_cast<float>(static_cast<int>(random.Next() % 5U) - 2) * 0.5F;
    case 2: return random.Range(-100.0F, 100.0F);
    default: return random.Range(-7.0F, 7.0F);
    }
}

Math::Vec3 Vector(Lcg& random, float lo, float hi) {
    const float x = random.Range(lo, hi);
    const float y = random.Range(lo, hi);
    const float z = random.Range(lo, hi);
    return {x, y, z};
}

Transform3D RandomTransform(Lcg& random) {
    Transform3D transform;
    transform.translation = Vector(random, -50.0F, 50.0F);
    const float rx = Angle(random);
    const float ry = Angle(random);
    const float rz = Angle(random);
    transform.rotationRadians = {rx, ry, rz};
    transform.scale = Vector(random, 0.01F, 5.0F);
    if (random.Next() % 4U == 0) transform.scale.x = -transform.scale.x;
    return transform;
}

PerspectiveCamera3D RandomCamera(Lcg& random) {
    PerspectiveCamera3D camera;
    camera.position = Vector(random, -20.0F, 20.0F);
    const float rx = Angle(random);
    const float ry = Angle(random);
    const float rz = Angle(random);
    camera.rotationRadians = {rx, ry, rz};
    camera.verticalFieldOfViewRadians = random.Range(0.2F, 2.8F);
    camera.nearClip = random.Next() % 3U == 0 ? 1.0e-4F : random.Range(0.01F, 1.0F);
    camera.farClip = camera.nearClip + random.Range(1.0F, 5000.0F);
    return camera;
}

SpriteSubmission RandomSprite(Lcg& random, CompositeLayer layer) {
    SpriteSubmission sprite;
    const float x = random.Range(-500.0F, 2000.0F);
    const float y = random.Range(-500.0F, 2000.0F);
    const float width = random.Range(0.5F, 900.0F);
    const float height = random.Range(0.5F, 900.0F);
    sprite.destinationPixels = {x, y, width, height};
    const float pivotX = random.Next() % 3U == 0 ? 0.5F : random.Range(-0.5F, 1.5F);
    const float pivotY = random.Next() % 3U == 0 ? 0.5F : random.Range(-0.5F, 1.5F);
    sprite.pivotNormalized = {pivotX, pivotY};
    sprite.rotationRadians = Angle(random);
    sprite.layer = layer;
    return sprite;
}

// Vertex uniform blocks of every draw, in pass and draw order.
std::vector<std::vector<std::byte>> UploadedMatrices(const PreparedFrame& frame) {
    std::vector<std::vector<std::byte>> result;
    for (const auto& pass : frame.passes) {
        for (const auto& draw : pass.draws) {
            if (!draw.vertexUniforms.empty()) result.push_back(draw.vertexUniforms[0]);
        }
    }
    return result;
}

void CheckSameBytes(const std::vector<std::byte>& uploaded, const Legacy::Matrix4& expected, std::size_t index) {
    REQUIRE(uploaded.size() == sizeof(expected));
    if (std::memcmp(uploaded.data(), &expected, sizeof(expected)) != 0) {
        std::array<float, 16> actual{};
        std::memcpy(actual.data(), uploaded.data(), sizeof(actual));
        for (std::size_t element = 0; element < 16; ++element) {
            CAPTURE(index);
            CAPTURE(element);
            CHECK(std::memcmp(&actual[element], &expected.values[element / 4][element % 4], sizeof(float)) == 0);
        }
    }
}

void CheckMeshLayer(MeshLayer layer, std::uint32_t seedOffset) {
    auto library = Library();
    Device device;
    Renderer renderer;
    REQUIRE(renderer.Initialize(device, library));
    Lcg random{gSeed + seedOffset};
    for (int frame = 0; frame < 40; ++frame) {
        device.width = 16U + random.Next() % 2400U;
        device.height = 16U + random.Next() % 1600U;
        const PerspectiveCamera3D camera = RandomCamera(random);
        const PerspectiveCamera3D viewModelCamera = RandomCamera(random);
        RenderQueue queue;
        queue.SetCamera(camera);
        queue.SetViewModelCamera(viewModelCamera);
        std::vector<Transform3D> transforms;
        for (int index = 0; index < 50; ++index) {
            MeshSubmission mesh;
            mesh.mesh = MeshHandle::FromParts(99, 1);
            mesh.transform = RandomTransform(random);
            mesh.layer = layer;
            REQUIRE(queue.Submit(mesh));
            transforms.push_back(mesh.transform);
        }
        REQUIRE(renderer.Render(queue));
        const auto uploaded = UploadedMatrices(device.captured);
        REQUIRE(uploaded.size() == transforms.size());
        const PerspectiveCamera3D& used = layer == MeshLayer::ViewModel ? viewModelCamera : camera;
        for (std::size_t index = 0; index < transforms.size(); ++index) {
            CheckSameBytes(uploaded[index],
                Legacy::MeshUniform(transforms[index], used, static_cast<float>(device.width),
                                    static_cast<float>(device.height)),
                index);
        }
    }
}

void CheckSpriteLayer(CompositeLayer layer, std::uint32_t seedOffset) {
    auto library = Library();
    Device device;
    Renderer renderer;
    REQUIRE(renderer.Initialize(device, library));
    Lcg random{gSeed + seedOffset};
    for (int frame = 0; frame < 40; ++frame) {
        device.width = 16U + random.Next() % 2400U;
        device.height = 16U + random.Next() % 1600U;
        RenderQueue queue;
        std::vector<SpriteSubmission> sprites;
        for (int index = 0; index < 50; ++index) {
            const SpriteSubmission sprite = RandomSprite(random, layer);
            REQUIRE(queue.Submit(sprite));
            sprites.push_back(sprite);
        }
        REQUIRE(renderer.Render(queue));
        const auto uploaded = UploadedMatrices(device.captured);
        REQUIRE(uploaded.size() == sprites.size());
        for (std::size_t index = 0; index < sprites.size(); ++index) {
            CheckSameBytes(uploaded[index],
                Legacy::SpriteUniform(sprites[index], static_cast<float>(device.width),
                                      static_cast<float>(device.height)),
                index);
        }
    }
}

TEST_CASE("World mesh uniforms are byte-identical to the frozen row-vector renderer") {
    CheckMeshLayer(MeshLayer::World, 1U);
}

TEST_CASE("WorldOverlay mesh uniforms are byte-identical to the frozen row-vector renderer") {
    CheckMeshLayer(MeshLayer::WorldOverlay, 2U);
}

TEST_CASE("ViewModel mesh uniforms are byte-identical to the frozen row-vector renderer") {
    CheckMeshLayer(MeshLayer::ViewModel, 3U);
}

TEST_CASE("Scene sprite uniforms are byte-identical to the frozen row-vector renderer") {
    CheckSpriteLayer(CompositeLayer::Scene, 4U);
}

TEST_CASE("Overlay sprite uniforms are byte-identical to the frozen row-vector renderer") {
    CheckSpriteLayer(CompositeLayer::Overlay, 5U);
}
} // namespace
