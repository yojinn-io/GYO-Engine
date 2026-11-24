// sdl_window (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_window"; Design Invariants SDL-3, SDL-4, SDL-7) on
// SDL's dummy video driver: SDL events pushed with SDL_PushEvent go through the adapter's pump and become the input
// and window Messages of the mapping table, in order; a full queue drops and counts; the writers run before input's
// and window's sets; a new World's first round writes the present state; the requests reach SDL; the runner ends
// when the game's close was applied. These tests call SDL, so they live in tests/adapters/sdl/ (SDL-1).
// Each CHECK carries a tag in brackets ("[sw-...]"): the mutants expect it
// (docs/architecture/target/v2/scripts/window_input_mutations.json).
#include <SDL3/SDL.h>

#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "engine/window_camera/WindowCameraModule.hpp"
#include "plugins/sdl/NativeWindow.hpp"
#include "plugins/sdl_window/SDLWindowPlugin.hpp"

#include "../../../src/EventQueue.hpp"
#include "../../../src/WindowBackend.hpp"
#include "../../../src/adapters/sdl/Translate.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDLWindow {

namespace C = GYO::Engine::Camera;
namespace I = GYO::Engine::Input;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace S = GYO::Plugins::SDLWindow;
namespace W = GYO::Engine::Window;

namespace {

// SDL's dummy video driver, set before SDL starts (ctest also sets SDL_VIDEODRIVER; the mutation runner does not).
const bool dummyDriver = SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");

W::WindowSettings Hidden() {
    static_cast<void>(dummyDriver);
    W::WindowSettings settings;
    settings.title = "sdl_window test";
    settings.size = {320, 240};
    settings.hidden = true;
    return settings;
}

std::unique_ptr<S::WindowBackend> MakeBackend() {
    S::BackendSettings settings;
    settings.window = Hidden();
    auto made = S::WindowBackend::Create(settings);
    REQUIRE_MESSAGE(static_cast<bool>(made), (made ? std::string() : made.error().message));
    return std::move(*made);
}

SDL_WindowID IdOf(const S::WindowBackend& backend) {
    return SDL_GetWindowID(reinterpret_cast<SDL_Window*>(backend.Native()));
}

void Push(SDL_Event event) { REQUIRE_MESSAGE(SDL_PushEvent(&event), SDL_GetError()); }

SDL_Event Key(SDL_WindowID id, SDL_Scancode code, bool down) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = id;
    e.key.scancode = code;
    e.key.down = down;
    e.key.mod = SDL_KMOD_LSHIFT;
    return e;
}

SDL_Event WindowEvent(SDL_WindowID id, SDL_EventType type) {
    SDL_Event e{};
    e.type = type;
    e.window.windowID = id;
    return e;
}

// Takes whatever SDL queued on its own (the window's creation), so a test sees only what it pushed.
void Drain(S::WindowBackend& backend) {
    S::EventQueue scratch(4096, 4096);
    backend.Pump(scratch);
}

template <class T, class V>
bool Is(const V& item) {
    return std::holds_alternative<T>(item);
}

} // namespace

