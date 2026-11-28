#pragma once

// sdl_gpu (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"; SDL declaration sections 3.3, 9.1, 9.2, 10.2 R2a;
// Q7 = (a)): draws render's RenderCommands on the window through SDL_GPU. Renderer side: it joins the World that
// draws, beside render_world.
//
// - Configure (Product level, once): takes the window sdl_window made (SDL::NativeWindows in the Product's buffer)
//   and, through its adapter, makes the GPU device, claims the window and makes the 2D and 3D passes' shaders.
//   Made here, so a replaced World's Build does not make the device again. No window (sdl_window not carried, or it
//   could not make one) is the startup error SDL-E2; a device that cannot be made is SDL-E3. Puts SDLGPUStatus in the
//   Product's buffer. OnDestroy (after every World, before sdl_window's: the reverse order) lets the device go.
// - Build (the World that draws; on the main thread): registers its handle Resource (bound to the thread) and mounts
//   GPUSubmit in render_world's Submit stage. Each prepared World gets its own number: when a World other than the
//   last one that drew submits, the earlier World's textures and meshes go first (a new World's ids start again at
//   1).
// - GPUSubmit, each frame (Bevy's render_system, bevy_render/src/renderer/mod.rs:82): sets the swapchain's present
//   mode when render's Presentation changed (Bevy's present_mode_changed, bevy_render/src/view/window/mod.rs:220),
//   applies this frame's texture and mesh asks, draws the passes (2D and 3D, in their order) on the window and shows
//   the picture, through the
//   adapter (v8-arch-gyo2-sdl-system-calls-via-adapter). It reads the render World and writes only the frame's
//   receipt, RenderFrameReceipt: what the frame got done (SDL-5). What went wrong is told
//   through SDLGPUStatus and is never the System's error: a System's error would break the World and end the
//   Product (O-11), and a minimized window or a refused texture is no reason for that.
//
// One World of a Product lists sdl_gpu (one window, one texture table, one mesh table): a second World that draws is
// not named by any Feature yet (SDL declaration section 10.2, R2a).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/orchestration/Plugin.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace GYO::Plugins::SDLGPU {

class Service;

// The present mode the window's swapchain has (SDL_GPUPresentMode in GYO's words), picked from render's Presentation
// (render's PresentMode falls back along a chain; window-mode-vsync declaration, section 4.3).
enum class SwapchainPresentMode : std::uint8_t {
    // Waits for the display (SDL_GPU_PRESENTMODE_VSYNC; every driver has it).
    Vsync = 1,
    // Shows a picture at once, may tear (SDL_GPU_PRESENTMODE_IMMEDIATE).
    Immediate,
    // The newest picture at the display's refresh, no tearing (SDL_GPU_PRESENTMODE_MAILBOX).
    Mailbox,
};
[[nodiscard]] constexpr const char* ToString(const SwapchainPresentMode mode) noexcept {
    switch (mode) {
    case SwapchainPresentMode::Vsync: return "Vsync";
    case SwapchainPresentMode::Immediate: return "Immediate";
    case SwapchainPresentMode::Mailbox: return "Mailbox";
    }
    return "Unknown";
}

// One pass of the last frame the device recorded (render-pipeline declaration 15.3). name: its debug group's name in
// a capture tool (RenderDoc, Xcode, PIX): its place in RenderCommands::passes, its kind and its target (PassLabel,
// src/PassLabels.hpp); the kind and the target are also in RenderCommands at that place. recorded: a render pass was
// recorded for it (false: its target is not mapped; the main target has no picture this frame, a skipped frame; or
// the frame's uploads failed and no pass was recorded). cpu: the time that went by on the CPU while the adapter
// recorded it (wall time on the recording thread, as Bevy's elapsed_cpu; not the System that made the pass); 0 when
// not recorded.
struct PassTiming final {
    std::string name;
    bool recorded = false;
    Engine::Time::Duration cpu{0};
};

// The last frame the device recorded: one PassTiming per pass of its RenderCommands, in their order. A frame
// recorded whether it was then handed to the GPU or not (a refused submission); a frame SDL gave no command buffer
// for is not recorded (the timing stays the one before: tell them apart by frame).
struct FrameTiming final {
    // The device's count of frames recorded (Submit and Capture), from 1.
    std::uint64_t frame = 0;
    std::vector<PassTiming> passes;
    // Debug groups pushed (and popped) on the frame's command buffer: one per recorded pass. The one thing of the
    // markers seen outside a capture tool (render-pipeline declaration 5.1: the count is what is checked).
    std::uint64_t debugGroups = 0;
};

