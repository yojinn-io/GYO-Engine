#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

#include "engine/input/PhysicalInputFrame.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "gyo/framework/WindowShell.hpp"

// The ImGui binding (framework/bindings/imgui, target GYO::ToolFrameworkImGui):
// ImGui on Engine's SDL renderer, run by the windowed shell. This header names
// only neutral types; ImGui, SDL and the native layer stay in its source.

namespace Gyo::Tools::Framework {

// The window's input as Engine's SDL input backend reads it: the one reading
// of SDL's events, which ImGui's composition overlay reads too (Design
// Invariant 2). ImGui still gets every event; reading the frame takes none
// from it. Pass the frame's text to ToUiTextInput (gyo/framework/UiTextInput.hpp)
// to edit a Ui field with the products' shared mapping.
class ImGuiShellInput {
public:
    // This shell frame's input, taken before the client's BeginFrame and the
    // same until its EndFrame. Each event is in one frame only.
    [[nodiscard]] virtual const Engine::Input::PhysicalInputFrame& Frame() const = 0;
    // Asks for text input this frame over area, in window coordinates (not
    // pixels; Engine's SdlInput::SetTextInputArea), with the cursor
    // cursorPixels from its left edge. Ask from BeginFrame or Draw (a request
    // made in EndFrame is dropped), every frame a field of the client's own
    // is edited. The binding applies it after ImGui's frame, so
    // the client's request wins over an ImGui text field in that frame; once
    // the client stops asking, text input goes back to ImGui (and off when no
    // ImGui field wants it).
    virtual void RequestTextInput(const Engine::Platform::Sdl::Rect& area, int cursorPixels) = 0;

protected:
    // The binding owns it; the client only uses it.
    ~ImGuiShellInput() = default;
};

// Valid for the client's whole life.
struct ImGuiShellContext final {
    Engine::Platform::Sdl::SdlPlatform& platform;
    // The binding's own renderer, lent to the client; it outlives the client.
    Engine::Render::Backend::Sdl::SdlRenderer& renderer;
    ImGuiShellInput& input;
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
    // The window was asked to close or the program to quit, between frames:
    // true ends the run with exit code 0, false keeps it running (ask the
    // user in the next Draw, then WantsExit). The default ends the run.
    [[nodiscard]] virtual bool CloseRequested() { return true; }
    // Once per BeginFrame, rendered or not.
    virtual void EndFrame() {}
};

// Called once the renderer exists and before ImGui is created. nullptr ends
// the run with exit code 1.
using ImGuiShellClientFactory = std::function<std::unique_ptr<ImGuiShellClient>(const ImGuiShellContext& context)>;

// The ImGui texture id (ImTextureID holds it unchanged, as ImGui::Image takes
// it) of a live texture of the binding's renderer (ImGuiShellContext::renderer,
// SdlRenderer::CreateStreamingTexture). Nothing for any other handle: a
// released texture, one never created, or another renderer's. The id is the
// texture's until it is released; take it again each frame rather than keep it.
// ImGui draws a frame's images after Draw, so release a texture in BeginFrame
// (or one not drawn this frame), never after drawing it in the same Draw.
[[nodiscard]] std::optional<std::uint64_t> ImGuiTextureId(const Engine::Render::Backend::Sdl::SdlRenderer& renderer,
                                                          Engine::Render::Backend::Sdl::SdlTextureHandle texture);

// RunWindowShell with the binding's own client, which creates the SDL
// renderer, the factory's client, ImGui with a system CJK chrome font and
// its two SDL backends, then calls Initialize. One at a time per process.
// Returns 0 when the client wants to exit or the window closes; 1 when the
// platform, the renderer, ImGui's backends or the client cannot start, or a
// frame cannot be cleared or presented. Logs through Base::Log, category
// "framework.imgui", with the window title in front of each line.
[[nodiscard]] int RunImGuiShell(const WindowShellOptions& options, const ImGuiShellClientFactory& factory);

} // namespace Gyo::Tools::Framework
