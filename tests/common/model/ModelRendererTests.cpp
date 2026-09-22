#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "model_renderer/ModelRenderer.hpp"
#include "render/RenderQueue.hpp"
#include "RenderDeviceStub.hpp"

#include <array>
#include <unordered_map>

using namespace Engine;

namespace {
class Device final : public Gyo::Tests::RenderDeviceStub {
public:
    std::unordered_map<Render::MeshHandle,std::vector<Render::Vertex3D>> meshes;
    std::unordered_map<Render::TextureHandle,std::vector<std::byte>> textures;
    std::uint32_t nextMesh{},nextTexture{},createMeshCalls{},createTextureCalls{};
    std::uint32_t failMeshCall{},failTextureCall{};
    bool failUpdate{};

    Base::Result<Render::MeshHandle,Render::RenderError> CreateMesh(const Render::MeshView& mesh) override {
        using R=Base::Result<Render::MeshHandle,Render::RenderError>;
        if(++createMeshCalls==failMeshCall) return R::Err(Render::RenderError::Make(
            Render::RenderErrorCode::ResourceCreationFailed,"injected mesh failure"));
        const auto handle=Render::MeshHandle::FromParts(++nextMesh,1);
        meshes[handle]={mesh.vertices.begin(),mesh.vertices.end()};
        return R::Ok(handle);
    }
    Base::Result<Render::TextureHandle,Render::RenderError> CreateTexture(const Render::ImageView& image) override {
        using R=Base::Result<Render::TextureHandle,Render::RenderError>;
        if(++createTextureCalls==failTextureCall) return R::Err(Render::RenderError::Make(
            Render::RenderErrorCode::ResourceCreationFailed,"injected texture failure"));
        const auto handle=Render::TextureHandle::FromParts(++nextTexture,1);
        textures[handle]={image.rgba8.begin(),image.rgba8.end()};
        return R::Ok(handle);
    }
    Base::Result<void,Render::RenderError> UpdateMeshVertices(
        Render::MeshHandle handle,std::span<const Render::Vertex3D> vertices) override {
        using R=Base::Result<void,Render::RenderError>;
        if(failUpdate) return R::Err(Render::RenderError::Make(
            Render::RenderErrorCode::ResourceCreationFailed,"injected update failure"));
        meshes.at(handle)={vertices.begin(),vertices.end()};
        return R::Ok();
    }
    Base::Result<void,Render::RenderError> ReleaseMesh(Render::MeshHandle handle) override {
        meshes.erase(handle);return Base::Result<void,Render::RenderError>::Ok();
    }
    Base::Result<void,Render::RenderError> ReleaseTexture(Render::TextureHandle handle) override {
        textures.erase(handle);return Base::Result<void,Render::RenderError>::Ok();
    }
};

std::shared_ptr<Model::ModelAsset> MakeModel() {
    auto model=std::make_shared<Model::ModelAsset>();
    model->nodes.push_back({"root",std::nullopt,{}});
    model->materials.push_back({"color"});
    Model::MeshPart mesh;
    mesh.vertices={{{0,0,0}},{{1,0,0}},{{0,1,0}}};
    mesh.indices={0,1,2};
    model->meshes.push_back(mesh);
    model->clips.push_back({"move",1,{{0,{{0,{0,0,0}},{1,{0,2,0}}},{},{}}}});
    return model;
}

const std::array<std::byte,4> pixels{std::byte{255},std::byte{128},std::byte{0},std::byte{255}};
const ModelRenderer::ModelMaterial material{
    Render::ImageView{1,1,4,pixels,Render::TextureColorSpace::SRgb},
    {0.5F,0.25F,1,1},Render::SamplerMode::LinearWrap};
}

