#include "engine/world_ui/WorldUiModule.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"

namespace GYO::Engine::WorldUi {

namespace {

// MapUiInput's memory between frames (a Local: nothing of the World's): whether the input method was composing when
// the last frame ended (a key pressed while it composes is the input method's, UI-5).
struct UiInputMapState final {
    bool composing = false;
};

// ComposeUi's memory (a Local): the text input request world_ui last wrote, so it writes only when its own wish
// changes and does not overwrite another writer's (ImGui's) every frame (IN-4).
struct UiRequestState final {
    std::optional<Input::TextInputRequest> written;
};

using SystemResult = Base::Result<void, Kernel::KernelError>;

const UiOptions& OptionsOf(const UiOptions* given) {
    static const UiOptions defaults{};
    return given != nullptr ? *given : defaults;
}

Ui::UiRootState StateFromRoot(const Ui::UiRoot&) { return Ui::UiRootState{}; }

// ---- MapUiInput

// One input event of the frame, in the order the events happened.
struct TimedEvent final {
    Time::TimePoint time{};
    // 0: a key, 1: a composition, 2: a commit. Keys first at the same time: a key that comes with a commit was
    // pressed while the input method still composed.
    int rank = 0;
    const Input::KeyboardInput* key = nullptr;
    const Input::TextComposition* composition = nullptr;
    const Input::TextCommitted* commit = nullptr;
};

bool AnyEditing(Kernel::Query<Kernel::Read<Ui::UiRootState>>& states) {
    bool editing = false;
    states.ForEach(
        [&editing](Kernel::EntityId, const Ui::UiRootState& state) {
            if (state.runtime && !state.runtime->InteractionState().editingElement.empty()) editing = true;
        });
    return editing;
}

void MapKey(const Input::KeyboardInput& key, const bool editing, const Window::Clipboard* clipboard, Ui::UiInput& out) {
    using Input::KeyCode;
    const bool shift = key.modifiers.Has(Input::KeyModifier::Shift);
    // The shortcut modifier: Control (Windows, Linux) or Command (macOS); both are taken, so no platform is chosen here.
    const bool shortcut = key.modifiers.Has(Input::KeyModifier::Control) || key.modifiers.Has(Input::KeyModifier::Super);
    const auto edit = [&out](const Ui::UiTextInputKind kind, std::string text = {}) {
        out.text.push_back(Ui::UiTextInput{kind, std::move(text), -1});
    };
    switch (key.keyCode) {
    case KeyCode::Tab:
        if (key.repeat) return;
        (shift ? out.focusPreviousPressed : out.focusNextPressed) = true;
        return;
    case KeyCode::ArrowUp:
        out.focusPreviousPressed = true;
        return;
    case KeyCode::ArrowDown:
        out.focusNextPressed = true;
        return;
    case KeyCode::ArrowLeft:
        if (editing) {
            edit(Ui::UiTextInputKind::MoveLeft);
        } else {
            out.adjustPreviousPressed = true;
        }
        return;
    case KeyCode::ArrowRight:
        if (editing) {
            edit(Ui::UiTextInputKind::MoveRight);
        } else {
            out.adjustNextPressed = true;
        }
        return;
    case KeyCode::Enter:
    case KeyCode::NumpadEnter:
        if (!key.repeat) out.activatePressed = true;
        return;
    case KeyCode::Escape:
        if (!key.repeat) out.cancelPressed = true;
        return;
    default: break;
    }
    if (!editing) return;
    switch (key.keyCode) {
    case KeyCode::Backspace: edit(Ui::UiTextInputKind::Backspace); return;
    case KeyCode::Delete: edit(Ui::UiTextInputKind::Delete); return;
    case KeyCode::Home: edit(Ui::UiTextInputKind::MoveHome); return;
    case KeyCode::End: edit(Ui::UiTextInputKind::MoveEnd); return;
    default: break;
    }
    // Shortcuts follow the layout's character (an AZERTY A is where a QWERTY Q is).
    const char32_t character = key.character;
    if (shortcut && (character == U'a' || character == U'A')) {
        edit(Ui::UiTextInputKind::SelectAll);
    } else if (shortcut && (character == U'v' || character == U'V') && clipboard != nullptr && !clipboard->text.empty()) {
        edit(Ui::UiTextInputKind::Insert, clipboard->text);
    }
}

// The default mapping (UI-6): input's Messages of the frame, merged by the time they happened (the types are read
// apart and their order is not kept, IN-1), and its pointer state -> UiInput.
SystemResult MapUiInput(Kernel::MessageReader<Input::KeyboardInput> keys,
                        Kernel::MessageReader<Input::TextComposition> compositions,
                        Kernel::MessageReader<Input::TextCommitted> commits,
                        Kernel::Res<Input::ButtonInput<Input::MouseButton>> buttons,
                        Kernel::Res<Input::CursorPosition> cursor, Kernel::Res<Window::Clipboard> clipboard,
                        Kernel::Query<Kernel::Read<Ui::UiRootState>> states, Kernel::Local<UiInputMapState> memory,
                        Kernel::ResMut<Ui::UiInput> out) {
    std::vector<TimedEvent> events;
    for (const Input::KeyboardInput* key : keys.Read()) {
        if (key->state == Input::ButtonState::Pressed) events.push_back({key->time, 0, key, nullptr, nullptr});
    }
    for (const Input::TextComposition* c : compositions.Read()) events.push_back({c->time, 1, nullptr, c, nullptr});
    for (const Input::TextCommitted* c : commits.Read()) events.push_back({c->time, 2, nullptr, nullptr, c});
    std::stable_sort(events.begin(), events.end(), [](const TimedEvent& a, const TimedEvent& b) {
        if (a.time != b.time) return a.time < b.time;
        return a.rank < b.rank;
    });

    const bool editing = AnyEditing(states);
    const Window::Clipboard* paste = &*clipboard;
    Ui::UiInput input;
    bool composing = memory->composing;
    for (const TimedEvent& event : events) {
        if (event.composition != nullptr) {
            composing = !event.composition->text.empty();
            input.text.push_back(Ui::UiTextInput{Ui::UiTextInputKind::Composition, event.composition->text,
                                                 event.composition->cursorByte});
        } else if (event.commit != nullptr) {
            composing = false;
            input.text.push_back(Ui::UiTextInput{Ui::UiTextInputKind::Insert, event.commit->text, -1});
        } else if (!composing) {
            // A key while the input method composes is the input method's (its Enter commits, its Escape cancels).
            MapKey(*event.key, editing, paste, input);
        }
    }
    memory->composing = composing;

    input.pointerAvailable = cursor->position.has_value();
    if (cursor->position) input.pointerPixels = *cursor->position;
    input.pointerPrimaryPressed = buttons->JustPressed(Input::MouseButton::Left);
    input.pointerPrimaryHeld = buttons->Pressed(Input::MouseButton::Left);
    input.pointerPrimaryReleased = buttons->JustReleased(Input::MouseButton::Left);
    out.Get() = std::move(input);
    return {};
}

// ---- UpdateUi and ComposeUi

struct CameraView final {
    Kernel::EntityId entity{};
    const Camera::ComputedCamera* computed = nullptr;
};

using CameraQuery = Kernel::Query<Kernel::Read<Camera::Camera>, Kernel::Read<Camera::ComputedCamera>>;
using TargetQuery = Kernel::Query<Kernel::Read<Camera::CameraTarget>>;

// The camera a root is laid out in: its own, or the active camera on target 0 with the largest (order, entity)
// (Bevy's DefaultUiCamera, bevy_ui/src/ui_node.rs:3471-3490). None when that camera is not valid.
std::optional<CameraView> CameraOf(const Ui::UiRoot& root, CameraQuery& cameras, TargetQuery& targets) {
    std::optional<CameraView> chosen;
    if (root.camera) {
        const Camera::Camera* camera = cameras.Get<Camera::Camera>(*root.camera);
        const Camera::ComputedCamera* computed = cameras.Get<Camera::ComputedCamera>(*root.camera);
        if (camera != nullptr && computed != nullptr) chosen = CameraView{*root.camera, computed};
    } else {
        std::int32_t bestOrder = 0;
        cameras.ForEach([&](const Kernel::EntityId id, const Camera::Camera& camera, const Camera::ComputedCamera& computed) {
            if (!camera.active) return;
            const Camera::CameraTarget* target = targets.Get<Camera::CameraTarget>(id);
            if (target != nullptr && target->id != 0) return;
            if (!chosen || camera.order > bestOrder || (camera.order == bestOrder && id > chosen->entity)) {
                chosen = CameraView{id, &computed};
                bestOrder = camera.order;
            }
        });
    }
    if (!chosen || !chosen->computed->valid || chosen->computed->viewport.size.x <= 0 ||
        chosen->computed->viewport.size.y <= 0) {
        return std::nullopt;
    }
    return chosen;
}

// The document a root shows this frame; null while its asset is not loaded.
std::shared_ptr<const Ui::UiDocument> DocumentOf(const Ui::UiRoot& root, const WorldAsset::AssetView& assets) {
    if (const auto* held = std::get_if<std::shared_ptr<const Ui::UiDocument>>(&root.document)) return *held;
    if (const auto* handle = std::get_if<Asset::Handle<Ui::UiAsset>>(&root.document)) {
        const Ui::UiAsset* asset = assets.Get(*handle);
        if (asset != nullptr) return asset->document;
    }
    return nullptr;
}

// Brings the root's runtime to its document and canvas. False: nothing to run this frame (not loaded, or failed).
bool Prepare(const Ui::UiRoot& root, const std::shared_ptr<const Ui::UiDocument>& document, Ui::UiRootState& state) {
    if (document == nullptr) {
        state.runtime.reset();
        state.drawList.commands.clear();
        state.editArea.reset();
        return false;
    }
    if (!state.runtime || state.runtime->Document() != document) {
        state.runtime.emplace();
        auto initialized = state.runtime->Initialize(document);
        if (!initialized) {
            state.runtime.reset();
            state.failure = std::move(initialized).error();
            return false;
        }
    }
    if (state.runtime->InteractionState().activeCanvas != root.canvas) {
        auto activated = state.runtime->ActivateCanvas(root.canvas);
        if (!activated) {
            state.failure = std::move(activated).error();
            return false;
        }
    }
    state.runtime->SetTextFilter(root.filter);
    return true;
}

void SetViewport(const CameraView& camera, Ui::UiRootState& state) {
    const Camera::Viewport& viewport = camera.computed->viewport;
    state.camera = camera.entity;
    state.viewportPosition = {static_cast<float>(viewport.position.x), static_cast<float>(viewport.position.y)};
    state.viewport = {static_cast<float>(viewport.size.x), static_cast<float>(viewport.size.y)};
    state.scaleFactor = camera.computed->scaleFactor;
}

Ui::UiBindingTable BindingsOf(Kernel::Query<Kernel::Read<Ui::UiBindings>>& bindings, const Kernel::EntityId id) {
    const Ui::UiBindings* found = bindings.Get<Ui::UiBindings>(id);
    return found != nullptr ? found->Table() : Ui::UiBindingTable{};
}

SystemResult UpdateUi(Kernel::Query<Kernel::Read<Ui::UiRoot>, Kernel::Write<Ui::UiRootState>> roots,
                      Kernel::Query<Kernel::Read<Ui::UiBindings>> bindings, CameraQuery cameras, TargetQuery targets,
                      Kernel::Res<Ui::UiInput> input, Kernel::Res<WorldAsset::AssetView> assets,
                      Kernel::ResMut<Ui::UiText> text, Kernel::MessageWriter<Ui::UiAction> actions) {
    roots.ForEach([&](const Kernel::EntityId id, const Ui::UiRoot& root, Kernel::Mut<Ui::UiRootState> stateMut) {
        Ui::UiRootState& state = stateMut.Get();
        state.failure.reset();
        if (!Prepare(root, DocumentOf(root, *assets), state)) return;
        const std::optional<CameraView> camera = CameraOf(root, cameras, targets);
        if (!camera) {
            state.camera.reset();
            return;
        }
        SetViewport(*camera, state);
        // The frame's input in this root's viewport: logical pixels of the window -> physical pixels of the viewport.
        Ui::UiInput local;
        if (root.acceptsInput) {
            local = *input;
            local.pointerPixels = {local.pointerPixels.x * state.scaleFactor - state.viewportPosition.x,
                                   local.pointerPixels.y * state.scaleFactor - state.viewportPosition.y};
        }
        auto updated = state.runtime->Update(local, BindingsOf(bindings, id), state.viewport, text.Get());
        if (!updated) {
            state.failure = std::move(updated).error();
            return;
        }
        for (Ui::UiActionEvent& event : updated.value()) {
            actions.Send(Ui::UiAction{id, std::move(event.action), std::move(event.sourceElement), std::move(event.payload)});
        }
    });
    return {};
}

// The fonts of a root's document that are loaded and not yet in text.
void AddFonts(const Ui::UiRoot& root, const WorldAsset::AssetView& assets, Ui::UiText& text, Ui::UiRootState& state) {
    const auto* handle = std::get_if<Asset::Handle<Ui::UiAsset>>(&root.document);
    if (handle == nullptr) return;
    const Ui::UiAsset* asset = assets.Get(*handle);
    if (asset == nullptr) return;
    for (const auto& [id, reference] : asset->fonts) {
        if (text.HasFont(id)) continue;
        const Text::FontData* font = assets.Get(reference);
        if (font == nullptr) continue;
        auto added = text.AddFont(id, *font);
        if (!added) {
            const Text::TextFailure& failure = added.error();
            state.failure = Ui::UiFailure(Ui::UiFailureCode::FontUnusable, "font '" + id + "' could not be added",
                                          {}, {}, Base::CauseDetail(failure));
        }
    }
}

void RequestTextures(const Ui::UiDrawList& drawList, const WorldAsset::AssetRequests& requests, Ui::UiRootState& state) {
    for (const Ui::UiDrawCommand& command : drawList.commands) {
        const auto* image = std::get_if<Ui::UiImageDraw>(&command);
        if (image == nullptr || image->textureAsset.empty() || state.Texture(image->textureAsset) != nullptr) {
            continue;
        }
        state.textures.emplace_back(image->textureAsset, requests.Load<Image::ImageData>(Asset::AssetId(image->textureAsset)));
    }
}

// The request for the input method in logical pixels of the window (the field and the caret were in physical pixels
// of the viewport).
Input::TextInputRequest RequestFor(const Ui::UiRootState& state) {
    const Ui::UiEditArea& area = *state.editArea;
    const float scale = state.scaleFactor > 0.0F ? state.scaleFactor : 1.0F;
    Input::TextInputRequest request;
    request.enabled = true;
    request.areaPosition = {(state.viewportPosition.x + area.field.x) / scale, (state.viewportPosition.y + area.field.y) / scale};
    request.areaSize = {area.field.width / scale, area.field.height / scale};
    request.cursorOffset = static_cast<std::int32_t>(std::lround(area.caretX / scale));
    return request;
}

SystemResult ComposeUi(Kernel::Query<Kernel::Read<Ui::UiRoot>, Kernel::Write<Ui::UiRootState>> roots,
                       Kernel::Query<Kernel::Read<Ui::UiBindings>> bindings, CameraQuery cameras, TargetQuery targets,
                       Kernel::Res<WorldAsset::AssetView> assets, Kernel::Res<WorldAsset::AssetRequests> requests,
                       Kernel::ResMut<Ui::UiText> textMut, Kernel::Local<UiRequestState> requestState,
                       Kernel::ResMut<Input::TextInputRequest> textInputRequest) {
    Ui::UiText& text = textMut.Get();
    std::optional<Input::TextInputRequest> wish;
    roots.ForEach([&](const Kernel::EntityId id, const Ui::UiRoot& root, Kernel::Mut<Ui::UiRootState> stateMut) {
        Ui::UiRootState& state = stateMut.Get();
        state.drawList.commands.clear();
        state.editArea.reset();
        if (!Prepare(root, DocumentOf(root, *assets), state)) return;
        const std::optional<CameraView> camera = CameraOf(root, cameras, targets);
        if (!camera) {
            state.camera.reset();
            return;
        }
        SetViewport(*camera, state);
        AddFonts(root, *assets, text, state);
        auto composed = state.runtime->Compose(BindingsOf(bindings, id), state.viewport, text);
        if (!composed) {
            state.failure = std::move(composed).error();
            return;
        }
        Ui::UiFrame frame = std::move(composed).value();
        state.drawList = std::move(frame.drawList);
        state.editArea = frame.editArea;
        RequestTextures(state.drawList, *requests, state);
        if (state.editArea && !wish) wish = RequestFor(state);
    });
    text.EndFrame();

    // Written only when world_ui's own wish changes (IN-4: another writer, ImGui, is not overwritten every frame).
    UiRequestState& written = *requestState;
    const Input::TextInputRequest next = wish.value_or(Input::TextInputRequest{});
    if (!written.written ? next.enabled : !(*written.written == next)) {
        textInputRequest.Get() = next;
        written.written = next;
    }
    return {};
}

Kernel::SystemOptions InSet(std::string name, const std::string& schedule, const std::string& stage,
                            std::vector<std::string> after) {
    Kernel::SystemOptions system;
    system.name = std::move(name);
    Kernel::Mount mount;
    mount.schedule = schedule;
    mount.stage = stage;
    mount.sets.push_back(std::string(kUiSet));
    mount.after = std::move(after);
    system.mounts.push_back(std::move(mount));
    return system;
}

template <class T>
void PutIfAbsent(Kernel::World& world, T value) {
    if (world.Resource<T>() == nullptr) world.SetResource(std::move(value));
}

} // namespace

void WorldUiModule::Build(Orchestration::WorldSetup& setup) {
    const UiOptions& options = OptionsOf(setup.Options<UiOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Component<Ui::UiRoot>({.name = "UiRoot"});
    r.Component<Ui::UiBindings>({.name = "UiBindings"});
    r.Component<Ui::UiRootState>({.name = "UiRootState"});
    r.InitialValue<Ui::UiRootState, &StateFromRoot>();
    r.Message<Ui::UiAction>({.name = "UiAction"});
    r.Resource<Ui::UiInput>({.name = "UiInput"});
    r.Resource<Ui::UiText>({.name = "UiText"});
    r.Set({.name = std::string(kUiSet), .schedule = options.inputSchedule});
    if (options.composeSchedule != options.inputSchedule) {
        r.Set({.name = std::string(kUiSet), .schedule = options.composeSchedule});
    }
    std::vector<std::string> afterInput{std::string(Input::kInputSet)};
    if (options.defaultInputMapping) {
        r.AddSystem<&MapUiInput>(InSet("MapUiInput", options.inputSchedule, options.inputStage, afterInput));
        afterInput.push_back("MapUiInput");
    }
    r.AddSystem<&UpdateUi>(InSet("UpdateUi", options.inputSchedule, options.inputStage, std::move(afterInput)));
    r.AddSystem<&ComposeUi>(InSet("ComposeUi", options.composeSchedule, options.composeStage,
                                  {std::string(Camera::kUpdateSet)}));
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent(world, Ui::UiInput{});
        PutIfAbsent(world, Ui::UiText{});
        return {};
    });
}

} // namespace GYO::Engine::WorldUi