TEST_CASE("sdl_window: SDL events become the Messages of the mapping table, in order (SDL-7)") {
    auto backend = MakeBackend();
    Drain(*backend);
    const SDL_WindowID id = IdOf(*backend);
    static const char committed[] = "x";
    Push(Key(id, SDL_SCANCODE_A, true));
    SDL_Event text{};
    text.type = SDL_EVENT_TEXT_INPUT;
    text.text.windowID = id;
    text.text.text = committed;
    Push(text);
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = id;
    motion.motion.x = 10.0F;
    motion.motion.y = 20.0F;
    motion.motion.xrel = 1.0F;
    motion.motion.yrel = -2.0F;
    Push(motion);
    SDL_Event button{};
    button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    button.button.windowID = id;
    button.button.button = SDL_BUTTON_X1;
    button.button.down = true;
    Push(button);
    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.windowID = id;
    wheel.wheel.y = 1.0F;
    wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    Push(wheel);
    Push(WindowEvent(id, SDL_EVENT_WINDOW_MOUSE_ENTER));
    Push(WindowEvent(id, SDL_EVENT_WINDOW_FOCUS_LOST));
    Push(WindowEvent(id, SDL_EVENT_WINDOW_CLOSE_REQUESTED));
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    Push(quit);
    SDL_Event user{};
    user.type = SDL_EVENT_USER;
    Push(user);
    Push(Key(id + 1000, SDL_SCANCODE_B, true));

    S::EventQueue queue(1024, 256);
    backend->Pump(queue);
    const std::vector<S::InputItem> input = queue.TakeInput();
    const std::vector<S::WindowItem> window = queue.TakeWindow();
    REQUIRE_MESSAGE(input.size() == 8, "[sw-map-input-count] 8 input Messages (key, text, moved+motion, button, wheel, entered, focus lost)");
    CHECK_MESSAGE(Is<I::KeyboardInput>(input[0]), "[sw-map-key] a key event becomes a KeyboardInput");
    const auto& key = std::get<I::KeyboardInput>(input[0]);
    CHECK_MESSAGE(key.keyCode == I::KeyCode::A, "[sw-map-scancode] the scancode is the HID KeyCode");
    CHECK_MESSAGE(key.modifiers.Has(I::KeyModifier::Shift), "[sw-map-mod] SDL's modifiers are carried as they are");
    CHECK_MESSAGE(Is<I::TextCommitted>(input[1]), "[sw-map-text] text input becomes a TextCommitted");
    CHECK_MESSAGE(std::get<I::TextCommitted>(input[1]).text == "x", "[sw-map-text-value] with its text");
    CHECK_MESSAGE(Is<I::CursorMoved>(input[2]), "[sw-map-moved] a motion becomes a CursorMoved");
    CHECK_MESSAGE(std::get<I::CursorMoved>(input[2]).position.x == 10.0F, "[sw-map-moved-value] at its place");
    CHECK_MESSAGE(Is<I::MouseMotion>(input[3]), "[sw-map-motion] and a MouseMotion right after it");
    CHECK_MESSAGE(std::get<I::MouseMotion>(input[3]).delta.y == -2.0F, "[sw-map-motion-value] with the relative motion");
    CHECK_MESSAGE(Is<I::MouseButtonInput>(input[4]), "[sw-map-button] a button event becomes a MouseButtonInput");
    CHECK_MESSAGE(std::get<I::MouseButtonInput>(input[4]).button == I::MouseButton::Back, "[sw-map-button-x1] X1 is Back");
    CHECK_MESSAGE(Is<I::MouseWheel>(input[5]), "[sw-map-wheel] a wheel event becomes a MouseWheel");
    CHECK_MESSAGE(std::get<I::MouseWheel>(input[5]).delta.y == 1.0F, "[sw-map-wheel-as-given] the wheel as the OS gives it (natural scrolling kept)");
    CHECK_MESSAGE(Is<I::CursorEntered>(input[6]), "[sw-map-entered] the mouse entering becomes a CursorEntered");
    CHECK_MESSAGE(Is<I::KeyboardFocusLost>(input[7]), "[sw-map-focus-input] a focus loss becomes input's KeyboardFocusLost too");
    REQUIRE_MESSAGE(window.size() == 3, "[sw-map-window-count] 3 window Messages (focus, close, quit)");
    CHECK_MESSAGE(Is<W::WindowFocused>(window[0]), "[sw-map-focus] a focus loss becomes a WindowFocused");
    CHECK_MESSAGE(!std::get<W::WindowFocused>(window[0]).focused, "[sw-map-focus-lost] not focused");
    CHECK_MESSAGE(Is<W::WindowCloseRequested>(window[1]), "[sw-map-close] a close request becomes a WindowCloseRequested");
    CHECK_MESSAGE(std::get<W::WindowCloseRequested>(window[1]).source == W::CloseSource::Window, "[sw-map-close-source] from the window");
    CHECK_MESSAGE(Is<W::WindowCloseRequested>(window[2]), "[sw-map-quit] SDL's quit becomes a WindowCloseRequested too");
    CHECK_MESSAGE(std::get<W::WindowCloseRequested>(window[2]).source == W::CloseSource::Application, "[sw-map-quit-source] from the application");
    CHECK_MESSAGE(queue.TakeDropped() == 0, "[sw-map-nothing-dropped] nothing dropped");
}

