// The input Module (docs/architecture/target/v2/input.zh-Hant.md, IN-1 to IN-5) and the window Module
// (docs/architecture/target/v2/window.zh-Hant.md, WN-1 to WN-4), with the window_camera glue. No backend: a test
// writer System, ordered `before` the Modules' public sets as every writer is, sends the Messages a backend would.
// Each CHECK carries a tag in brackets ("[in-...]", "[wn-...]", "[wc-...]"): the mutants expect it
// (docs/architecture/target/v2/scripts/window_input_mutations.json).
#include <doctest/doctest.h>

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
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

namespace C = GYO::Engine::Camera;
namespace I = GYO::Engine::Input;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::Window;
namespace WC = GYO::Engine::WindowCamera;
using GYO::Engine::Math::Vec2;
using TimePoint = GYO::Engine::Time::TimePoint;
using namespace std::chrono_literals;

namespace {

// What the test writer sends in the next frame, by type.
struct Pending {
    std::vector<I::KeyboardInput> keys;
    std::vector<I::KeyboardFocusLost> focusLost;
    std::vector<I::InputEventsDropped> dropped;
    std::vector<I::MouseButtonInput> buttons;
    std::vector<I::CursorMoved> moved;
    std::vector<I::CursorLeft> left;
    std::vector<I::MouseMotion> motion;
    std::vector<I::MouseWheel> wheel;
    std::vector<I::TextComposition> composition;
    std::vector<I::TextCommitted> committed;
    std::vector<W::WindowResized> resized;
    std::vector<W::WindowFocused> focused;
    std::vector<W::WindowCloseRequested> closeRequested;
    std::vector<W::ClipboardChanged> clipboard;
};
Pending pending;

using SystemResult = K::SystemResult;

template <class T>
void SendAll(std::vector<T>& list, K::MessageWriter<T>& writer) {
    for (T& m : list) writer.Send(std::move(m));
    list.clear();
}

SystemResult WriteInput(K::MessageWriter<I::KeyboardInput> keys, K::MessageWriter<I::KeyboardFocusLost> focusLost,
                        K::MessageWriter<I::InputEventsDropped> dropped, K::MessageWriter<I::MouseButtonInput> buttons,
                        K::MessageWriter<I::CursorMoved> moved, K::MessageWriter<I::CursorLeft> left,
                        K::MessageWriter<I::MouseMotion> motion, K::MessageWriter<I::MouseWheel> wheel,
                        K::MessageWriter<I::TextComposition> composition, K::MessageWriter<I::TextCommitted> committed) {
    SendAll(pending.keys, keys);
    SendAll(pending.focusLost, focusLost);
    SendAll(pending.dropped, dropped);
    SendAll(pending.buttons, buttons);
    SendAll(pending.moved, moved);
    SendAll(pending.left, left);
    SendAll(pending.motion, motion);
    SendAll(pending.wheel, wheel);
    SendAll(pending.composition, composition);
    SendAll(pending.committed, committed);
    return {};
}

SystemResult WriteWindow(K::MessageWriter<W::WindowResized> resized, K::MessageWriter<W::WindowFocused> focused,
                         K::MessageWriter<W::WindowCloseRequested> closeRequested, K::MessageWriter<W::ClipboardChanged> clipboard) {
    SendAll(pending.resized, resized);
    SendAll(pending.focused, focused);
    SendAll(pending.closeRequested, closeRequested);
    SendAll(pending.clipboard, clipboard);
    return {};
}

O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("transform"));
    units.modules.push_back({"transform", std::make_unique<GYO::Engine::Transform::TransformModule>()});
    units.rows.push_back(ModuleRow("camera", {"transform"}));
    units.modules.push_back({"camera", std::make_unique<C::CameraModule>()});
    units.rows.push_back(ModuleRow("input"));
    units.modules.push_back({"input", std::make_unique<I::InputModule>()});
    units.rows.push_back(ModuleRow("window"));
    units.modules.push_back({"window", std::make_unique<W::WindowModule>()});
    units.rows.push_back(ModuleRow("window_camera", {"window", "camera"}));
    units.modules.push_back({"window_camera", std::make_unique<WC::WindowCameraModule>()});
    return units;
}

