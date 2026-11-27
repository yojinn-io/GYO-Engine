#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

#include <string>
#include <utility>

#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/render/Presentation.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderMeshes.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "plugins/sdl/NativeWindow.hpp"

#include "GPUDevice.hpp"
#include "RenderContracts.hpp"

namespace GYO::Plugins::SDLGPU {

namespace O = Engine::Orchestration;
namespace R = Engine::Render;
namespace RW = Engine::RenderWorld;

// sdl_gpu's Product-level state (not public): the adapter and the counts. Held by the unit (made in Configure, the
// device let go in OnDestroy); the World holds only a handle Resource pointing here.
class Service final {
public:
    explicit Service(std::unique_ptr<GPUDevice> device) : device_(std::move(device)) {}

    // world: which World draws (its handle's number). A World other than the last one that drew has its own texture
    // and mesh ids, starting again at 1: the textures and meshes of the earlier World go first. A World whose
    // replacement failed keeps drawing with its own number, and nothing was forgotten (the new one never drew).
    // presentation: the World's (render_world's; nullptr when it has none: left as it is). Set only when it differs
    // from what was asked last (a refusal is not tried again until it changes: the swapchain waits for the GPU).
    void Submit(const std::uint64_t world, const R::RenderCommands& commands, const R::RenderTextures& textures,
                const R::RenderMeshes& meshes, const R::Presentation* presentation) {
        if (!device_) return;
        if (world != drawingWorld_) {
            device_->ForgetMeshes();
            device_->ForgetTextures();
            drawingWorld_ = world;
        }
        if (presentation != nullptr && presentation->presentMode != askedPresentMode_) {
            askedPresentMode_ = presentation->presentMode;
            ++presentModeSets_;
            if (!device_->SetPresentMode(askedPresentMode_)) {
                ++refusedPresentModes_;
                lastFailure_ = device_->LastError();
            }
        }
        RequireRenderContracts(commands, textures);
        FrameTally tally;
        switch (device_->Submit(textures.Pending(), meshes.Pending(), commands, tally)) {
        case FrameOutcome::Shown: ++shown_; break;
        case FrameOutcome::Skipped: ++skipped_; break;
        case FrameOutcome::Failed:
            ++failed_;
            lastFailure_ = device_->LastError();
            break;
        }
        if (!tally.lastRefusal.empty()) lastFailure_ = tally.lastRefusal;
        refusedAsks_ += tally.refusedAsks;
        refusedMeshAsks_ += tally.refusedMeshAsks;
        skippedDraws_ += tally.skippedDraws;
        unmappedPasses_ += tally.unmappedPasses;
    }
    // The number of a newly prepared World.
    std::uint64_t NextWorld() noexcept { return ++preparedWorlds_; }
    void Shutdown() { device_.reset(); }
    [[nodiscard]] std::size_t TextureCount() const noexcept { return device_ ? device_->TextureCount() : 0; }
    [[nodiscard]] std::size_t MeshCount() const noexcept { return device_ ? device_->MeshCount() : 0; }
    [[nodiscard]] SwapchainPresentMode ActivePresentMode() const noexcept {
        return device_ ? device_->ActivePresentMode() : SwapchainPresentMode::Vsync;
    }

