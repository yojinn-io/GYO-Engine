#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <exception>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/base/Log.hpp"

namespace Engine::Platform::Sdl {

namespace {

// Sets a flag for a scope and restores its previous value when the scope ends,
// also when an observer throws, so nested scopes keep the outer one's value.
class FlagScope final {
public:
    explicit FlagScope(bool& flag) noexcept : flag_(&flag), previous_(flag) { *flag_ = true; }
    ~FlagScope() { *flag_ = previous_; }

    FlagScope(const FlagScope&) = delete;
    FlagScope& operator=(const FlagScope&) = delete;

private:
    bool* flag_;
    bool previous_;
};

// Installs a value for a scope and restores the previous one when the scope
// ends, also when an observer throws (nested pumps, Design Invariant 7).
template <class T>
class ValueScope final {
public:
    ValueScope(T& slot, T value) noexcept : slot_(&slot), previous_(std::move(slot)) { *slot_ = std::move(value); }
    ~ValueScope() { *slot_ = std::move(previous_); }

    ValueScope(const ValueScope&) = delete;
    ValueScope& operator=(const ValueScope&) = delete;

private:
    T* slot_;
    T previous_;
};

// Depth of SDL event watch callbacks on this thread (the live frame and event
// watches). SDL marks its watcher list as dispatching with a flag, not a
// depth, so adding or removing a watch, and pushing, inside a callback is
// unsafe (Design Invariant 7).
thread_local int dispatchDepth = 0;

class DispatchScope final {
public:
    DispatchScope() noexcept { ++dispatchDepth; }
    ~DispatchScope() { --dispatchDepth; }
    DispatchScope(const DispatchScope&) = delete;
    DispatchScope& operator=(const DispatchScope&) = delete;
};

// Set on this thread while an injected event is pushed (read by event watches,
// which SDL calls on the pushing thread).
thread_local const SDL_Event* injectedEvent = nullptr;

thread_local std::optional<ClockPair> lastWatchPair;

// Event watches registered with SDL now (read-only test seam).
std::atomic<int> registeredWatches{0};

// Answers of path dialogs, shared by the platform and every request SDL still
// holds: an answer that arrives after the platform is gone lands here and is
// freed with the last request (Design Invariant 10). Each answer travels as a
// list node made before SDL has the request, so a callback only splices it in.
struct PathDialogMailbox final {
    std::mutex mutex;
    std::list<PathDialogAnswer> answers;
};

// One request from the Show call until its last callback. It keeps the filter
// and location strings SDL points into alive until then.
struct PendingPathDialog final {
    std::shared_ptr<PathDialogMailbox> mailbox;
    PathDialogRequest request;
    std::vector<SDL_DialogFileFilter> filters;  // points into request.filters
    // Two nodes: a Failed answer may be followed by one more. SDL 3.4 on
    // Windows reports Failed when its modern dialog cannot start, then falls
    // back to the legacy dialog, which answers again with the user's choice
    // (src/dialog/windows/SDL_windowsdialog.c).
    std::list<PathDialogAnswer> answers;
};

// The requests SDL holds, by key; SDL gets the key, not the request. A request
// leaves on its first answer that is not Failed, or on its second answer; a
// later callback finds nothing and returns. Never destroyed, so a callback that
// arrives after the platform, or during static destruction, finds it.
struct PendingPathDialogs final {
    std::mutex mutex;
    std::uintptr_t nextKey{1};
    std::unordered_map<std::uintptr_t, std::unique_ptr<PendingPathDialog>> requests;
};

PendingPathDialogs& PendingDialogs() {
    static auto* const pending = new PendingPathDialogs();
    return *pending;
}

// SDL's dialog callback: any thread, maybe before the Show call returns, maybe
// after the platform is destroyed, maybe twice for one request. It touches no
// platform state and never asserts (an assertion handler may throw, which would
// terminate here). Lock order: the registry, then a mailbox.
void SDLCALL OnPathDialogClosed(void* userdata, const char* const* files, int) noexcept {
    PendingPathDialogs& registry = PendingDialogs();
    const std::lock_guard registryLock(registry.mutex);
    const auto found = registry.requests.find(reinterpret_cast<std::uintptr_t>(userdata));
    if (found == registry.requests.end()) return;  // answered for good already
    PendingPathDialog& pending = *found->second;
    std::list<PathDialogAnswer> node;
    node.splice(node.end(), pending.answers, pending.answers.begin());
    PathDialogAnswer& answer = node.front();
    try {
        if (files == nullptr) {
            answer.outcome = PathDialogOutcome::Failed;
            answer.error = SDL_GetError();
        } else if (files[0] == nullptr || files[0][0] == '\0') {
            // zenity reports a cancelled dialog as one empty path.
            answer.outcome = PathDialogOutcome::Cancelled;
        } else {
            answer.outcome = PathDialogOutcome::Chosen;
            answer.path = files[0];
        }
    } catch (const std::bad_alloc&) {
        // The text could not be kept: still an answer, without it.
        answer.outcome = PathDialogOutcome::Failed;
        answer.path.clear();
        answer.error.clear();
    }
    const bool last = answer.outcome != PathDialogOutcome::Failed || pending.answers.empty();
    {
        PathDialogMailbox& mailbox = *pending.mailbox;
        const std::lock_guard mailboxLock(mailbox.mutex);
        mailbox.answers.splice(mailbox.answers.end(), node);
    }
    if (last) registry.requests.erase(found);
}
static_assert(noexcept(OnPathDialogClosed(nullptr, nullptr, 0)),
              "the dialog callback must not let an exception cross SDL's stack (Design Invariant 8)");

void ShowWithSdl(PathDialogCallback callback, void* userdata, SDL_Window* window, PathDialogKind kind,
                 const SDL_DialogFileFilter* filters, int filterCount, const char* defaultLocation) noexcept {
    static constexpr SdlDialogFunctions sdl{&SDL_ShowOpenFileDialog, &SDL_ShowSaveFileDialog,
                                            &SDL_ShowOpenFolderDialog};
    ShowWithSdlFunctions(sdl, callback, userdata, window, kind, filters, filterCount, defaultLocation);
}

SdlPlatformError MakeSdlError(
    SdlPlatformErrorCode code,
    std::string message) {
    return SdlPlatformError::Make(code, std::move(message), SDL_GetError());
}

} // namespace

struct SdlPlatform::Impl final {
    static bool SDLCALL WatchEvent(void* userdata, SDL_Event* event);

