#pragma once

#include <functional>
#include <memory>
#include <string>

struct SDL_Renderer;
struct SDL_Window;

namespace Gyo::Tools::Framework {

struct ImGuiSdlHostOptions final {
    std::string title; // the window title, also the prefix of the host's log lines
    int width{1600};
    int height{900};
    bool resizable{true};
};

// The native window and renderer, valid while the host runs.
struct ImGuiSdlHostContext final {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
};

// One tool window. The host owns ImGui and writes no side file (Design
// Invariant 4). It shuts ImGui down before it destroys the client, and
// destroys the client before the renderer and the window.
class ImGuiSdlClient {
public:
    virtual ~ImGuiSdlClient() = default;
    // After ImGui and its SDL backends are ready. false ends the run with
    // exit code 1; the client logs why.
    [[nodiscard]] virtual bool Initialize(const ImGuiSdlHostContext& context) = 0;
    // Before ImGui's frame starts: apply what arrived between frames.
    virtual void BeginFrame() {}
    // Inside ImGui's frame.
    virtual void Draw() = 0;
    // Checked after Draw; true ends the run with exit code 0.
    [[nodiscard]] virtual bool WantsExit() const = 0;
    // After every frame, rendered or not.
    virtual void EndFrame() {}
};

// Called once the window and renderer exist and before ImGui is created.
using ImGuiSdlClientFactory = std::function<std::unique_ptr<ImGuiSdlClient>(const ImGuiSdlHostContext&)>;

// Creates the window, the SDL renderer and ImGui with a system CJK font, then
// runs the factory's client until it wants to exit or the window closes.
// Returns the exit code: 0, or 1 when anything failed.
[[nodiscard]] int RunImGuiSdlHost(const ImGuiSdlHostOptions& options, const ImGuiSdlClientFactory& factory);

} // namespace Gyo::Tools::Framework
