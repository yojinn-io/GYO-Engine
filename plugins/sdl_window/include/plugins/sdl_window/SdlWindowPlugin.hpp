#pragma once

// sdl_window (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_window"; SDL declaration sections 3.3, 6.2, 9.1, 9.2):
// the window, the OS events and the window's runner, through SDL. It translates and holds no state of its own on the
// event side (Design Invariant SDL-7): SDL events become input's and window's Messages, the game's requests become
// SDL calls. The input and window Modules keep the state.
//
// - Configure (Product level, once): starts SDL's video and makes the window from window's WindowSettings (product
//   options), puts the window's opaque handle in the Product's buffer (Sdl::NativeWindows) and an SdlWindowPump for a
//   product's own runner, and installs the window's runner unless SdlWindowOptions::installRunner is false. When the
//   Product is destroyed (OnDestroy, after every World) the window is destroyed and SDL's video stops.
// - Build (the one World that lists it; on the main thread): registers its handle Resource (bound to the thread),
//   mounts WriteInputMessages and WriteWindowMessages in Main/PreUpdate `before` input's and window's sets
//   (v8-arch-gyo2-sdl-input-writer-preupdate), and ApplyWindowRequests in Main/Last.
// - The runner, each frame: pumps the OS events into sdl_window's queue, runs the Product's frame, and ends when the
//   game's WindowRequest::close was applied (Success) or the frame failed (Failure). It does not look at the events.
//
// One World of a Product lists sdl_window (SDL declaration section 9.1): its queue is one, so a second World would
// take nothing, and the requests of the World that runs later would win. Another World that wants input gets it
// from the author, who reads the Messages here and writes them there (section 9.2 point 2).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "engine/orchestration/Plugin.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Plugins::SdlWindow {

class Service;

// The Plugin's settings (ProductDescription::options; none given: these defaults). The window itself is described
// by window's WindowSettings.
struct SdlWindowOptions final {
    // false: no runner; the product's runner calls SdlWindowPump::Pump before each RunFrame.
    bool installRunner = true;
    // OS events waiting at once (each becomes its Messages together); more are dropped and counted. At most the
    // kernel's Message capacity (1024).
    std::size_t queueCapacity = 1024;
    // The runner waits for an OS event when a frame took less than this (zero: no wait; drawing to the window waits
    // for the display instead, batch R2a).
    std::chrono::nanoseconds minFrameInterval{0};
    // Longer clipboard texts are cut (window's ClipboardChanged::truncated).
    std::size_t clipboardLimit = 1U << 20U;
    // SDL_HINT_IME_IMPLEMENTED_UI "composition": the program draws the input method's composition itself (Windows).
    // Read once per process, before the window is made.
    bool drawComposition = false;
};

// Where the Systems run in the World that lists sdl_window (WorldDescription::options; none given: the default
// skeleton). The writers must be in input's and window's schedule and stage (they are ordered before their sets).
struct SdlWindowWorldOptions final {
    std::string schedule = std::string(Engine::Orchestration::DefaultSchedules::frame);
    std::string writeStage = std::string(Engine::Orchestration::DefaultSchedules::preUpdate);
    std::string applyStage = std::string(Engine::Orchestration::DefaultSchedules::last);
};

// For a product's own runner (SdlWindowOptions::installRunner false), in the Product's buffer.
class SdlWindowPump final {
public:
    explicit SdlWindowPump(Service& service) : service_(&service) {}
    // Takes every pending OS event into sdl_window's queue; call it before each RunFrame, on the main thread.
    void Pump();
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

class SdlWindowPlugin final : public Engine::Orchestration::Plugin {
public:
    SdlWindowPlugin();
    ~SdlWindowPlugin() override;

    void Configure(Engine::Orchestration::ProductSetup& setup) override;
    void Build(Engine::Orchestration::WorldSetup& setup) override;

private:
    // Made in Configure; the unit outlives the Product's buffer and every World (O-15).
    std::unique_ptr<Service> service_;
};

} // namespace GYO::Plugins::SdlWindow
