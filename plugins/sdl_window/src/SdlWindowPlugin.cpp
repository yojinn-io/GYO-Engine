#include "plugins/sdl_window/SdlWindowPlugin.hpp"

#include <chrono>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "engine/base/Result.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/Product.hpp"
#include "engine/os/Console.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "plugins/sdl/NativeWindow.hpp"

#include "Service.hpp"

namespace GYO::Plugins::SdlWindow {

namespace In = Engine::Input;
namespace Win = Engine::Window;
namespace O = Engine::Orchestration;

namespace {

using SystemResult = Engine::Base::Result<void, Kernel::KernelError>;

const SdlWindowWorldOptions& WorldOptionsOf(const SdlWindowWorldOptions* given) {
    static const SdlWindowWorldOptions defaults{};
    return given != nullptr ? *given : defaults;
}

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

// The input half of the queue, each item to its Message type, in the order the OS sent them; then the drop count.
// In a new World's first round the present state comes first (SDL-7 exception (2)): the cursor's place when it is
// over the window. Keys held across a World's making are not written (the OS gives no press for them; their
// release is a release of a key not held, which input ignores).
SystemResult WriteInputMessages(Kernel::ResMut<SdlWindowHandle> handle, Kernel::MessageWriter<In::KeyboardInput> keys,
                                Kernel::MessageWriter<In::KeyboardFocusLost> focusLost,
                                Kernel::MessageWriter<In::InputEventsDropped> dropped,
                                Kernel::MessageWriter<In::MouseButtonInput> buttons, Kernel::MessageWriter<In::CursorMoved> moved,
                                Kernel::MessageWriter<In::MouseMotion> motion, Kernel::MessageWriter<In::CursorEntered> entered,
                                Kernel::MessageWriter<In::CursorLeft> left, Kernel::MessageWriter<In::MouseWheel> wheel,
                                Kernel::MessageWriter<In::TextComposition> composition,
                                Kernel::MessageWriter<In::TextCommitted> committed) {
    if (!handle.Present() || handle.Read().service == nullptr) return {};
    Service& service = *handle.Read().service;
    if (handle.Read().inputSnapshotPending) {
        handle.Get().inputSnapshotPending = false;
        if (WindowBackend* backend = service.Backend()) {
            const WindowSnapshot now = backend->Snapshot();
            const Engine::Time::TimePoint at = Engine::Time::Now();
            if (now.cursor) {
                entered.Send(In::CursorEntered{at});
                moved.Send(In::CursorMoved{*now.cursor, at});
            }
        }
    }
    for (InputItem& item : service.Queue().TakeInput()) {
        std::visit(Overloaded{
                       [&](In::KeyboardInput& m) { keys.Send(std::move(m)); },
                       [&](In::KeyboardFocusLost& m) { focusLost.Send(std::move(m)); },
                       [&](In::MouseButtonInput& m) { buttons.Send(std::move(m)); },
                       [&](In::CursorMoved& m) { moved.Send(std::move(m)); },
                       [&](In::MouseMotion& m) { motion.Send(std::move(m)); },
                       [&](In::CursorEntered& m) { entered.Send(std::move(m)); },
                       [&](In::CursorLeft& m) { left.Send(std::move(m)); },
                       [&](In::MouseWheel& m) { wheel.Send(std::move(m)); },
                       [&](In::TextComposition& m) { composition.Send(std::move(m)); },
                       [&](In::TextCommitted& m) { committed.Send(std::move(m)); },
                   },
                   item);
    }
    if (const std::uint64_t count = service.Queue().TakeDropped(); count > 0) {
        dropped.Send(In::InputEventsDropped{count, Engine::Time::Now()});
    }
    return {};
}

// The window half of the queue. In a new World's first round the present size, focus and clipboard come first.
SystemResult WriteWindowMessages(Kernel::ResMut<SdlWindowHandle> handle, Kernel::MessageWriter<Win::WindowResized> resized,
                                 Kernel::MessageWriter<Win::WindowFocused> focused,
                                 Kernel::MessageWriter<Win::WindowCloseRequested> closeRequested,
                                 Kernel::MessageWriter<Win::ClipboardChanged> clipboard) {
    if (!handle.Present() || handle.Read().service == nullptr) return {};
    Service& service = *handle.Read().service;
    if (handle.Read().windowSnapshotPending) {
        handle.Get().windowSnapshotPending = false;
        if (WindowBackend* backend = service.Backend()) {
            WindowSnapshot now = backend->Snapshot();
            const Engine::Time::TimePoint at = Engine::Time::Now();
            resized.Send(Win::WindowResized{now.physicalSize, now.scaleFactor, at});
            focused.Send(Win::WindowFocused{now.focused, at});
            clipboard.Send(Win::ClipboardChanged{std::move(now.clipboard), now.clipboardTruncated, at});
        }
    }
    for (WindowItem& item : service.Queue().TakeWindow()) {
        std::visit(Overloaded{
                       [&](Win::WindowResized& m) { resized.Send(std::move(m)); },
                       [&](Win::WindowFocused& m) { focused.Send(std::move(m)); },
                       [&](Win::WindowCloseRequested& m) { closeRequested.Send(std::move(m)); },
                       [&](Win::ClipboardChanged& m) { clipboard.Send(std::move(m)); },
                   },
                   item);
    }
    return {};
}

// The request side (Bevy's changed_windows in Last, bevy_winit/src/system.rs; through the adapter,
// -sdl-system-calls-via-adapter): each field applied only when it differs from what SDL accepted last; a refusal is
// counted and tried again next frame. It is not the System's error (the World goes on; SdlWindowPump tells it).
// The close request is applied last: the window goes and the runner ends after this frame.
SystemResult ApplyWindowRequests(Kernel::Res<SdlWindowHandle> handle, Kernel::Res<In::TextInputRequest> textInput,
                                 Kernel::Res<Win::WindowRequest> window, Kernel::MessageReader<Win::WarpCursor> warps,
                                 Kernel::MessageReader<Win::CopyToClipboard> copies) {
    const auto warpList = warps.Read();
    const auto copyList = copies.Read();
    if (!handle.Present() || handle->service == nullptr) return {};
    Service& service = *handle->service;
    WindowBackend* backend = service.Backend();
    if (backend == nullptr || service.Closed()) return {};
    AppliedRequests& applied = service.Applied();
    const auto apply = [&](bool accepted, auto& cached, const auto& wanted) {
        if (accepted) {
            cached = wanted;
        } else {
            service.Refused(backend->LastError());
        }
    };
    if (textInput.Present() && !(*textInput == applied.textInput)) {
        apply(backend->SetTextInput(*textInput), applied.textInput, *textInput);
    }
    if (window.Present()) {
        const Win::WindowRequest& request = *window;
        if (request.title && *request.title != applied.title) apply(backend->SetTitle(*request.title), applied.title, *request.title);
        if (request.size && !(*request.size == applied.size)) apply(backend->SetSize(*request.size), applied.size, *request.size);
        if (request.resizable && *request.resizable != applied.resizable) {
            apply(backend->SetResizable(*request.resizable), applied.resizable, *request.resizable);
        }
        if (request.cursorIcon != applied.cursorIcon) apply(backend->SetCursorIcon(request.cursorIcon), applied.cursorIcon, request.cursorIcon);
        if (request.cursorVisible != applied.cursorVisible) {
            apply(backend->SetCursorVisible(request.cursorVisible), applied.cursorVisible, request.cursorVisible);
        }
    }
    for (const Win::WarpCursor* warp : warpList) {
        if (!backend->WarpCursor(warp->position)) service.Refused(backend->LastError());
    }
    for (const Win::CopyToClipboard* copy : copyList) {
        if (!backend->SetClipboard(copy->text)) service.Refused(backend->LastError());
    }
    if (window.Present() && window->close) service.Close();
    return {};
}

Kernel::SystemOptions Writer(std::string name, const SdlWindowWorldOptions& options, std::string_view beforeSet) {
    Kernel::SystemOptions system;
    system.name = std::move(name);
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.writeStage;
    mount.before.push_back(std::string(beforeSet));
    system.mounts.push_back(std::move(mount));
    return system;
}

// The window's runner (SDL declaration section 9.1; Bevy's winit runner): pump, frame, until the window closed or
// a frame failed. It looks at no event: the close request travels as window's Message and comes back as the game's
// WindowRequest::close, applied by ApplyWindowRequests.
O::RunnerExit RunWindow(O::Product& product, Service& service, std::chrono::nanoseconds minFrameInterval) {
    using Clock = Engine::Time::MonotonicClock;
    while (!service.Closed()) {
        const Clock::time_point frameStart = Clock::now();
        if (Engine::OS::Console::RequestedShutdown()) service.ShutdownRequested(frameStart);
        service.Pump();
        auto ran = product.RunFrame();
        if (!ran) {
            const O::OrchestrationError& error = ran.error();
            if (error.failure) return O::RunnerExit{O::RunnerExitCode::Failure, "the Product ended: " + error.failure->Text()};
            return O::RunnerExit{O::RunnerExitCode::Failure, std::string(O::ToString(error.code)) + ": " + error.message +
                                                                 (error.detail.empty() ? "" : "\n" + error.detail)};
        }
        if (minFrameInterval.count() > 0 && !service.Closed()) {
            const auto left = minFrameInterval - (Clock::now() - frameStart);
            if (left.count() > 0 && service.Backend() != nullptr) {
                service.Backend()->WaitForEvent(std::chrono::duration_cast<std::chrono::nanoseconds>(left).count());
            }
        }
    }
    return O::RunnerExit{};
}

} // namespace

void SdlWindowPump::Pump() { service_->Pump(); }
bool SdlWindowPump::Closed() const { return service_->Closed(); }
std::uint64_t SdlWindowPump::DroppedEvents() const { return service_->Queue().DroppedTotal(); }
std::uint64_t SdlWindowPump::RefusedRequests() const { return service_->RefusedRequests(); }
const std::string& SdlWindowPump::LastRefusal() const { return service_->LastRefusal(); }

SdlWindowPlugin::SdlWindowPlugin() = default;
SdlWindowPlugin::~SdlWindowPlugin() = default;

void SdlWindowPlugin::Configure(O::ProductSetup& setup) {
    const SdlWindowOptions* given = setup.Options<SdlWindowOptions>();
    const SdlWindowOptions options = given != nullptr ? *given : SdlWindowOptions{};
    const Win::WindowSettings* settingsGiven = setup.Options<Win::WindowSettings>();
    BackendSettings settings;
    settings.window = settingsGiven != nullptr ? *settingsGiven : Win::WindowSettings{};
    settings.drawComposition = options.drawComposition;
    settings.clipboardLimit = options.clipboardLimit;
    auto backend = WindowBackend::Create(settings);
    if (!backend) {
        // A startup error: no World is built (O-2). The service is still made, so Build finds it and the Systems
        // have nothing to do.
        setup.Report("SDL-E1", std::string("sdl_window could not make the window (") + ToString(backend.error().code) +
                                  "): " + backend.error().message);
        service_ = std::make_unique<Service>(nullptr, options.queueCapacity, settings.window);
        return;
    }
    service_ = std::make_unique<Service>(std::move(*backend), options.queueCapacity, settings.window);
    Service& service = *service_;
    setup.Buffer().Create<Sdl::NativeWindows>(Sdl::NativeWindows{service.Backend()->Native()});
    setup.Buffer().Create<SdlWindowPump>(service);
    if (options.installRunner) {
        const std::chrono::nanoseconds interval = options.minFrameInterval;
        setup.InstallRunner("sdl_window", [&service, interval](O::Product& product) { return RunWindow(product, service, interval); });
    }
    // After every World (O-15): nothing in a World points at the window any more.
    setup.OnDestroy([&service] { service.Shutdown(); });
}

void SdlWindowPlugin::Build(O::WorldSetup& setup) {
    const SdlWindowWorldOptions& options = WorldOptionsOf(setup.Options<SdlWindowWorldOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<SdlWindowHandle>({.name = "SdlWindowHandle", .threadBound = true});
    r.AddSystem<&WriteInputMessages>(Writer("SdlWindowWriteInput", options, In::kInputSet));
    r.AddSystem<&WriteWindowMessages>(Writer("SdlWindowWriteWindow", options, Win::kWindowSet));
    Kernel::SystemOptions apply;
    apply.name = "SdlWindowApplyRequests";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.applyStage;
    apply.mounts.push_back(std::move(mount));
    r.AddSystem<&ApplyWindowRequests>(std::move(apply));
    // Put on the thread that prepares the World (the runner's, the main thread; SDL-4). A new or replaced World's
    // first round writes the present state.
    Service* service = service_.get();
    setup.Prepare([service](Kernel::World& world) -> Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(SdlWindowHandle{service, true, true});
        return {};
    });
}

} // namespace GYO::Plugins::SdlWindow