TEST_CASE("sdl_window: a focus loss is one SDL event and two Messages, all or none") {
    auto backend = MakeBackend();
    Drain(*backend);
    const SDL_WindowID id = IdOf(*backend);
    Push(WindowEvent(id, SDL_EVENT_WINDOW_FOCUS_LOST));
    S::EventQueue queue(1024, 256);
    backend->Pump(queue);
    const auto input = queue.TakeInput();
    const auto window = queue.TakeWindow();
    CHECK_MESSAGE((input.size() == 1 && Is<I::KeyboardFocusLost>(input[0])), "[sw-focus-input] input's KeyboardFocusLost");
    CHECK_MESSAGE((window.size() == 1 && Is<W::WindowFocused>(window[0])), "[sw-focus-window] and window's WindowFocused");
}

TEST_CASE("sdl_window: a full queue drops whole events and counts them (SDL-7 exception (1))") {
    auto backend = MakeBackend();
    Drain(*backend);
    const SDL_WindowID id = IdOf(*backend);
    Push(Key(id, SDL_SCANCODE_A, true));
    Push(Key(id, SDL_SCANCODE_B, true));
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = id;
    Push(motion);  // two Messages: does not fit in the one place left
    Push(Key(id, SDL_SCANCODE_C, true));
    Push(Key(id, SDL_SCANCODE_D, true));
    S::EventQueue queue(3, 256);
    backend->Pump(queue);
    const auto input = queue.TakeInput();
    CHECK_MESSAGE(input.size() == 3, "[sw-drop-kept] the events that fit are kept (a, b, c)");
    CHECK_MESSAGE(queue.TakeDropped() == 2, "[sw-drop-count] the motion and d are dropped and counted");
}

TEST_CASE("sdl_window: a flood of input does not crowd out the window's events") {
    auto backend = MakeBackend();
    Drain(*backend);
    const SDL_WindowID id = IdOf(*backend);
    Push(Key(id, SDL_SCANCODE_A, true));
    Push(Key(id, SDL_SCANCODE_B, true));
    Push(Key(id, SDL_SCANCODE_C, true));
    Push(WindowEvent(id, SDL_EVENT_WINDOW_CLOSE_REQUESTED));
    S::EventQueue queue(2, 4);
    backend->Pump(queue);
    CHECK_MESSAGE(queue.TakeWindow().size() == 1, "[sw-window-own-room] the close request has room of its own");
    CHECK_MESSAGE(queue.TakeDropped() == 1, "[sw-window-own-room-drop] only the third key is dropped");
}

TEST_CASE("sdl_window: within one pump the times strictly increase in delivered order") {
    using namespace std::chrono_literals;
    const auto base = GYO::Engine::Time::TimePoint{} + 10s;
    std::optional<GYO::Engine::Time::TimePoint> last;
    Translated left;
    left.input.emplace_back(I::CursorLeft{base + 5ms});
    Adapter::OrderTimes(left, last);
    Translated moved;
    moved.input.emplace_back(I::CursorMoved{{1.0F, 2.0F}, base + 2ms});
    moved.input.emplace_back(I::MouseMotion{{1.0F, 0.0F}, base + 2ms});
    Adapter::OrderTimes(moved, last);
    const auto movedAt = std::get<I::CursorMoved>(moved.input[0]).time;
    CHECK_MESSAGE(movedAt == base + 5ms + 1ns, "[sw-times-increase] a later delivered event stamped earlier comes 1 ns after");
    CHECK_MESSAGE(std::get<I::MouseMotion>(moved.input[1]).time == movedAt, "[sw-times-same-event] one event's Messages share its time");
}

