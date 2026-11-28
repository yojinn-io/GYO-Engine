#pragma once

// What sdl_gpu asserts of a frame before it draws it (render-pipeline declaration 4.3 and 14.11: "契約（寫明、後端斷言）"):
// render's contracts the frame's data alone tells. In GYO's words: no SDL type (SDL-1).

#include "engine/base/Assert.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Plugins::SDLGPU {

// RP-3: "讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言" (FirstUnwrittenRead); a 3D pass's depth target the size
// of its colour target on the data side (FirstDepthOfAnotherSize). Each is a Programmer Error.
inline void RequireRenderContracts(const Engine::Render::RenderCommands& commands, const Engine::Render::RenderTextures& textures) {
    GYO_ASSERT(!Engine::Render::FirstUnwrittenRead(commands, textures) &&
               "a pass samples a render target neither drawn on this frame nor retained with a picture (RP-3)");
    GYO_ASSERT(!Engine::Render::FirstDepthOfAnotherSize(commands, textures) && "a 3D pass's depth target is not the size of its target");
}

} // namespace GYO::Plugins::SDLGPU
