#include "RetroFPS/App/ObjectFpsRuntimeClient.hpp"

#include "RetroFPS/App/ObjectFpsUi.hpp"

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/input/InputActionMap.hpp"
#include "input/backend/sdl/SdlInput.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "ui/UiDocumentCodec.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace fps {
namespace {

using Engine::Input::InputActionId;
using Engine::Input::InputAxisId;

struct ObjectFpsBindings final {
    InputAxisId moveForward{InputAxisId::FromString("object_fps_v2.move.forward")};
    InputAxisId moveRight{InputAxisId::FromString("object_fps_v2.move.right")};
    InputAxisId lookX{InputAxisId::FromString("object_fps_v2.look.x")};
    InputAxisId lookY{InputAxisId::FromString("object_fps_v2.look.y")};
    InputActionId fire{InputActionId::FromString("object_fps_v2.fire")};
    InputActionId reload{InputActionId::FromString("object_fps_v2.reload")};
    InputActionId jump{InputActionId::FromString("object_fps_v2.jump")};
    InputActionId holster{InputActionId::FromString("object_fps_v2.holster")};
    InputActionId menuPrevious{
        InputActionId::FromString("object_fps_v2.menu.previous")};
    InputActionId menuNext{InputActionId::FromString("object_fps_v2.menu.next")};
    InputActionId menuAdjustPrevious{
        InputActionId::FromString("object_fps_v2.menu.adjust_previous")};
    InputActionId menuAdjustNext{
        InputActionId::FromString("object_fps_v2.menu.adjust_next")};
    InputActionId confirm{InputActionId::FromString("object_fps_v2.menu.confirm")};
    InputActionId back{InputActionId::FromString("object_fps_v2.menu.back")};
};

[[nodiscard]] Engine::Input::InputActionMap MakeActionMap(
    const ObjectFpsBindings& bindings) {
    using Engine::Input::Key;
    using Engine::Input::MouseButton;
    using Engine::Input::PointerAxis;

    Engine::Input::InputActionMap map;
    map.BindDigitalAxis(bindings.moveForward, Key::S, Key::W);
    map.BindDigitalAxis(bindings.moveRight, Key::A, Key::D);
    // Preserve raw pointer deltas at the GYO action boundary. The game's
    // PlayerSettings owns look sensitivity policy.
    map.BindPointerAxis(bindings.lookX, PointerAxis::DeltaX);
    map.BindPointerAxis(bindings.lookY, PointerAxis::DeltaY);
    map.Bind(bindings.fire, MouseButton::Left);
    map.Bind(bindings.reload, Key::R);
    map.Bind(bindings.jump, Key::Space);
    map.Bind(bindings.holster, Key::H);
    map.Bind(bindings.menuPrevious, Key::W);
    map.Bind(bindings.menuPrevious, Key::Up);
    map.Bind(bindings.menuNext, Key::S);
    map.Bind(bindings.menuNext, Key::Down);
    map.Bind(bindings.menuAdjustPrevious, Key::Left);
    map.Bind(bindings.menuAdjustNext, Key::Right);
    map.Bind(bindings.confirm, Key::Enter);
    map.Bind(bindings.back, Key::Escape);
    return map;
}

} // namespace

struct ObjectFpsRuntimeClient::Impl final {
    Engine::Platform::Sdl::SdlPlatform* platform{};
    Engine::Input::Backend::Sdl::SdlInput* input{};
    Engine::Asset::AssetManager* assets{};
    ObjectFpsPresentation* presentation{};
    ObjectFpsRuntimeClientConfig config;
    ObjectFpsBindings bindings;
    Engine::Input::InputActionMap actionMap{MakeActionMap(bindings)};
    GameSession session;
    ObjectFpsUi ui;
    ObjectFpsDisplaySettings displaySettings;
    Engine::Ui::UiDrawList uiDrawList;
    std::vector<GameSessionCommand> pendingCommands;
    std::string lastError;
    int exitCode{};
    bool initialized{};
    bool previousFocused{};

