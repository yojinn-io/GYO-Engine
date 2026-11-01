#pragma once

#include <functional>
#include <memory>

#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/runtime/FrameContext.hpp"
#include "engine/runtime/RuntimeControl.hpp"

namespace Gyo::Tools::Framework {

// Engine's platform options as they are; the shell copies no field.
struct WindowShellOptions final {
    Engine::Platform::Sdl::SdlPlatformOptions platform;
};

// What runs inside the windowed shell. Its destructor runs while the platform
// still exists.
class WindowShellClient {
public:
    virtual ~WindowShellClient() = default;
    // Inside PumpEvents' observer, for every event of a pump before that pump
    // decides to stop: queries only (docs/architecture/platform-sdl.md,
    // Design Invariant 7).
    virtual void OnEvent(const Engine::Platform::Sdl::PlatformEvent& event) { static_cast<void>(event); }
    [[nodiscard]] virtual Engine::Runtime::RuntimeControl Update(const Engine::Runtime::FrameContext& frame) = 0;
    // Only after an Update that returned Continue, with the same frame.
    [[nodiscard]] virtual Engine::Runtime::RuntimeControl Render(const Engine::Runtime::FrameContext& frame) = 0;
    // Read once after the loop ends, for any reason, before the client is
    // destroyed.
    [[nodiscard]] virtual int ExitCode() const noexcept { return 0; }
};

// Called once, after the platform exists. nullptr: the client could not
// start (it logs why), and the shell returns 1.
using WindowShellClientFactory =
    std::function<std::unique_ptr<WindowShellClient>(Engine::Platform::Sdl::SdlPlatform& platform)>;

// The windowed shell (docs/framework/architecture.md, Design Invariant 6), on
// the main thread. It creates the platform, then the client, and runs one
// Engine::Runtime::RuntimeLoop whose ProcessEvents pumps the platform with the
// client's OnEvent as observer. It creates no render device, sets no
// live-frame handler, makes no stop request and installs no log handler or
// SDL log bridge. Returns 1 when the platform or the client cannot start
// (logged through Base::Log, category "framework.shell"), otherwise the
// client's ExitCode. The client is destroyed before the platform, also while
// an exception unwinds; no exception is caught.
[[nodiscard]] int RunWindowShell(const WindowShellOptions& options, const WindowShellClientFactory& factory);

} // namespace Gyo::Tools::Framework
