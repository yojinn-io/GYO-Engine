#pragma once

// The name of each pass of a frame (render-pipeline declaration 15.3): the name of its debug group in a capture tool
// (RenderDoc, Xcode, PIX) and of its timing in SDLGPUStatus. RenderCommands carries no names, so a pass is named by
// what the data says of it: its place in RenderCommands::passes (which makes the name unique and tells which pass it
// is), its kind (render's PassKindName), a full-screen pass's shader and its target. No SDL here: the adapter
// (src/adapters/sdl/GPUDevice.cpp) pushes the groups.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "engine/render/RenderCommands.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

namespace GYO::Plugins::SDLGPU {

// Writes the pass's name over out (its storage kept): "#<index> <kind>[ <shader>] -> main" or "... -> target <id>",
// e.g. "#2 PassFullscreen test_invert.frag -> target 5".
inline void WritePassLabel(std::string& out, const Engine::Render::RenderPass& pass, const std::size_t index) {
    namespace R = Engine::Render;
    out.assign("#");
    out += std::to_string(index);
    out += ' ';
    out += R::PassKindName(pass);
    if (const auto* fullscreen = std::get_if<R::PassFullscreen>(&pass)) {
        out += ' ';
        out += fullscreen->shader;
    }
    const std::optional<R::TextureId> target = R::TargetOf(pass);
    if (target) {
        out += " -> target ";
        out += std::to_string(target->value);
    } else {
        out += " -> main";
    }
}

[[nodiscard]] inline std::string PassLabel(const Engine::Render::RenderPass& pass, const std::size_t index) {
    std::string label;
    WritePassLabel(label, pass, index);
    return label;
}

// Begins a frame's timing over the last one (its storage kept: the names are written over, not made anew each
// frame): frame's number, one PassTiming per pass of commands, in their order, named, none recorded, no debug group.
inline void BeginTiming(FrameTiming& timing, const std::uint64_t frame, const Engine::Render::RenderCommands& commands) {
    timing.frame = frame;
    timing.debugGroups = 0;
    timing.passes.resize(commands.passes.size());
    for (std::size_t i = 0; i < commands.passes.size(); ++i) {
        PassTiming& pass = timing.passes[i];
        WritePassLabel(pass.name, commands.passes[i], i);
        pass.recorded = false;
        pass.cpu = Engine::Time::Duration{0};
    }
}

} // namespace GYO::Plugins::SDLGPU
