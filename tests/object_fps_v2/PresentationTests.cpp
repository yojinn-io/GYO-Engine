#include <doctest/doctest.h>
#include "TestAssets.hpp"
#include "RetroFPS/App/EnemyPresentation.hpp"
#include "render/RenderQueue.hpp"
#include "RenderDeviceStub.hpp"
#include <unordered_map>

using namespace fps;
using namespace fps::tests;
namespace {
class Device final:public Gyo::Tests::RenderDeviceStub {
public:
    std::unordered_map<Engine::Render::MeshHandle,std::vector<Engine::Render::Vertex3D>> meshes;
    std::uint32_t nextMesh{},nextTexture{};
    std::size_t createCalls{},updateCalls{};
    Engine::Base::Result<Engine::Render::MeshHandle,Engine::Render::RenderError>
    CreateMesh(const Engine::Render::MeshView& mesh) override {
        ++createCalls;auto h=Engine::Render::MeshHandle::FromParts(++nextMesh,1);
        meshes[h]={mesh.vertices.begin(),mesh.vertices.end()};
        return Engine::Base::Result<Engine::Render::MeshHandle,Engine::Render::RenderError>::Ok(h);
    }
    Engine::Base::Result<Engine::Render::TextureHandle,Engine::Render::RenderError>
    CreateTexture(const Engine::Render::ImageView&) override {
        return Engine::Base::Result<Engine::Render::TextureHandle,Engine::Render::RenderError>::Ok(
            Engine::Render::TextureHandle::FromParts(++nextTexture,1));
    }
    Engine::Base::Result<void,Engine::Render::RenderError> UpdateMeshVertices(
        Engine::Render::MeshHandle h,std::span<const Engine::Render::Vertex3D> vertices) override {
        ++updateCalls;meshes.at(h)={vertices.begin(),vertices.end()};
        return Engine::Base::Result<void,Engine::Render::RenderError>::Ok();
    }
    Engine::Base::Result<void,Engine::Render::RenderError> ReleaseMesh(Engine::Render::MeshHandle h) override {
        meshes.erase(h);return Engine::Base::Result<void,Engine::Render::RenderError>::Ok();
    }
    Engine::Base::Result<void,Engine::Render::RenderError> ReleaseTexture(Engine::Render::TextureHandle) override {
        return Engine::Base::Result<void,Engine::Render::RenderError>::Ok();
    }
};
}

TEST_CASE("v2 enemy presentation submits real posed mesh parts and authoritative collision overlays") {
    auto session=StartedSession();auto snapshot=session.Snapshot();
    Device device;EnemyPresentation presentation;
    auto& app=ProductionApplication();std::string error;
    REQUIRE_MESSAGE(presentation.Initialize(device,app.Assets(),app.Content()->Data().enemies,error),error);
    Engine::Render::RenderQueue queue;queue.SetCamera({});
    REQUIRE_MESSAGE(presentation.Submit(snapshot,queue,false,error),error);
    REQUIRE_FALSE(queue.Meshes().empty());
    const auto modelDraws=queue.Meshes().size();
    CHECK(device.createCalls==modelDraws);
    for(const auto& draw:queue.Meshes()) {
        CHECK(draw.layer==Engine::Render::MeshLayer::World);
        CHECK(device.meshes.at(draw.mesh).size()>4); // No billboard quad path.
    }
    const auto poses=snapshot.enemies.front().pose;
    snapshot.screen=GameScreen::Paused;
    queue.Reset();queue.SetCamera({});
    REQUIRE_MESSAGE(presentation.Submit(snapshot,queue,true,error),error);
    const auto expectedWireCount=1+snapshot.worldCollisionBoxes.size()+[&] {
        std::size_t n=0;
        for(const auto& enemy:snapshot.enemies) if(enemy.state!=EnemyState::Dead)
            n+=1+enemy.hurtboxes.size()+(enemy.attackShape?1:0);
        return n;
    }();
    REQUIRE(queue.Meshes().size()==modelDraws+expectedWireCount);
    for(std::size_t i=modelDraws;i<queue.Meshes().size();++i)
        CHECK(queue.Meshes()[i].layer==Engine::Render::MeshLayer::WorldOverlay);
    const auto created=device.createCalls,updated=device.updateCalls;
    queue.Reset();queue.SetCamera({});
    REQUIRE_MESSAGE(presentation.Submit(snapshot,queue,false,error),error);
    CHECK(queue.Meshes().size()==modelDraws);
    CHECK(device.createCalls==created);
    CHECK(device.updateCalls-updated==modelDraws); // Disabled wires are neither updated nor submitted.
    for(std::size_t node=0;node<poses.globalTransforms.size();++node)
        CHECK(snapshot.enemies.front().pose.globalTransforms[node].values==poses.globalTransforms[node].values);
    snapshot.activeStage.reset();
    queue.Reset();queue.SetCamera({});
    REQUIRE_MESSAGE(presentation.Submit(snapshot,queue,true,error),error);
    CHECK(device.meshes.empty());
    CHECK(queue.Meshes().empty());
}

TEST_CASE("enemy retirement and restarting a stage never reuse stale instance vertices") {
    auto session=StartedSession();auto snapshot=session.Snapshot();
    Device device;EnemyPresentation presentation;std::string error;
    auto& app=ProductionApplication();
    REQUIRE_MESSAGE(presentation.Initialize(device,app.Assets(),app.Content()->Data().enemies,error),error);
    Engine::Render::RenderQueue queue;queue.SetCamera({});
    REQUIRE_MESSAGE(presentation.Submit(snapshot,queue,false,error),error);
    REQUIRE_FALSE(queue.Meshes().empty());
    const auto previous=queue.Meshes().front().mesh;
    snapshot.enemies.clear();queue.Reset();queue.SetCamera({});
    REQUIRE_MESSAGE(presentation.Submit(snapshot,queue,false,error),error);
    CHECK(device.meshes.empty());
    snapshot=session.Snapshot();queue.Reset();queue.SetCamera({});
    REQUIRE_MESSAGE(presentation.Submit(snapshot,queue,false,error),error);
    REQUIRE_FALSE(queue.Meshes().empty());
    CHECK(queue.Meshes().front().mesh!=previous);
    presentation.Reset();
    CHECK(device.meshes.empty());
}
