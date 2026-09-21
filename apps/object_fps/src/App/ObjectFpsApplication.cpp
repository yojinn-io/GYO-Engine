#include "RetroFPS/App/ObjectFpsApplication.hpp"
#include "RetroFPS/App/CampaignContentLoader.hpp"
#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/App/ObjectFpsRuntimeClient.hpp"

#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/runtime/RuntimeLoop.hpp"
#include "input/backend/sdl/SdlInput.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "render/Renderer.hpp"
#include "render/ShaderLibrary.hpp"
#include "text/backend/sdl_ttf/SdlTtfTextRasterizer.hpp"

#include <SDL3/SDL.h>
#include <utility>

namespace fps {
namespace {
template<class Error> std::string Explain(const Error& error) {
    return error.message + (error.detail.empty() ? "" : ": " + error.detail);
}
}

struct ObjectFpsApplication::Impl final {
    Engine::Asset::AssetCatalog catalog;
    Engine::Asset::Loading::LoaderRegistry loaders;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy cache{{
        Engine::Asset::Core::AssetCachePolicy::Mode::KeepWhileReferenced, 120, true, 0, 0}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, cache};
    std::shared_ptr<const CampaignContent> content;
    Engine::Render::ShaderLibrary shaders;
    std::unique_ptr<Engine::Platform::Sdl::SdlPlatform> platform;
    std::unique_ptr<Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice> renderDevice;
    Engine::Render::Renderer renderer;
    std::unique_ptr<Engine::Text::Backend::SdlTtf::SdlTtfTextRasterizer> textRasterizer;
    ObjectFpsPresentation presentation;
    std::unique_ptr<Engine::Input::Backend::Sdl::SdlInput> input;
    std::unique_ptr<ObjectFpsRuntimeClient> client;
};

ObjectFpsApplication::ObjectFpsApplication() : impl_(std::make_unique<Impl>()) {}
ObjectFpsApplication::~ObjectFpsApplication() = default;

bool ObjectFpsApplication::InitializeContent(const std::filesystem::path& assetRoot, std::string& error) {
    namespace Asset = Engine::Asset;
    error.clear();
    auto manifest = Asset::ContentManifest::Load(assetRoot);
    if (!manifest) { error = Explain(manifest.error()); return false; }
    auto catalog = manifest.value().LoadCatalogs(assetRoot);
    if (!catalog) { error = Explain(catalog.error()); return false; }
    impl_->catalog = std::move(catalog).value();
    for (const auto& bundle : manifest.value().shaderBundles) {
        Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = (assetRoot / bundle.path).string();
        options.allowAbsolutePath = false;
        options.allowEscapeAssetsRoot = false;
        const auto loaded = impl_->shaders.AppendBundle(impl_->source,
            Asset::Resolver::AssetPathResolver(std::move(options)));
        if (!loaded) { error = Explain(loaded.error()); return false; }
    }
    if (!impl_->shaders.CompleteFormats()) {
        error = "Shader bundles have no complete common format"; return false;
    }
    for (const auto format : {Engine::Render::ShaderFormat::DXIL,
             Engine::Render::ShaderFormat::SPIRV, Engine::Render::ShaderFormat::Metallib}) {
        if (!(impl_->shaders.CompleteFormats() & Engine::Render::FormatBit(format))) continue;
        for (const auto* id : {"builtin/unlit", "builtin/scene_post"}) {
            const auto program = impl_->shaders.FindProgram(id, format);
            if (!program) { error = Explain(program.error()); return false; }
        }
    }
    const auto addLoader = [&](auto loader) {
        const auto registered = impl_->loaders.Register(std::move(loader));
        if (!registered) { error = Explain(registered.error()); return false; }
        return true;
    };
    if (!addLoader(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>()) ||
        !addLoader(std::make_unique<Asset::Loaders::FontLoader>()) ||
        !addLoader(std::make_unique<Asset::Loaders::TextLoader>()) ||
        !addLoader(std::make_unique<Asset::Loaders::SdlImage::SdlImageTextureLoader>())) return false;
    auto content = CampaignContentLoader::Load(impl_->assets);
    if (!content) { error = content.error; return false; }
    impl_->content = std::make_shared<const CampaignContent>(std::move(*content.content));
    return true;
}

bool ObjectFpsApplication::InitializeGraphics(const ObjectFpsApplicationOptions& options, std::string& error) {
    error.clear();
    if (!impl_->content) { error = "Game content must be initialized before graphics"; return false; }
    Engine::Platform::Sdl::SdlPlatformOptions platformOptions;
    platformOptions.title = options.title;
    platformOptions.width = options.width;
    platformOptions.height = options.height;
    platformOptions.resizable = false;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(platformOptions);
    if (!platform) { error = Explain(platform.error()); return false; }
    impl_->platform = std::move(platform).value();
    Engine::Render::Backend::SdlGpu::SdlGpuOptions gpuOptions;
    gpuOptions.availableShaderFormats = impl_->shaders.CompleteFormats();
    gpuOptions.driver = options.gpuDriver;
    auto device = Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice::Create(*impl_->platform, gpuOptions);
    if (!device) { error = Explain(device.error()); return false; }
    impl_->renderDevice = std::move(device).value();
    const auto ready = impl_->renderer.Initialize(*impl_->renderDevice, impl_->shaders);
    if (!ready) { error = Explain(ready.error()); return false; }
    const auto format = Engine::Render::ShaderFormatName(*impl_->renderer.ActiveShaderFormat());
    SDL_Log("GYO GPU: driver=%s, shader=%.*s, available_formats=%u, bundle=%s",
        impl_->renderDevice->GetInfo().driver.c_str(), static_cast<int>(format.size()), format.data(),
        impl_->renderDevice->GetInfo().shaderFormats, impl_->shaders.Version().c_str());
    auto text = Engine::Text::Backend::SdlTtf::SdlTtfTextRasterizer::Create();
    if (!text) { error = Explain(text.error()); return false; }
    impl_->textRasterizer = std::move(text).value();
    ObjectFpsPresentationConfig presentation;
    presentation.world = options.session.world;
    presentation.viewportWidth = static_cast<float>(options.width);
    presentation.viewportHeight = static_cast<float>(options.height);
    if (!impl_->presentation.Initialize(*impl_->renderDevice, impl_->renderer,
        *impl_->textRasterizer, impl_->assets, impl_->content, presentation, error)) return false;
    impl_->input = std::make_unique<Engine::Input::Backend::Sdl::SdlInput>(*impl_->platform);
    ObjectFpsRuntimeClientConfig runtime;
    runtime.viewportWidth = presentation.viewportWidth;
    runtime.viewportHeight = presentation.viewportHeight;
    impl_->client = std::make_unique<ObjectFpsRuntimeClient>(*impl_->platform,
        *impl_->input, impl_->assets, impl_->presentation, runtime);
    return impl_->client->Initialize(impl_->content, options.session, error);
}

int ObjectFpsApplication::Run() {
    Engine::Runtime::RuntimeLoop loop(*impl_->client);
    loop.Run();
    if (!impl_->client->LastError().empty())
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", impl_->client->LastError().c_str());
    return impl_->client->ExitCode();
}

const Engine::Asset::AssetCatalog& ObjectFpsApplication::Catalog() const { return impl_->catalog; }
Engine::Asset::AssetManager& ObjectFpsApplication::Assets() { return impl_->assets; }
const std::shared_ptr<const CampaignContent>& ObjectFpsApplication::Content() const { return impl_->content; }
Engine::Render::ShaderLibrary& ObjectFpsApplication::Shaders() { return impl_->shaders; }
Engine::Platform::Sdl::SdlPlatform& ObjectFpsApplication::Platform() { return *impl_->platform; }
Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice& ObjectFpsApplication::RenderDevice() { return *impl_->renderDevice; }
Engine::Render::Renderer& ObjectFpsApplication::Renderer() { return impl_->renderer; }
ObjectFpsPresentation& ObjectFpsApplication::Presentation() { return impl_->presentation; }
ObjectFpsRuntimeClient& ObjectFpsApplication::Client() { return *impl_->client; }
} // namespace fps