    SDL_Window* window{};
    // The pair of the innermost running pump (Design Invariant 3). A nested
    // pump keeps its own and restores the outer one when it returns.
    std::optional<ClockPair> pumpPair;
    LiveFrameHandler liveFrameHandler;
    std::exception_ptr liveFrameError;
    bool pumping{};
    bool inLiveFrame{};
    std::shared_ptr<PathDialogMailbox> pathDialogs = std::make_shared<PathDialogMailbox>();
};

namespace {

bool IsPathDialogKind(const PathDialogKind kind) noexcept {
    return kind == PathDialogKind::OpenFile || kind == PathDialogKind::SaveFile || kind == PathDialogKind::OpenFolder;
}

} // namespace

Base::Result<std::unique_ptr<SdlPlatform>, SdlPlatformError>
SdlPlatform::Create(const SdlPlatformOptions& options) {

    if (options.width <= 0 || options.height <= 0) {
        return Base::Err(SdlPlatformError::Make(
            SdlPlatformErrorCode::InvalidWindowSize,
            "SdlPlatform: window dimensions must be positive"));
    }

    // Made before the window, so running out of memory leaks no SDL resource.
    auto impl = std::make_unique<Impl>();

    // SDL reads the hint when its video starts; it is process-wide.
    if (options.imeComposition) {
        static_cast<void>(SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition"));
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return Base::Err(MakeSdlError(
            SdlPlatformErrorCode::InitializationFailed,
            "SdlPlatform: SDL video initialization failed"));
    }

    const SDL_WindowFlags windowFlags = options.resizable ? SDL_WINDOW_RESIZABLE : 0;
    SDL_Window* window = SDL_CreateWindow(
        options.title.c_str(),
        options.width,
        options.height,
        windowFlags);

    if (window == nullptr) {
        auto error = MakeSdlError(
            SdlPlatformErrorCode::WindowCreationFailed,
            "SdlPlatform: SDL window creation failed");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return Base::Err(std::move(error));
    }

    impl->window = window;
    return std::unique_ptr<SdlPlatform>(new SdlPlatform(std::move(impl)));
}

SdlPlatform::SdlPlatform(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

SdlPlatform::~SdlPlatform() {
    SDL_RemoveEventWatch(&Impl::WatchEvent, impl_.get());
    if (impl_->window != nullptr) {
        SDL_DestroyWindow(impl_->window);
        impl_->window = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void SdlPlatform::SetLiveFrameHandler(LiveFrameHandler handler) {
    // Replacing the handler while it runs would destroy the running function.
    GYO_ASSERT(!impl_->inLiveFrame);
    const bool wasSet = static_cast<bool>(impl_->liveFrameHandler);
    impl_->liveFrameHandler = std::move(handler);
    const bool isSet = static_cast<bool>(impl_->liveFrameHandler);
    // The watch exists only while a handler is set.
    if (isSet && !wasSet) {
        SDL_AddEventWatch(&Impl::WatchEvent, impl_.get());
    } else if (!isSet && wasSet) {
        SDL_RemoveEventWatch(&Impl::WatchEvent, impl_.get());
    }
}

bool SDLCALL SdlPlatform::Impl::WatchEvent(void* userdata, SDL_Event* event) {
    // Watches run on whichever thread pushes an event, so the platform's state
    // is read only after the main-thread check.
    if (event->type != SDL_EVENT_WINDOW_EXPOSED || event->window.data1 != 1 ||
        !SDL_IsMainThread()) {
        return true;
    }
    auto* platform = static_cast<Impl*>(userdata);
    if (event->window.windowID != SDL_GetWindowID(platform->window) ||
        !platform->pumping || platform->inLiveFrame || !platform->liveFrameHandler ||
        platform->liveFrameError) {
        return true;
    }

    const DispatchScope dispatch;
    const FlagScope live(platform->inLiveFrame);
    // An exception must not unwind through SDL and the OS modal loop: it is
    // kept and rethrown by PumpEvents once SDL returns.
    try {
        platform->liveFrameHandler();
    } catch (...) {
        if (!platform->liveFrameError) platform->liveFrameError = std::current_exception();
    }
    return true;
}

Runtime::RuntimeControl SdlPlatform::PumpEvents(const EventObserver& observer) {
    Runtime::RuntimeControl control = Runtime::RuntimeControl::Continue;
    const SDL_WindowID ownWindowId = SDL_GetWindowID(impl_->window);

    // Live frames may run only from inside this function.
    const FlagScope pumping(impl_->pumping);
    // One clock pair per pump; a nested pump's pair lives only while it runs.
    const ValueScope<std::optional<ClockPair>> pair(impl_->pumpPair, SampleClockPair());
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (observer) {
            const PlatformEvent view(&event, ConvertSdlTimestamp(event.common.timestamp, *impl_->pumpPair, Time::Now()));
            observer(view);
        }

        if (event.type == SDL_EVENT_QUIT) {
            control = Runtime::RuntimeControl::Stop;
        }

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            event.window.windowID == ownWindowId) {
            control = Runtime::RuntimeControl::Stop;
        }
    }
    // The pump finishes its pass first: leaving SDL_PollEvent early would leave
    // SDL's end-of-pass marker queued and cut the next pump short.
    if (impl_->liveFrameError) {
        std::rethrow_exception(std::exchange(impl_->liveFrameError, nullptr));
    }

    return control;
}

Base::Result<std::filesystem::path, SdlPlatformError> BaseDirectory() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::BaseDirectoryUnavailable,
                                      "SdlPlatform: the base directory is unavailable"));
    }
    // SDL gives UTF-8; the bytes become a path as they are (no GYO::Os edge from this layer).
    std::u8string text;
    for (const char* c = base; *c != '\0'; ++c) text.push_back(static_cast<char8_t>(*c));
    return std::filesystem::path(text);
}

