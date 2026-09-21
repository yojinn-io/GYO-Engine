#include "RetroFPS/App/CampaignContentLoader.hpp"
#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/App/ObjectFpsRuntimeClient.hpp"
#if defined(OBJECT_FPS_WITH_DIAGNOSTICS)
#include "ObjectFpsDiagnostics.hpp"
#endif
#include "gyo/AppConfig.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/runtime/RuntimeLoop.hpp"
#include "input/backend/sdl/SdlInput.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "render/Renderer.hpp"
#include "render/ShaderLibrary.hpp"
#include "text/backend/sdl_ttf/SdlTtfTextRasterizer.hpp"
#include "ui/UiDocumentCodec.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <filesystem>
#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#ifndef GYO_DEFAULT_GPU_DRIVER
#define GYO_DEFAULT_GPU_DRIVER "auto"
#endif

namespace {

template <class Error>
void LogError(const Error& error) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "%s%s%s",
        error.message.c_str(),
        error.detail.empty() ? "" : ": ",
        error.detail.c_str());
}

void LogError(const std::string& message) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
}

[[nodiscard]] bool HasArgument(
    int argc,
    char* argv[],
    const std::string_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == expected) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::filesystem::path CaptureDirectory(int argc, char* argv[]) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::string_view(argv[index]) == "--capture-dir") return argv[index + 1];
    }
    return {};
}

[[nodiscard]] std::filesystem::path ExecutableRoot() {
    if (const char* basePath = SDL_GetBasePath(); basePath != nullptr) {
        return std::filesystem::path(basePath);
    }
    return {};
}

bool LoadShaders(const std::filesystem::path& root, Engine::Render::ShaderLibrary& library) {
    Engine::Asset::Loading::NativeFileAssetSource source;
    for (const auto* path : {Gyo::AppConfig::BuiltinShaders, Gyo::AppConfig::Shaders}) {
        Engine::Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = (root / path).string();
        const auto loaded = library.AppendBundle(source,
            Engine::Asset::Resolver::AssetPathResolver(options));
        if (!loaded) { LogError(loaded.error()); return false; }
    }
    if (!library.CompleteFormats()) {
        LogError(std::string{"shader bundles have no complete common format"}); return false;
    }
    for (const auto format : {Engine::Render::ShaderFormat::DXIL,
             Engine::Render::ShaderFormat::SPIRV, Engine::Render::ShaderFormat::Metallib}) {
        if (!(library.CompleteFormats() & Engine::Render::FormatBit(format))) continue;
        for (const auto* id : {"builtin/unlit", "builtin/scene_post", "game/object_fps/channel_swap"}) {
            const auto program = library.FindProgram(id, format);
            if (!program) { LogError(program.error()); return false; }
        }
    }
    return true;
}