K::SystemOptions Before(std::string name, std::string set) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::string(O::DefaultSchedules::frame);
    m.stage = std::string(O::DefaultSchedules::preUpdate);
    m.before.push_back(std::move(set));
    o.mounts.push_back(std::move(m));
    return o;
}

std::unique_ptr<O::Product> Make(O::OptionBag productOptions = {}) {
    pending = Pending{};
    O::ProductDescription d;
    d.name = "game";
    d.options = std::move(productOptions);
    O::WorldDescription w;
    w.name = "Main";
    w.build = [](O::WorldSetup& setup) {
        setup.Registrar().AddSystem<&WriteInput>(Before("TestWriteInput", std::string(I::kInputSet)));
        setup.Registrar().AddSystem<&WriteWindow>(Before("TestWriteWindow", std::string(W::kWindowSet)));
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }
void Frame(O::Product& p) {
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}
template <class T>
const T& Res(O::Product& p) {
    const T* value = WorldOf(p).Resource<T>();
    REQUIRE(value != nullptr);
    return *value;
}

I::KeyboardInput Key(I::KeyCode code, I::ButtonState state, bool repeat = false) {
    I::KeyboardInput k;
    k.keyCode = code;
    k.state = state;
    k.repeat = repeat;
    return k;
}
constexpr I::ButtonState kDown = I::ButtonState::Pressed;
constexpr I::ButtonState kUp = I::ButtonState::Released;
TimePoint At(int ms) { return TimePoint{} + std::chrono::milliseconds(ms); }

} // namespace

TEST_CASE("input: a key press is seen the same frame, just pressed once, held until released") {
    auto p = Make();
    pending.keys.push_back(Key(I::KeyCode::W, kDown));
    Frame(*p);
    const auto& keys = Res<I::ButtonInput<I::KeyCode>>(*p);
    CHECK_MESSAGE(keys.Pressed(I::KeyCode::W), "[in-press-same-frame] the press is in the state of the frame it was written");
    CHECK_MESSAGE(keys.JustPressed(I::KeyCode::W), "[in-just-pressed] just pressed in that frame");
    Frame(*p);
    CHECK_MESSAGE(Res<I::ButtonInput<I::KeyCode>>(*p).Pressed(I::KeyCode::W), "[in-held] still held a frame later");
    CHECK_MESSAGE(!Res<I::ButtonInput<I::KeyCode>>(*p).JustPressed(I::KeyCode::W), "[in-just-cleared] no longer just pressed");
    pending.keys.push_back(Key(I::KeyCode::W, kUp));
    Frame(*p);
    CHECK_MESSAGE(!Res<I::ButtonInput<I::KeyCode>>(*p).Pressed(I::KeyCode::W), "[in-released] released");
    CHECK_MESSAGE(Res<I::ButtonInput<I::KeyCode>>(*p).JustReleased(I::KeyCode::W), "[in-just-released] just released");
}

TEST_CASE("input: a repeat and a release of a key not held change nothing (IN-3)") {
    auto p = Make();
    pending.keys.push_back(Key(I::KeyCode::Enter, kDown));
    Frame(*p);
    pending.keys.push_back(Key(I::KeyCode::Enter, kDown, true));
    Frame(*p);
    CHECK_MESSAGE(!Res<I::ButtonInput<I::KeyCode>>(*p).JustPressed(I::KeyCode::Enter), "[in-repeat-not-press] a repeat is no new press");
    pending.keys.push_back(Key(I::KeyCode::Escape, kUp));
    Frame(*p);
    CHECK_MESSAGE(!Res<I::ButtonInput<I::KeyCode>>(*p).JustReleased(I::KeyCode::Escape),
                  "[in-unheld-release] the release of a key not held is no release");
}