std::string_view PlatformName() noexcept {
    return SDL_GetPlatform();
}

int RunEntry(int (*body)(int, char*[]), const int argc, char* argv[]) noexcept {
    try {
        return body(argc, argv);
    } catch (const std::exception& error) {
        // Two lines, so reporting allocates nothing in this noexcept function.
        Base::Log(Base::LogLevel::Error, "platform", "unhandled exception:");
        Base::Log(Base::LogLevel::Error, "platform", error.what());
    } catch (...) {
        Base::Log(Base::LogLevel::Error, "platform", "unhandled exception");
    }
    return 1;
}

void ShowWithSdlFunctions(const SdlDialogFunctions& functions, PathDialogCallback callback, void* userdata,
                          SDL_Window* window, PathDialogKind kind, const SDL_DialogFileFilter* filters,
                          int filterCount, const char* defaultLocation) noexcept {
    switch (kind) {
    case PathDialogKind::OpenFile:
        functions.openFile(callback, userdata, window, filters, filterCount, defaultLocation, false);
        return;
    case PathDialogKind::SaveFile:
        functions.saveFile(callback, userdata, window, filters, filterCount, defaultLocation);
        return;
    case PathDialogKind::OpenFolder:
        functions.openFolder(callback, userdata, window, defaultLocation, false);
        return;
    }
}

