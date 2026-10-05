// Batch 06 characterization: the in-world pointer capture policy moved from
// per-native-event SDL handling to Engine input events (IP-1). The oracle below
// is the pre-batch HandleNativeEvent in-world branch, copied unchanged except
// that its state lives in a struct. Both see the same synthetic SDL events
// through the same Engine SdlInput; every difference must be one declared in
// the v6 HANDOFF (batch 06).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PointerCapture.hpp"
#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using fps::pvp::ApplyPointerCaptureEvents;
using fps::pvp::PointerCaptureState;

namespace {

struct OracleState {
    PointerCaptureState capture;
    float width{};
    float height{};
    unsigned releases{};
};

// Pre-batch PvpApplication::Impl::HandleNativeEvent (in world), with
// ReleasePointer's state changes inlined; `focused` is the input snapshot's
// windowFocused right after SdlInput handled this event.
void OracleHandle(OracleState& o, const SDL_Event& event, const SDL_WindowID windowId, const bool focused) {
    auto& c = o.capture;
    const auto releasePointer = [&] {
        c.inputCaptured = false;
        c.pendingShotEdge = c.pendingReloadEdge = false;
        c.leftButtonDown = false;
        ++o.releases;
    };
    if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == windowId) {
        o.width = static_cast<float>(event.window.data1 > 1 ? event.window.data1 : 1);
        o.height = static_cast<float>(event.window.data2 > 1 ? event.window.data2 : 1);
    }
    if (((event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) && event.window.windowID != windowId) ||
        (event.type == SDL_EVENT_KEY_DOWN && event.key.windowID != windowId) ||
        ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
            event.button.windowID != windowId)) return;
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MOVED ||
        event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_MINIMIZED) {
        c.windowInteraction = true;
        releasePointer();
    } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_TAB && !event.key.repeat &&
               focused && !c.windowInteraction) {
        if (c.inputCaptured) { c.windowInteraction = true; releasePointer(); }
        else { c.inputCaptured = true; c.pointerAcquiredThisFrame = true; }
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
               focused && !c.windowInteraction &&
               event.button.x >= 0 && event.button.x < o.width && event.button.y >= 0 && event.button.y < o.height) {
        const bool rising = !c.leftButtonDown;
        c.leftButtonDown = true;
        if (!c.inputCaptured) {
            c.inputCaptured = true;
            c.pointerAcquiredThisFrame = true;
        } else if (rising && !c.pointerAcquiredThisFrame) c.pendingShotEdge = true;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
        c.leftButtonDown = false;
    }
}

struct Harness {
    std::unique_ptr<Engine::Platform::Sdl::SdlPlatform> platform;
    std::unique_ptr<Engine::Input::Backend::Sdl::SdlInput> input;
    SDL_WindowID window{};
    int windowWidth{}, windowHeight{};
    OracleState oracle;
    PointerCaptureState current;

    Harness() {
        auto created = Engine::Platform::Sdl::SdlPlatform::Create();
        REQUIRE(created);
        platform = std::move(created.value());
        input = std::make_unique<Engine::Input::Backend::Sdl::SdlInput>(*platform);
        window = SDL_GetWindowID(platform->NativeWindow());
        REQUIRE(SDL_GetWindowSize(platform->NativeWindow(), &windowWidth, &windowHeight));
        oracle.width = static_cast<float>(windowWidth);
        oracle.height = static_cast<float>(windowHeight);
    }

    // One ProcessEvents: both paths start with the per-frame flags cleared.
    unsigned Frame(const std::vector<SDL_Event>& events) {
        for (auto* c : {&oracle.capture, &current}) {
            c->pointerAcquiredThisFrame = c->windowInteraction = false;
            c->pendingShotEdge = false;
        }
        oracle.releases = 0;
        input->BeginFrame();
        const bool focusedAtStart = input->Snapshot().windowFocused;
        for (const auto& event : events) {
            input->HandleEvent(event);
            OracleHandle(oracle, event, window, input->Snapshot().windowFocused);
        }
        input->EndFrame();
        return ApplyPointerCaptureEvents(current, input->Snapshot().events, focusedAtStart).releases;
    }