TEST_CASE("sdl_window: logical pixels per SDL window coordinate") {
    CHECK_MESSAGE(Adapter::LogicalPerCoordinate(1.0F, 1.5F) == 1.0F / 1.5F, "[sw-coord-windows-150] Windows at 150 %: 1 / 1.5");
    CHECK_MESSAGE(Adapter::LogicalPerCoordinate(2.0F, 2.0F) == 1.0F, "[sw-coord-retina] a Retina Mac: 1 (points)");
    CHECK_MESSAGE(Adapter::LogicalPerCoordinate(0.0F, 2.0F) == 1.0F, "[sw-coord-unknown] no usable value: 1");
}

TEST_CASE("sdl_window: MouseMotion in the device's units or in logical pixels") {
    const auto device = Adapter::MotionDelta(3.0F, -2.0F, 1.0F / 1.5F, Adapter::MotionUnit::Device);
    CHECK_MESSAGE((device.x == 3.0F && device.y == -2.0F), "[sw-motion-device] as SDL gives it");
    const auto logical = Adapter::MotionDelta(3.0F, -3.0F, 1.0F / 1.5F, Adapter::MotionUnit::Logical);
    CHECK_MESSAGE((logical.x == doctest::Approx(2.0F) && logical.y == doctest::Approx(-2.0F)),
                  "[sw-motion-logical] Windows at 150 %: window coordinates are pixels, times 1 / 1.5");
}

TEST_CASE("sdl_window: the input method's positions become bytes of the composition") {
    using Adapter::EditingByteOffset;
    using Adapter::EditingUnit;
    // が is 3 bytes, a 1, U+1F600 4 (two UTF-16 units).
    const std::string_view text = "\xE3\x81\x8C" "a" "\xF0\x9F\x98\x80" "b";
    CHECK_MESSAGE(EditingByteOffset(text, 2, EditingUnit::CodePoint) == 4, "[sw-ime-codepoints]");
    CHECK(EditingByteOffset(text, 3, EditingUnit::CodePoint) == 8);
    CHECK_MESSAGE(EditingByteOffset(text, 4, EditingUnit::Utf16) == 8, "[sw-ime-utf16]");
    CHECK_MESSAGE(EditingByteOffset(text, 3, EditingUnit::Utf16) == 4, "[sw-ime-utf16-inside] inside a pair: rounds down");
    CHECK(EditingByteOffset(text, 99, EditingUnit::CodePoint) == static_cast<std::int32_t>(text.size()));
    CHECK_MESSAGE(EditingByteOffset(text, -1, EditingUnit::CodePoint) == -1, "[sw-ime-not-given]");
    CHECK_MESSAGE(Adapter::EditingUnitOf("windows") == EditingUnit::Utf16, "[sw-ime-unit-windows]");
    CHECK(Adapter::EditingUnitOf("cocoa") == EditingUnit::CodePoint);
    CHECK(Adapter::EditingUnitOf(nullptr) == EditingUnit::CodePoint);
}