// Bounded validation mode: real assets and GPU presentation with explicit
// snapshots. This makes action endpoints and aspect-ratio framing reviewable
// without racing enemies or relying on wall-clock timing in a screenshot.
int RunVisualProbe(
    Engine::Platform::Sdl::SdlPlatform& platform,
    Engine::Render::Renderer& renderer,
    fps::ObjectFpsPresentation& presentation,
    Engine::Asset::AssetManager& assets,
    const std::shared_ptr<const fps::CampaignContent>& content,
    const bool smoke,
    const bool reloadSequence,
    const std::filesystem::path& captureDirectory,
    const float width,
    const float height) {
    if (!captureDirectory.empty()) {
        std::error_code directoryError;
        std::filesystem::create_directories(captureDirectory, directoryError);
        if (directoryError) { LogError(directoryError.message()); return 1; }
    }
    const auto documentHandle = assets.Load(
        Engine::Asset::AssetId::FromString("object_fps.ui.screens"),
        Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::Text()));
    if (!documentHandle) { LogError(documentHandle.error()); return 1; }
    const auto text = assets.GetSharedConst<Engine::Asset::Loaders::TextAsset>(documentHandle.value());
    assets.Release(documentHandle.value());
    if (!text) { LogError(std::string{"visual probe UI text is missing"}); return 1; }
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(text->text, "object_fps.ui.screens");
    if (!parsed) { LogError(parsed.error().message); return 1; }
    fps::ObjectFpsUi ui;
    std::string error;
    if (!ui.Initialize(std::make_shared<const Engine::Ui::UiDocument>(std::move(parsed).value()), error)) {
        LogError(error); return 1;
    }
    const auto& stage = content->Stages().front();
    const auto& definition = content->Data().weapons.GetDefinitions().front();
    fps::GameSessionSnapshot snapshot;
    snapshot.screen = fps::GameScreen::Playing;
    snapshot.activeStage = fps::ActiveStageSnapshot{
        stage.definition.id, stage.definition.name, 0, content->Stages().size(), true};
    snapshot.player = fps::PlayerSnapshot{
        stage.map.GetSpawnPosition(), 1.6F, 0, 0, 100, 100};
    // The verification room has a clear approach to its exit: keep the white
    // door and its adjacent wall visible while checking weapon framing.
    const auto exit = stage.map.GetNextMapExitCell();
    if (exit.row >= 2 && stage.map.IsWalkable(
            static_cast<std::ptrdiff_t>(exit.row) - 2,
            static_cast<std::ptrdiff_t>(exit.column))) {
        snapshot.player->position = stage.map.GetCellCenter({exit.row - 2, exit.column});
    }
    snapshot.weapon.weaponId = definition.id;
    snapshot.weapon.magazineAmmo = definition.magazineCapacity;
    snapshot.weapon.reserveAmmo = definition.reserveAmmo;
    snapshot.weaponPresentation.weaponId = definition.id;
    constexpr std::array actions{fps::WeaponAction::Idle, fps::WeaponAction::Shoot,
        fps::WeaponAction::Reload, fps::WeaponAction::Draw, fps::WeaponAction::Hide,
        fps::WeaponAction::Holstered};
    constexpr std::array names{"Idle", "Shoot", "Reload", "Draw", "Hide", "Holstered"};
    const std::array durations{0.0F, definition.fireIntervalSeconds, definition.reloadSeconds,
        definition.drawSeconds, definition.hideSeconds, 0.0F};
    const std::size_t reloadIntervals = static_cast<std::size_t>(std::ceil(
        static_cast<double>(definition.reloadSeconds) * 60.0));
    std::size_t action = 0;
    float progress = 0.5F;
    std::size_t smokeFrame = 0;
    std::size_t skippedCaptures = 0;
    bool running = true;
    while (running) {
        if (platform.PumpEvents([&](const SDL_Event& event) {
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
            if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_6) {
                action = static_cast<std::size_t>(event.key.scancode - SDL_SCANCODE_1);
            }
            if (event.key.scancode == SDL_SCANCODE_LEFT) progress = (std::max)(0.0F, progress - 0.1F);
            if (event.key.scancode == SDL_SCANCODE_RIGHT) progress = (std::min)(1.0F, progress + 0.1F);
            if (event.key.scancode == SDL_SCANCODE_SPACE) snapshot.player->feetY = snapshot.player->feetY > 0 ? 0 : 0.6F;
        }) == Engine::Runtime::RuntimeControl::Stop) break;
        if (reloadSequence) {
            action = 2;
            progress = (std::min)(1.0F, static_cast<float>(smokeFrame) /
                (60.0F * definition.reloadSeconds));
            snapshot.player->feetY = 0.0F;
        } else if (smoke) {
            action = smokeFrame / 3;
            progress = static_cast<float>(smokeFrame % 3) * 0.5F;
            snapshot.player->feetY = smokeFrame % 2 ? 0.6F : 0.0F;
        }
        snapshot.weaponPresentation.action = actions[action];
        snapshot.weaponPresentation.durationSeconds = durations[action];
        snapshot.weaponPresentation.elapsedSeconds = progress * durations[action];
        snapshot.weapon.reloading = actions[action] == fps::WeaponAction::Reload;
        snapshot.weapon.reloadProgress = snapshot.weapon.reloading ? progress : 0.0F;
        const std::string title = std::string{"Object_FPS Mark23 preview | "} + names[action] +
            " " + std::to_string(static_cast<int>(progress * 100)) + "% | 1-6 action, arrows time, Space height, Esc close";
        SDL_SetWindowTitle(platform.NativeWindow(), title.c_str());
        if (!captureDirectory.empty()) renderer.RequestSceneCapture();
        Engine::Ui::UiDrawList draw;
        if (!ui.Compose(snapshot, {}, {width, height}, draw, error) ||
            !presentation.Present(snapshot, {}, draw, error)) { LogError(error); return 1; }
        if (!captureDirectory.empty()) {
            auto capture = renderer.TakeSceneCapture();
            if (!capture) {
                if (++skippedCaptures > 180) {
                    LogError(std::string{"visual probe could not capture a presented frame"});
                    return 1;
                }
                SDL_Delay(16);
                continue;
            }
            skippedCaptures = 0;
            SDL_Surface* surface = SDL_CreateSurfaceFrom(
                static_cast<int>(capture->width), static_cast<int>(capture->height),
                SDL_PIXELFORMAT_RGBA32, capture->rgba8.data(), static_cast<int>(capture->width * 4));
            if (!surface) { LogError(std::string{SDL_GetError()}); return 1; }
            const std::string sampleName = reloadSequence
                ? "frame_" + std::to_string(smokeFrame)
                : std::to_string(static_cast<int>(progress * 100));
            const auto path = captureDirectory / (std::string{names[action]} + "_" + sampleName + ".bmp");
            const bool saved = SDL_SaveBMP(surface, path.string().c_str());
            SDL_DestroySurface(surface);
            if (!saved) { LogError(std::string{SDL_GetError()}); return 1; }
        }
        if ((smoke || reloadSequence) && ++smokeFrame ==
            (reloadSequence ? reloadIntervals + 1 : actions.size() * 3)) return 0;
        SDL_Delay(16);
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    namespace Asset = Engine::Asset;
    namespace SdlInput = Engine::Input::Backend::Sdl;
    namespace SdlPlatform = Engine::Platform::Sdl;
    namespace SdlGpu = Engine::Render::Backend::SdlGpu;
    namespace SdlTtf = Engine::Text::Backend::SdlTtf;