    std::string Mismatch(const unsigned releases) const {
        const auto& a = oracle.capture;
        const auto& b = current;
        std::string out;
        const auto field = [&](const char* name, bool x, bool y) {
            if (x != y) out += std::string(name) + " oracle=" + (x ? "1" : "0") + " current=" + (y ? "1" : "0") + "; ";
        };
        field("inputCaptured", a.inputCaptured, b.inputCaptured);
        field("leftButtonDown", a.leftButtonDown, b.leftButtonDown);
        field("pendingShotEdge", a.pendingShotEdge, b.pendingShotEdge);
        field("pendingReloadEdge", a.pendingReloadEdge, b.pendingReloadEdge);
        field("pointerAcquiredThisFrame", a.pointerAcquiredThisFrame, b.pointerAcquiredThisFrame);
        field("windowInteraction", a.windowInteraction, b.windowInteraction);
        if (oracle.releases != releases)
            out += "released oracle=" + std::to_string(oracle.releases) + " current=" + std::to_string(releases) + "; ";
        return out;
    }
};

SDL_Event WindowEvent(SDL_EventType type, SDL_WindowID window, int data1 = 0, int data2 = 0) {
    SDL_Event e{};
    e.type = type;
    e.window.windowID = window;
    e.window.data1 = data1;
    e.window.data2 = data2;
    return e;
}

SDL_Event Key(bool down, SDL_WindowID window, SDL_Scancode scancode, bool repeat = false) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = window;
    e.key.scancode = scancode;
    e.key.down = down;
    e.key.repeat = repeat;
    return e;
}

SDL_Event Button(bool down, SDL_WindowID window, Uint8 button, float x, float y) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID = window;
    e.button.button = button;
    e.button.down = down;
    e.button.x = x;
    e.button.y = y;
    return e;
}

// Deterministic generator: the sequences are part of the test.
struct Lcg {
    std::uint64_t state;
    std::uint32_t Next(std::uint32_t bound) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33) % bound;
    }
};

SDL_Event RandomEvent(Lcg& random, const Harness& h) {
    const SDL_WindowID other = h.window + 1;
    const float w = static_cast<float>(h.windowWidth), ht = static_cast<float>(h.windowHeight);
    const SDL_WindowID target = random.Next(8) == 0 ? other : h.window;
    // Window events interrupt everything, so they are rarer than clicks;
    // otherwise shot edges would hardly ever be reached.
    switch (random.Next(28)) {
    case 0: case 1: return WindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED, target);
    case 2: return WindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST, target);
    case 3: return WindowEvent(SDL_EVENT_WINDOW_MOVED, target, 10, 20);
    // Resizes report the real window size: a synthetic event does not resize
    // the window, and declared difference 2 is tested on its own.
    case 4: return WindowEvent(SDL_EVENT_WINDOW_RESIZED, target, h.windowWidth, h.windowHeight);
    case 5: return WindowEvent(SDL_EVENT_WINDOW_MINIMIZED, target);
    case 6: return WindowEvent(SDL_EVENT_WINDOW_RESTORED, target);
    case 7: case 8: return Key(true, target, SDL_SCANCODE_TAB, random.Next(4) == 0);
    case 9: return Key(false, target, SDL_SCANCODE_TAB);
    case 10: case 11: return Key(random.Next(2) == 0, target, SDL_SCANCODE_W, random.Next(4) == 0);
    case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: {
        const bool inside = random.Next(5) != 0;
        const float x = inside ? static_cast<float>(random.Next(static_cast<std::uint32_t>(w))) : w + 3.0f;
        const float y = inside ? static_cast<float>(random.Next(static_cast<std::uint32_t>(ht))) : -1.0f;
        return Button(true, target, SDL_BUTTON_LEFT, x, y);
    }
    case 20: case 21: case 22: case 23: case 24: return Button(false, target, SDL_BUTTON_LEFT, 5, 5);
    case 25: return Button(random.Next(2) == 0, target, SDL_BUTTON_RIGHT, 5, 5);
    default: {
        SDL_Event e{};
        e.type = SDL_EVENT_MOUSE_MOTION;
        e.motion.windowID = target;
        e.motion.xrel = 3;
        return e;
    }
    }
}

} // namespace

