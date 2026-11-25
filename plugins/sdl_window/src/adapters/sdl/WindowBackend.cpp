// sdl_window's adapter (Design Invariant SDL-1: the only place of sdl_window, with its private Translate.hpp, that
// includes SDL3/). It implements WindowBackend (../../WindowBackend.hpp) and the mapping table of SDL-7.
#include "../../WindowBackend.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <string_view>
#include <utility>
#include <utility>

#include "engine/base/Assert.hpp"

#include "Translate.hpp"

namespace GYO::Plugins::SDLWindow {

namespace In = Engine::Input;
namespace Win = Engine::Window;

namespace Adapter {

namespace {

bool InKnownRange(const int code) noexcept {
    return (code >= 4 && code <= 101) || (code >= 104 && code <= 115) || (code >= 224 && code <= 231);
}

// A UTF-8 text cut at limit bytes, back to the start of a character.
std::string CutUtf8(const char* text, const std::size_t limit, bool& truncated) {
    std::string value = text != nullptr ? text : "";
    truncated = value.size() > limit;
    if (!truncated) return value;
    std::size_t end = limit;
    while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xC0U) == 0x80U) --end;
    value.resize(end);
    return value;
}

std::string ClipboardText(const std::size_t limit, bool& truncated) {
    char* text = SDL_GetClipboardText();
    std::string value = CutUtf8(text, limit, truncated);
    SDL_free(text);
    return value;
}

Win::WindowResized ResizedNow(SDL_Window* window, const Engine::Time::TimePoint time) {
    int w = 0;
    int h = 0;
    static_cast<void>(SDL_GetWindowSizeInPixels(window, &w, &h));
    float scale = SDL_GetWindowDisplayScale(window);
    if (!(scale > 0.0F)) scale = 1.0F;
    return Win::WindowResized{{w, h}, scale, time};
}

bool OwnWindow(SDL_Window* window, const SDL_WindowID id) noexcept { return id == SDL_GetWindowID(window); }

bool IsFullscreen(SDL_Window* window) noexcept { return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0; }

} // namespace

In::KeyCode KeyCodeOf(const SDL_Scancode scancode) noexcept {
    const int code = static_cast<int>(scancode);
    return InKnownRange(code) ? static_cast<In::KeyCode>(code) : In::KeyCode::Unidentified;
}

In::KeyModifiers ModifiersOf(const SDL_Keymod mod) noexcept {
    In::KeyModifiers out;
    if ((mod & SDL_KMOD_SHIFT) != 0) out.Add(In::KeyModifier::Shift);
    if ((mod & SDL_KMOD_CTRL) != 0) out.Add(In::KeyModifier::Control);
    if ((mod & SDL_KMOD_ALT) != 0) out.Add(In::KeyModifier::Alt);
    if ((mod & SDL_KMOD_GUI) != 0) out.Add(In::KeyModifier::Super);
    if ((mod & SDL_KMOD_CAPS) != 0) out.Add(In::KeyModifier::CapsLock);
    if ((mod & SDL_KMOD_NUM) != 0) out.Add(In::KeyModifier::NumLock);
    return out;
}

char32_t CharacterOf(const SDL_Scancode scancode) noexcept {
    const SDL_Keycode key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, true);
    if ((key & SDLK_SCANCODE_MASK) != 0 || key < 0x20U || key == 0x7FU) return 0;
    return static_cast<char32_t>(key);
}

float LogicalPerWindowCoordinate(SDL_Window* window) noexcept {
    return LogicalPerCoordinate(SDL_GetWindowPixelDensity(window), SDL_GetWindowDisplayScale(window));
}

namespace {

// A size in logical pixels to SDL's window coordinates (pixels on Windows).
bool SetWindowSizeLogical(SDL_Window* window, const Engine::Math::Vec2i logicalSize) {
    const float toWindow = 1.0F / LogicalPerWindowCoordinate(window);
    return SDL_SetWindowSize(window, static_cast<int>(std::lround(static_cast<float>(logicalSize.x) * toWindow)),
                             static_cast<int>(std::lround(static_cast<float>(logicalSize.y) * toWindow)));
}

} // namespace