TEST_CASE("sdl_window: a text editing event becomes a TextComposition in bytes") {
    auto backend = MakeBackend();
    Drain(*backend);
    SDL_Event edit{};
    edit.type = SDL_EVENT_TEXT_EDITING;
    edit.edit.windowID = IdOf(*backend);
    edit.edit.text = "\xE3\x81\x8C\xE3\x81\xA3\xE3\x81\x93";  // がっこ
    edit.edit.start = 1;
    edit.edit.length = 2;
    Push(edit);
    S::EventQueue queue(16, 16);
    backend->Pump(queue);
    const auto input = queue.TakeInput();
    REQUIRE(input.size() == 1);
    const auto& composition = std::get<I::TextComposition>(input[0]);
    // The dummy driver is not "windows": code points.
    CHECK_MESSAGE(composition.cursorByte == 3, "[sw-ime-cursor-bytes]");
    CHECK_MESSAGE(composition.selectionBytes == 6, "[sw-ime-selection-bytes]");
    // A cursor without a length (IBus, Fcitx): the length is not given. A length without its start: neither is.
    edit.edit.length = -1;
    Push(edit);
    edit.edit.start = -1;
    edit.edit.length = 2;
    Push(edit);
    backend->Pump(queue);
    const auto more = queue.TakeInput();
    REQUIRE(more.size() == 2);
    const auto& noLength = std::get<I::TextComposition>(more[0]);
    CHECK(noLength.cursorByte == 3);
    CHECK_MESSAGE(noLength.selectionBytes == -1, "[sw-ime-length-not-given]");
    const auto& noStart = std::get<I::TextComposition>(more[1]);
    CHECK_MESSAGE((noStart.cursorByte == -1 && noStart.selectionBytes == -1), "[sw-ime-start-not-given]");
}

TEST_CASE("sdl_window: an event's time is on the engine base, no later than its delivery (SDL-3)") {
    auto backend = MakeBackend();
    Drain(*backend);
    const SDL_WindowID id = IdOf(*backend);
    const auto before = GYO::Engine::Time::Now();
    Push(Key(id, SDL_SCANCODE_A, true));  // SDL stamps it with SDL_GetTicksNS at the push
    S::EventQueue queue(16, 16);
    backend->Pump(queue);
    const auto after = GYO::Engine::Time::Now();
    const auto input = queue.TakeInput();
    REQUIRE(input.size() == 1);
    const auto time = std::get<I::KeyboardInput>(input[0]).time;
    CHECK_MESSAGE(time <= after, "[sw-time-not-later] no later than its delivery");
    CHECK_MESSAGE(time >= before - std::chrono::milliseconds(5), "[sw-time-at-push] about when it was pushed");
}

namespace {

O::RegisterRow Row(std::string name, O::UnitKind kind, bool library, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = kind;
    row.libraryOnly = library;
    row.defaultOn = kind == O::UnitKind::Module;
    row.depends = std::move(depends);
    return row;
}

O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    units.rows.push_back(Row("transform", O::UnitKind::Module, false));
    units.modules.push_back({"transform", std::make_unique<GYO::Engine::Transform::TransformModule>()});
    units.rows.push_back(Row("camera", O::UnitKind::Module, false, {"transform"}));
    units.modules.push_back({"camera", std::make_unique<C::CameraModule>()});
    units.rows.push_back(Row("input", O::UnitKind::Module, false));
    units.modules.push_back({"input", std::make_unique<I::InputModule>()});
    units.rows.push_back(Row("window", O::UnitKind::Module, false));
    units.modules.push_back({"window", std::make_unique<W::WindowModule>()});
    units.rows.push_back(Row("window_camera", O::UnitKind::Module, false, {"window", "camera"}));
    units.modules.push_back({"window_camera", std::make_unique<GYO::Engine::WindowCamera::WindowCameraModule>()});
    units.rows.push_back(Row("sdl", O::UnitKind::Plugin, true));
    units.rows.push_back(Row("sdl_window", O::UnitKind::Plugin, false, {"sdl", "input", "window"}));
    units.plugins.push_back({"sdl_window", std::make_unique<S::SDLWindowPlugin>()});
    return units;
}