TEST_CASE("input: a focus loss releases every key, not the mouse buttons") {
    auto p = Make();
    pending.keys.push_back(Key(I::KeyCode::A, kDown));
    pending.keys.push_back(Key(I::KeyCode::ShiftLeft, kDown));
    pending.buttons.push_back(I::MouseButtonInput{I::MouseButton::Left, kDown, {}, {}});
    Frame(*p);
    pending.focusLost.push_back(I::KeyboardFocusLost{});
    Frame(*p);
    const auto& keys = Res<I::ButtonInput<I::KeyCode>>(*p);
    CHECK_MESSAGE(!keys.AnyPressed(), "[in-focus-releases-keys] no key is held after a focus loss");
    CHECK_MESSAGE(keys.JustReleased(I::KeyCode::A), "[in-focus-just-released] the keys are just released");
    CHECK_MESSAGE(Res<I::ButtonInput<I::MouseButton>>(*p).Pressed(I::MouseButton::Left),
                  "[in-focus-keeps-buttons] a mouse button stays held (as in Bevy)");
}

TEST_CASE("input: dropped events release every key and every button (IN-3)") {
    auto p = Make();
    pending.keys.push_back(Key(I::KeyCode::D, kDown));
    pending.buttons.push_back(I::MouseButtonInput{I::MouseButton::Right, kDown, {}, {}});
    Frame(*p);
    pending.dropped.push_back(I::InputEventsDropped{3, {}});
    Frame(*p);
    CHECK_MESSAGE(!Res<I::ButtonInput<I::KeyCode>>(*p).AnyPressed(), "[in-dropped-keys] no key is held after a drop");
    CHECK_MESSAGE(!Res<I::ButtonInput<I::MouseButton>>(*p).AnyPressed(), "[in-dropped-buttons] no button is held after a drop");
}

TEST_CASE("input: the mouse sums are this frame's only") {
    auto p = Make();
    pending.motion.push_back(I::MouseMotion{{3.0F, -1.0F}, {}});
    pending.motion.push_back(I::MouseMotion{{2.0F, 4.0F}, {}});
    pending.wheel.push_back(I::MouseWheel{{0.0F, 1.0F}, {}});
    Frame(*p);
    CHECK_MESSAGE(Res<I::AccumulatedMouseMotion>(*p).delta.x == 5.0F, "[in-motion-sum] the motions of the frame are summed");
    CHECK_MESSAGE(Res<I::AccumulatedMouseMotion>(*p).delta.y == 3.0F, "[in-motion-sum-y] both axes");
    CHECK_MESSAGE(Res<I::AccumulatedMouseScroll>(*p).delta.y == 1.0F, "[in-scroll-sum] the wheel of the frame");
    Frame(*p);
    CHECK_MESSAGE(Res<I::AccumulatedMouseMotion>(*p).delta.x == 0.0F, "[in-motion-reset] zero in a frame without motion");
    CHECK_MESSAGE(Res<I::AccumulatedMouseScroll>(*p).delta.y == 0.0F, "[in-scroll-reset] zero in a frame without wheel");
}

TEST_CASE("input: the cursor's place, and nothing once it left (told apart by time)") {
    auto p = Make();
    pending.moved.push_back(I::CursorMoved{{10.0F, 20.0F}, At(1)});
    Frame(*p);
    REQUIRE(Res<I::CursorPosition>(*p).position.has_value());
    CHECK_MESSAGE(Res<I::CursorPosition>(*p).position->x == 10.0F, "[in-cursor-moved] the last place");
    pending.moved.push_back(I::CursorMoved{{11.0F, 20.0F}, At(2)});
    pending.left.push_back(I::CursorLeft{At(3)});
    Frame(*p);
    CHECK_MESSAGE(!Res<I::CursorPosition>(*p).position.has_value(), "[in-cursor-left] a later leave clears the place");
    pending.left.push_back(I::CursorLeft{At(4)});
    pending.moved.push_back(I::CursorMoved{{12.0F, 21.0F}, At(5)});
    Frame(*p);
    CHECK_MESSAGE(Res<I::CursorPosition>(*p).position.has_value(), "[in-cursor-back] a later move gives a place again");
}

TEST_CASE("input: the composition in progress, cleared by a later commit") {
    auto p = Make();
    pending.composition.push_back(I::TextComposition{"にほ", 2, 0, At(1)});
    Frame(*p);
    CHECK_MESSAGE(Res<I::TextInput>(*p).composition == "にほ", "[in-composition] the composition in progress");
    pending.committed.push_back(I::TextCommitted{"日本", At(2)});
    Frame(*p);
    CHECK_MESSAGE(Res<I::TextInput>(*p).composition.empty(), "[in-commit-clears] a commit ends the composition");
}