    [[nodiscard]] GameFrameInput TranslateGameInput(
        const Engine::Input::InputActionFrame& actions,
        const Engine::Input::PhysicalInputFrame& physical) noexcept {
        const Engine::Input::InputActionState fire = actions.Action(bindings.fire);
        GameFrameInput translated;
        translated.moveForward = actions.Axis(bindings.moveForward);
        translated.moveRight = actions.Axis(bindings.moveRight);
        translated.lookDeltaX = actions.Axis(bindings.lookX);
        translated.lookDeltaY = actions.Axis(bindings.lookY);
        translated.lookEnabled =
            physical.pointer.relativeMode && physical.windowFocused;
        translated.fireHeld = fire.held;
        translated.firePressed = fire.pressed;
        translated.reloadPressed = actions.Action(bindings.reload).pressed;
        translated.jumpPressed = actions.Action(bindings.jump).pressed;
        translated.jumpHeld = actions.Action(bindings.jump).held;
        translated.holsterTogglePressed = actions.Action(bindings.holster).pressed;
        translated.holsterToggleHeld = actions.Action(bindings.holster).held;
        // Non-playing Escape is consumed by UiRuntime's canvas cancel action.
        translated.backPressed =
            session.Snapshot().screen == GameScreen::Playing &&
            actions.Action(bindings.back).pressed;
        translated.focusLost = previousFocused && !physical.windowFocused;
        return translated;
    }

    [[nodiscard]] Engine::Ui::UiInputFrame TranslateUiInput(
        const Engine::Input::InputActionFrame& actions,
        const Engine::Input::PhysicalInputFrame& physical) const noexcept {
        const Engine::Input::ButtonState& pointer =
            physical.Get(Engine::Input::MouseButton::Left);
        Engine::Ui::UiInputFrame translated;
        translated.focusPreviousPressed =
            actions.Action(bindings.menuPrevious).pressed;
        translated.focusNextPressed = actions.Action(bindings.menuNext).pressed;
        translated.adjustPreviousPressed =
            actions.Action(bindings.menuAdjustPrevious).pressed;
        translated.adjustNextPressed =
            actions.Action(bindings.menuAdjustNext).pressed;
        translated.activatePressed = actions.Action(bindings.confirm).pressed;
        translated.cancelPressed = actions.Action(bindings.back).pressed;
        translated.pointerAvailable =
            !physical.pointer.relativeMode && physical.windowFocused;
        translated.pointerPixels = {
            physical.pointer.x,
            physical.pointer.y,
        };
        translated.pointerPrimaryPressed = pointer.pressed;
        translated.pointerPrimaryHeld = pointer.held;
        translated.pointerPrimaryReleased = pointer.released;
        return translated;
    }

    [[nodiscard]] bool InitializeUi(std::string& error) {
        if (!config.uiDocument.IsValid()) {
            error = "Object_FPS UI document asset id is invalid";
            return false;
        }
        const auto loaded = assets->Load(
            config.uiDocument,
            Engine::Asset::AssetRequest::WithTypeHint(
                Engine::Asset::AssetType::Text()));
        if (!loaded) {
            error = "failed to load Object_FPS UI JSON '" +
                    config.uiDocument.debugName + "': " + loaded.error().message;
            return false;
        }
        const Engine::Asset::AssetHandle handle = loaded.value();
        const auto text = assets->GetSharedConst<
            Engine::Asset::Loaders::TextAsset>(handle);
        if (!text) {
            assets->Release(handle);
            error = "Object_FPS UI asset loaded without a TextAsset payload";
            return false;
        }
        auto parsed = Engine::Ui::UiDocumentCodec::Parse(
            text->text,
            config.uiDocument.debugName);
        assets->Release(handle);
        if (!parsed) {
            error = "invalid Object_FPS UI JSON: " + parsed.error().message;
            if (!parsed.error().jsonPointer.empty()) {
                error += " at " + parsed.error().jsonPointer;
            }
            return false;
        }
        auto document = std::make_shared<const Engine::Ui::UiDocument>(
            std::move(parsed.value()));
        return ui.Initialize(std::move(document), error);
    }
};

ObjectFpsRuntimeClient::ObjectFpsRuntimeClient(
    Engine::Platform::Sdl::SdlPlatform& platform,
    Engine::Input::Backend::Sdl::SdlInput& input,
    Engine::Asset::AssetManager& assets,
    ObjectFpsPresentation& presentation,
    const ObjectFpsRuntimeClientConfig config) noexcept
    : impl_(std::make_unique<Impl>()) {
    impl_->platform = &platform;
    impl_->input = &input;
    impl_->assets = &assets;
    impl_->presentation = &presentation;
    impl_->config = config;
    impl_->previousFocused = input.Snapshot().windowFocused;
}

ObjectFpsRuntimeClient::~ObjectFpsRuntimeClient() = default;

bool ObjectFpsRuntimeClient::Initialize(
    std::shared_ptr<const CampaignContent> content,
    const GameSessionConfig& config,
    std::string& error) {
    error.clear();
    impl_->lastError.clear();
    if (!impl_->presentation->IsInitialized()) {
        error = "ObjectFpsRuntimeClient requires initialized GYO presentation";
        return false;
    }
    if (!impl_->InitializeUi(error)) {
        return false;
    }
    if (!impl_->session.Initialize(std::move(content), config, error)) {
        return false;
    }
    impl_->initialized = true;
    return true;
}