std::unique_ptr<O::Product> Make(bool runner = false, W::WindowSettings settings = Hidden(), std::size_t queueCapacity = 1024,
                                 S::MouseMotionUnit mouseMotion = S::MouseMotionUnit::Device) {
    static_cast<void>(dummyDriver);
    O::ProductDescription d;
    d.name = "game";
    S::SDLWindowOptions options;
    options.installRunner = runner;
    options.queueCapacity = queueCapacity;
    options.mouseMotion = mouseMotion;
    d.options.Set(options);
    d.options.Set(settings);
    O::WorldDescription w;
    w.name = "Main";
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }
S::SDLWindowPump& PumpOf(O::Product& p) {
    S::SDLWindowPump* pump = p.Buffer().Find<S::SDLWindowPump>();
    REQUIRE(pump != nullptr);
    return *pump;
}
SDL_Window* WindowOf(O::Product& p) {
    const auto* windows = p.Buffer().Find<GYO::Plugins::SDL::NativeWindows>();
    REQUIRE(windows != nullptr);
    REQUIRE(windows->main != nullptr);
    return reinterpret_cast<SDL_Window*>(windows->main);
}
void Frame(O::Product& p) {
    PumpOf(p).Pump();
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}
template <class T>
const T& Res(O::Product& p) {
    const T* value = WorldOf(p).Resource<T>();
    REQUIRE(value != nullptr);
    return *value;
}
template <class T>
T& ResMut(O::Product& p) {
    T* value = WorldOf(p).ResourceMut<T>();
    REQUIRE(value != nullptr);
    return *value;
}

} // namespace

