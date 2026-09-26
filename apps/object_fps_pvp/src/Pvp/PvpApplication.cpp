#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/PredictionElapsedTime.hpp"
#include "RetroFPS/Pvp/SnapshotTimeline.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/runtime/RuntimeLoop.hpp"
#include "input/backend/sdl/SdlInput.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "render/PrimitiveMesh.hpp"
#include "render/Renderer.hpp"
#include "text/backend/sdl_ttf/SdlTtfTextRasterizer.hpp"
#include "ui/UiDocumentCodec.hpp"
#include "ui/UiRenderer.hpp"
#include "ui/UiRuntime.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <numbers>
#include <optional>
#include <utility>

namespace fps::pvp {
namespace {
using Control = Engine::Runtime::RuntimeControl;
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point end, Clock::time_point begin) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}
template<class Error> std::string Explain(const Error& error) {
    return error.message + (error.detail.empty() ? "" : ": " + error.detail);
}
std::string ExplainUi(const Engine::Ui::UiError& error) {
    return error.message + (error.jsonPointer.empty() ? "" : " at " + error.jsonPointer);
}
const PlayerState* FindPlayer(const WorldSnapshot& snapshot, PlayerId id) {
    const auto found = std::find_if(snapshot.players.begin(), snapshot.players.end(),
        [id](const PlayerState& player) { return player.playerId == id; });
    return found == snapshot.players.end() ? nullptr : &*found;
}
void AddText(Engine::Ui::UiDrawList& list, std::string text,
    Engine::Ui::UiRect bounds, float size = 18.0F) {
    Engine::Ui::UiTextDraw draw;
    draw.utf8 = std::move(text);
    draw.boundsPixels = bounds;
    draw.fontAssetId = "object_fps_pvp.font.ui";
    draw.pointSizePixels = size;
    draw.color = {0.9F, 0.96F, 1.0F, 1.0F};
    list.commands.emplace_back(std::move(draw));
}
} // namespace

struct PvpApplication::Impl final {
    Engine::Asset::AssetCatalog catalog;
    Engine::Asset::Loading::LoaderRegistry loaders;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy cache{{
        Engine::Asset::Core::AssetCachePolicy::Mode::KeepWhileReferenced, 120, true, 0, 0}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, cache};
    std::optional<Arena> arena;
    Engine::Render::ShaderLibrary shaders;
    std::unique_ptr<Engine::Platform::Sdl::SdlPlatform> platform;
    std::unique_ptr<Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice> device;
    Engine::Render::Renderer renderer;
    std::unique_ptr<Engine::Text::Backend::SdlTtf::SdlTtfTextRasterizer> text;
    Engine::Ui::UiRenderer uiRenderer;
    Engine::Ui::UiRuntime ui;
    std::unique_ptr<Engine::Input::Backend::Sdl::SdlInput> input;
    Engine::Render::MeshHandle cube;
    Engine::Render::MeshHandle floor;
    Engine::Render::RenderQueue queue;
    ClientConnection connection;
    ClientConnectionState state;
    std::unique_ptr<LocalPlayerPrediction> prediction;
    PredictionElapsedTime predictionElapsed;
    SnapshotTimeline timeline;
    std::uint64_t connectionGeneration{}, lastSnapshotTick{}, ingressHistoryDrops{};
    std::optional<RemoteMovementObservation> remoteMovement;
    std::optional<PresentedMovementObservation> presentedMovement;
    std::uint64_t skippedPresentationFrames{};
    std::string address{"127.0.0.1:8080"};
    std::string lastError;
    std::string localStatus;
    float width{1280}, height{720}, yaw{}, pitch{};
    PlayerId viewPlayer{}, renderedPlayer{};
    bool editing{}, suppressUiEnter{}, inputCaptured{}, windowInteraction{}, initialized{}, quit{};
    int exitCode{};