void NativeAccess::ShowPathDialog(SdlPlatform& platform, PathDialogRequest request,
                                  const PathDialogLauncher& launcher) {
    SdlPlatform::Impl& impl = *platform.impl_;
    GYO_ASSERT(!impl.pumping && dispatchDepth == 0);
    GYO_ASSERT(IsPathDialogKind(request.kind));
    for (const PathDialogFilter& filter : request.filters) GYO_ASSERT(!filter.pattern.empty());
    auto pending = std::make_unique<PendingPathDialog>();
    pending->mailbox = impl.pathDialogs;
    pending->request = std::move(request);
    pending->answers.push_back(PathDialogAnswer{.tag = pending->request.tag});
    pending->answers.push_back(PathDialogAnswer{.tag = pending->request.tag});
    for (const PathDialogFilter& filter : pending->request.filters) {
        pending->filters.push_back({filter.name.c_str(), filter.pattern.c_str()});
    }
    const std::string& location = pending->request.defaultLocation;
    const char* defaultLocation = location.empty() ? nullptr : location.c_str();
    const SDL_DialogFileFilter* filters = pending->filters.empty() ? nullptr : pending->filters.data();
    const int filterCount = static_cast<int>(pending->filters.size());
    const PathDialogKind kind = pending->request.kind;
    std::uintptr_t key{};
    {
        PendingPathDialogs& registry = PendingDialogs();
        const std::lock_guard lock(registry.mutex);
        key = registry.nextKey++;
        registry.requests.emplace(key, std::move(pending));  // the last step that may throw
    }
    // From here SDL has the request until OnPathDialogClosed takes it out, which
    // may happen before show returns and on another thread: nothing above is
    // touched again.
    launcher.show(&OnPathDialogClosed, reinterpret_cast<void*>(key), impl.window, kind, filters, filterCount,
                  defaultLocation);
}