#if !defined(OBJECT_FPS_WITH_DIAGNOSTICS)
    // A product-only build must reject unavailable validation modes instead of
    // silently entering the interactive game and hanging an external runner.
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument != "--startup-smoke-test" &&
            (argument == "--validate-package" || argument == "--smoke-test" ||
             argument.ends_with("-smoke-test"))) {
            LogError(std::string{"Optional diagnostics are not enabled: "} + std::string{argument});
            return 2;
        }
    }
#endif

    const auto executableRoot = ExecutableRoot();
    if (executableRoot.empty()) { LogError(std::string{"cannot locate executable directory"}); return 1; }
    Engine::Render::ShaderLibrary shaders;
    if (!LoadShaders(executableRoot, shaders)) return 1;
#if defined(OBJECT_FPS_WITH_DIAGNOSTICS)
    if (HasArgument(argc, argv, "--validate-package")) return ValidatePackage(executableRoot, shaders);
    const bool headlessSmoke = HasArgument(argc, argv, "--headless-smoke-test");
    if (headlessSmoke && ValidatePackage(executableRoot, shaders) != 0) return 1;
#endif

    // CPU content loading is shared by interactive startup and both packaged
    // smoke modes. All paths resolve assets relative to the executable.
    const std::filesystem::path assetRoot = executableRoot / Gyo::AppConfig::Assets;
    Asset::Resolver::AssetPathResolver::Options resolverOptions;
    resolverOptions.assetsRoot = assetRoot.string();
    resolverOptions.allowAbsolutePath = false;
    resolverOptions.allowEscapeAssetsRoot = false;
    Asset::Resolver::AssetPathResolver resolver(std::move(resolverOptions));
    Asset::Catalog::CatalogParser parser;
    Asset::AssetCatalog catalog;
    const auto catalogResult = catalog.LoadFromFile(
        (assetRoot / "asset_catalog.json").string(), parser, resolver);
    if (!catalogResult) {
        LogError(catalogResult.error());
        return 1;
    }

    Asset::Resolver::AssetPathResolver::Options commonOptions;
    const std::filesystem::path commonRoot = executableRoot / Gyo::AppConfig::CommonAssets;
    commonOptions.assetsRoot = commonRoot.string();
    Asset::Resolver::AssetPathResolver commonResolver(std::move(commonOptions));
    const auto commonCatalog = catalog.AppendFromFile(
        (commonRoot / "asset_catalog.json").string(), parser, commonResolver);
    if (!commonCatalog) {
        LogError(commonCatalog.error());
        return 1;
    }

    Asset::Loading::LoaderRegistry loaders;
    auto modelLoaderResult = loaders.Register(
        std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>());
    if (!modelLoaderResult) {
        LogError(modelLoaderResult.error());
        return 1;
    }
    auto fontLoaderResult =
        loaders.Register(std::make_unique<Asset::Loaders::FontLoader>());
    if (!fontLoaderResult) {
        LogError(fontLoaderResult.error());
        return 1;
    }
    auto textLoaderResult =
        loaders.Register(std::make_unique<Asset::Loaders::TextLoader>());
    if (!textLoaderResult) {
        LogError(textLoaderResult.error());
        return 1;
    }
    auto imageLoaderResult = loaders.Register(
        std::make_unique<
            Asset::Loaders::SdlImage::SdlImageTextureLoader>());
    if (!imageLoaderResult) {
        LogError(imageLoaderResult.error());
        return 1;
    }

    Asset::Loading::NativeFileAssetSource source;
    Asset::Loading::AssetPipeline pipeline(source, loaders);
    Asset::Core::AssetStorage storage;
    Asset::Core::AssetLifetime lifetime;
    Asset::Core::AssetCachePolicy cache({
        Asset::Core::AssetCachePolicy::Mode::KeepWhileReferenced,
        120,
        true,
        0,
        0,
    });
    Asset::AssetManager assets(
        catalog, pipeline, storage, lifetime, cache);

    fps::CampaignContentBuildResult contentResult =
        fps::CampaignContentLoader::Load(assets);
    if (!contentResult) {
        LogError(contentResult.error);
        return 1;
    }
    auto content = std::make_shared<const fps::CampaignContent>(
        std::move(*contentResult.content));

