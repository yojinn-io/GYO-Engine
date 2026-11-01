#pragma once

// The native layer of the SDL platform (docs/architecture/platform-sdl.md).
// Including this header, or linking GYO::PlatformSDLNative, declares direct
// use of SDL. The neutral header SdlPlatform.hpp never names SDL.

#include <SDL3/SDL.h>

#include <cstdint>
#include <optional>

#include "engine/platform/sdl/SdlPlatform.hpp"

namespace Engine::Platform::Sdl {

// Both clocks sampled together: engine = midpoint of two Engine reads taken
// around one SDL read, bracket = the span between those Engine reads.
struct ClockPair final {
    Time::TimePoint engine{};
    std::uint64_t sdlNs{};
    Time::Duration bracket{};
};

// The pair from three reads taken in this order: Engine, SDL, Engine. Pure.
[[nodiscard]] ClockPair MakeClockPair(Time::TimePoint engineBefore, std::uint64_t sdlNs,
                                      Time::TimePoint engineAfter) noexcept;
// The two clocks a pair reads. SampleClockPair uses Time::Now and
// SDL_GetTicksNS; tests pass fake readers to check the reads exactly.
struct ClockReaders final {
    Time::TimePoint (*engine)() noexcept;
    std::uint64_t (*sdl)() noexcept;
};
// Reads Engine, SDL, Engine in this order and makes the pair from them.
[[nodiscard]] ClockPair SampleClockPairWith(const ClockReaders& readers) noexcept;
// SampleClockPairWith on the real clocks.
[[nodiscard]] ClockPair SampleClockPair() noexcept;

// An SDL event time (SDL_GetTicksNS domain) on the Engine time base:
// engine + (sdlNs - pair.sdlNs), never later than ceiling; 0 (no time) maps to
// ceiling. Pure; the timestamp contract of docs/architecture/platform-sdl.md.
[[nodiscard]] Time::TimePoint ConvertSdlTimestamp(std::uint64_t sdlNs, const ClockPair& pair,
                                                  Time::TimePoint ceiling) noexcept;

// Read-only testing seam (release builds have it too): the pair of the
// innermost running pump, empty outside PumpEvents. Nothing can replace the
// pair the platform samples (Design Invariant 3).
[[nodiscard]] std::optional<ClockPair> CurrentClockPairForTesting(const SdlPlatform& platform);
// The pair the last event watch callback on this thread sampled; empty before
// the first one (read-only seam, release builds too).
[[nodiscard]] std::optional<ClockPair> LastWatchClockPairForTesting() noexcept;
// Event watches currently registered with SDL by the platform (read-only seam).
[[nodiscard]] int RegisteredEventWatchesForTesting() noexcept;

// SDL's dialog callback, noexcept on purpose: the platform's callback must not
// let an exception cross SDL's or the OS's stack (Design Invariant 8), and a
// callback without noexcept does not convert to this type.
using PathDialogCallback = void(SDLCALL*)(void* userdata, const char* const* files, int filter) noexcept;

// The one step a test replaces: handing a prepared request to the OS. The real
// launcher is ShowWithSdlFunctions over SDL's own functions; a test's launcher keeps
// the arguments and calls callback(userdata, ...) when and on which thread it
// likes, also twice, as SDL may. It writes no input frame (Design Invariant 4)
// and cannot throw: by then the request is SDL's.
struct PathDialogLauncher final {
    void (*show)(PathDialogCallback callback, void* userdata, SDL_Window* window, PathDialogKind kind,
                 const SDL_DialogFileFilter* filters, int filterCount, const char* defaultLocation) noexcept;
};
// The SDL functions the real launcher calls, one per kind; a test passes fakes
// to check what each kind hands SDL.
struct SdlDialogFunctions final {
    decltype(&SDL_ShowOpenFileDialog) openFile;
    decltype(&SDL_ShowSaveFileDialog) saveFile;
    decltype(&SDL_ShowOpenFolderDialog) openFolder;
};
// The real launcher over a given table: one call of the kind's function, one
// file at most, this window, the filters (not for a folder) and the location.
void ShowWithSdlFunctions(const SdlDialogFunctions& functions, PathDialogCallback callback, void* userdata,
                          SDL_Window* window, PathDialogKind kind, const SDL_DialogFileFilter* filters,
                          int filterCount, const char* defaultLocation) noexcept;
// ShowPathDialog through a given launcher; ShowPathDialog is this with the
// real one. The same operation rules apply (GYO_ASSERT).
void ShowPathDialogWith(SdlPlatform& platform, PathDialogRequest request, const PathDialogLauncher& launcher);

class NativeAccess final {
public:
    [[nodiscard]] static SDL_Window* Window(const SdlPlatform& platform) noexcept;
    [[nodiscard]] static std::optional<ClockPair> PumpPair(const SdlPlatform& platform);
    static void ShowPathDialog(SdlPlatform& platform, PathDialogRequest request, const PathDialogLauncher& launcher);

    [[nodiscard]] static const SDL_Event& Event(const PlatformEvent& event) noexcept {
        return *static_cast<const SDL_Event*>(event.native_);
    }

    // A neutral view of a native event, for code that builds SDL events itself
    // (tests, adapters). Valid as long as the event and the returned view live.
    [[nodiscard]] static PlatformEvent View(const SDL_Event& event, Time::TimePoint timestamp) noexcept {
        return PlatformEvent(&event, timestamp);
    }
};

// Pushes an event as injected test input (Design Invariant 4): through SDL's
// event queue only, marked as injected for event watches on this thread. Main
// thread; never inside a live frame or an event watch callback (GYO_ASSERT).
// False when SDL did not queue it (for example a filter dropped it).
[[nodiscard]] bool PushInjectedEvent(SdlPlatform& platform, SDL_Event& event);

[[nodiscard]] inline SDL_Window* NativeWindow(const SdlPlatform& platform) noexcept {
    return NativeAccess::Window(platform);
}

[[nodiscard]] inline const SDL_Event& NativeEvent(const PlatformEvent& event) noexcept {
    return NativeAccess::Event(event);
}

} // namespace Engine::Platform::Sdl
