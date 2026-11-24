#pragma once

// sdl_gpu's adapter interface (Design Invariant SDL-1; v8-arch-gyo2-sdl-system-calls-via-adapter): what the unit and
// GPUSubmit call, in GYO's words. Implemented in src/adapters/sdl/GPUDevice.cpp, the only place of sdl_gpu's runtime
// code that includes SDL3/. Every call is on the main thread (the thread that made the window); the adapter asserts
// it (SDL-4).

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "plugins/sdl/NativeWindow.hpp"

namespace GYO::Plugins::SDLGPU {

// Why the device could not be made (SDL's text in message). A Failure (E2, naming N25): nothing was made; Configure
// reports it as the startup error SDL-E3.
enum class DeviceFailureCode : std::uint8_t {
    // SDL's video could not start (Vulkan needs it even without a window).
    VideoFailed = 1,
    // No GPU driver takes the compiled shaders' formats.
    DeviceFailed,
    // The device could not draw to the window.
    ClaimFailed,
    // The 2D pass's shaders, pipelines, samplers or white texture could not be made.
    SetupFailed,
};
[[nodiscard]] constexpr const char* ToString(const DeviceFailureCode code) noexcept {
    switch (code) {
    case DeviceFailureCode::VideoFailed: return "VideoFailed";
    case DeviceFailureCode::DeviceFailed: return "DeviceFailed";
    case DeviceFailureCode::ClaimFailed: return "ClaimFailed";
    case DeviceFailureCode::SetupFailed: return "SetupFailed";
    }
    return "Unknown";
}
using DeviceFailure = Engine::Base::Error<DeviceFailureCode>;

// What became of one frame (SDL declaration section 10.2, "GPUSubmit 的錯誤怎麼出去": none of these is the System's
// error; each is told through SDLGPUStatus).
enum class FrameOutcome : std::uint8_t {
    // Drawn and handed to the window to show.
    Shown = 1,
    // The window gave no picture to draw on (minimized, hidden or occluded): nothing drawn, the asks applied.
    Skipped,
    // SDL refused a step (the text in LastError()); what is applied stays applied.
    Failed,
};

// Why a picture could not be read back (SDL's text in message): a step was refused.
enum class CaptureFailureCode : std::uint8_t {
    Refused = 1,
};
[[nodiscard]] constexpr const char* ToString(const CaptureFailureCode code) noexcept {
    switch (code) {
    case CaptureFailureCode::Refused: return "Refused";
    }
    return "Unknown";
}
using CaptureFailure = Engine::Base::Error<CaptureFailureCode>;

// A picture read back from the GPU: RGBA8, row 0 on top, no padding (image's layout).
struct CapturedImage final {
    Engine::Render::TargetSize size;
    std::vector<std::uint8_t> pixels;
};

// What happened to the asks and draws of one frame that SDL or the data did not allow (counted, the frame goes on).
struct FrameTally final {
    // Texture asks SDL refused (a texture too large, no memory): the id has no GPU texture; draws naming it are
    // skipped.
    std::uint64_t refusedAsks = 0;
    // Draws skipped: their texture has no GPU texture, or their pipeline could not be made.
    std::uint64_t skippedDraws = 0;
    // Passes whose target has no picture here (the window is target 0; nothing else is mapped yet).
    std::uint64_t unmappedPasses = 0;
    // SDL's text for the last refusal of this frame (a texture, a pipeline); empty when none.
    std::string lastRefusal;
};

class GPUDevice final {
public:
    // Starts SDL's video (with GYO's hints, as sdl_window does), makes the GPU device for the formats the shaders were
    // compiled to, claims the window when one is given (nullptr: a device without a window, which draws only to
    // Capture) and makes the 2D pass's shaders, pipelines, samplers and white texture. debug: SDL's GPU validation (the
    // tests; SDL warns and goes on where the driver has none).
    [[nodiscard]] static Engine::Base::Result<std::unique_ptr<GPUDevice>, DeviceFailure> Create(SDL::NativeWindow* window,
                                                                                                bool debug = false);
    ~GPUDevice();
    GPUDevice(const GPUDevice&) = delete;
    GPUDevice& operator=(const GPUDevice&) = delete;

    // Applies the asks in order, then draws the passes on the window and shows the picture. Target 0 is the window.
    FrameOutcome Submit(std::span<const Engine::Render::TextureOp> asks, const Engine::Render::RenderCommands& commands,
                        FrameTally& tally);
    // Applies the asks, draws the passes on a picture of this size instead of the window (target 0 is the picture)
    // and reads it back, waiting for the GPU. A zero size is a Programmer Error.
    [[nodiscard]] Engine::Base::Result<CapturedImage, CaptureFailure> Capture(std::span<const Engine::Render::TextureOp> asks,
                                                                           const Engine::Render::RenderCommands& commands,
                                                                           Engine::Render::TargetSize size, FrameTally& tally);
    // Lets every texture of the asks go (a new World's ids start again at 1). The white texture stays.
    void ForgetTextures();
    // The textures that have a GPU texture now (the white one not counted).
    [[nodiscard]] std::size_t TextureCount() const noexcept;
    // The GPU driver SDL chose ("vulkan", "direct3d12", "metal").
    [[nodiscard]] const std::string& DriverName() const noexcept;
    [[nodiscard]] const std::string& LastError() const noexcept { return lastError_; }

private:
    struct State;
    explicit GPUDevice(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    std::string lastError_;
};

} // namespace GYO::Plugins::SDLGPU