void OrderTimes(Translated& translated, std::optional<Engine::Time::TimePoint>& last) {
    if (translated.Size() == 0) return;
    Engine::Time::TimePoint time{};
    if (!translated.input.empty()) {
        time = std::visit([](const auto& m) { return m.time; }, translated.input.front());
    } else {
        time = std::visit([](const auto& m) { return m.time; }, translated.window.front());
    }
    if (last && time <= *last) time = *last + std::chrono::nanoseconds(1);
    last = time;
    for (InputItem& item : translated.input) std::visit([time](auto& m) { m.time = time; }, item);
    for (WindowItem& item : translated.window) std::visit([time](auto& m) { m.time = time; }, item);
}

EditingUnit EditingUnitOf(const char* videoDriver) noexcept {
    return videoDriver != nullptr && std::string_view(videoDriver) == "windows" ? EditingUnit::Utf16 : EditingUnit::CodePoint;
}

std::int32_t EditingByteOffset(const std::string_view utf8, const std::int32_t units, const EditingUnit unit) noexcept {
    if (units < 0) return -1;
    std::int32_t left = units;
    std::size_t offset = 0;
    while (left > 0 && offset < utf8.size()) {
        std::size_t next = offset + 1;
        while (next < utf8.size() && (static_cast<unsigned char>(utf8[next]) & 0xC0U) == 0x80U) ++next;
        // Four bytes of UTF-8 are a code point outside the Basic Multilingual Plane: two UTF-16 units.
        const std::int32_t width = unit == EditingUnit::Utf16 && next - offset == 4 ? 2 : 1;
        if (width > left) break;
        left -= width;
        offset = next;
    }
    return static_cast<std::int32_t>(offset);
}