#if defined(OBJECT_FPS_WITH_DIAGNOSTICS)
    if (headlessSmoke) {
        std::string report;
        std::string error;
        if (!RunHeadlessSmoke(content, report, error)) { LogError(error); return 1; }
        SDL_Log("%s", report.c_str());
        return 0;
    }
#endif

    if (HasArgument(argc, argv, "--startup-smoke-test")) {
        // The same startup path has decoded game data, maps, weapon settings
        // and FBX-derived muzzle calibration, and checked shader artifacts.
        // Push validation stops here; gameplay simulation belongs to the
        // separate headless smoke and graphics to the shader/GPU probes.
        SDL_Log("Startup smoke passed: stages=%zu, weapons=%zu, shader_programs=%zu, "
                "bundle=%s, window=none, gpu=none",
            content->Stages().size(), content->Data().weapons.GetDefinitions().size(),
            shaders.ProgramIds().size(), shaders.Version().c_str());
        return 0;
    }

    SdlGpu::SdlGpuOptions gpuOptions;
    gpuOptions.availableShaderFormats = shaders.CompleteFormats();
    gpuOptions.driver = GYO_DEFAULT_GPU_DRIVER;
    for (int index=1;index<argc;++index) {
        if (std::string_view(argv[index]) != "--gpu-driver") continue;
        if (++index >= argc) { LogError(std::string{"--gpu-driver requires auto, d3d12, vulkan or metal"}); return 2; }
        gpuOptions.driver = argv[index];
    }

    SdlPlatform::SdlPlatformOptions platformOptions;
    platformOptions.title = Gyo::AppConfig::DisplayName;
    platformOptions.width = 1280;
    platformOptions.height = 720;
    if (HasArgument(argc, argv, "--preview-4x3")) platformOptions.width = 960;
    if (HasArgument(argc, argv, "--preview-21x9")) platformOptions.width = 1680;
    platformOptions.resizable = false;

    auto platformResult = SdlPlatform::SdlPlatform::Create(platformOptions);
    if (!platformResult) {
        LogError(platformResult.error());
        return 1;
    }
    auto platform = std::move(platformResult).value();

    auto renderResult = SdlGpu::SdlGpuRenderDevice::Create(*platform, gpuOptions);
    if (!renderResult) {
        LogError(renderResult.error());
        return 1;
    }
    auto renderDevice = std::move(renderResult).value();
    Engine::Render::Renderer renderer;
    const auto rendererReady = renderer.Initialize(*renderDevice, shaders);
    if (!rendererReady) { LogError(rendererReady.error()); return 1; }
    const auto activeFormat = Engine::Render::ShaderFormatName(*renderer.ActiveShaderFormat());
    SDL_Log("GYO GPU: driver=%s, shader=%.*s, available_formats=%u, bundle=%s",
        renderDevice->GetInfo().driver.c_str(), static_cast<int>(activeFormat.size()), activeFormat.data(),
        renderDevice->GetInfo().shaderFormats, shaders.Version().c_str());