Engine::Runtime::RuntimeControl ObjectFpsRuntimeClient::ProcessEvents(
    const Engine::Runtime::FrameContext&) {
    if (!impl_->initialized) {
        impl_->lastError = "ObjectFpsRuntimeClient is not initialized";
        impl_->exitCode = 1;
        return Engine::Runtime::RuntimeControl::Stop;
    }
    impl_->input->BeginFrame();
    const Engine::Runtime::RuntimeControl control = impl_->platform->PumpEvents(
        [this](const SDL_Event& event) { impl_->input->HandleEvent(event); });
    impl_->input->EndFrame();
    return control;
}

Engine::Runtime::RuntimeControl ObjectFpsRuntimeClient::Update(
    const Engine::Runtime::FrameContext& frame) {
    const Engine::Input::PhysicalInputFrame& physical = impl_->input->Snapshot();
    const Engine::Input::InputActionFrame actions =
        impl_->actionMap.Evaluate(physical);
    const Engine::Ui::UiInputFrame uiInput =
        impl_->TranslateUiInput(actions, physical);
    const GameFrameInput gameInput =
        impl_->TranslateGameInput(actions, physical);

    impl_->assets->BeginFrame(frame.frameIndex);
    impl_->assets->Update();

    if (!impl_->ui.Update(
            impl_->session.Snapshot(),
            uiInput,
            {impl_->config.viewportWidth, impl_->config.viewportHeight},
            impl_->displaySettings,
            impl_->pendingCommands,
            impl_->lastError)) {
        impl_->pendingCommands.clear();
        impl_->exitCode = 1;
        return Engine::Runtime::RuntimeControl::Stop;
    }

    const float deltaSeconds = static_cast<float>(
        std::clamp(frame.deltaSeconds, 0.0, 0.05));
    if (!impl_->session.Advance(
            deltaSeconds,
            gameInput,
            std::span<const GameSessionCommand>(impl_->pendingCommands),
            impl_->lastError)) {
        impl_->pendingCommands.clear();
        impl_->exitCode = 1;
        return Engine::Runtime::RuntimeControl::Stop;
    }
    impl_->pendingCommands.clear();

    const bool wantsRelativeMouse =
        impl_->session.Snapshot().screen == GameScreen::Playing &&
        physical.windowFocused;
    const auto relativeResult =
        impl_->input->SetRelativeMouseMode(wantsRelativeMouse);
    if (!relativeResult) {
        impl_->lastError = relativeResult.error().message;
        if (!relativeResult.error().detail.empty()) {
            impl_->lastError += ": " + relativeResult.error().detail;
        }
        impl_->exitCode = 1;
        return Engine::Runtime::RuntimeControl::Stop;
    }
    impl_->previousFocused = physical.windowFocused;
    return impl_->session.Snapshot().quitRequested
        ? Engine::Runtime::RuntimeControl::Stop
        : Engine::Runtime::RuntimeControl::Continue;
}

Engine::Runtime::RuntimeControl ObjectFpsRuntimeClient::Render(
    const Engine::Runtime::FrameContext& frame) {
    if (!impl_->ui.Compose(
            impl_->session.Snapshot(),
            impl_->displaySettings,
            {impl_->config.viewportWidth, impl_->config.viewportHeight},
            impl_->uiDrawList,
            impl_->lastError) ||
        !impl_->presentation->Present(
            impl_->session.Snapshot(),
            impl_->displaySettings,
            impl_->uiDrawList,
            impl_->lastError)) {
        impl_->exitCode = 1;
        return Engine::Runtime::RuntimeControl::Stop;
    }
    static_cast<void>(frame);
    return Engine::Runtime::RuntimeControl::Continue;
}

const GameSessionSnapshot& ObjectFpsRuntimeClient::Query() const noexcept {
    return impl_->session.Snapshot();
}

void ObjectFpsRuntimeClient::Submit(GameSessionCommand command) {
    impl_->pendingCommands.push_back(std::move(command));
}

std::span<const GameSessionEvent> ObjectFpsRuntimeClient::Events() const noexcept {
    return impl_->session.Events();
}

const std::string& ObjectFpsRuntimeClient::LastError() const noexcept {
    return impl_->lastError;
}

int ObjectFpsRuntimeClient::ExitCode() const noexcept {
    return impl_->exitCode;
}

const ObjectFpsDisplaySettings&
ObjectFpsRuntimeClient::DisplaySettings() const noexcept {
    return impl_->displaySettings;
}

} // namespace fps