Translated Translate(const SDL_Event& event, const SDL::ClockPair& pair, const Engine::Time::TimePoint ceiling,
                     SDL_Window* window, const std::size_t clipboardLimit, const EditingUnit editingUnit,
                     const MotionUnit motionUnit) {
    Translated out;
    const Engine::Time::TimePoint time = SDL::ConvertSDLTime(event.common.timestamp, pair, ceiling);
    const auto logical = [window](const float x, const float y) {
        const float k = LogicalPerWindowCoordinate(window);
        return Engine::Math::Vec2{x * k, y * k};
    };
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        if (!OwnWindow(window, event.key.windowID)) break;
        In::KeyboardInput m;
        m.keyCode = KeyCodeOf(event.key.scancode);
        m.state = event.key.down ? In::ButtonState::Pressed : In::ButtonState::Released;
        m.repeat = event.key.repeat;
        m.character = CharacterOf(event.key.scancode);
        m.modifiers = ModifiersOf(event.key.mod);
        m.time = time;
        out.input.emplace_back(m);
        break;
    }
    case SDL_EVENT_TEXT_EDITING: {
        if (!OwnWindow(window, event.edit.windowID)) break;
        // input's unit is bytes of the text (TextComposition): SDL's units are converted here, at the edge.
        In::TextComposition m;
        m.text = event.edit.text != nullptr ? event.edit.text : "";
        m.cursorByte = EditingByteOffset(m.text, event.edit.start, editingUnit);
        // Each of start and length may be -1 (not given; IBus and Fcitx send a cursor without a length): a length
        // without its start is not given either. The sum is taken in 64 bits (the values come from the OS) and clamped.
        if (event.edit.start >= 0 && event.edit.length >= 0) {
            const std::int64_t end = std::min<std::int64_t>(std::int64_t{event.edit.start} + event.edit.length, INT32_MAX);
            m.selectionBytes = EditingByteOffset(m.text, static_cast<std::int32_t>(end), editingUnit) - m.cursorByte;
        }
        m.time = time;
        out.input.emplace_back(std::move(m));
        break;
    }
    case SDL_EVENT_TEXT_INPUT:
        if (!OwnWindow(window, event.text.windowID)) break;
        out.input.emplace_back(In::TextCommitted{event.text.text != nullptr ? event.text.text : "", time});
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        if (!OwnWindow(window, event.motion.windowID)) break;
        const float k = LogicalPerWindowCoordinate(window);
        out.input.emplace_back(In::CursorMoved{{event.motion.x * k, event.motion.y * k}, time});
        out.input.emplace_back(In::MouseMotion{MotionDelta(event.motion.xrel, event.motion.yrel, k, motionUnit), time});
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!OwnWindow(window, event.button.windowID)) break;
        In::MouseButton button{};
        switch (event.button.button) {
        case SDL_BUTTON_LEFT: button = In::MouseButton::Left; break;
        case SDL_BUTTON_MIDDLE: button = In::MouseButton::Middle; break;
        case SDL_BUTTON_RIGHT: button = In::MouseButton::Right; break;
        case SDL_BUTTON_X1: button = In::MouseButton::Back; break;
        case SDL_BUTTON_X2: button = In::MouseButton::Forward; break;
        default: return out;  // a button input does not name: nothing
        }
        out.input.emplace_back(In::MouseButtonInput{button, event.button.down ? In::ButtonState::Pressed : In::ButtonState::Released,
                                                    logical(event.button.x, event.button.y), time});
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        if (!OwnWindow(window, event.wheel.windowID)) break;
        // As the OS gives it, natural scrolling included (as Bevy; SDL marks it FLIPPED and leaves the sign alone).
        out.input.emplace_back(In::MouseWheel{{event.wheel.x, event.wheel.y}, time});
        break;
    }
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.input.emplace_back(In::CursorEntered{time});
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.input.emplace_back(In::CursorLeft{time});
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.window.emplace_back(Win::WindowFocused{true, time});
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.window.emplace_back(Win::WindowFocused{false, time});
        out.input.emplace_back(In::KeyboardFocusLost{time});
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
        if (!OwnWindow(window, event.window.windowID)) break;
        Win::WindowResized m = ResizedNow(window, time);
        m.physicalSize = {event.window.data1, event.window.data2};
        out.window.emplace_back(m);
        break;
    }
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.window.emplace_back(ResizedNow(window, time));
        break;
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.window.emplace_back(Win::WindowModeChanged{Win::WindowMode::BorderlessFullscreen, time});
        break;
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.window.emplace_back(Win::WindowModeChanged{Win::WindowMode::Windowed, time});
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (!OwnWindow(window, event.window.windowID)) break;
        out.window.emplace_back(Win::WindowCloseRequested{Win::CloseSource::Window, time});
        break;
    case SDL_EVENT_QUIT:
        // The OS asked the program to quit (macOS Cmd+Q, logging off). SDL installs no signal handlers here
        // (SDL_HINT_NO_SIGNAL_HANDLERS) and does not quit on the last window's close
        // (SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE), so nothing else makes it.
        out.window.emplace_back(Win::WindowCloseRequested{Win::CloseSource::Application, time});
        break;
    case SDL_EVENT_CLIPBOARD_UPDATE: {
        bool truncated = false;
        std::string text = ClipboardText(clipboardLimit, truncated);
        out.window.emplace_back(Win::ClipboardChanged{std::move(text), truncated, time});
        break;
    }
    default:
        break;  // every other event becomes nothing
    }
    return out;
}

} // namespace Adapter

namespace {

void RequireMainThread() { GYO_ASSERT(SDL_IsMainThread() && "sdl_window's adapter is called on the main thread only (SDL-4)"); }

Engine::Time::TimePoint EngineNow() noexcept { return Engine::Time::Now(); }
std::uint64_t SdlNow() noexcept { return SDL_GetTicksNS(); }

SDL_SystemCursor SystemCursorOf(const Win::CursorIcon icon) noexcept {
    switch (icon) {
    case Win::CursorIcon::Default: return SDL_SYSTEM_CURSOR_DEFAULT;
    case Win::CursorIcon::Text: return SDL_SYSTEM_CURSOR_TEXT;
    case Win::CursorIcon::Pointer: return SDL_SYSTEM_CURSOR_POINTER;
    case Win::CursorIcon::Crosshair: return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case Win::CursorIcon::Move: return SDL_SYSTEM_CURSOR_MOVE;
    case Win::CursorIcon::ResizeEw: return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case Win::CursorIcon::ResizeNs: return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case Win::CursorIcon::ResizeNwse: return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case Win::CursorIcon::ResizeNesw: return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case Win::CursorIcon::NotAllowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    case Win::CursorIcon::Wait: return SDL_SYSTEM_CURSOR_WAIT;
    case Win::CursorIcon::Progress: return SDL_SYSTEM_CURSOR_PROGRESS;
    }
    return SDL_SYSTEM_CURSOR_DEFAULT;
}

} // namespace