#if defined(OBJECT_FPS_WITH_DIAGNOSTICS)
    if (HasArgument(argc, argv, "--shader-smoke-test")) return RunShaderProbe(*platform, renderer);
#endif

    auto textRasterizerResult = SdlTtf::SdlTtfTextRasterizer::Create();
    if (!textRasterizerResult) {
        LogError(textRasterizerResult.error());
        return 1;
    }
    auto textRasterizer = std::move(textRasterizerResult).value();

#if defined(OBJECT_FPS_WITH_DIAGNOSTICS)
    if (HasArgument(argc, argv, "--muzzle-smoke-test")) {
        return RunMuzzleProbe(*platform, *renderDevice, renderer, assets, *content,
            CaptureDirectory(argc, argv));
    }
#endif

    const bool smokeTest = HasArgument(argc, argv, "--smoke-test");
    const bool menuSmokeTest = HasArgument(argc, argv, "--menu-smoke-test");
    if (smokeTest && menuSmokeTest) {
        LogError(std::string{
            "--smoke-test and --menu-smoke-test are mutually exclusive"});
        return 2;
    }
    fps::GameSessionConfig sessionConfig;
    if (smokeTest) {
        sessionConfig.fadeOutSeconds = 0.0001F;
        sessionConfig.fadeInSeconds = 0.0001F;
    }
    fps::ObjectFpsPresentationConfig presentationConfig;
    presentationConfig.world = sessionConfig.world;
    presentationConfig.viewportWidth = static_cast<float>(platformOptions.width);
    presentationConfig.viewportHeight = static_cast<float>(platformOptions.height);

    fps::ObjectFpsPresentation presentation;
    std::string error;
    if (!presentation.Initialize(
            *renderDevice,
            renderer,
            *textRasterizer,
            assets,
            content,
            presentationConfig,
            error)) {
        LogError(error);
        return 1;
    }

    if (HasArgument(argc, argv, "--viewmodel-preview") ||
        HasArgument(argc, argv, "--viewmodel-smoke-test") ||
        HasArgument(argc, argv, "--reload-smoke-test")) {
        return RunVisualProbe(*platform, renderer, presentation, assets, content,
            HasArgument(argc, argv, "--viewmodel-smoke-test"),
            HasArgument(argc, argv, "--reload-smoke-test"),
            CaptureDirectory(argc, argv),
            presentationConfig.viewportWidth, presentationConfig.viewportHeight);
    }

    SdlInput::SdlInput input(*platform);
    fps::ObjectFpsRuntimeClientConfig runtimeConfig;
    runtimeConfig.startCampaignImmediately = smokeTest;
    runtimeConfig.stopAfterFirstPlayingFrame = smokeTest;
    runtimeConfig.stopAfterFirstMenuFrame = menuSmokeTest;
    runtimeConfig.viewportWidth = presentationConfig.viewportWidth;
    runtimeConfig.viewportHeight = presentationConfig.viewportHeight;
    fps::ObjectFpsRuntimeClient client(
        *platform,
        input,
        assets,
        presentation,
        runtimeConfig);
    if (!client.Initialize(content, sessionConfig, error)) {
        LogError(error);
        return 1;
    }

    Engine::Runtime::RuntimeLoop loop(client);
    loop.Run();
    if (!client.LastError().empty()) {
        LogError(client.LastError());
    }
    return client.ExitCode();
}
