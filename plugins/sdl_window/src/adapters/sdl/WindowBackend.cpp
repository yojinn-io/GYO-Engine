// sdl_window's adapter (Design Invariant SDL-1: the only place of sdl_window, with its private Translate.hpp, that
// includes SDL3/). It implements WindowBackend (../../WindowBackend.hpp) and the mapping table of SDL-7.
#include "../../WindowBackend.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <utility>

#include "engine/base/Assert.hpp"

#include "Translate.hpp"

namespace GYO::Plugins::SdlWindow {

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
    const float density = SDL_GetWindowPixelDensity(window);
    const float scale = SDL_GetWindowDisplayScale(window);
    if (!(density > 0.0F) || !(scale > 0.0F)) return 1.0F;
    return density / scale;
}

Translated Translate(const SDL_Event& event, const Sdl::ClockPair& pair, const Engine::Time::TimePoint ceiling,
                     SDL_Window* window, const std::size_t clipboardLimit) {
    Translated out;
    const Engine::Time::TimePoint time = Sdl::ConvertSdlTime(event.common.timestamp, pair, ceiling);
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
    case SDL_EVENT_TEXT_EDITING:
        if (!OwnWindow(window, event.edit.windowID)) break;
        out.input.emplace_back(In::TextComposition{event.edit.text != nullptr ? event.edit.text : "",
                                                   std::max<std::int32_t>(event.edit.start, 0),
                                                   std::max<std::int32_t>(event.edit.length, 0), time});
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (!OwnWindow(window, event.text.windowID)) break;
        out.input.emplace_back(In::TextCommitted{event.text.text != nullptr ? event.text.text : "", time});
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        if (!OwnWindow(window, event.motion.windowID)) break;
        const float k = LogicalPerWindowCoordinate(window);
        out.input.emplace_back(In::CursorMoved{{event.motion.x * k, event.motion.y * k}, time});
        out.input.emplace_back(In::MouseMotion{{event.motion.xrel * k, event.motion.yrel * k}, time});
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
        const float flip = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
        out.input.emplace_back(In::MouseWheel{{event.wheel.x * flip, event.wheel.y * flip}, time});
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
    std::array<SDL_Cursor*, 12> cursors{};
};

Engine::Base::Result<std::unique_ptr<WindowBackend>, BackendError> WindowBackend::Create(const BackendSettings& settings) {
    RequireMainThread();
    // The hints are read when the video starts and are process-wide. os owns the process's signals
    // (InstallShutdownRequests); a closed window is the game's close request, not SDL's quit.
    static_cast<void>(SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"));
    static_cast<void>(SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0"));
    if (settings.drawComposition) static_cast<void>(SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition"));
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return Engine::Base::Err(BackendError::Make(BackendErrorCode::VideoFailed, std::string("SDL_InitSubSystem(VIDEO): ") + SDL_GetError()));
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (settings.window.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (settings.window.hidden) flags |= SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow(settings.window.title.c_str(), settings.window.size.x, settings.window.size.y, flags);
    if (window == nullptr) {
        BackendError error = BackendError::Make(BackendErrorCode::WindowFailed, std::string("SDL_CreateWindow: ") + SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return Engine::Base::Err(std::move(error));
    }
    // SDL makes the window in its window coordinates, which are pixels on Windows: a size asked in logical pixels
    // is set again at the display's scale.
    const float perCoordinate = Adapter::LogicalPerWindowCoordinate(window);
    if (perCoordinate != 1.0F) {
        static_cast<void>(SDL_SetWindowSize(window, static_cast<int>(std::lround(static_cast<float>(settings.window.size.x) / perCoordinate)),
                                            static_cast<int>(std::lround(static_cast<float>(settings.window.size.y) / perCoordinate))));
    }
    auto state = std::make_unique<State>();
    state->window = window;
    state->clipboardLimit = settings.clipboardLimit;
    return std::unique_ptr<WindowBackend>(new WindowBackend(std::move(state)));
}

WindowBackend::WindowBackend(std::unique_ptr<State> state) : state_(std::move(state)) {}

WindowBackend::~WindowBackend() {
    RequireMainThread();
    for (SDL_Cursor*& cursor : state_->cursors) {
        if (cursor != nullptr) SDL_DestroyCursor(cursor);
        cursor = nullptr;
    }
    if (state_->window != nullptr) SDL_DestroyWindow(state_->window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void WindowBackend::Pump(EventQueue& queue) {
    RequireMainThread();
    // One clock pair per pump (SDL declaration section 8). The pump takes SDL's whole pass: leaving SDL_PollEvent
    // early would leave its end-of-pass marker queued and cut the next pump short (master's pit 4).
    const Sdl::ClockPair pair = Sdl::SampleClockPair({&EngineNow, &SdlNow});
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        queue.Accept(Adapter::Translate(event, pair, Engine::Time::Now(), state_->window, state_->clipboardLimit));
    }
}

void WindowBackend::WaitForEvent(const std::int64_t ns) {
    RequireMainThread();
    const std::int64_t ms = std::clamp<std::int64_t>((ns + 999'999) / 1'000'000, 0, 1000);
    static_cast<void>(SDL_WaitEventTimeout(nullptr, static_cast<Sint32>(ms)));
}

WindowSnapshot WindowBackend::Snapshot() const {
    RequireMainThread();
    WindowSnapshot now;
    SDL_Window* window = state_->window;
    const Win::WindowResized size = Adapter::ResizedNow(window, Engine::Time::Now());
    now.physicalSize = size.physicalSize;
    now.scaleFactor = size.scaleFactor;
    now.focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (SDL_GetMouseFocus() == window) {
        float x = 0.0F;
        float y = 0.0F;
        static_cast<void>(SDL_GetMouseState(&x, &y));
        const float k = Adapter::LogicalPerWindowCoordinate(window);
        now.cursor = Engine::Math::Vec2{x * k, y * k};
    }
    now.clipboard = Adapter::ClipboardText(state_->clipboardLimit, now.clipboardTruncated);
    return now;
}

Sdl::NativeWindow* WindowBackend::Native() const noexcept { return reinterpret_cast<Sdl::NativeWindow*>(state_->window); }

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
    const float toWindow = 1.0F / Adapter::LogicalPerWindowCoordinate(state_->window);
    return Done(SDL_SetWindowSize(state_->window, static_cast<int>(std::lround(static_cast<float>(logicalSize.x) * toWindow)),
                                  static_cast<int>(std::lround(static_cast<float>(logicalSize.y) * toWindow))),
                lastError_);
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

} // namespace GYO::Plugins::SdlWindow
