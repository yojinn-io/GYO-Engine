#pragma once

// sdl_gpu (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"; SDL declaration sections 3.3, 9.1, 9.2, 10.2 R2a;
// Q7 = (a)): draws render's RenderCommands on the window through SDL_GPU. Renderer side: it joins the World that
// draws, beside render_world.
//
// - Configure (Product level, once): takes the window sdl_window made (SDL::NativeWindows in the Product's buffer)
//   and, through its adapter, makes the GPU device, claims the window and makes the 2D pass's shaders and pipelines.
//   Made here, so a replaced World's Build does not make the device again. No window (sdl_window not carried, or it
//   could not make one) is the startup error SDL-E2; a device that cannot be made is SDL-E3. Puts SDLGPUStatus in the
//   Product's buffer. OnDestroy (after every World, before sdl_window's: the reverse order) lets the device go.
// - Build (the World that draws; on the main thread): registers its handle Resource (bound to the thread) and mounts
//   GPUSubmit in render_world's Submit stage. Each prepared World gets its own number: when a World other than the
//   last one that drew submits, the earlier World's textures go first (a new World's texture ids start again at 1).
// - GPUSubmit, each frame (Bevy's render_system, bevy_render/src/renderer/mod.rs:82): applies this frame's texture
//   asks, draws the passes on the window and shows the picture, through the adapter
//   (v8-arch-gyo2-sdl-system-calls-via-adapter). It only reads the render World (SDL-5). What went wrong is told
//   through SDLGPUStatus and is never the System's error: a System's error would break the World and end the
//   Product (O-11), and a minimized window or a refused texture is no reason for that.
//
// One World of a Product lists sdl_gpu (one window, one texture table): a second World that draws is not named by any
// Feature yet (SDL declaration section 10.2, R2a).

#include <cstdint>
#include <memory>
#include <string>

#include "engine/orchestration/Plugin.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Plugins::SDLGPU {

class Service;

// What became of the frames, in the Product's buffer (the counts are since the device was made).
class SDLGPUStatus final {
public:
    explicit SDLGPUStatus(Service& service) : service_(&service) {}
    // Frames drawn and handed to the window.
    [[nodiscard]] std::uint64_t FramesShown() const;
    // Frames the window gave no picture for (minimized, hidden): nothing drawn, the texture asks applied.
    [[nodiscard]] std::uint64_t FramesSkipped() const;
    // Frames SDL refused a step of; LastFailure() is the last refusal's text.
    [[nodiscard]] std::uint64_t FramesFailed() const;
    [[nodiscard]] const std::string& LastFailure() const;
    // Texture asks SDL refused (that texture has no GPU texture), draws skipped because their texture has none, passes
    // whose target is not mapped (only target 0, the window, is).
    [[nodiscard]] std::uint64_t RefusedTextureAsks() const;
    [[nodiscard]] std::uint64_t SkippedDraws() const;
    [[nodiscard]] std::uint64_t UnmappedPasses() const;
    // The GPU driver SDL chose ("vulkan", "direct3d12", "metal").
    [[nodiscard]] const std::string& DriverName() const;

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
