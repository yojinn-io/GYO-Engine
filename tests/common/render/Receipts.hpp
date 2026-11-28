#pragma once

// A backend for headless tests (render-pipeline declaration 14.13): one that gets everything of a frame done, every
// size asked applied, every render target a pass draws on drawn. A render World without a backend gets nothing done
// (no target ever has a picture); a test that wants the frames done mounts this in its Submit stage.

#include <span>
#include <string>
#include <variant>

#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/SystemTraits.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"

namespace GYO::Test {

// The receipt of a frame of these passes and asks, all of it done.
inline Engine::Render::RenderFrameReceipt AllDoneReceipt(const Engine::Render::RenderCommands& commands,
                                                         const std::span<const Engine::Render::TextureOp> asks) {
    namespace R = Engine::Render;
    R::RenderFrameReceipt receipt;
    for (const R::TextureOp& ask : asks) {
        if (const auto* sized = std::get_if<R::TextureTargetSize>(&ask)) receipt.RecordApplied(sized->id, sized->size);
    }
    for (const R::RenderPass& pass : commands.passes) {
        const R::PassWrites writes = R::WritesOf(pass);
        if (writes.colour && writes.colourTarget) receipt.RecordDrawn(*writes.colourTarget);
        if (writes.depthTarget) receipt.RecordDrawn(*writes.depthTarget);
    }
    return receipt;
}

// The backend's System: writes the frame's receipt, all done.
inline Kernel::SystemResult AllDoneBackend(Kernel::Res<Engine::Render::RenderCommands> commands,
                                           Kernel::Res<Engine::Render::RenderTextures> textures,
                                           Kernel::ResMut<Engine::Render::RenderFrameReceipt> receipt) {
    receipt.Get() = AllDoneReceipt(*commands, textures->Pending());
    return {};
}

inline constexpr std::string_view kAllDoneBackend = "TestAllDoneBackend";

// Mounts AllDoneBackend in the render World's Submit stage of schedule.
inline void AddAllDoneBackend(Kernel::Registrar& r, std::string schedule) {
    Kernel::SystemOptions o;
    o.name = std::string(kAllDoneBackend);
    Kernel::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::string(Engine::RenderWorld::kSubmitStage);
    o.mounts.push_back(std::move(m));
    r.AddSystem<&AllDoneBackend>(std::move(o));
}

} // namespace GYO::Test