struct WindowBackend::State {
    SDL_Window* window = nullptr;
    std::size_t clipboardLimit = 0;
    // The unit of the input method's positions, of the video driver SDL runs (it does not change while the window is).
    Adapter::EditingUnit editingUnit = Adapter::EditingUnit::CodePoint;
    // MouseMotion's unit; Logical holds SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE set while the window lives.
    Adapter::MotionUnit motionUnit = Adapter::MotionUnit::Device;
    std::array<SDL_Cursor*, 12> cursors{};
    // The live frame (SetLiveFrame): the function, whether the watch is added, and the pump in progress.
    std::function<void()> liveFrame;
    bool watchAdded = false;
    EventQueue* pumpQueue = nullptr;
    bool inLiveFrame = false;
    // An exception that left the live frame: kept here (it must not cross SDL's C code, its watcher lock and the
    // OS's modal loop: master's pit 4) and thrown again once the pump or the wait returned.
    std::exception_ptr liveException;
    // The request side's own cache (as sdl_window's AppliedRequests, not event state: SDL-7): a fullscreen asked for
    // that SDL's flags do not show yet (macOS enters asynchronously), and a Windowed size (logical pixels) held while
    // the window is or is about to be fullscreen (SetSize). Whether the window is fullscreen is asked of SDL, never
    // remembered: the player leaves fullscreen too.
    bool fullscreenPending = false;
    std::optional<Engine::Math::Vec2i> heldWindowedSize;
};

namespace {

// SDL calls it for every event pushed, on the pushing thread, inside its watcher lock (master's pit 6): it does
// nothing unless SetLiveFrame's conditions all hold. It never drops the event (true).
bool SDLCALL LiveFrameWatch(void* userdata, SDL_Event* event) {
    auto* state = static_cast<WindowBackend::State*>(userdata);
    if (event->type != SDL_EVENT_WINDOW_EXPOSED || event->window.data1 != 1) return true;
    if (!SDL_IsMainThread() || state->pumpQueue == nullptr || state->inLiveFrame || !state->liveFrame) return true;
    if (state->window == nullptr || event->window.windowID != SDL_GetWindowID(state->window)) return true;
    if (state->liveException) return true;  // a frame already failed with an exception in this pump
    Translated now;
    now.window.emplace_back(Adapter::ResizedNow(state->window, Engine::Time::Now()));
    static_cast<void>(state->pumpQueue->Accept(std::move(now)));
    state->inLiveFrame = true;
    try {
        state->liveFrame();
    } catch (...) {
        state->liveException = std::current_exception();
    }
    state->inLiveFrame = false;
    return true;
}

} // namespace