    ~Impl() {
        connection.Leave();
        uiRenderer.Reset();
        if (device) {
            if (cube.IsValid()) static_cast<void>(device->ReleaseMesh(cube));
            if (floor.IsValid()) static_cast<void>(device->ReleaseMesh(floor));
        }
    }

    bool InWorld() const {
        return state.phase == ConnectionPhase::Playing && state.snapshot &&
            FindPlayer(*state.snapshot, state.playerId) != nullptr;
    }
    Engine::Ui::UiViewport Viewport() const { return {width, height}; }

    void EditAddress(bool value) {
        editing = value;
        if (value) SDL_StartTextInput(platform->NativeWindow());
        else SDL_StopTextInput(platform->NativeWindow());
    }

    void AppendAddress(const char* value) {
        if (!value) return;
        for (; *value != '\0' && address.size() < 128; ++value) {
            const unsigned char c = static_cast<unsigned char>(*value);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '.' || c == ':' ||
                c == '-' || c == '[' || c == ']') address += static_cast<char>(c);
        }
    }

    void ReleasePointer() {
        if (inputCaptured) SDL_Log("PvP releasing pointer player=%llu application_ms=%llu",
            static_cast<unsigned long long>(state.playerId), static_cast<unsigned long long>(SDL_GetTicks()));
        inputCaptured = false;
        const auto released = input->SetRelativeMouseMode(false);
        if (!released) {
            lastError = Explain(released.error());
            exitCode = 1;
            quit = true;
        }
    }

    void HandleNativeEvent(const SDL_Event& event) {
        if (InWorld()) {
            const auto windowId = SDL_GetWindowID(platform->NativeWindow());
            if (((event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) && event.window.windowID != windowId) ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.windowID != windowId) ||
                (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.windowID != windowId)) return;
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MOVED ||
                event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_MINIMIZED) {
                // Release during the native event, before the next simulation
                // update. Moving a window must not retain gameplay mouse lock.
                windowInteraction = true;
                ReleasePointer();
            } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_TAB && !event.key.repeat &&
                       input->Snapshot().windowFocused && !windowInteraction) {
                if (inputCaptured) ReleasePointer();
                else inputCaptured = true;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
                       input->Snapshot().windowFocused && !windowInteraction &&
                       event.button.x >= 0 && event.button.x < width && event.button.y >= 0 && event.button.y < height) {
                inputCaptured = true;
            }
            return;
        }
        if (!editing) return;
        if (event.type == SDL_EVENT_TEXT_INPUT) AppendAddress(event.text.text);
        if (event.type != SDL_EVENT_KEY_DOWN) return;
        if (event.key.key == SDLK_BACKSPACE && !address.empty()) address.pop_back();
        if ((event.key.mod & SDL_KMOD_CTRL) != 0 && event.key.key == SDLK_A) address.clear();
        if ((event.key.mod & SDL_KMOD_CTRL) != 0 && event.key.key == SDLK_V) {
            char* clipboard = SDL_GetClipboardText();
            AppendAddress(clipboard);
            SDL_free(clipboard);
        }
        if (event.key.key == SDLK_RETURN || event.key.key == SDLK_ESCAPE) {
            EditAddress(false);
            suppressUiEnter = true;
        }
    }

    Engine::Ui::UiBindingTable Bindings() const {
        Engine::Ui::UiBindingTable result;
        result.emplace("address", address + (editing ? "_" : ""));
        result.emplace("hint", editing
            ? "Type host:port. Ctrl+A clears, Ctrl+V pastes, Enter confirms."
            : "Click the address to edit. Arrows / Enter also navigate.");
        std::string room = "No room found. Refresh or create one.";
        if (!state.rooms.empty()) {
            const auto& first = state.rooms.front();
            room = "ROOM " + first.id + "  /  " + std::to_string(first.players) +
                " of " + std::to_string(first.capacity) + " slots occupied";
        }
        result.emplace("room", std::move(room));
        std::string status = "Ready. Start a match or join the listed room.";
        if (state.phase == ConnectionPhase::Requesting) status = "Contacting Gateway...";
        else if (state.phase == ConnectionPhase::Connecting) status = "Joining authoritative Match...";
        else if (state.phase == ConnectionPhase::Playing) status = "Joined. Waiting for first world snapshot...";
        else if (!localStatus.empty()) status = localStatus;
        if (!state.error.empty()) status = state.error;
        result.emplace("status", std::move(status));
        return result;
    }

    void Execute(const std::string& action) {
        if (action == "pvp.edit_address") { EditAddress(!editing); return; }
        EditAddress(false);
        localStatus.clear();
        if (action == "pvp.leave") { connection.Leave(); return; }
        if (action == "pvp.quit") { connection.Leave(); quit = true; return; }
        if (state.phase != ConnectionPhase::Lobby) return;
        if (action == "pvp.refresh") connection.Refresh(address);
        else if (action == "pvp.create") connection.CreateAndJoin(address);
        else if (action == "pvp.join") {
            if (state.rooms.empty()) localStatus = "No room selected. Refresh the room list first.";
            else connection.Join(address, state.rooms.front().id);
        }
    }

    void RefreshState() {
        auto received = connection.Drain();
        state = std::move(received.state);
        if (connectionGeneration != received.generation) {
            connectionGeneration = received.generation;
            timeline.Reset();
            lastSnapshotTick = 0;
            viewPlayer = 0;
            if (prediction) prediction->Reset();
            predictionElapsed.Reset();
        }
        ingressHistoryDrops = received.snapshotHistoryOverflowCount;
        const auto* self = state.snapshot ? FindPlayer(*state.snapshot, state.playerId) : nullptr;
        if (state.phase != ConnectionPhase::Playing || !self) {
            timeline.Reset();
            lastSnapshotTick = 0;
            remoteMovement.reset();
            viewPlayer = 0;
            renderedPlayer = 0;
            inputCaptured = false;
            if (prediction) prediction->Reset();
            predictionElapsed.Reset();
            return;
        }
        if (viewPlayer != state.playerId) {
            timeline.Reset();
            lastSnapshotTick = 0;
            prediction->Reset();
            predictionElapsed.Reset();
            yaw = self->yaw;
            pitch = self->pitch;
            viewPlayer = state.playerId;
            inputCaptured = false;
        }
        // Consume the complete receipt-stamped batch before selecting this
        // frame's timeline bracket. Never play a stalled backlog one frame at a time.
        for (const auto& sample : received.snapshots)
            static_cast<void>(timeline.Push(sample.snapshot, sample.receivedAt));
        if (state.snapshot->tick > lastSnapshotTick) {
            prediction->Reconcile(*self, state.snapshot->tick);
            lastSnapshotTick = state.snapshot->tick;
        }
    }

    bool SubmitBox(Engine::Render::Float3 center, Engine::Render::Float3 scale,
        Engine::Render::Color color, float rotation = 0.0F) {
        Engine::Render::MeshSubmission draw;
        draw.mesh = cube;
        draw.transform = {center, {0, rotation, 0}, scale};
        draw.material.tint = color;
        const auto result = queue.Submit(draw);
        if (!result) { lastError = Explain(result.error()); return false; }
        return true;
    }

    bool PrepareWorld() {
        remoteMovement.reset();
        const auto& position = prediction->Observation().renderPosition;
        queue.SetCamera({{position.x, position.y + arena->eyeHeight, position.z},
            {pitch, yaw, 0}, 1.0471975512F, 0.05F, 150.0F});
        // Simple checker floor gives movement depth cues without campaign assets.
        const float tile = arena->cellSize;
        for (float z = 0; z < arena->depth; z += tile) {
            for (float x = 0; x < arena->width; x += tile) {
                const float w = (std::min)(tile, arena->width - x);
                const float d = (std::min)(tile, arena->depth - z);
                Engine::Render::MeshSubmission draw;
                draw.mesh = floor;
                draw.transform = {{x + w * .5F, 0, z + d * .5F}, {}, {w, 1, d}};
                const bool even = (static_cast<int>(x / tile) + static_cast<int>(z / tile)) % 2 == 0;
                draw.material.tint = even ? Engine::Render::Color{.11F,.15F,.19F,1} : Engine::Render::Color{.15F,.20F,.24F,1};
                draw.doubleSided = true;
                const auto result = queue.Submit(draw);
                if (!result) { lastError = Explain(result.error()); return false; }
            }
        }
        for (const auto& wall : arena->walls) {
            const auto& a = wall.minimum;
            const auto& b = wall.maximum;
            if (!SubmitBox({(a.x+b.x)*.5F,(a.y+b.y)*.5F,(a.z+b.z)*.5F},
                {b.x-a.x,b.y-a.y,b.z-a.z}, {.20F,.31F,.39F,1})) return false;
            if (!SubmitBox({(a.x+b.x)*.5F,b.y-.06F,(a.z+b.z)*.5F},
                {b.x-a.x+.01F,.12F,b.z-a.z+.01F}, {.15F,.65F,.70F,1})) return false;
        }
        const auto presentationTime = Clock::now();
        for (const auto& player : state.snapshot->players) {
            if (player.playerId == state.playerId) continue;
            const auto sampled = timeline.Sample(player.playerId, presentationTime);
            const auto& presented = sampled ? sampled->player : player;
            const auto position = presented.position;
            remoteMovement = RemoteMovementObservation{player.playerId, position};
            auto& observation = *remoteMovement;
            observation.movementEpoch = presented.movementEpoch;
            observation.yaw = presented.yaw;
            observation.pitch = presented.pitch;
            observation.ingressHistoryDrops = ingressHistoryDrops;
            if (sampled) {
                observation.lowerTick = sampled->lowerTick;
                observation.upperTick = sampled->upperTick;
                observation.presentationTick = sampled->presentationTick;
                observation.interpolationAlpha = sampled->alpha;
                observation.latestReceiveAgeSeconds = sampled->latestReceiveAgeSeconds;
                observation.missingFutureSnapshot = sampled->missingFutureSnapshot;
                observation.holdSeconds = sampled->holdSeconds;
                observation.totalHoldSeconds = sampled->totalHoldSeconds;
                observation.historySize = sampled->historySize;
                observation.holdCount = sampled->holdCount;
                observation.gapCount = sampled->gapCount;
                observation.historyEvictions = sampled->historyEvictions;
                observation.phaseReanchors = sampled->phaseReanchors;
                observation.holding = sampled->holding;
                observation.lowerResolvedCommand = sampled->lowerResolvedCommand;
                observation.upperResolvedCommand = sampled->upperResolvedCommand;
            }
            const Engine::Render::Color color{.96F,.35F,.10F,1};
            if (!SubmitBox({position.x,position.y+arena->bodyHeight*.5F,position.z},
                {arena->radius*2,arena->bodyHeight,arena->radius*2},color,presented.yaw)) return false;
            if (!SubmitBox({position.x+std::sin(presented.yaw)*arena->radius,
                position.y+arena->eyeHeight, position.z+std::cos(presented.yaw)*arena->radius},
                {.24F,.12F,.12F},{.95F,.95F,.95F,1},presented.yaw)) return false;
        }
        return true;
    }

    Control Fail(std::string message) {
        lastError = std::move(message);
        exitCode = 1;
        return Control::Stop;
    }
};