void ShowPathDialogWith(SdlPlatform& platform, PathDialogRequest request, const PathDialogLauncher& launcher) {
    NativeAccess::ShowPathDialog(platform, std::move(request), launcher);
}

void SdlPlatform::ShowPathDialog(PathDialogRequest request) {
    NativeAccess::ShowPathDialog(*this, std::move(request), PathDialogLauncher{&ShowWithSdl});
}

std::vector<PathDialogAnswer> SdlPlatform::TakePathDialogAnswers() {
    GYO_ASSERT(SDL_IsMainThread());
    PathDialogMailbox& mailbox = *impl_->pathDialogs;
    std::list<PathDialogAnswer> arrived;
    {
        const std::lock_guard lock(mailbox.mutex);
        arrived.splice(arrived.end(), mailbox.answers);
    }
    std::vector<PathDialogAnswer> answers;
    try {
        answers.reserve(arrived.size());
    } catch (...) {
        // Out of memory: the answers stay for the next call, in their order.
        const std::lock_guard lock(mailbox.mutex);
        mailbox.answers.splice(mailbox.answers.begin(), arrived);
        throw;
    }
    for (PathDialogAnswer& answer : arrived) answers.push_back(std::move(answer));
    return answers;
}

std::string SdlPlatform::ClipboardText() const {
    char* text = SDL_GetClipboardText();
    std::string result = text != nullptr ? text : "";
    SDL_free(text);
    return result;
}

WindowState SdlPlatform::QueryWindow() const {
    SDL_Window* window = impl_->window;
    WindowState state;
    static_cast<void>(SDL_GetWindowPosition(window, &state.rect.x, &state.rect.y));
    static_cast<void>(SDL_GetWindowSize(window, &state.rect.width, &state.rect.height));
    // Platforms without border information report failure: zeros stay.
    if (!SDL_GetWindowBordersSize(window, &state.borders.top, &state.borders.left, &state.borders.bottom,
                                  &state.borders.right)) {
        state.borders = {};
    }
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    state.flags.occluded = (flags & SDL_WINDOW_OCCLUDED) != 0;
    state.flags.hidden = (flags & SDL_WINDOW_HIDDEN) != 0;
    state.flags.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    state.flags.inputFocus = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    state.flags.relativeMouseMode = SDL_GetWindowRelativeMouseMode(window);
    return state;
}

Base::Result<DisplayState, SdlPlatformError> SdlPlatform::QueryDisplay() const {
    const SDL_DisplayID display = SDL_GetDisplayForWindow(impl_->window);
    if (display == 0) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::DisplayUnavailable,
                                      "SdlPlatform: the window has no display"));
    }
    DisplayState state;
    SDL_Rect bounds{};
    if (SDL_GetDisplayUsableBounds(display, &bounds)) {
        state.usableBounds = {bounds.x, bounds.y, bounds.w, bounds.h};
    }
    if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display)) {
        state.refreshRate = mode->refresh_rate;
    }
    return state;
}

std::string_view SdlPlatform::VideoDriverName() const {
    const char* name = SDL_GetCurrentVideoDriver();
    return name != nullptr ? std::string_view(name) : std::string_view();
}

std::optional<std::uint64_t> SdlPlatform::X11WindowNumber() const {
    const Sint64 number = SDL_GetNumberProperty(SDL_GetWindowProperties(impl_->window),
                                                SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (number == 0) return std::nullopt;
    return static_cast<std::uint64_t>(number);
}

Base::Result<void, SdlPlatformError> SdlPlatform::SetWindowSize(const int width, const int height) {
    GYO_ASSERT(width > 0 && height > 0);
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    if (!SDL_SetWindowSize(impl_->window, width, height)) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::WindowOperationFailed,
                                      "SdlPlatform: setting the window size failed"));
    }
    return {};
}

