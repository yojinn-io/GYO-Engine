// render_world (docs/architecture/target/v2/render.zh-Hant.md section 5, RD-2, RD-3): render's data in the World,
// the Submit and Cleanup stages after the last stage, the frame's data emptied after Submit. No backend: a recorder
// System in Submit stands where sdl_gpu's GPUSubmit stands. Tags "[rw-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/Presentation.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace RW = GYO::Engine::RenderWorld;

namespace {

std::vector<std::string> trail;
int passesToWrite = 0;

K::SystemResult Produce(K::ResMut<R::RenderCommands> commands, K::ResMut<R::RenderTextures> textures) {
    for (int i = 0; i < passesToWrite; ++i) commands.Get().passes.push_back(R::Pass2D{});
    if (passesToWrite > 0) textures.Get().Create(R::TextureDesc{1, 1}, std::vector<std::uint8_t>(4, 255));
    passesToWrite = 0;
    trail.push_back("produce");
    return {};
}
K::SystemResult AtLast(K::Res<R::RenderCommands>) {
    trail.push_back("last");
    return {};
}
// Where a backend submits: what it sees.
K::SystemResult AtSubmit(K::Res<R::RenderCommands> commands, K::Res<R::RenderTextures> textures) {
    trail.push_back("submit passes=" + std::to_string(commands->passes.size()) + " asks=" + std::to_string(textures->Pending().size()) +
                    " live=" + std::to_string(textures->LiveCount()));
    return {};
}

K::SystemOptions At(std::string name, std::string_view stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::string(O::DefaultSchedules::frame);
    m.stage = std::string(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    O::RegisterRow row;
    row.name = "render_world";
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    units.rows.push_back(std::move(row));
    units.modules.push_back({"render_world", std::make_unique<RW::RenderWorldModule>()});
    return units;
}

std::unique_ptr<O::Product> Make(bool askAtPrepare = false) {
    trail.clear();
    passesToWrite = 0;
    O::ProductDescription d;
    d.name = "draws";
    O::WorldDescription w;
    w.name = "Render";
    w.build = [askAtPrepare](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.AddSystem<&Produce>(At("TestProduce", O::DefaultSchedules::update));
        r.AddSystem<&AtLast>(At("TestLast", O::DefaultSchedules::last));
        r.AddSystem<&AtSubmit>(At("TestSubmit", RW::kSubmitStage));
        if (askAtPrepare) {
            setup.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                world.ResourceMut<R::RenderTextures>()->Create(R::TextureDesc{2, 2}, std::vector<std::uint8_t>(16, 0));
                return {};
            });
        }
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

void Frame(O::Product& p) {
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

} // namespace

TEST_CASE("render_world: Submit runs after the last stage and sees the frame's passes and asks; Cleanup empties them") {
    auto p = Make();
    passesToWrite = 2;
    Frame(*p);
    REQUIRE(trail.size() == 3);
    CHECK_MESSAGE(trail[0] == "produce", "[rw-order-update] the producer runs in Update");
    CHECK_MESSAGE(trail[1] == "last", "[rw-submit-after-last] Submit comes after Last");
    CHECK_MESSAGE(trail[2] == "submit passes=2 asks=1 live=1", "[rw-submit-sees] Submit sees this frame's passes and asks");
    const K::World& world = p->World("Render").GetWorld();
    CHECK_MESSAGE(world.Resource<R::RenderCommands>()->passes.empty(), "[rw-cleanup-passes] the passes are gone after the frame");
    CHECK_MESSAGE(world.Resource<R::RenderTextures>()->Pending().empty(), "[rw-cleanup-asks] the asks are gone after the frame");
    CHECK_MESSAGE(world.Resource<R::RenderTextures>()->LiveCount() == 1, "[rw-cleanup-keeps-textures] the texture stays");

    trail.clear();
    Frame(*p);
    CHECK_MESSAGE(trail.back() == "submit passes=0 asks=0 live=1", "[rw-next-frame-empty] the next frame starts empty");
}

TEST_CASE("render_world: an ask made before the first frame reaches the first Submit") {
    auto p = Make(true);
    Frame(*p);
    CHECK_MESSAGE(trail.back() == "submit passes=0 asks=1 live=1", "[rw-prepare-ask-kept] an ask from the prepare function is not lost");
}

TEST_CASE("render_world: without a backend the World runs and the data is still emptied") {
    auto p = Make();
    passesToWrite = 1;
    Frame(*p);
    Frame(*p);
    const K::World& world = p->World("Render").GetWorld();
    CHECK_MESSAGE(world.Resource<R::RenderCommands>()->passes.empty(), "[rw-no-backend] nobody draws, nothing piles up");
}

TEST_CASE("render_world: Presentation is the default in a new World and stays as the game set it, frame after frame") {
    auto p = Make();
    K::World& world = p->World("Render").GetWorld();
    REQUIRE(world.Resource<R::Presentation>() != nullptr);
    CHECK_MESSAGE(world.Resource<R::Presentation>()->presentMode == R::PresentMode::AutoVsync, "[rw-present-default] VSync on by default");
    world.ResourceMut<R::Presentation>()->presentMode = R::PresentMode::AutoNoVsync;
    Frame(*p);
    Frame(*p);
    CHECK_MESSAGE(world.Resource<R::Presentation>()->presentMode == R::PresentMode::AutoNoVsync, "[rw-present-kept] Cleanup leaves it");
}