TEST_CASE("sdl_window: a key pushed before a frame is in input's state that frame (the writer runs before the set)") {
    auto p = Make();
    PumpOf(*p).Pump();  // what SDL queued at the window's making
    Push(Key(SDL_GetWindowID(WindowOf(*p)), SDL_SCANCODE_W, true));
    Frame(*p);
    CHECK_MESSAGE(Res<I::ButtonInput<I::KeyCode>>(*p).JustPressed(I::KeyCode::W),
                  "[sw-order-same-frame] the press is in the state of the same frame (before kInputSet)");
    SDL_Event resized = WindowEvent(SDL_GetWindowID(WindowOf(*p)), SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
    resized.window.data1 = 640;
    resized.window.data2 = 480;
    Push(resized);
    Frame(*p);
    CHECK_MESSAGE((Res<W::WindowState>(*p).physicalSize == GYO::Engine::Math::Vec2i{640, 480}),
                  "[sw-order-window-same-frame] the size is in window's state the same frame (before kWindowSet)");
}

TEST_CASE("sdl_window: dropped events reach input, which releases what was held") {
    auto p = Make(false, Hidden(), 2);
    Frame(*p);
    const SDL_WindowID id = SDL_GetWindowID(WindowOf(*p));
    Push(Key(id, SDL_SCANCODE_A, true));
    Push(Key(id, SDL_SCANCODE_B, true));
    Push(Key(id, SDL_SCANCODE_C, true));  // does not fit: dropped and counted
    Frame(*p);
    CHECK_MESSAGE(PumpOf(*p).DroppedEvents() == 1, "[sw-dropped-total] the pump counts the drop");
    CHECK_MESSAGE(!Res<I::ButtonInput<I::KeyCode>>(*p).AnyPressed(),
                  "[sw-dropped-reaches-input] InputEventsDropped makes input release every key");
}

TEST_CASE("sdl_window: the close request is applied at the end of the frame") {
    auto p = Make();
    Frame(*p);
    CHECK_MESSAGE(!PumpOf(*p).Closed(), "[sw-close-not-yet] open until the game asks");
    ResMut<W::WindowRequest>(*p).close = true;
    Frame(*p);
    CHECK_MESSAGE(PumpOf(*p).Closed(), "[sw-close-applied] WindowRequest::close closes the window");
}

TEST_CASE("sdl_window: a new World's first round writes the window's present state (SDL-7 exception (2))") {
    auto p = Make();
    Frame(*p);
    int w = 0;
    int h = 0;
    REQUIRE(SDL_GetWindowSizeInPixels(WindowOf(*p), &w, &h));
    CHECK_MESSAGE(Res<W::WindowState>(*p).known, "[sw-first-known] the size is known after the first round");
    CHECK_MESSAGE((Res<W::WindowState>(*p).physicalSize == GYO::Engine::Math::Vec2i{w, h}), "[sw-first-size] the window's pixel size");
    const C::TargetSize* target = Res<C::CameraTargetSizes>(*p).Find(C::CameraTarget{0});
    CHECK_MESSAGE(target != nullptr, "[sw-first-camera] and the camera's main target has it the same frame (window_camera)");
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", O::ReplaceRequest{})));
    CHECK_MESSAGE(!Res<W::WindowState>(*p).known, "[sw-replaced-unknown] a replaced World starts without the size");
    Frame(*p);
    CHECK_MESSAGE(Res<W::WindowState>(*p).known, "[sw-replaced-known] and has it after its first round");
}

TEST_CASE("sdl_window: the requests reach SDL a frame later, only when they change") {
    auto p = Make();
    Frame(*p);
    ResMut<I::TextInputRequest>(*p).enabled = true;
    ResMut<W::WindowRequest>(*p).title = std::string("renamed");
    ResMut<W::WindowRequest>(*p).resizable = false;
    Frame(*p);
    SDL_Window* window = WindowOf(*p);
    CHECK_MESSAGE(SDL_TextInputActive(window), "[sw-request-text-input] the text input request starts SDL's text input");
    CHECK_MESSAGE(std::string(SDL_GetWindowTitle(window)) == "renamed", "[sw-request-title] the title request");
    // SDL's dummy driver has no SetWindowResizable (SDL_video.c:3126 keeps the flag): the resizable request cannot be
    // seen here; the window made not resizable is checked below.
    ResMut<I::TextInputRequest>(*p).enabled = false;
    Frame(*p);
    CHECK_MESSAGE(!SDL_TextInputActive(window), "[sw-request-text-input-off] and stops it");
    CHECK_MESSAGE(PumpOf(*p).RefusedRequests() == 0, "[sw-request-none-refused] SDL refused nothing");
}

TEST_CASE("sdl_window: a window that cannot be made is the startup error SDL-E1") {
    // Over SDL_VIDEODRIVER (ctest sets it), then back to the dummy driver.
    REQUIRE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "gyo-no-such-driver", SDL_HINT_OVERRIDE));
    O::ProductDescription d;
    d.name = "game";
    d.options.Set(Hidden());
    O::WorldDescription w;
    w.name = "Main";
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    static_cast<void>(SDL_ResetHint(SDL_HINT_VIDEO_DRIVER));
    // false when SDL_VIDEODRIVER is set (the environment wins), which is the dummy driver too.
    static_cast<void>(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"));
    REQUIRE_MESSAGE(!built, "[sw-e1-refused] no Product without the window");
    bool reported = false;
    for (const auto& error : built.error().unitErrors) reported = reported || error.code == "SDL-E1";
    CHECK_MESSAGE(reported, "[sw-e1-code] sdl_window reports SDL-E1");
}

TEST_CASE("sdl_window: MouseMotionUnit::LogicalPixels holds SDL's relative system scale while the window lives") {
    {
        auto device = Make();
        CHECK_MESSAGE(!SDL_GetHintBoolean(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, false),
                      "[sw-motion-default-raw] the default leaves SDL's relative motion raw");
    }
    {
        auto p = Make(false, Hidden(), 1024, S::MouseMotionUnit::LogicalPixels);
        CHECK_MESSAGE(SDL_GetHintBoolean(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, false),
                      "[sw-motion-hint-held] the option reaches the adapter: the system scale is on while the window lives");
        PumpOf(*p).Pump();
        SDL_Event motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID(WindowOf(*p));
        motion.motion.x = 10.0F;
        motion.motion.y = 10.0F;
        motion.motion.xrel = 6.0F;
        motion.motion.yrel = 0.0F;
        Push(motion);
        Frame(*p);
        const float k = Adapter::LogicalPerWindowCoordinate(WindowOf(*p));
        CHECK_MESSAGE(Res<I::AccumulatedMouseMotion>(*p).delta.x == doctest::Approx(6.0F * k),
                      "[sw-motion-logical-pump] the pumped motion is in logical pixels");
    }
    CHECK_MESSAGE(!SDL_GetHintBoolean(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, false),
                  "[sw-motion-hint-reset] put back when the window goes");
}

