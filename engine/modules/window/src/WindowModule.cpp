#include "engine/window/WindowModule.hpp"

#include <utility>

#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/window/Window.hpp"

namespace GYO::Engine::Window {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const WindowOptions& OptionsOf(const WindowOptions* given) {
    static const WindowOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// The state from this frame's Messages, each type in its order: the last size, the last focus, the last clipboard
// text. A close request sets WindowRequest::close when closeWhenRequested is on (Bevy's close_when_requested,
// bevy_window/src/system.rs).
SystemResult UpdateWindowState(Kernel::ResMut<WindowState> state, Kernel::ResMut<Clipboard> clipboard,
                               Kernel::ResMut<WindowRequest> request, Kernel::MessageReader<WindowResized> resized,
                               Kernel::MessageReader<WindowFocused> focused,
                               Kernel::MessageReader<WindowCloseRequested> closeRequested,
                               Kernel::MessageReader<ClipboardChanged> clipboardChanged) {
    const auto sizes = resized.Read();
    if (!sizes.empty()) {
        WindowState& s = state.Get();
        s.physicalSize = sizes.back()->physicalSize;
        s.scaleFactor = sizes.back()->scaleFactor;
        s.known = true;
    }
    const auto focus = focused.Read();
    if (!focus.empty()) state.Get().focused = focus.back()->focused;
    const auto texts = clipboardChanged.Read();
    if (!texts.empty()) clipboard.Get() = Clipboard{texts.back()->text, texts.back()->truncated};
    if (!closeRequested.Read().empty() && request.Read().closeWhenRequested) request.Get().close = true;
    return {};
}

template <class T>
void PutIfAbsent(Kernel::World& world, T value) {
    if (world.Resource<T>() == nullptr) world.SetResource(std::move(value));
}

} // namespace

void WindowModule::Configure(Orchestration::ProductSetup& setup) {
    const WindowSettings* settings = setup.Options<WindowSettings>();
    closeWhenRequested_ = settings != nullptr ? settings->closeWhenRequested : WindowSettings{}.closeWhenRequested;
}

void WindowModule::Build(Orchestration::WorldSetup& setup) {
    const WindowOptions& options = OptionsOf(setup.Options<WindowOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Message<WindowResized>({.name = "WindowResized"});
    r.Message<WindowFocused>({.name = "WindowFocused"});
    r.Message<WindowCloseRequested>({.name = "WindowCloseRequested"});
    r.Message<ClipboardChanged>({.name = "ClipboardChanged"});
    r.Message<WarpCursor>({.name = "WarpCursor"});
    r.Message<CopyToClipboard>({.name = "CopyToClipboard"});
    r.Resource<WindowState>({.name = "WindowState"});
    r.Resource<Clipboard>({.name = "Clipboard"});
    r.Resource<WindowRequest>({.name = "WindowRequest"});
    r.Set({.name = std::string(kWindowSet), .schedule = options.schedule});
    Kernel::SystemOptions update;
    update.name = "UpdateWindowState";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.sets.push_back(std::string(kWindowSet));
    update.mounts.push_back(std::move(mount));
    r.AddSystem<&UpdateWindowState>(std::move(update));
    // A new World knows nothing of the window (a value carried over, O-7, stays); its backend writes the present
    // size and focus in the World's first round (WN-3, SDL-7 exception (2)).
    const bool closeWhenRequested = closeWhenRequested_;
    setup.Prepare([closeWhenRequested](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent(world, WindowState{});
        PutIfAbsent(world, Clipboard{});
        WindowRequest request;
        request.closeWhenRequested = closeWhenRequested;
        PutIfAbsent(world, std::move(request));
        return {};
    });
}

} // namespace GYO::Engine::Window