TEST_CASE("model resources share textures while instances own independent posed vertices") {
    Device device;
    const auto model=MakeModel();
    auto resourceResult=ModelRenderer::ModelResource::Create(device,model,{&material,1});
    REQUIRE(resourceResult);
    auto resource=std::move(resourceResult.value());
    Model::Pose pose;
    REQUIRE(Model::MakeDefaultPose(*model,pose));
    auto firstResult=ModelRenderer::ModelInstance::Create(resource,pose,{0,-1,0});
    auto secondResult=ModelRenderer::ModelInstance::Create(resource,pose);
    REQUIRE(firstResult); REQUIRE(secondResult);
    auto first=std::move(firstResult.value()),second=std::move(secondResult.value());
    REQUIRE(Model::SamplePose(*model,0,0.5,Model::PlaybackMode::Clamp,pose));
    REQUIRE(first->UpdatePose(pose));
    Render::RenderQueue queue;
    queue.SetCamera({});
    queue.SetViewModelCamera({});
    Render::Transform3D placement;placement.translation={3,4,5};
    REQUIRE(first->Submit(queue,placement,Render::MeshLayer::ViewModel,{1,0.5F,1,1}));
    REQUIRE(second->Submit(queue,{}));
    REQUIRE(queue.Meshes().size()==2);
    CHECK(queue.Meshes()[0].mesh!=queue.Meshes()[1].mesh);
    CHECK(queue.Meshes()[0].material.texture==queue.Meshes()[1].material.texture);
    CHECK(queue.Meshes()[0].material.sampler==Render::SamplerMode::LinearWrap);
    CHECK(queue.Meshes()[0].material.tint.green==doctest::Approx(0.125));
    CHECK(queue.Meshes()[0].layer==Render::MeshLayer::ViewModel);
    CHECK(queue.Meshes()[0].transform.translation.z==5);
    CHECK(device.meshes.at(queue.Meshes()[0].mesh)[0].position.y==0);
    CHECK(device.meshes.at(queue.Meshes()[1].mesh)[0].position.y==0);
    REQUIRE(Model::SamplePose(*model,0,1,Model::PlaybackMode::Clamp,pose));
    REQUIRE(first->UpdatePose(pose));
    CHECK(device.meshes.at(queue.Meshes()[0].mesh)[0].position.y==1);
    CHECK(device.meshes.at(queue.Meshes()[1].mesh)[0].position.y==0);
    CHECK(device.createTextureCalls==1);
    resource.reset();
    first.reset();
    CHECK(device.meshes.size()==1);
    CHECK(device.textures.size()==1);
    second.reset();
    CHECK(device.meshes.empty());
    CHECK(device.textures.empty());
}

TEST_CASE("model renderer cleans up partial uploads and reports device failures") {
    Device device;
    auto model=MakeModel();
    model->materials.push_back({"second"});
    const std::array materials{material,material};
    device.failTextureCall=2;
    CHECK_FALSE(ModelRenderer::ModelResource::Create(device,model,materials));
    CHECK(device.textures.empty());
    device.failTextureCall=0;
    auto resource=ModelRenderer::ModelResource::Create(device,model,materials);
    REQUIRE(resource);
    // The caller's asset is immutable after resource construction. This copy
    // provides two parts before constructing a separate resource.
    auto twoParts=std::make_shared<Model::ModelAsset>(*model);
    twoParts->meshes.push_back(twoParts->meshes[0]);
    resource=ModelRenderer::ModelResource::Create(device,twoParts,materials);
    REQUIRE(resource);
    Model::Pose pose;REQUIRE(Model::MakeDefaultPose(*twoParts,pose));
    device.failMeshCall=2;
    CHECK_FALSE(ModelRenderer::ModelInstance::Create(resource.value(),pose));
    CHECK(device.meshes.empty());
    device.failMeshCall=0;
    auto instance=ModelRenderer::ModelInstance::Create(resource.value(),pose);
    REQUIRE(instance);
    device.failUpdate=true;
    const auto update=instance.value()->UpdatePose(pose);
    REQUIRE_FALSE(update);
    CHECK(update.error().find("injected update failure")!=std::string::npos);
}

TEST_CASE("untextured model materials and malformed poses have explicit behavior") {
    Device device;
    const auto model=MakeModel();
    const ModelRenderer::ModelMaterial untextured{};
    CHECK_FALSE(ModelRenderer::ModelResource::Create(device,model,{}));
    auto resource=ModelRenderer::ModelResource::Create(device,model,{&untextured,1});
    REQUIRE(resource);
    CHECK(device.createTextureCalls==0);
    CHECK_FALSE(ModelRenderer::ModelInstance::Create(resource.value(),{}));
    CHECK(device.meshes.empty());
    Model::Pose pose;REQUIRE(Model::MakeDefaultPose(*model,pose));
    auto instance=ModelRenderer::ModelInstance::Create(resource.value(),pose);
    REQUIRE(instance);
    Render::RenderQueue queue;
    queue.SetCamera({});
    REQUIRE(instance.value()->Submit(queue,{}));
    CHECK_FALSE(queue.Meshes()[0].material.texture.IsValid());
}