PvpApplication::PvpApplication() : impl_(std::make_unique<Impl>()) {}
PvpApplication::~PvpApplication() = default;

bool PvpApplication::InitializeContent(const std::filesystem::path& assetRoot, std::string& error) {
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
    if (!impl_->shaders.CompleteFormats()) { error = "Shader bundles have no complete common format"; return false; }
    const auto fontLoader = impl_->loaders.Register(std::make_unique<Asset::Loaders::FontLoader>());
    if (!fontLoader) { error = Explain(fontLoader.error()); return false; }
    const auto textLoader = impl_->loaders.Register(std::make_unique<Asset::Loaders::TextLoader>());
    if (!textLoader) { error = Explain(textLoader.error()); return false; }
    impl_->arena = Arena::Load(assetRoot / "pvp_arena.json", error);
    if (!impl_->arena) return false;
    impl_->prediction = std::make_unique<LocalPlayerPrediction>(*impl_->arena);
    impl_->predictionElapsed.Reset();
    impl_->connection.SetArenaIdentity(impl_->arena->id, impl_->arena->version);
    const auto loaded = impl_->assets.Load(Asset::AssetId::FromString("object_fps_pvp.ui.pvp_lobby"),
        Asset::AssetRequest::WithTypeHint(Asset::AssetType::Text()));
    if (!loaded) { error = Explain(loaded.error()); return false; }
    const auto text = impl_->assets.GetSharedConst<Asset::Loaders::TextAsset>(loaded.value());
    if (!text) {
        impl_->assets.Release(loaded.value());
        error = "PvP Lobby asset has no text payload";
        return false;
    }
    auto document = Engine::Ui::UiDocumentCodec::Parse(text->text, "object_fps_pvp.ui.pvp_lobby");
    impl_->assets.Release(loaded.value());
    if (!document) { error = ExplainUi(document.error()); return false; }
    const auto ready = impl_->ui.Initialize(std::make_shared<const Engine::Ui::UiDocument>(std::move(document).value()));
    if (!ready) { error = ExplainUi(ready.error()); return false; }
    const auto activated = impl_->ui.ActivateCanvas("lobby");
    if (!activated) { error = ExplainUi(activated.error()); return false; }
    return true;
}

