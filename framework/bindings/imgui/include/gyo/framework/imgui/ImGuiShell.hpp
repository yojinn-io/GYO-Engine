#pragma once

#include <functional>
#include <memory>

#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "gyo/framework/WindowShell.hpp"

// The ImGui binding (framework/bindings/imgui, target GYO::ToolFrameworkImGui):
// ImGui on Engine's SDL renderer, run by the windowed shell. This header names
// only neutral types; ImGui, SDL and the native layer stay in its source.

namespace Gyo::Tools::Framework {

// Valid for the client's whole life.
struct ImGuiShellContext final {
    Engine::Platform::Sdl::SdlPlatform& platform;
    // The binding's own renderer, lent to the client; it outlives the client.
    Engine::Render::Backend::Sdl::SdlRenderer& renderer;
};

// One window drawn with ImGui. The binding writes no side file (no imgui.ini,
// no ImGui log). The client is created after the renderer and before ImGui;
// ImGui is shut down before the client is destroyed, and the renderer after.
class ImGuiShellClient {
public:
    virtual ~ImGuiShellClient() = default;
    // After ImGui and its SDL backends are ready. false ends the run with
    // exit code 1; the client logs why.
    [[nodiscard]] virtual bool Initialize(const ImGuiShellContext& context) = 0;
    // Before ImGui's frame starts: apply what arrived between frames.
    virtual void BeginFrame() {}
    // Inside ImGui's frame.
    virtual void Draw() = 0;
    // Checked after Draw; true ends the run with exit code 0.
    [[nodiscard]] virtual bool WantsExit() const = 0;
    // Once per BeginFrame, rendered or not.
    virtual void EndFrame() {}
};

// Called once the renderer exists and before ImGui is created. nullptr ends
// the run with exit code 1.
using ImGuiShellClientFactory = std::function<std::unique_ptr<ImGuiShellClient>(const ImGuiShellContext& context)>;

// RunWindowShell with the binding's own client, which creates the SDL
// renderer, the factory's client, ImGui with a system CJK chrome font and
// its two SDL backends, then calls Initialize. One at a time per process.
// Returns 0 when the client wants to exit or the window closes; 1 when the
// platform, the renderer, ImGui's backends or the client cannot start, or a
// frame cannot be cleared or presented. Logs through Base::Log, category
// "framework.imgui", with the window title in front of each line.
[[nodiscard]] int RunImGuiShell(const WindowShellOptions& options, const ImGuiShellClientFactory& factory);

} // namespace Gyo::Tools::Framework