Engine::Base::Result<std::unique_ptr<WindowBackend>, BackendFailure> WindowBackend::Create(const BackendSettings& settings) {
    RequireMainThread();
    // The hints are process-wide (sdl.zh-Hant.md, SDL's global state): each is set for as long as it is read and put
    // back afterwards. os owns the process's signals (InstallShutdownRequests): SDL reads NO_SIGNAL_HANDLERS when its
    // events start (VIDEO implies EVENTS). The IME hint is read once when the video starts (master pit 15). A closed
    // window is the game's close request, not SDL's quit: QUIT_ON_LAST_WINDOW_CLOSE is read while the window lives.
    static_cast<void>(SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"));
    if (settings.drawComposition) static_cast<void>(SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition"));
    const bool started = SDL_InitSubSystem(SDL_INIT_VIDEO);
    std::string startError = started ? std::string() : std::string("SDL_InitSubSystem(VIDEO): ") + SDL_GetError();
    static_cast<void>(SDL_ResetHint(SDL_HINT_NO_SIGNAL_HANDLERS));
    if (settings.drawComposition) static_cast<void>(SDL_ResetHint(SDL_HINT_IME_IMPLEMENTED_UI));
    if (!started) return Engine::Base::Err(BackendFailure::Make(BackendFailureCode::VideoFailed, std::move(startError)));
    static_cast<void>(SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0"));
    // The OS's pointer speed in relative mode: SDL reads it through a hint callback whenever it changes, so it is
    // held while the window lives and put back with it.
    if (settings.logicalMotion) static_cast<void>(SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "1"));
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (settings.window.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (settings.window.hidden) flags |= SDL_WINDOW_HIDDEN;
    // No fullscreen mode is set: SDL's fullscreen is the desktop's (borderless).
    const bool fullscreen = settings.window.mode == Win::WindowMode::BorderlessFullscreen;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window* window = SDL_CreateWindow(settings.window.title.c_str(), settings.window.size.x, settings.window.size.y, flags);
    if (window == nullptr) {
        BackendFailure failure = BackendFailure::Make(BackendFailureCode::WindowFailed, std::string("SDL_CreateWindow: ") + SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        static_cast<void>(SDL_ResetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE));
        if (settings.logicalMotion) static_cast<void>(SDL_ResetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE));
        return Engine::Base::Err(std::move(failure));
    }
    auto state = std::make_unique<State>();
    state->fullscreenPending = fullscreen;
    // SDL makes the window in its window coordinates, which are pixels on Windows: a size asked in logical pixels
    // is set again at the display's scale (made fullscreen: held for when it becomes Windowed).
    if (Adapter::LogicalPerWindowCoordinate(window) != 1.0F) {
        if (fullscreen) {
            state->heldWindowedSize = settings.window.size;
        } else {
            static_cast<void>(Adapter::SetWindowSizeLogical(window, settings.window.size));
        }
    }
    state->window = window;
    state->clipboardLimit = settings.clipboardLimit;
    state->editingUnit = Adapter::EditingUnitOf(SDL_GetCurrentVideoDriver());
    state->motionUnit = settings.logicalMotion ? Adapter::MotionUnit::Logical : Adapter::MotionUnit::Device;
    return std::unique_ptr<WindowBackend>(new WindowBackend(std::move(state)));
}

WindowBackend::WindowBackend(std::unique_ptr<State> state) : state_(std::move(state)) {}

WindowBackend::~WindowBackend() {
    RequireMainThread();
    if (state_->watchAdded) SDL_RemoveEventWatch(&LiveFrameWatch, state_.get());
    for (SDL_Cursor*& cursor : state_->cursors) {
        if (cursor != nullptr) SDL_DestroyCursor(cursor);
        cursor = nullptr;
    }
    if (state_->window != nullptr) SDL_DestroyWindow(state_->window);
    // Only its own subsystem, never SDL_Quit (another sdl* Plugin may still use SDL; sdl.zh-Hant.md).
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    static_cast<void>(SDL_ResetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE));
    if (state_->motionUnit == Adapter::MotionUnit::Logical) {
        static_cast<void>(SDL_ResetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE));
    }
}

void WindowBackend::Pump(EventQueue& queue) {
    RequireMainThread();
    // One clock pair per pump (SDL declaration section 8). The pump takes SDL's whole pass: leaving SDL_PollEvent
    // early would leave its end-of-pass marker queued and cut the next pump short (master's pit 4).
    const SDL::ClockPair pair = SDL::SampleClockPair({&EngineNow, &SdlNow});
    std::optional<Engine::Time::TimePoint> last;
    SDL_Event event{};
    state_->pumpQueue = &queue;  // the live frame's condition "while Pump is running"
    while (SDL_PollEvent(&event)) {
        // A fullscreen asked for and refused or left before the flags showed it: nothing is pending any more.
        if (event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN && Adapter::OwnWindow(state_->window, event.window.windowID)) {
            state_->fullscreenPending = false;
        }
        Translated translated = Adapter::Translate(event, pair, Engine::Time::Now(), state_->window, state_->clipboardLimit,
                                                   state_->editingUnit, state_->motionUnit);
        Adapter::OrderTimes(translated, last);
        static_cast<void>(queue.Accept(std::move(translated)));
    }
    state_->pumpQueue = nullptr;
    RethrowLiveException();
}