bool PvpApplication::InitializeGraphics(const PvpApplicationOptions& options, std::string& error) {
    const auto started = Clock::now();
    error.clear();
    if (!impl_->arena) { error = "PvP content must be initialized before graphics"; return false; }
    Engine::Platform::Sdl::SdlPlatformOptions platformOptions;
    platformOptions.title = options.title;
    platformOptions.width = options.width;
    platformOptions.height = options.height;
    platformOptions.resizable = false;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(platformOptions);
    if (!platform) { error = Explain(platform.error()); return false; }
    impl_->platform = std::move(platform).value();
    Engine::Render::Backend::SdlGpu::SdlGpuOptions gpu;
    gpu.availableShaderFormats = impl_->shaders.CompleteFormats();
    gpu.driver = options.gpuDriver;
    gpu.vsync = options.vsync;
    auto device = Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice::Create(*impl_->platform, gpu);
    if (!device) { error = Explain(device.error()); return false; }
    impl_->device = std::move(device).value();
    const auto renderer = impl_->renderer.Initialize(*impl_->device, impl_->shaders);
    if (!renderer) { error = Explain(renderer.error()); return false; }
    auto text = Engine::Text::Backend::SdlTtf::SdlTtfTextRasterizer::Create();
    if (!text) { error = Explain(text.error()); return false; }
    impl_->text = std::move(text).value();
    const auto ui = impl_->uiRenderer.Initialize(*impl_->device, *impl_->text, impl_->assets);
    if (!ui) { error = ExplainUi(ui.error()); return false; }
    auto cube = impl_->device->CreateMesh(Engine::Render::MakeUnitCube().View());
    if (!cube) { error = Explain(cube.error()); return false; }
    impl_->cube = cube.value();
    auto floor = impl_->device->CreateMesh(Engine::Render::MakeUnitQuadXZ().View());
    if (!floor) { error = Explain(floor.error()); return false; }
    impl_->floor = floor.value();
    impl_->input = std::make_unique<Engine::Input::Backend::Sdl::SdlInput>(*impl_->platform);
    impl_->width = static_cast<float>(options.width);
    impl_->height = static_cast<float>(options.height);
    impl_->address = options.gateway;
    impl_->initialized = true;
    SDL_Log("PvP graphics ready driver=%s initialization_ms=%.1f",
        impl_->device->GetInfo().driver.c_str(), Milliseconds(Clock::now(), started));
    return true;
}

