#include "engine/input/InputModule.hpp"

#include <utility>

#include "engine/base/Result.hpp"
#include "engine/input/Input.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"

namespace GYO::Engine::Input {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const InputOptions& OptionsOf(const InputOptions* given) {
    static const InputOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// Bevy's keyboard_input_system (bevy_input keyboard.rs:170-204): the frame's key events in order, then a focus loss
// (or dropped events) releases every key. Repeats change nothing; a release of a key not held changes nothing
// (SDL sends the focus loss first and the key-up after it, SDL_keyboard.c:354-356).
SystemResult UpdateKeyboard(Kernel::ResMut<ButtonInput<KeyCode>> keys, Kernel::MessageReader<KeyboardInput> input,
                            Kernel::MessageReader<KeyboardFocusLost> focusLost,
                            Kernel::MessageReader<InputEventsDropped> dropped) {
    ButtonInput<KeyCode>& state = keys.Get();
    state.Clear();
    for (const KeyboardInput* event : input.Read()) {
        if (event->state == ButtonState::Pressed) {
            state.Press(event->keyCode);
        } else {
            state.Release(event->keyCode);
        }
    }
    const bool lost = !focusLost.Read().empty();
    const bool droppedAny = !dropped.Read().empty();
    if (lost || droppedAny) state.ReleaseAll();
    return {};
}

// Bevy's mouse_button_input_system (bevy_input mouse.rs). Dropped events release every button too (IN-3); a focus
// loss does not (as in Bevy: the OS sends the button-up to the window that captured the press).
SystemResult UpdateMouseButtons(Kernel::ResMut<ButtonInput<MouseButton>> buttons,
                                Kernel::MessageReader<MouseButtonInput> input,
                                Kernel::MessageReader<InputEventsDropped> dropped) {
    ButtonInput<MouseButton>& state = buttons.Get();
    state.Clear();
    for (const MouseButtonInput* event : input.Read()) {
        if (event->state == ButtonState::Pressed) {
            state.Press(event->button);
        } else {
            state.Release(event->button);
        }
    }
    if (!dropped.Read().empty()) state.ReleaseAll();
    return {};
}

// Bevy's accumulate_mouse_motion_system and accumulate_mouse_scroll_system: the sums of this frame.
SystemResult AccumulateMouse(Kernel::ResMut<AccumulatedMouseMotion> motion, Kernel::ResMut<AccumulatedMouseScroll> scroll,
                             Kernel::MessageReader<MouseMotion> motions, Kernel::MessageReader<MouseWheel> wheels) {
    Math::Vec2 moved{};
    for (const MouseMotion* event : motions.Read()) moved = moved + event->delta;
    Math::Vec2 scrolled{};
    for (const MouseWheel* event : wheels.Read()) scrolled = scrolled + event->delta;
    motion.Get().delta = moved;
    scroll.Get().delta = scrolled;
    return {};
}

// The cursor's place: the last CursorMoved; nullopt when a CursorLeft came at or after it (the two types are read
// apart, so their order is told by time; a tie means the cursor left). A CursorEntered alone changes nothing: the
// place is known at the next CursorMoved (the backend writes one with the entry).
SystemResult UpdateCursor(Kernel::ResMut<CursorPosition> cursor, Kernel::MessageReader<CursorMoved> moves,
                          Kernel::MessageReader<CursorLeft> left) {
    std::optional<Math::Vec2> moved;
    Time::TimePoint movedAt{};
    for (const CursorMoved* event : moves.Read()) {
        moved = event->position;
        movedAt = event->time;
    }
    std::optional<Time::TimePoint> leftAt;
    for (const CursorLeft* event : left.Read()) leftAt = event->time;
    if (moved && (!leftAt || movedAt > *leftAt)) {
        cursor.Get().position = moved;
    } else if (leftAt) {
        cursor.Get().position.reset();
    }
    return {};
}

// The composition in progress: the last TextComposition, cleared by a commit that comes at or after it.
SystemResult UpdateTextInput(Kernel::ResMut<TextInput> text, Kernel::MessageReader<TextComposition> compositions,
                             Kernel::MessageReader<TextCommitted> commits) {
    const TextComposition* last = nullptr;
    for (const TextComposition* event : compositions.Read()) last = event;
    std::optional<Time::TimePoint> committedAt;
    for (const TextCommitted* event : commits.Read()) committedAt = event->time;
    if (last != nullptr && (!committedAt || last->time > *committedAt)) {
        text.Get() = TextInput{last->text, last->cursor, last->selectionLength};
    } else if (committedAt) {
        text.Get() = TextInput{};
    }
    return {};
}

Kernel::SystemOptions InSet(std::string name, const InputOptions& options) {
    Kernel::SystemOptions system;
    system.name = std::move(name);
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.sets.push_back(std::string(kInputSet));
    system.mounts.push_back(std::move(mount));
    return system;
}

template <class T>
void PutIfAbsent(Kernel::World& world, T value) {
    if (world.Resource<T>() == nullptr) world.SetResource(std::move(value));
}

} // namespace

void InputModule::Build(Orchestration::WorldSetup& setup) {
    const InputOptions& options = OptionsOf(setup.Options<InputOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Message<KeyboardInput>({.name = "KeyboardInput"});
    r.Message<KeyboardFocusLost>({.name = "KeyboardFocusLost"});
    r.Message<InputEventsDropped>({.name = "InputEventsDropped"});
    r.Message<MouseButtonInput>({.name = "MouseButtonInput"});
    r.Message<CursorMoved>({.name = "CursorMoved"});
    r.Message<MouseMotion>({.name = "MouseMotion"});
    r.Message<CursorEntered>({.name = "CursorEntered"});
    r.Message<CursorLeft>({.name = "CursorLeft"});
    r.Message<MouseWheel>({.name = "MouseWheel"});
    r.Message<TextComposition>({.name = "TextComposition"});
    r.Message<TextCommitted>({.name = "TextCommitted"});
    r.Resource<ButtonInput<KeyCode>>({.name = "KeyboardButtons"});
    r.Resource<ButtonInput<MouseButton>>({.name = "MouseButtons"});
    r.Resource<AccumulatedMouseMotion>({.name = "AccumulatedMouseMotion"});
    r.Resource<AccumulatedMouseScroll>({.name = "AccumulatedMouseScroll"});
    r.Resource<CursorPosition>({.name = "CursorPosition"});
    r.Resource<TextInput>({.name = "TextInput"});
    r.Resource<TextInputRequest>({.name = "TextInputRequest"});
    r.Set({.name = std::string(kInputSet), .schedule = options.schedule});
    r.AddSystem<&UpdateKeyboard>(InSet("UpdateKeyboard", options));
    r.AddSystem<&UpdateMouseButtons>(InSet("UpdateMouseButtons", options));
    r.AddSystem<&AccumulateMouse>(InSet("AccumulateMouse", options));
    r.AddSystem<&UpdateCursor>(InSet("UpdateCursor", options));
    r.AddSystem<&UpdateTextInput>(InSet("UpdateTextInput", options));
    // A new World starts with nothing held (a value carried over, O-7, stays). Its backend writes what it knows of
    // the present (the cursor, the focus) in the World's first round (IN-2, SDL-7 exception (2)).
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent(world, ButtonInput<KeyCode>{});
        PutIfAbsent(world, ButtonInput<MouseButton>{});
        PutIfAbsent(world, AccumulatedMouseMotion{});
        PutIfAbsent(world, AccumulatedMouseScroll{});
        PutIfAbsent(world, CursorPosition{});
        PutIfAbsent(world, TextInput{});
        PutIfAbsent(world, TextInputRequest{});
        return {};
    });
}

} // namespace GYO::Engine::Input
