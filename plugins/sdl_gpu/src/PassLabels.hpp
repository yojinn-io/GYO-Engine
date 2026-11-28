#pragma once

// The name of each pass of a frame (render-pipeline declaration 15.3): the name of its debug group in a capture tool
// (RenderDoc, Xcode, PIX) and of its timing in SDLGPUStatus. RenderCommands carries no names, so a pass is named by
// what the data says of it: its place in RenderCommands::passes (which makes the name unique and tells which pass it
// is), its kind, a full-screen pass's shader and its target. No SDL here: the adapter (src/adapters/sdl/GPUDevice.cpp)
// pushes the groups.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include "engine/render/RenderCommands.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

namespace GYO::Plugins::SDLGPU {

// "#<index> <kind>[ <shader>] -> main" or "... -> target <id>", e.g. "#2 PassFullscreen test_invert.frag -> target 5".
[[nodiscard]] inline std::string PassLabel(const Engine::Render::RenderPass& pass, const std::size_t index) {
    namespace R = Engine::Render;
    std::string label = "#" + std::to_string(index) + " ";
    label += std::visit(
        [](const auto& p) -> std::string {
            using Pass = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<Pass, R::Pass2D>) return "Pass2D";
            if constexpr (std::is_same_v<Pass, R::Pass3D>) return "Pass3D";
            if constexpr (std::is_same_v<Pass, R::PassFullscreen>) return "PassFullscreen " + p.shader;
            if constexpr (std::is_same_v<Pass, R::PassDepth>) return "PassDepth";
        },
        pass);
    const std::optional<R::TextureId> target = R::TargetOf(pass);
    label += target ? " -> target " + std::to_string(target->value) : std::string(" -> main");
    return label;
}

// A frame's timing before it is recorded: one PassTiming per pass of commands, in their order, named, none recorded.
[[nodiscard]] inline FrameTiming TimingOf(const std::uint64_t frame, const Engine::Render::RenderCommands& commands) {
    FrameTiming timing;
    timing.frame = frame;
    timing.passes.reserve(commands.passes.size());
    for (std::size_t i = 0; i < commands.passes.size(); ++i) timing.passes.push_back(PassTiming{PassLabel(commands.passes[i], i)});
    return timing;
}

} // namespace GYO::Plugins::SDLGPU