    std::uint64_t shown_ = 0;
    std::uint64_t skipped_ = 0;
    std::uint64_t failed_ = 0;
    std::uint64_t refusedAsks_ = 0;
    std::uint64_t refusedMeshAsks_ = 0;
    std::uint64_t skippedDraws_ = 0;
    std::uint64_t unmappedPasses_ = 0;
    std::uint64_t presentModeSets_ = 0;
    std::uint64_t refusedPresentModes_ = 0;
    std::string lastFailure_;
    std::string driver_;

private:
    std::unique_ptr<GPUDevice> device_;
    std::uint64_t preparedWorlds_ = 0;
    std::uint64_t drawingWorld_ = 0;
    // SDL claims the window with VSYNC: AutoVsync is what the swapchain has before anything was asked.
    R::PresentMode askedPresentMode_ = R::PresentMode::AutoVsync;
};

namespace {

using SystemResult = Engine::Base::Result<void, Kernel::KernelError>;

// The World's handle (registered by Build; bound to the thread that puts it, DI-KS 14). Hashes nothing: it points at
// a service outside the World (as sdl_window's handle).
struct SDLGPUHandle final {
    Service* service = nullptr;
    // Which World this is, for the texture and mesh tables (Service::Submit).
    std::uint64_t world = 0;
};
inline void HashValue(Kernel::Hasher&, const SDLGPUHandle&) {}

// render_world's Submit stage: the frame's presentation, asks and passes to the window (read only, SDL-5).
SystemResult GPUSubmit(Kernel::Res<SDLGPUHandle> handle, Kernel::Res<R::RenderCommands> commands, Kernel::Res<R::RenderTextures> textures,
                       Kernel::Res<R::RenderMeshes> meshes, Kernel::Res<R::Presentation> presentation) {
    if (!handle.Present() || handle->service == nullptr || !commands.Present() || !textures.Present() || !meshes.Present()) return {};
    handle->service->Submit(handle->world, *commands, *textures, *meshes, presentation.Present() ? &*presentation : nullptr);
    return {};
}

const RW::RenderWorldOptions& RenderWorldOptionsOf(const RW::RenderWorldOptions* given) {
    static const RW::RenderWorldOptions defaults{};
    return given != nullptr ? *given : defaults;
}

} // namespace

std::uint64_t SDLGPUStatus::FramesShown() const { return service_->shown_; }
std::uint64_t SDLGPUStatus::FramesSkipped() const { return service_->skipped_; }
std::uint64_t SDLGPUStatus::FramesFailed() const { return service_->failed_; }
const std::string& SDLGPUStatus::LastFailure() const { return service_->lastFailure_; }
std::uint64_t SDLGPUStatus::RefusedTextureAsks() const { return service_->refusedAsks_; }
std::uint64_t SDLGPUStatus::RefusedMeshAsks() const { return service_->refusedMeshAsks_; }
std::uint64_t SDLGPUStatus::SkippedDraws() const { return service_->skippedDraws_; }
std::uint64_t SDLGPUStatus::UnmappedPasses() const { return service_->unmappedPasses_; }
const std::string& SDLGPUStatus::DriverName() const { return service_->driver_; }
SwapchainPresentMode SDLGPUStatus::ActivePresentMode() const { return service_->ActivePresentMode(); }
std::uint64_t SDLGPUStatus::PresentModeSets() const { return service_->presentModeSets_; }
std::uint64_t SDLGPUStatus::RefusedPresentModes() const { return service_->refusedPresentModes_; }
std::size_t SDLGPUStatus::TextureCount() const { return service_->TextureCount(); }
std::size_t SDLGPUStatus::MeshCount() const { return service_->MeshCount(); }

SDLGPUPlugin::SDLGPUPlugin() = default;
SDLGPUPlugin::~SDLGPUPlugin() = default;

void SDLGPUPlugin::Configure(O::ProductSetup& setup) {
    const SDL::NativeWindows* windows = setup.Buffer().Find<SDL::NativeWindows>();
    if (windows == nullptr || windows->main == nullptr) {
        // A startup error: no World is built (O-2). The service is still made, so Build finds it.
        setup.Report("SDL-E2", "sdl_gpu has no window to draw to: the product carries no sdl_window, or sdl_window could not make the window");
        service_ = std::make_unique<Service>(nullptr);
        return;
    }
    auto device = GPUDevice::Create(windows->main);
    if (!device) {
        setup.Report("SDL-E3", std::string("sdl_gpu could not make the GPU device (") + ToString(device.error().code) +
                                  "): " + device.error().message);
        service_ = std::make_unique<Service>(nullptr);
        return;
    }
    const std::string driver = (*device)->DriverName();
    service_ = std::make_unique<Service>(std::move(*device));
    service_->driver_ = driver;
    Service& service = *service_;
    setup.Buffer().Create<SDLGPUStatus>(service);
    // After every World (O-15), before sdl_window's OnDestroy (registered earlier, so run later): the device lets the
    // window go while it still exists.
    setup.OnDestroy([&service] { service.Shutdown(); });
}

void SDLGPUPlugin::Build(O::WorldSetup& setup) {
    const RW::RenderWorldOptions& options = RenderWorldOptionsOf(setup.Options<RW::RenderWorldOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<SDLGPUHandle>({.name = "SDLGPUHandle", .threadBound = true});
    Kernel::SystemOptions submit;
    submit.name = "GPUSubmit";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = std::string(RW::kSubmitStage);
    submit.mounts.push_back(std::move(mount));
    r.AddSystem<&GPUSubmit>(std::move(submit));
    // On the thread that prepares the World (the runner's, the main thread; SDL-4).
    Service* service = service_.get();
    setup.Prepare([service](Kernel::World& world) -> Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(SDLGPUHandle{service, service != nullptr ? service->NextWorld() : 0});
        return {};
    });
}

} // namespace GYO::Plugins::SDLGPU
