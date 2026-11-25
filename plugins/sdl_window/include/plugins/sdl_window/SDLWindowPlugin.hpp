#pragma once

// sdl_window (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_window"; SDL declaration sections 3.3, 6.2, 9.1, 9.2):
// the window, the OS events and the window's runner, through SDL. It translates and holds no state of its own on the
// event side (Design Invariant SDL-7): SDL events become input's and window's Messages, the game's requests become
// SDL calls. The input and window Modules keep the state.
//
// - Configure (Product level, once): starts SDL's video and makes the window from window's WindowSettings (product
//   options), puts the window's opaque handle in the Product's buffer (SDL::NativeWindows) and an SDLWindowPump for a
//   product's own runner, and installs the window's runner unless SDLWindowOptions::installRunner is false. When the
//   Product is destroyed (OnDestroy, after every World) the window is destroyed and SDL's video stops.
// - Build (the one World that lists it; on the main thread): registers its handle Resource (bound to the thread),
//   mounts WriteInputMessages and WriteWindowMessages in Main/PreUpdate `before` input's and window's sets
//   (v8-arch-gyo2-sdl-input-writer-preupdate), and ApplyWindowRequests in Main/Last.
// - The runner, each frame: pumps the OS events into sdl_window's queue, runs the Product's frame, and ends when the
//   game's WindowRequest::close was applied (Success) or the frame failed (Failure). It does not look at the events.
//   While the OS holds the pump in its modal loop (a live resize, a dragged title bar) it runs the frame from SDL's
//   live-resize expose (SDLWindowOptions::liveFrame; SDL declaration R3). The Worlds named in ownThreadWorlds it
//   lends to threads of their own (O-13; R5): each runs its rounds there, waiting for its next fixed step, while the
//   main thread pumps and runs the others; they are stopped, joined and returned before the runner returns. Between
//   two frames it lets the product replace Worlds (SDLWindowOptions::betweenFrames), the lent ones taken back first.
//
// One World of a Product lists sdl_window (SDL declaration section 9.1): its queue is one, so a second World would
// take nothing, and the requests of the World that runs later would win. Another World that wants input gets it
// from the author, who reads the Messages here and writes them there (section 9.2 point 2).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/Plugin.hpp"
#include "engine/orchestration/Product.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Plugins::SDLWindow {

class Service;

// The unit of input's MouseMotion (SDLWindowOptions::mouseMotion). The game cannot convert the backend's units itself:
// it does not know them (they differ by platform and by relative mode), so the choice is made here, where they are
// known. winit, under Bevy, has no such choice: DeviceEvent::MouseMotion is always the device's raw motion and
// CursorMoved the cursor's place (bevy_winit/src/state.rs:459); SDL 3.4 has no raw motion outside relative mode, and
// a game that draws its own cursor while the cursor is locked (to push past the window's edge) needs the
// cursor's speed, not the device's counts.
enum class MouseMotionUnit : std::uint8_t {
    // As SDL gives it: while the cursor is locked (CursorGrab::Locked), the device's own motion; otherwise the
    // cursor's motion. For a camera turned by the mouse.
    Device = 1,
    // Logical pixels, with the OS's pointer speed also while the cursor is locked (SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE,
    // held while the window lives): what the cursor would have moved. For a cursor the game draws.
    LogicalPixels,
};

// The product's World replacements between two frames of the window's runner (O-8; a scene change: Godot's
// SceneTree flushes its pending scene after a frame's processing the same way). Replacing needs every World in the
// Product's hands (O-13: a lent one is refused), and the lent Worlds' threads are the runner's, so the runner offers
// the moment: after a frame, on the main thread, it asks `wanted`; when that is true it takes the Worlds of
// ownThreadWorlds back (each finishes its round), calls `run`, and lends them again. The live frame (the OS's modal
// loop) never replaces.
struct SDLWindowBetweenFrames final {
    // Whether the product wants to replace Worlds now; called after every frame, the lent Worlds still lent (it reads
    // only the main thread's Worlds and the Product's buffer). Empty: never.
    std::function<bool(Engine::Orchestration::Product&)> wanted;
    // The replacements (Product::ReplaceWorld), with every World in the Product's hands. An Err (a refused
    // replacement passed on, or the product's own) ends the runner with a Failure carrying it.
    std::function<Engine::Base::Result<void, Engine::Orchestration::OrchestrationError>(Engine::Orchestration::Product&)> run;
};

