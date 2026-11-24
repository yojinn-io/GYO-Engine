#include "engine/render_world/RenderWorldModule.hpp"

#include <utility>

#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderMeshes.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::RenderWorld {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const RenderWorldOptions& OptionsOf(const RenderWorldOptions* given) {
    static const RenderWorldOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// After the frame was drawn (kCleanupStage): this frame's passes and texture asks are gone; the textures stay.
SystemResult ClearRenderFrame(Kernel::ResMut<Render::RenderCommands> commands, Kernel::ResMut<Render::RenderTextures> textures,
                              Kernel::ResMut<Render::RenderMeshes> meshes) {
    commands.Get().passes.clear();
    textures.Get().ClearPending();
    meshes.Get().ClearPending();
    return {};
}

template <class T>
void PutIfAbsent(Kernel::World& world) {
    if (world.Resource<T>() == nullptr) world.SetResource(T{});
}

} // namespace

void RenderWorldModule::Build(Orchestration::WorldSetup& setup) {
    const RenderWorldOptions& options = OptionsOf(setup.Options<RenderWorldOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<Render::RenderCommands>({.name = "RenderCommands"});
    r.Resource<Render::RenderTextures>({.name = "RenderTextures"});
    r.Resource<Render::RenderMeshes>({.name = "RenderMeshes"});
    r.InsertStage(options.schedule, options.after, Kernel::StagePlace::After, std::string(kSubmitStage));
    r.InsertStage(options.schedule, kSubmitStage, Kernel::StagePlace::After, std::string(kCleanupStage));
    Kernel::SystemOptions clear;
    clear.name = "ClearRenderFrame";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = std::string(kCleanupStage);
    clear.mounts.push_back(std::move(mount));
    r.AddSystem<&ClearRenderFrame>(std::move(clear));
    if (!setup.ExtractFrom().empty()) {
        Kernel::ScheduleOptions extract;
        extract.name = std::string(kExtractSchedule);
        extract.extract = true;
        extract.stages.push_back(std::string(kExtractSchedule));
        r.Schedule(std::move(extract));
        setup.Frame().InsertBefore(options.schedule, std::string(kExtractSchedule));
    }
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent<Render::RenderCommands>(world);
        PutIfAbsent<Render::RenderTextures>(world);
        PutIfAbsent<Render::RenderMeshes>(world);
        return {};
    });
}

} // namespace GYO::Engine::RenderWorld