Control PvpApplication::ProcessEvents(const Engine::Runtime::FrameContext&) {
    if (!impl_->initialized) return impl_->Fail("PvP application is not initialized");
    const auto started = Clock::now();
    impl_->suppressUiEnter = false;
    impl_->windowInteraction = false;
    impl_->input->BeginFrame();
    const auto control = impl_->platform->PumpEvents([this](const SDL_Event& event) {
        impl_->input->HandleEvent(event);
        impl_->HandleNativeEvent(event);
    });
    impl_->input->EndFrame();
    const auto elapsed = Milliseconds(Clock::now(), started);
    if (elapsed >= 250) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PvP slow event processing elapsed_ms=%.1f", elapsed);
    return control;
}

Control PvpApplication::Update(const Engine::Runtime::FrameContext& frame) {
    if (!impl_->initialized) return impl_->Fail("PvP application is not initialized");
    const auto started = Clock::now();
    if (frame.deltaSeconds >= .1) {
        MovementTraceEvent event;
        event.kind = MovementTraceKind::RuntimeGap;
        event.playerId = impl_->state.playerId;
        event.frameSeconds = frame.deltaSeconds;
        TraceMovement(event);
    }
    using Engine::Input::Key;
    const auto& physical = impl_->input->Snapshot();
    impl_->assets.BeginFrame(frame.frameIndex);
    impl_->assets.Update();
    impl_->RefreshState();
    if (impl_->InWorld()) {
        impl_->EditAddress(false);
        if (physical.Get(Key::Escape).pressed) {
            impl_->connection.Leave();
            impl_->RefreshState();
        } else {
            // Consume a presentation frame's raw mouse delta exactly once.
            if (impl_->inputCaptured && physical.windowFocused && physical.pointer.relativeMode) {
                impl_->yaw = std::remainder(impl_->yaw + physical.pointer.deltaX * .0025F,
                    2.0F * std::numbers::pi_v<float>);
                impl_->pitch = std::clamp(impl_->pitch + physical.pointer.deltaY * .0025F, -MovementMaximumPitch, MovementMaximumPitch);
            }
            const float forward = impl_->inputCaptured && physical.windowFocused ?
                static_cast<float>(physical.Get(Key::W).held) - static_cast<float>(physical.Get(Key::S).held) : 0;
            const float right = impl_->inputCaptured && physical.windowFocused ?
                static_cast<float>(physical.Get(Key::D).held) - static_cast<float>(physical.Get(Key::A).held) : 0;
            // Refresh/reconciliation may cover a stall since the runtime took
            // its frame timestamp. Sample here, after that work, so the next
            // frame cannot charge the same elapsed interval a second time.
            const double movementElapsed = impl_->predictionElapsed.Sample(Clock::now());
            if (impl_->prediction->Advance(movementElapsed, forward, right, impl_->yaw, impl_->pitch))
                impl_->connection.SendInput(impl_->prediction->PendingInput());
        }
    } else {
        Engine::Ui::UiInputFrame uiInput;
        uiInput.focusPreviousPressed = !impl_->editing && physical.Get(Key::Up).pressed;
        uiInput.focusNextPressed = !impl_->editing && physical.Get(Key::Down).pressed;
        uiInput.activatePressed = !impl_->editing && !impl_->suppressUiEnter && physical.Get(Key::Enter).pressed;
        uiInput.pointerAvailable = physical.windowFocused && !physical.pointer.relativeMode;
        uiInput.pointerPixels = {physical.pointer.x, physical.pointer.y};
        const auto& click = physical.Get(Engine::Input::MouseButton::Left);
        uiInput.pointerPrimaryPressed = click.pressed;
        uiInput.pointerPrimaryHeld = click.held;
        uiInput.pointerPrimaryReleased = click.released;
        const auto updated = impl_->ui.Update(uiInput, impl_->Bindings(), impl_->Viewport());
        if (!updated) return impl_->Fail(ExplainUi(updated.error()));
        for (const auto& event : updated.value()) impl_->Execute(event.action);
    }
    const auto mouseStarted = Clock::now();
    const auto relative = impl_->input->SetRelativeMouseMode(impl_->InWorld() && impl_->inputCaptured && physical.windowFocused);
    if (!relative) return impl_->Fail(Explain(relative.error()));
    const auto finished = Clock::now();
    if (Milliseconds(finished, started) >= 250)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PvP slow update player=%llu elapsed_ms=%.1f mouse_mode_ms=%.1f frame_gap_ms=%.1f",
            static_cast<unsigned long long>(impl_->state.playerId), Milliseconds(finished, started),
            Milliseconds(finished, mouseStarted), frame.deltaSeconds * 1000);
    return impl_->quit ? Control::Stop : Control::Continue;
}