Base::Result<void, SdlPlatformError> SdlPlatform::SetWindowPosition(const int x, const int y) {
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    if (!SDL_SetWindowPosition(impl_->window, x, y)) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::WindowOperationFailed,
                                      "SdlPlatform: setting the window position failed"));
    }
    return {};
}

void SdlPlatform::Raise() {
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    static_cast<void>(SDL_RaiseWindow(impl_->window));
}

bool SdlPlatform::Sync() {
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    return SDL_SyncWindow(impl_->window);
}

struct EventWatch::Node final {
    static bool SDLCALL Trampoline(void* userdata, SDL_Event* event);

    EventWatchCallback callback{};
    void* context{};
    SDL_WindowID window{};
};

namespace {

// The observed kind of an event of the given window; empty for any other.
std::optional<ObservedEvent> Observe(const SDL_Event& event, const SDL_WindowID window) {
    ObservedEvent observed;
    SDL_WindowID from = 0;
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
        observed.kind = ObservedEventKind::MouseMotion;
        from = event.motion.windowID;
        observed.sourceId = event.motion.which;
        observed.deltaX = event.motion.xrel;
        observed.deltaY = event.motion.yrel;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        observed.kind = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? ObservedEventKind::MouseButtonPressed
                                                                  : ObservedEventKind::MouseButtonReleased;
        from = event.button.windowID;
        observed.sourceId = event.button.which;
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED: observed.kind = ObservedEventKind::WindowFocusGained; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_FOCUS_LOST: observed.kind = ObservedEventKind::WindowFocusLost; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_MOVED: observed.kind = ObservedEventKind::WindowMoved; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_RESIZED: observed.kind = ObservedEventKind::WindowResized; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_MINIMIZED: observed.kind = ObservedEventKind::WindowMinimized; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_RESTORED: observed.kind = ObservedEventKind::WindowRestored; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_OCCLUDED: observed.kind = ObservedEventKind::WindowOccluded; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_EXPOSED: observed.kind = ObservedEventKind::WindowExposed; from = event.window.windowID; break;
    case SDL_EVENT_WINDOW_HIDDEN: observed.kind = ObservedEventKind::WindowHidden; from = event.window.windowID; break;
    default: return std::nullopt;
    }
    if (from != window) return std::nullopt;
    return observed;
}

} // namespace

bool SDLCALL EventWatch::Node::Trampoline(void* userdata, SDL_Event* event) {
    const auto* node = static_cast<const Node*>(userdata);
    // Filter first: SDL also calls watchers for events without a time.
    auto observed = Observe(*event, node->window);
    if (!observed) return true;
    const DispatchScope dispatch;
    // One pair per callback (Design Invariant 3), sampled by the one function
    // that fixes the reads and their order; its second Engine read (engine is
    // the midpoint) is the observation time.
    const ClockPair pair = SampleClockPair();
    const Time::TimePoint after = pair.engine + (pair.bracket - pair.bracket / 2);
    lastWatchPair = pair;
    // The event PushInjectedEvent is pushing on this thread; real events SDL
    // queues meanwhile (for example from a live frame inside the push) are not.
    observed->injected = event == injectedEvent;
    observed->observedAt = after;
    observed->timestamp = ConvertSdlTimestamp(event->common.timestamp, pair, after);
    node->callback(node->context, *observed);
    return true;
}

EventWatch::EventWatch() noexcept = default;

EventWatch::EventWatch(std::unique_ptr<Node> node) noexcept : node_(std::move(node)) {}

EventWatch::EventWatch(EventWatch&& other) noexcept : node_(std::move(other.node_)) {}

EventWatch& EventWatch::operator=(EventWatch&& other) {
    if (this != &other) {
        Remove();
        node_ = std::move(other.node_);
    }
    return *this;
}

EventWatch::~EventWatch() {
    Remove();
}

