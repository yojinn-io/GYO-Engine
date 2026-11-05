#include "gyo/framework/Preview.hpp"

#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/base/Assert.hpp"
#include "engine/base/Error.hpp"
#include "engine/os/Files.hpp"
#include "engine/render/Renderer.hpp"
#include "engine/render/ShaderLibrary.hpp"
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"

#include <utility>

namespace Gyo::Tools::Framework {

namespace Render = Engine::Render;
using GpuDevice = Render::Backend::SdlGpu::SdlGpuRenderDevice;

namespace {

PreviewError Failed(const PreviewErrorCode code, std::string message, std::string detail) {
    return PreviewError::Make(code, std::move(message), std::move(detail));
}

} // namespace

struct OffscreenPreview::Impl final {
    Render::ShaderLibrary shaders;
    std::unique_ptr<GpuDevice> device;
    Render::Renderer renderer;
    Render::RenderQueue queue;
    bool frameOpen{};

    ~Impl() {
        // The renderer releases its device objects before the device goes.
        renderer.Reset();
    }
};

OffscreenPreview::OffscreenPreview(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
OffscreenPreview::~OffscreenPreview() = default;

Result<std::unique_ptr<OffscreenPreview>, PreviewError> OffscreenPreview::Create(
    Engine::Platform::Sdl::SdlPlatform& platform, const PreviewOptions& options) {
    auto impl = std::make_unique<Impl>();
    auto root = Engine::Os::Files::PathToUtf8(options.shaderBundle);
    if (!root) {
        return Engine::Base::Err(Failed(PreviewErrorCode::StartFailed, "the shader bundle path is not a UTF-8 path",
                                        Engine::Base::CauseDetail(root.error())));
    }
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Resolver::AssetPathResolver::Options resolver;
    resolver.assetsRoot = std::move(root.value());
    auto bundle = impl->shaders.AppendBundle(source, Engine::Asset::Resolver::AssetPathResolver(resolver));
    if (!bundle) {
        return Engine::Base::Err(Failed(PreviewErrorCode::StartFailed, "cannot load the shader bundle " + resolver.assetsRoot,
                                        Engine::Base::CauseDetail(bundle.error())));
    }
    Render::Backend::SdlGpu::SdlGpuOffscreenOptions device;
    device.width = options.width;
    device.height = options.height;
    device.driver = options.driver;
    device.availableShaderFormats = impl->shaders.CompleteFormats();
    auto created = GpuDevice::CreateOffscreen(platform, device);
    if (!created) {
        return Engine::Base::Err(Failed(PreviewErrorCode::StartFailed, "cannot create the offscreen GPU device",
                                        Engine::Base::CauseDetail(created.error())));
    }
    impl->device = std::move(created.value());
    auto initialized = impl->renderer.Initialize(*impl->device, impl->shaders);
    if (!initialized) {
        return Engine::Base::Err(Failed(PreviewErrorCode::StartFailed, "cannot start Engine's renderer",
                                        Engine::Base::CauseDetail(initialized.error())));
    }
    return std::unique_ptr<OffscreenPreview>(new OffscreenPreview(std::move(impl)));
}

GpuDevice& OffscreenPreview::Device() noexcept { return *impl_->device; }

Result<Render::RenderQueue*, PreviewError> OffscreenPreview::BeginFrame(const std::uint32_t width, const std::uint32_t height,
                                                                        const Render::PerspectiveCamera3D& camera,
                                                                        const Render::Color& clearColor) {
    GYO_ASSERT(!impl_->frameOpen && "OffscreenPreview::BeginFrame while a frame is open");
    // The device keeps the requested size and remakes its frame target only
    // when the size differs: nothing to compare here.
    auto sized = impl_->device->SetFrameSize(width, height);
    if (!sized) {
        return Engine::Base::Err(Failed(PreviewErrorCode::DrawFailed, "the preview cannot take that frame size",
                                        Engine::Base::CauseDetail(sized.error())));
    }
    impl_->queue.Reset({clearColor, {}});
    impl_->queue.SetCamera(camera);
    impl_->frameOpen = true;
    return &impl_->queue;
}

Result<Render::TextureReadback, PreviewError> OffscreenPreview::EndFrame() {
    GYO_ASSERT(impl_->frameOpen && "OffscreenPreview::EndFrame without an open frame");
    impl_->frameOpen = false;
    auto rendered = impl_->renderer.Render(impl_->queue);
    if (!rendered) {
        return Engine::Base::Err(Failed(PreviewErrorCode::DrawFailed, "Engine's renderer cannot draw the frame",
                                        Engine::Base::CauseDetail(rendered.error())));
    }
    auto read = impl_->device->ReadFrame();
    if (!read) {
        return Engine::Base::Err(Failed(PreviewErrorCode::DrawFailed, "cannot read the frame back",
                                        Engine::Base::CauseDetail(read.error())));
    }
    return std::move(read.value());
}

} // namespace Gyo::Tools::Framework