Control PvpApplication::Render(const Engine::Runtime::FrameContext& context) {
    impl_->presentedMovement.reset();
    const auto started = Clock::now();
    const bool world = impl_->InWorld();
    const bool firstWorldFrame = world && impl_->renderedPlayer != impl_->state.playerId;
    Engine::Render::FrameDescription frame;
    frame.clearColor = {.04F,.065F,.10F,1};
    impl_->queue.Reset(frame);
    Engine::Ui::UiDrawList draws;
    if (impl_->InWorld()) {
        if (!impl_->PrepareWorld()) return impl_->Fail(impl_->lastError);
        const float scale = (std::min)(impl_->width/1280.0F, impl_->height/720.0F);
        draws.commands.emplace_back(Engine::Ui::UiQuadDraw{{16,16,510*scale,62*scale},{.015F,.025F,.04F,.88F}});
        AddText(draws, "PLAYERS ONLINE: " + std::to_string(impl_->state.snapshot->players.size()),
            {30,24,490*scale,25*scale},18*scale);
        AddText(draws, "WASD move  |  Mouse look  |  Tab cursor  |  ESC leave", {30,50,490*scale,22*scale},14*scale);
        draws.commands.emplace_back(Engine::Ui::UiQuadDraw{{impl_->width*.5F-5,impl_->height*.5F-1,10,2},{.9F,.96F,1,1}});
        draws.commands.emplace_back(Engine::Ui::UiQuadDraw{{impl_->width*.5F-1,impl_->height*.5F-5,2,10},{.9F,.96F,1,1}});
        if (!impl_->inputCaptured || !impl_->input->Snapshot().windowFocused)
            AddText(draws, "Click inside the window to resume input", {impl_->width*.5F-190,110,480,30});
    } else {
        const auto composed = impl_->ui.Compose(impl_->Bindings(), impl_->Viewport());
        if (!composed) return impl_->Fail(ExplainUi(composed.error()));
        draws = composed.value();
    }
    const auto prepared = Clock::now();
    const auto ui = impl_->uiRenderer.Submit(draws, impl_->queue);
    if (!ui) return impl_->Fail(ExplainUi(ui.error()));
    const auto uiFinished = Clock::now();
    const auto rendered = impl_->renderer.Render(impl_->queue);
    if (!rendered) return impl_->Fail(Explain(rendered.error()));
    const auto finished = Clock::now();
    if (firstWorldFrame || Milliseconds(finished, started) >= 250 || context.deltaSeconds >= .25)
        SDL_Log("PvP render player=%llu first_world_frame=%d frame_gap_ms=%.1f prepare_ms=%.1f ui_ms=%.1f render_ms=%.1f presented=%d",
            static_cast<unsigned long long>(impl_->state.playerId), firstWorldFrame, context.deltaSeconds * 1000,
            Milliseconds(prepared, started), Milliseconds(uiFinished, prepared), Milliseconds(finished, uiFinished),
            rendered.value() == Engine::Render::PresentStatus::Presented);
    if (rendered.value() == Engine::Render::PresentStatus::Presented) {
        impl_->renderedPlayer = world ? impl_->state.playerId : 0;
        if (world) {
            if (impl_->remoteMovement) {
                const auto& remote = *impl_->remoteMovement;
                PlayerState pose{remote.playerId, remote.renderPosition, remote.yaw, remote.pitch};
                pose.movementEpoch = remote.movementEpoch;
                impl_->remoteMovement->holding = impl_->timeline.CommitPresented(finished, &pose, remote.holding);
                impl_->remoteMovement->holdCount = impl_->timeline.HoldCount();
                impl_->remoteMovement->holdSeconds = impl_->timeline.HoldSeconds();
                impl_->remoteMovement->totalHoldSeconds = impl_->timeline.TotalHoldSeconds();
            } else (void)impl_->timeline.CommitPresented(finished, nullptr, false);
        }
        if (world) impl_->presentedMovement = PresentedMovementObservation{
            context.frameIndex, std::chrono::duration<double>(finished.time_since_epoch()).count(),
            impl_->state.playerId, impl_->prediction->Observation(), impl_->remoteMovement,
            impl_->skippedPresentationFrames, impl_->connectionGeneration};
        if (world) {
            MovementTraceEvent event;
            event.kind = MovementTraceKind::Presentation;
            event.timeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(finished.time_since_epoch()).count();
            event.playerId = impl_->state.playerId;
            event.epoch = impl_->prediction->Observation().movementEpoch;
            event.sequence = impl_->prediction->Observation().latestCommand;
            event.authorityTick = impl_->prediction->Observation().authorityTick;
            event.frameSeconds = context.deltaSeconds;
            TraceMovement(event);
        }
    } else {
        ++impl_->skippedPresentationFrames;
        // A minimized window has no swapchain wait to pace the runtime loop.
        SDL_Delay(10);
    }
    return Control::Continue;
}

