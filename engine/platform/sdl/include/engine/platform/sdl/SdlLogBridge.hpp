#pragma once

// Forwards SDL's log lines to the Engine log facade (engine/base/Log.hpp)
// while it lives (docs/architecture/platform-sdl.md, "Log bridge"). Opt-in:
// without a bridge SDL's output is unchanged.

namespace Engine::Platform::Sdl {

struct SdlLogBridgeOptions final {
    // true: SDL's previous output still receives every line (tee).
    // false: only the Engine log facade receives them.
    bool keepSdlOutput = true;
};

// One at a time. While it exists, the Engine log handler must not call SDL's
// log functions, and no code may call them while holding a lock the handler
// takes (lock order: SDL's log lock, then the handler's locks). It
// must be destroyed before SDL quits. Destruction restores SDL's previous
// output only if the bridge is still SDL's output; replacing it meanwhile is a
// Programmer Error.
class SdlLogBridge final {
public:
    explicit SdlLogBridge(SdlLogBridgeOptions options = {});
    // Contains a GYO_ASSERT, so it is not noexcept (engine/base/Assert.hpp).
    ~SdlLogBridge() noexcept(false);

    SdlLogBridge(const SdlLogBridge&) = delete;
    SdlLogBridge& operator=(const SdlLogBridge&) = delete;
    SdlLogBridge(SdlLogBridge&&) = delete;
    SdlLogBridge& operator=(SdlLogBridge&&) = delete;
};

// Whether a bridge is SDL's log output right now.
[[nodiscard]] bool SdlLogBridgeInstalled() noexcept;

} // namespace Engine::Platform::Sdl
