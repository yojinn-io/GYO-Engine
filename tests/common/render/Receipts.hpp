#pragma once

// The receipt of a backend that got everything of a frame done (render-pipeline declaration 14.12): every size asked
// applied, every render target a pass draws on drawn. The headless tests' stand-in for sdl_gpu's GPUSubmit.

#include <span>
#include <variant>

#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Test {

inline Engine::Render::RenderFrameReceipt AllDone(const Engine::Render::RenderCommands& commands,
                                                  const std::span<const Engine::Render::TextureOp> asks) {
    namespace R = Engine::Render;
    R::RenderFrameReceipt receipt;
    for (const R::TextureOp& ask : asks) {
        if (const auto* sized = std::get_if<R::TextureTargetSize>(&ask)) receipt.Applied(sized->id, sized->size);
    }
    for (const R::RenderPass& pass : commands.passes) {
        const R::PassWrites writes = R::WritesOf(pass);
        if (writes.colour && writes.colourTarget) receipt.Drawn(*writes.colourTarget);
        if (writes.depthTarget) receipt.Drawn(*writes.depthTarget);
    }
    return receipt;
}

} // namespace GYO::Test