int PvpApplication::Run() {
    Engine::Runtime::RuntimeLoop loop(*this);
    loop.Run();
    impl_->connection.Leave();
    impl_->RefreshState();
    if (!impl_->lastError.empty()) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", impl_->lastError.c_str());
    return impl_->exitCode;
}
void PvpApplication::SetGatewayAddress(std::string address) { impl_->address = std::move(address); }
ClientConnection& PvpApplication::Connection() { return impl_->connection; }
Engine::Render::Renderer& PvpApplication::Renderer() { return impl_->renderer; }
Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice& PvpApplication::RenderDevice() { return *impl_->device; }
Engine::Platform::Sdl::SdlPlatform& PvpApplication::Platform() { return *impl_->platform; }
const LocalMovementObservation& PvpApplication::LocalMovement() const noexcept {
    static const LocalMovementObservation inactive;
    return impl_->prediction ? impl_->prediction->Observation() : inactive;
}
const std::optional<RemoteMovementObservation>& PvpApplication::RemoteMovement() const noexcept {
    return impl_->remoteMovement;
}
const std::optional<PresentedMovementObservation>& PvpApplication::PresentedMovement() const noexcept {
    return impl_->presentedMovement;
}
std::uint64_t PvpApplication::SkippedPresentationFrames() const noexcept {
    return impl_->skippedPresentationFrames;
}
const std::string& PvpApplication::LastError() const noexcept { return impl_->lastError; }
int PvpApplication::ExitCode() const noexcept { return impl_->exitCode; }
} // namespace fps::pvp