void EventWatch::Remove() {
    if (!node_) return;
    // SDL marks its watcher list as dispatching with a flag, not a depth:
    // removing from inside a callback or a live frame could free a node SDL
    // still calls (Design Invariant 7).
    GYO_ASSERT(dispatchDepth == 0);
    // SDL destroys its watcher lock without taking it when the last platform
    // quits the events subsystem; only the main thread, which owns that
    // teardown, may remove (Design Invariant 7, D38).
    GYO_ASSERT(SDL_IsMainThread());
    // Waits for a callback running on another thread (SDL's watcher lock).
    SDL_RemoveEventWatch(&Node::Trampoline, node_.get());
    registeredWatches.fetch_sub(1);
    node_.reset();
}

EventWatch SdlPlatform::AddEventWatch(const EventWatchCallback callback, void* context) {
    GYO_ASSERT(callback != nullptr);
    GYO_ASSERT(dispatchDepth == 0);
    auto node = std::make_unique<EventWatch::Node>();
    node->callback = callback;
    node->context = context;
    node->window = SDL_GetWindowID(impl_->window);
    if (!SDL_AddEventWatch(&EventWatch::Node::Trampoline, node.get())) throw std::bad_alloc();
    registeredWatches.fetch_add(1);
    return EventWatch(std::move(node));
}

std::optional<ClockPair> LastWatchClockPairForTesting() noexcept {
    return lastWatchPair;
}

int RegisteredEventWatchesForTesting() noexcept {
    return registeredWatches.load();
}

bool PushInjectedEvent(SdlPlatform& platform, SDL_Event& event) {
    static_cast<void>(platform);
    GYO_ASSERT(dispatchDepth == 0);
    // SDL hands the pushed event itself to the watchers, on this thread.
    const ValueScope<const SDL_Event*> injected(injectedEvent, &event);
    return SDL_PushEvent(&event);
}

SDL_Window* NativeAccess::Window(const SdlPlatform& platform) noexcept {
    return platform.impl_->window;
}

ClockPair MakeClockPair(const Time::TimePoint engineBefore, const std::uint64_t sdlNs,
                        const Time::TimePoint engineAfter) noexcept {
    return {engineBefore + (engineAfter - engineBefore) / 2, sdlNs, engineAfter - engineBefore};
}

ClockPair SampleClockPairWith(const ClockReaders& readers) noexcept {
    // Separate statements: the three reads happen in this order.
    const Time::TimePoint before = readers.engine();
    const std::uint64_t sdl = readers.sdl();
    const Time::TimePoint after = readers.engine();
    return MakeClockPair(before, sdl, after);
}

ClockPair SampleClockPair() noexcept {
    static constexpr ClockReaders real{
        [] () noexcept { return Time::Now(); },
        [] () noexcept -> std::uint64_t { return SDL_GetTicksNS(); },
    };
    return SampleClockPairWith(real);
}

Time::TimePoint ConvertSdlTimestamp(const std::uint64_t sdlNs, const ClockPair& pair,
                                    const Time::TimePoint ceiling) noexcept {
    if (sdlNs == 0) return ceiling;  // added without a time (SDL_PeepEvents)
    // (int64)(sdlNs - pair.sdlNs): the unsigned difference converted modulo
    // 2^64 (C++20), so an event before the pair maps before it, without UB.
    const std::chrono::nanoseconds since{static_cast<std::int64_t>(sdlNs - pair.sdlNs)};
    const Time::TimePoint converted = pair.engine + std::chrono::duration_cast<Time::Duration>(since);
    return converted < ceiling ? converted : ceiling;
}

std::optional<ClockPair> NativeAccess::PumpPair(const SdlPlatform& platform) {
    return platform.impl_->pumpPair;
}

std::optional<ClockPair> CurrentClockPairForTesting(const SdlPlatform& platform) {
    return NativeAccess::PumpPair(platform);
}

} // namespace Engine::Platform::Sdl