TEST_CASE("input: a writer without `before` the set is a frame late (why every writer orders itself)") {
    pending = Pending{};
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.build = [](O::WorldSetup& setup) {
        K::SystemOptions o;
        o.name = "UnorderedWriter";
        K::Mount m;
        m.schedule = std::string(O::DefaultSchedules::frame);
        m.stage = std::string(O::DefaultSchedules::preUpdate);
        o.mounts.push_back(std::move(m));
        setup.Registrar().AddSystem<&WriteInput>(std::move(o));
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE(static_cast<bool>(built));
    O::Product& p = **built;
    pending.keys.push_back(Key(I::KeyCode::Space, kDown));
    Frame(p);
    CHECK_MESSAGE(!Res<I::ButtonInput<I::KeyCode>>(p).Pressed(I::KeyCode::Space),
                  "[in-unordered-late] registered after the Module, unordered: input ran first");
    Frame(p);
    CHECK_MESSAGE(Res<I::ButtonInput<I::KeyCode>>(p).Pressed(I::KeyCode::Space), "[in-unordered-next] seen a frame later");
}

TEST_CASE("window: the state from the Messages, and a close request sets close") {
    auto p = Make();
    CHECK_MESSAGE(!Res<W::WindowState>(*p).known, "[wn-unknown-first] a new World does not know the size");
    pending.resized.push_back(W::WindowResized{{2560, 1440}, 2.0F, {}});
    pending.focused.push_back(W::WindowFocused{true, {}});
    pending.clipboard.push_back(W::ClipboardChanged{"copied", false, {}});
    Frame(*p);
    const auto& state = Res<W::WindowState>(*p);
    CHECK_MESSAGE(state.known, "[wn-known] the size is known after a WindowResized");
    CHECK_MESSAGE((state.physicalSize == GYO::Engine::Math::Vec2i{2560, 1440}), "[wn-size] the physical size");
    CHECK_MESSAGE(state.LogicalSize().x == 1280.0F, "[wn-logical] the logical size is physical / scale");
    CHECK_MESSAGE(state.focused, "[wn-focus] the focus");
    CHECK_MESSAGE(Res<W::Clipboard>(*p).text == "copied", "[wn-clipboard] the clipboard mirror");
    CHECK_MESSAGE(!Res<W::WindowRequest>(*p).close, "[wn-no-close] nothing asked to close yet");
    pending.closeRequested.push_back(W::WindowCloseRequested{W::CloseSource::Window, {}});
    Frame(*p);
    CHECK_MESSAGE(Res<W::WindowRequest>(*p).close, "[wn-close-when-requested] a close request sets close (closeWhenRequested on)");
}

TEST_CASE("window: closeWhenRequested off leaves the close to the game") {
    O::OptionBag options;
    W::WindowSettings settings;
    settings.closeWhenRequested = false;
    options.Set(settings);
    auto p = Make(std::move(options));
    pending.closeRequested.push_back(W::WindowCloseRequested{W::CloseSource::Shutdown, {}});
    Frame(*p);
    CHECK_MESSAGE(!Res<W::WindowRequest>(*p).close, "[wn-close-left-to-game] no close without closeWhenRequested");
}

TEST_CASE("window_camera: the window's size becomes the main target's, written only when it differs") {
    auto p = Make();
    Frame(*p);
    CHECK_MESSAGE(Res<C::CameraTargetSizes>(*p).Find(C::CameraTarget{0}) == nullptr, "[wc-none-unknown] no size before it is known");
    pending.resized.push_back(W::WindowResized{{1920, 1080}, 1.5F, {}});
    Frame(*p);
    const C::TargetSize* size = Res<C::CameraTargetSizes>(*p).Find(C::CameraTarget{0});
    REQUIRE(size != nullptr);
    CHECK_MESSAGE((size->physicalSize == GYO::Engine::Math::Vec2i{1920, 1080}), "[wc-size] the main target is the window's size");
    CHECK_MESSAGE(size->scaleFactor == 1.5F, "[wc-scale] with its scale");
}