TEST_CASE("sdl_window: a window made not resizable") {
    W::WindowSettings settings = Hidden();
    settings.resizable = false;
    auto p = Make(false, settings);
    CHECK_MESSAGE((SDL_GetWindowFlags(WindowOf(*p)) & SDL_WINDOW_RESIZABLE) == 0, "[sw-settings-not-resizable] made without resizing");
}

TEST_CASE("sdl_window: the clipboard both ways and a warp") {
    auto p = Make();
    Frame(*p);
    SDL_Event update{};
    update.type = SDL_EVENT_CLIPBOARD_UPDATE;
    REQUIRE(SDL_SetClipboardText("from the OS"));
    Push(update);
    Frame(*p);
    CHECK_MESSAGE(Res<W::Clipboard>(*p).text == "from the OS", "[sw-clipboard-in] a clipboard update reaches window's mirror");
    struct Copy {
        static K::SystemResult Send(K::MessageWriter<W::CopyToClipboard> copy) {
            copy.Send(W::CopyToClipboard{"from the game"});
            return {};
        }
    };
    REQUIRE(static_cast<bool>(p->InsertSystems("Main", "test", [](K::Registrar& r) {
        K::SystemOptions o;
        o.name = "TestCopy";
        K::Mount m;
        m.schedule = std::string(O::DefaultSchedules::frame);
        m.stage = std::string(O::DefaultSchedules::update);
        o.mounts.push_back(std::move(m));
        r.AddSystem<&Copy::Send>(std::move(o));
    })));
    struct Warp {
        static K::SystemResult Send(K::MessageWriter<W::WarpCursor> warp) {
            warp.Send(W::WarpCursor{{5.0F, 6.0F}});
            return {};
        }
    };
    REQUIRE(static_cast<bool>(p->InsertSystems("Main", "test", [](K::Registrar& r) {
        K::SystemOptions o;
        o.name = "TestWarp";
        K::Mount m;
        m.schedule = std::string(O::DefaultSchedules::frame);
        m.stage = std::string(O::DefaultSchedules::update);
        o.mounts.push_back(std::move(m));
        r.AddSystem<&Warp::Send>(std::move(o));
    })));
    Frame(*p);
    char* text = SDL_GetClipboardText();
    CHECK_MESSAGE(std::string(text) == "from the game", "[sw-clipboard-out] a copy request reaches SDL's clipboard");
    SDL_free(text);
    CHECK_MESSAGE(PumpOf(*p).RefusedRequests() == 0, "[sw-warp-accepted] the warp (not observable on the dummy driver) was not refused");
}

TEST_CASE("sdl_window: the runner ends with Success when the game's close was applied") {
    auto p = Make(true);
    Push(WindowEvent(SDL_GetWindowID(WindowOf(*p)), SDL_EVENT_WINDOW_CLOSE_REQUESTED));
    const O::RunnerExit exit = p->Run();
    CHECK_MESSAGE(exit.code == O::RunnerExitCode::Success, "[sw-runner-close] the close request ends the loop: " << exit.reason);
    CHECK_MESSAGE(PumpOf(*p).Closed(), "[sw-runner-closed] the window was closed");
}

} // namespace GYO::Plugins::SDLWindow
