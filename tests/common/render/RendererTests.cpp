#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <array>
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
    Base::Result<Bytes, Asset::Loading::AssetError> ReadAll(std::string_view path) override {
        return Base::Result<Bytes, Asset::Loading::AssetError>::Ok(files.at(std::string(path)));
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
        auto handle = MeshHandle::FromParts(serial++,1); meshes.insert(handle); return R<MeshHandle>::Ok(handle);
    }
    R<void> ReleaseMesh(MeshHandle handle) override { meshes.erase(handle); return R<void>::Ok(); }
    R<TextureHandle> CreateTexture(const ImageView& image) override {
        auto handle = TextureHandle::FromParts(serial++,1);
        textures.emplace(handle, TextureDesc{image.width,image.height}); return R<TextureHandle>::Ok(handle);
    }
    R<TextureHandle> CreateTexture(const TextureDesc& desc) override {
        if (failTargets) return R<TextureHandle>::Err(RenderError::Make(RenderErrorCode::ResourceCreationFailed,"injected target failure"));
        auto handle = TextureHandle::FromParts(serial++,1); textures.emplace(handle,desc); return R<TextureHandle>::Ok(handle);
    }
    R<void> ReleaseTexture(TextureHandle handle) override { textures.erase(handle); return R<void>::Ok(); }
    R<ShaderHandle> CreateShader(const ShaderArtifact& artifact) override {
        auto handle = ShaderHandle::FromParts(serial++,1); shaders.emplace(handle,artifact); ++shaderCreates; return R<ShaderHandle>::Ok(handle);
    }
    R<void> ReleaseShader(ShaderHandle handle) override { shaders.erase(handle); return R<void>::Ok(); }
    R<PipelineHandle> CreatePipeline(const PipelineDesc& desc) override {
        auto handle = PipelineHandle::FromParts(serial++,1); pipelines.emplace(handle,desc); ++pipelineCreates; return R<PipelineHandle>::Ok(handle);
    }
    R<void> ReleasePipeline(PipelineHandle handle) override { pipelines.erase(handle); return R<void>::Ok(); }
    R<std::optional<AcquiredFrame>> AcquireFrame() override {
        ++acquisitions;
        if (skip) return R<std::optional<AcquiredFrame>>::Ok(std::nullopt);
        return R<std::optional<AcquiredFrame>>::Ok(AcquiredFrame{acquisitions,width,height,TextureFormat::Bgra8SRgb});
    }
    R<PresentStatus> SubmitFrame(const AcquiredFrame&, const PreparedFrame& frame) override {
        ++submissions; captured = frame;
        if (failSubmit) return R<PresentStatus>::Err(RenderError::Make(RenderErrorCode::SubmissionFailed,"injected submit failure"));
        return R<PresentStatus>::Ok(PresentStatus::Presented);
    }
    void AbandonFrame(const AcquiredFrame&) noexcept override { ++abandons; }
    R<TextureReadback> ReadTexture(TextureHandle handle) override {
        ++reads; const auto& desc = textures.at(handle);
        TextureReadback result{desc.width, desc.height, desc.width*8+8, TextureFormat::Rgba16Float,{}};
        result.bytes.resize(static_cast<std::size_t>(result.rowPitch)*desc.height);
        const std::array<std::uint16_t,4> half{0x3800,0,0,0x3c00};
        for (unsigned y=0;y<desc.height;++y) for (unsigned x=0;x<desc.width;++x)
            std::memcpy(result.bytes.data()+y*result.rowPitch+x*8,half.data(),8);
        return R<TextureReadback>::Ok(std::move(result));
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
} // namespace