// The Plugin's settings (ProductDescription::options; none given: these defaults). The window itself is described
// by window's WindowSettings.
struct SDLWindowOptions final {
    // false: no runner; the product's runner calls SDLWindowPump::Pump before each RunFrame.
    bool installRunner = true;
    // Messages waiting at once, input's and window's apart (each OS event becomes its Messages together); an event
    // that does not fit is dropped and counted. Each at most the kernel's Message capacity (1024).
    std::size_t queueCapacity = 1024;
    std::size_t windowQueueCapacity = 256;
    // The runner waits for an OS event when a frame took less than this (zero: no wait; drawing to the window waits
    // for the display instead, batch R2a).
    std::chrono::nanoseconds minFrameInterval{0};
    // The live frame (R3): the runner runs a frame while the OS holds the pump in its modal loop. false: the picture
    // stops while the window is resized or dragged (the Worlds of ownThreadWorlds go on).
    bool liveFrame = true;
    // The Worlds the runner lends to a thread of its own each (O-13; R5; live-frame-sim-thread declaration 4.3): a
    // World with a fixed step whose rounds must go on while the main thread is held (a networked client's
    // simulation). Each must be lendable (Product::Lend: not an Extract source or reader). Empty: every World on the
    // main thread.
    std::vector<std::string> ownThreadWorlds;
    // A lent World without a fixed step to wait for (or paused, or at scale 0) waits this long between rounds.
    std::chrono::nanoseconds ownThreadIdle{std::chrono::milliseconds(4)};
    // The product's World replacements between two frames (none: the runner replaces nothing).
    SDLWindowBetweenFrames betweenFrames;
    // Longer clipboard texts are cut (window's ClipboardChanged::truncated).
    std::size_t clipboardLimit = 1U << 20U;
    // SDL_HINT_IME_IMPLEMENTED_UI "composition": the program draws the input method's composition itself (Windows).
    // Read once per process, before the window is made.
    bool drawComposition = false;
    // The unit of input's MouseMotion (MouseMotionUnit).
    MouseMotionUnit mouseMotion = MouseMotionUnit::Device;
};

// Where the Systems run in the World that lists sdl_window (WorldDescription::options; none given: the default
// skeleton). The writers must be in input's and window's schedule and stage (they are ordered before their sets).
struct SDLWindowWorldOptions final {
    std::string schedule = std::string(Engine::Orchestration::DefaultSchedules::frame);
    std::string writeStage = std::string(Engine::Orchestration::DefaultSchedules::preUpdate);
    std::string applyStage = std::string(Engine::Orchestration::DefaultSchedules::last);
};

// For a product's own runner (SDLWindowOptions::installRunner false), in the Product's buffer.
class SDLWindowPump final {
public:
    explicit SDLWindowPump(Service& service) : service_(&service) {}
    // Takes every pending OS event into sdl_window's queue; call it before each RunFrame, on the main thread.
    void Pump();
    // The live frame for a product's own runner (R3): called from inside Pump while the OS holds it in its modal loop
    // (SDLWindowOptions::liveFrame does it for the Plugin's runner). Empty: none.
    void SetLiveFrame(std::function<void()> frame);
    // The game's WindowRequest::close was applied: the window is closed and the loop should end.
    [[nodiscard]] bool Closed() const;
    // OS events dropped because the queue was full, since the window was made.
    [[nodiscard]] std::uint64_t DroppedEvents() const;
    // SDL refused a request this many times (each is tried again next frame); the last refusal's text.
    [[nodiscard]] std::uint64_t RefusedRequests() const;
    [[nodiscard]] const std::string& LastRefusal() const;

private:
    Service* service_;
};

class SDLWindowPlugin final : public Engine::Orchestration::Plugin {
public:
    SDLWindowPlugin();
    ~SDLWindowPlugin() override;

    void Configure(Engine::Orchestration::ProductSetup& setup) override;
    void Build(Engine::Orchestration::WorldSetup& setup) override;

private:
    // Made in Configure; the unit outlives the Product's buffer and every World (O-15).
    std::unique_ptr<Service> service_;
};

} // namespace GYO::Plugins::SDLWindow