// A whole frame's GPU time from its fence (SDL_GPU 3.4 has no timestamp query: render-pipeline declaration 4.7, Q1
// (a)): at most this long passed between handing the frame to the GPU and seeing it done. An upper bound: it holds the
// GPU's work on the frames before it and how late the device looked (a frame on the window is looked at by the next
// Submit, so within about a frame; a Capture waits for its own).
struct GPUFrameTime final {
    // As FrameTiming's.
    std::uint64_t frame = 0;
    Engine::Time::Duration atMost{0};
};

// What became of the frames, in the Product's buffer (the counts are since the device was made).
class SDLGPUStatus final {
public:
    explicit SDLGPUStatus(Service& service) : service_(&service) {}
    // Frames drawn and handed to the window.
    [[nodiscard]] std::uint64_t FramesShown() const;
    // Frames the window gave no picture for (minimized, hidden): nothing drawn on it, the texture asks applied, the
    // render targets drawn.
    [[nodiscard]] std::uint64_t FramesSkipped() const;
    // Frames SDL refused a step of. LastFailure(): SDL's text for the last refused frame, texture or pipeline.
    [[nodiscard]] std::uint64_t FramesFailed() const;
    [[nodiscard]] const std::string& LastFailure() const;
    // Texture asks SDL refused (that texture has no GPU texture; also the textures made in a frame whose uploads
    // failed), mesh asks SDL refused (the same for meshes), draws skipped (their texture or mesh has no GPU object,
    // their pipeline could not be made, or a 3D pass's depth texture could not be), passes whose target is not mapped
    // (neither the main target, the window, nor a render target of render's asks that SDL made), each once a frame;
    // draws that sample the target their pass draws on are skipped too.
    [[nodiscard]] std::uint64_t RefusedTextureAsks() const;
    [[nodiscard]] std::uint64_t RefusedMeshAsks() const;
    [[nodiscard]] std::uint64_t SkippedDraws() const;
    [[nodiscard]] std::uint64_t UnmappedPasses() const;
    // The GPU driver SDL chose ("vulkan", "direct3d12", "metal").
    [[nodiscard]] const std::string& DriverName() const;
    // The swapchain's present mode now (Vsync until render's Presentation asks for another).
    [[nodiscard]] SwapchainPresentMode ActivePresentMode() const;
    // Times the swapchain's present mode was set (each waits for the GPU): once per change of Presentation, never per
    // frame. Of these, the ones SDL refused (the text in LastFailure()); a refused one is not tried again until
    // Presentation changes.
    [[nodiscard]] std::uint64_t PresentModeSets() const;
    [[nodiscard]] std::uint64_t RefusedPresentModes() const;
    // The textures of render's asks that have a GPU texture now, render targets included (the white one not counted).
    [[nodiscard]] std::size_t TextureCount() const;
    // The meshes of render's asks that have GPU buffers now.
    [[nodiscard]] std::size_t MeshCount() const;
    // The last frame recorded: each pass's name, whether it was recorded and its CPU time, and the debug groups
    // (render-pipeline declaration 15), as it was then (a copy); none before the first frame or without the device.
    // To print, not judged here.
    [[nodiscard]] std::optional<FrameTiming> LastFrameTiming() const;
    // The latest frame seen done on the GPU and the upper bound of its GPU time; none before one is seen. Usually a
    // frame or two before LastFrameTiming's: nothing waits for the GPU to tell it.
    [[nodiscard]] std::optional<GPUFrameTime> LastGPUFrameTime() const;

private:
    Service* service_;
};

class SDLGPUPlugin final : public Engine::Orchestration::Plugin {
public:
    SDLGPUPlugin();
    ~SDLGPUPlugin() override;

    void Configure(Engine::Orchestration::ProductSetup& setup) override;
    void Build(Engine::Orchestration::WorldSetup& setup) override;

private:
    // Made in Configure; the unit outlives the Product's buffer and every World (O-15).
    std::unique_ptr<Service> service_;
};

} // namespace GYO::Plugins::SDLGPU