TEST_CASE("Pointer capture: hand-written sequences match the pre-batch native handling") {
    Harness h;
    const auto w = h.window;
    const std::vector<std::vector<SDL_Event>> frames{
        {WindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED, w)},
        {Button(true, w, SDL_BUTTON_LEFT, 10, 10)},                              // capture
        {Button(false, w, SDL_BUTTON_LEFT, 10, 10), Button(true, w, SDL_BUTTON_LEFT, 10, 10)},  // shot edge
        {Button(true, w, SDL_BUTTON_LEFT, 10, 10), Button(false, w, SDL_BUTTON_LEFT, 10, 10),
         Button(true, w, SDL_BUTTON_LEFT, 10, 10)},                              // press, release, press
        {Key(true, w, SDL_SCANCODE_TAB)},                                        // Tab releases
        {Key(true, w, SDL_SCANCODE_TAB, true)},                                  // a repeat does nothing
        {Key(true, w, SDL_SCANCODE_TAB)},                                        // Tab captures
        {WindowEvent(SDL_EVENT_WINDOW_MOVED, w, 1, 2), Key(true, w, SDL_SCANCODE_TAB)},  // interaction blocks Tab
        {Button(true, w, SDL_BUTTON_LEFT, 10, 10), WindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST, w),
         Button(true, w, SDL_BUTTON_LEFT, 10, 10)},                              // capture, lose focus, unfocused click
        {WindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED, w), Button(true, w, SDL_BUTTON_LEFT, 10, 10)},  // focus then click
        {Button(true, w + 1, SDL_BUTTON_LEFT, 10, 10), WindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST, w + 1)},  // other window
        {Button(true, w, SDL_BUTTON_LEFT, -1, 10), Button(true, w, SDL_BUTTON_RIGHT, 10, 10)},  // outside; right button
        {WindowEvent(SDL_EVENT_WINDOW_RESIZED, w, h.windowWidth, h.windowHeight)},
        {WindowEvent(SDL_EVENT_WINDOW_MINIMIZED, w), WindowEvent(SDL_EVENT_WINDOW_RESTORED, w)},
    };
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const unsigned releases = h.Frame(frames[i]);
        INFO("frame " << i);
        CHECK(h.Mismatch(releases) == "");
    }
}

TEST_CASE("Pointer capture: seeded random sequences match the pre-batch native handling") {
    for (std::uint64_t seed : {1ULL, 2ULL, 3ULL, 42ULL, 2026ULL}) {
        Harness h;
        Lcg random{seed};
        std::size_t mismatches = 0;
        std::string first;
        unsigned captures = 0, shots = 0, releases = 0;
        for (int frame = 0; frame < 3000; ++frame) {
            std::vector<SDL_Event> events;
            const auto count = random.Next(7);
            for (std::uint32_t i = 0; i < count; ++i) events.push_back(RandomEvent(random, h));
            const unsigned released = h.Frame(events);
            captures += h.current.pointerAcquiredThisFrame;
            shots += h.current.pendingShotEdge;
            releases += released > 0;
            const auto mismatch = h.Mismatch(released);
            if (!mismatch.empty() && mismatches++ == 0) first = "frame " + std::to_string(frame) + ": " + mismatch;
        }
        INFO("seed " << seed << " first mismatch: " << first);
        CHECK(mismatches == 0);
        // The sequences exercise every outcome.
        CHECK(captures > 50);
        CHECK(shots > 50);
        CHECK(releases > 50);
    }
}

TEST_CASE("Pointer capture: declared difference 2, inside-window uses the size when SDL handles the click") {
    // A click queued before a resize that SDL has already applied: the old
    // handling used the product's last seen size, the new one the window's
    // size when the click is handled.
    Harness h;
    const auto w = h.window;
    h.Frame({WindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED, w)});
    const float x = static_cast<float>(h.windowWidth) - 10.0f;  // inside the old size
    REQUIRE(SDL_SetWindowSize(h.platform->NativeWindow(), h.windowWidth / 2, h.windowHeight / 2));
    int newWidth{}, newHeight{};
    REQUIRE(SDL_GetWindowSize(h.platform->NativeWindow(), &newWidth, &newHeight));
    REQUIRE(static_cast<float>(newWidth) <= x);  // outside the new size
    const unsigned releases = h.Frame({Button(true, w, SDL_BUTTON_LEFT, x, 10)});
    CHECK(h.oracle.capture.inputCaptured);     // old: inside its stale size
    CHECK_FALSE(h.current.inputCaptured);      // new: outside the real size
    CHECK(releases == 0);
}

TEST_CASE("Pointer capture: only a release of a captured pointer is reported as one") {
    // The product logs "releasing pointer" only for a captured pointer, as before.
    using Engine::Input::InputEvent;
    using Engine::Input::InputEventKind;
    PointerCaptureState state;
    const InputEvent click{.kind = InputEventKind::MouseButtonPressed, .button = Engine::Input::MouseButton::Left,
                           .x = 1, .y = 1, .insideWindow = true};
    const InputEvent moved{.kind = InputEventKind::WindowMoved};

    auto result = ApplyPointerCaptureEvents(state, std::vector{click}, true);
    CHECK(state.inputCaptured);
    CHECK(result.releases == 0);

    state.pointerAcquiredThisFrame = state.windowInteraction = false;
    result = ApplyPointerCaptureEvents(state, std::vector{moved, moved}, true);
    CHECK_FALSE(state.inputCaptured);
    CHECK(result.releases == 2);
    CHECK(result.releasesWhileCaptured == 1);
}