void WindowBackend::RethrowLiveException() {
    if (!state_->liveException) return;
    std::exception_ptr exception = std::exchange(state_->liveException, nullptr);
    std::rethrow_exception(exception);
}

void WindowBackend::SetLiveFrame(std::function<void()> frame) {
    RequireMainThread();
    state_->liveFrame = std::move(frame);
    if (state_->liveFrame && !state_->watchAdded) {
        state_->watchAdded = SDL_AddEventWatch(&LiveFrameWatch, state_.get());
        if (!state_->watchAdded) lastError_ = std::string("SDL_AddEventWatch: ") + SDL_GetError();
    }
}

bool WindowBackend::LiveFrameReady() const noexcept { return !state_->liveFrame || state_->watchAdded; }

bool WindowBackend::InLiveFrame() const noexcept { return state_->inLiveFrame; }

void WindowBackend::WaitForEvent(const std::int64_t ns, EventQueue& queue) {
    RequireMainThread();
    const std::int64_t ms = std::clamp<std::int64_t>((ns + 999'999) / 1'000'000, 0, 1000);
    // SDL pumps while it waits (SDL_WaitEventTimeout calls SDL_PumpEventsInternal): the OS's modal loop can start
    // here too, so the live frame's condition holds while waiting as well.
    state_->pumpQueue = &queue;
    static_cast<void>(SDL_WaitEventTimeout(nullptr, static_cast<Sint32>(ms)));
    state_->pumpQueue = nullptr;
    RethrowLiveException();
}

WindowSnapshot WindowBackend::Snapshot() const {
    RequireMainThread();
    WindowSnapshot now;
    SDL_Window* window = state_->window;
    const Win::WindowResized size = Adapter::ResizedNow(window, Engine::Time::Now());
    now.physicalSize = size.physicalSize;
    now.scaleFactor = size.scaleFactor;
    now.focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    now.mode = Adapter::IsFullscreen(window) ? Win::WindowMode::BorderlessFullscreen : Win::WindowMode::Windowed;
    now.clipboard = Adapter::ClipboardText(state_->clipboardLimit, now.clipboardTruncated);
    return now;
}

std::optional<Engine::Math::Vec2> WindowBackend::CursorNow() const {
    RequireMainThread();
    SDL_Window* window = state_->window;
    if (SDL_GetMouseFocus() != window) return std::nullopt;
    float x = 0.0F;
    float y = 0.0F;
    static_cast<void>(SDL_GetMouseState(&x, &y));
    const float k = Adapter::LogicalPerWindowCoordinate(window);
    return Engine::Math::Vec2{x * k, y * k};
}

SDL::NativeWindow* WindowBackend::Native() const noexcept { return reinterpret_cast<SDL::NativeWindow*>(state_->window); }

namespace {

bool Done(const bool accepted, std::string& lastError) {
    if (!accepted) lastError = SDL_GetError();
    return accepted;
}

} // namespace

bool WindowBackend::SetTextInput(const In::TextInputRequest& request) {
    RequireMainThread();
    SDL_Window* window = state_->window;
    if (!request.enabled) return Done(SDL_StopTextInput(window), lastError_);
    const float toWindow = 1.0F / Adapter::LogicalPerWindowCoordinate(window);
    const SDL_Rect area{static_cast<int>(std::lround(request.areaPosition.x * toWindow)),
                        static_cast<int>(std::lround(request.areaPosition.y * toWindow)),
                        static_cast<int>(std::lround(request.areaSize.x * toWindow)),
                        static_cast<int>(std::lround(request.areaSize.y * toWindow))};
    const int cursor = static_cast<int>(std::lround(static_cast<float>(request.cursorOffset) * toWindow));
    if (!Done(SDL_SetTextInputArea(window, &area, cursor), lastError_)) return false;
    return Done(SDL_StartTextInput(window), lastError_);
}

bool WindowBackend::SetTitle(const std::string& title) {
    RequireMainThread();
    return Done(SDL_SetWindowTitle(state_->window, title.c_str()), lastError_);
}

bool WindowBackend::SetSize(const Engine::Math::Vec2i logicalSize) {
    RequireMainThread();
    if (state_->fullscreenPending || Adapter::IsFullscreen(state_->window)) {
        state_->heldWindowedSize = logicalSize;
        return true;
    }
    state_->heldWindowedSize.reset();  // a size given now replaces one held from before
    return Done(Adapter::SetWindowSizeLogical(state_->window, logicalSize), lastError_);
}

bool WindowBackend::SetMode(const Win::WindowMode mode) {
    RequireMainThread();
    const bool fullscreen = mode == Win::WindowMode::BorderlessFullscreen;
    // No fullscreen display mode is ever set (SDL_SetWindowFullscreenMode): SDL's fullscreen is the desktop's.
    if (!Done(SDL_SetWindowFullscreen(state_->window, fullscreen), lastError_)) return false;
    state_->fullscreenPending = fullscreen && !Adapter::IsFullscreen(state_->window);
    return true;
}

bool WindowBackend::FlushWindowedSize() {
    RequireMainThread();
    // A fullscreen asked for has come (the flags show it) or ended (SDL left it: LEAVE_FULLSCREEN, Pump).
    if (state_->fullscreenPending && Adapter::IsFullscreen(state_->window)) state_->fullscreenPending = false;
    if (!state_->heldWindowedSize || state_->fullscreenPending || Adapter::IsFullscreen(state_->window)) return true;
    const Engine::Math::Vec2i size = *std::exchange(state_->heldWindowedSize, std::nullopt);
    return Done(Adapter::SetWindowSizeLogical(state_->window, size), lastError_);
}

bool WindowBackend::SetCursorGrab(const Win::CursorGrab grab) {
    RequireMainThread();
    SDL_Window* window = state_->window;
    const bool locked = grab == Win::CursorGrab::Locked;
    if (!Done(SDL_SetWindowRelativeMouseMode(window, locked), lastError_)) return false;
    return Done(SDL_SetWindowMouseGrab(window, grab == Win::CursorGrab::Confined), lastError_);
}

bool WindowBackend::SetResizable(const bool resizable) {
    RequireMainThread();
    return Done(SDL_SetWindowResizable(state_->window, resizable), lastError_);
}

bool WindowBackend::SetCursorIcon(const Win::CursorIcon icon) {
    RequireMainThread();
    SDL_Cursor*& cursor = state_->cursors[static_cast<std::size_t>(icon)];
    if (cursor == nullptr) cursor = SDL_CreateSystemCursor(SystemCursorOf(icon));
    if (cursor == nullptr) {
        lastError_ = SDL_GetError();
        return false;
    }
    return Done(SDL_SetCursor(cursor), lastError_);
}

bool WindowBackend::SetCursorVisible(const bool visible) {
    RequireMainThread();
    return Done(visible ? SDL_ShowCursor() : SDL_HideCursor(), lastError_);
}

bool WindowBackend::WarpCursor(const Engine::Math::Vec2 logicalPosition) {
    RequireMainThread();
    const float toWindow = 1.0F / Adapter::LogicalPerWindowCoordinate(state_->window);
    SDL_WarpMouseInWindow(state_->window, logicalPosition.x * toWindow, logicalPosition.y * toWindow);
    return true;
}

bool WindowBackend::SetClipboard(const std::string& text) {
    RequireMainThread();
    return Done(SDL_SetClipboardText(text.c_str()), lastError_);
}

void WindowBackend::Close() {
    RequireMainThread();
    if (state_->window != nullptr) SDL_HideWindow(state_->window);
}

} // namespace GYO::Plugins::SDLWindow
