#include "engine/platform/sdl/SdlLogBridge.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <string_view>

#include "engine/base/Assert.hpp"
#include "engine/base/Log.hpp"

namespace Engine::Platform::Sdl {
namespace {

struct BridgeState final {
    SDL_LogOutputFunction previous{};
    void* previousData{};
    bool keepSdlOutput{true};
};

BridgeState state;
std::atomic<bool> bridged{false};
// Set while this thread is inside the bridge: a line logged to SDL from the
// Engine handler goes to SDL's previous output only (no recursion).
thread_local bool forwarding = false;

std::string_view CategoryName(const int category) noexcept {
    switch (category) {
    case SDL_LOG_CATEGORY_APPLICATION: return "sdl.application";
    case SDL_LOG_CATEGORY_ERROR: return "sdl.error";
    case SDL_LOG_CATEGORY_ASSERT: return "sdl.assert";
    case SDL_LOG_CATEGORY_SYSTEM: return "sdl.system";
    case SDL_LOG_CATEGORY_AUDIO: return "sdl.audio";
    case SDL_LOG_CATEGORY_VIDEO: return "sdl.video";
    case SDL_LOG_CATEGORY_RENDER: return "sdl.render";
    case SDL_LOG_CATEGORY_INPUT: return "sdl.input";
    case SDL_LOG_CATEGORY_TEST: return "sdl.test";
    case SDL_LOG_CATEGORY_GPU: return "sdl.gpu";
    default: return "sdl.custom";
    }
}

Base::LogLevel Level(const SDL_LogPriority priority) noexcept {
    switch (priority) {
    case SDL_LOG_PRIORITY_TRACE:
    case SDL_LOG_PRIORITY_VERBOSE:
    case SDL_LOG_PRIORITY_DEBUG: return Base::LogLevel::Debug;
    case SDL_LOG_PRIORITY_WARN: return Base::LogLevel::Warning;
    case SDL_LOG_PRIORITY_ERROR:
    case SDL_LOG_PRIORITY_CRITICAL: return Base::LogLevel::Error;
    default: return Base::LogLevel::Info;
    }
}

// SDL calls this inside its log lock, on the logging thread.
void SDLCALL Forward(void*, const int category, const SDL_LogPriority priority, const char* message) {
    // The state is the single bridge's; the userdata SDL passes is not trusted.
    const BridgeState* bridge = &state;
    if (forwarding) {
        if (bridge->previous) bridge->previous(bridge->previousData, category, priority, message);
        return;
    }
    forwarding = true;
    if (bridge->keepSdlOutput && bridge->previous) bridge->previous(bridge->previousData, category, priority, message);
    Base::Log(Level(priority), CategoryName(category), message ? std::string_view(message) : std::string_view());
    forwarding = false;
}

} // namespace

SdlLogBridge::SdlLogBridge(const SdlLogBridgeOptions options) {
    const bool wasBridged = bridged.exchange(true);
    GYO_ASSERT(!wasBridged);
    // Creates SDL's log lock: without it the output function and its data are
    // read and written unlocked.
    static_cast<void>(SDL_GetLogPriority(SDL_LOG_CATEGORY_APPLICATION));
    SDL_GetLogOutputFunction(&state.previous, &state.previousData);
    state.keepSdlOutput = options.keepSdlOutput;
    SDL_SetLogOutputFunction(&Forward, &state);
}

SdlLogBridge::~SdlLogBridge() {
    // Restores only its own installation; finding another output means someone
    // replaced the bridge while it lived (a Programmer Error).
    // Restore before releasing the one-at-a-time guard: a bridge created on
    // another thread meanwhile would otherwise take this bridge as SDL's
    // previous output and forward to itself.
    const bool installed = SdlLogBridgeInstalled();
    if (installed) SDL_SetLogOutputFunction(state.previous, state.previousData);
    bridged.store(false);
    GYO_ASSERT(installed);
}

bool SdlLogBridgeInstalled() noexcept {
    SDL_LogOutputFunction current{};
    void* data{};
    SDL_GetLogOutputFunction(&current, &data);
    return current == &Forward && data == &state;
}

} // namespace Engine::Platform::Sdl
