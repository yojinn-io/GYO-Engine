#include "plugins/sdl_window/SDLWindowPlugin.hpp"

#include <chrono>
#include <optional>
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
#include "engine/threads/RoleThread.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "plugins/sdl/NativeWindow.hpp"

#include "Service.hpp"

namespace GYO::Plugins::SDLWindow {

namespace In = Engine::Input;
namespace Win = Engine::Window;
namespace O = Engine::Orchestration;

namespace {

using SystemResult = Engine::Base::Result<void, Kernel::KernelError>;

const SDLWindowWorldOptions& WorldOptionsOf(const SDLWindowWorldOptions* given) {
    static const SDLWindowWorldOptions defaults{};
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
SystemResult WriteInputMessages(Kernel::ResMut<SDLWindowHandle> handle, Kernel::MessageWriter<In::KeyboardInput> keys,
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
            if (const std::optional<Engine::Math::Vec2> cursor = backend->CursorNow()) {
                const Engine::Time::TimePoint at = Engine::Time::Now();
                entered.Send(In::CursorEntered{at});
                moved.Send(In::CursorMoved{*cursor, at});
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
SystemResult WriteWindowMessages(Kernel::ResMut<SDLWindowHandle> handle, Kernel::MessageWriter<Win::WindowResized> resized,
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
// counted and tried again next frame. It is not the System's error (the World goes on; SDLWindowPump tells it).
// The close request is applied last: the window goes and the runner ends after this frame.
SystemResult ApplyWindowRequests(Kernel::Res<SDLWindowHandle> handle, Kernel::Res<In::TextInputRequest> textInput,
                                 Kernel::Res<Win::WindowRequest> window, Kernel::MessageReader<Win::WarpCursor> warps,
                                 Kernel::MessageReader<Win::ResizeWindow> resizes,
                                 Kernel::MessageReader<Win::CopyToClipboard> copies) {
    const auto warpList = warps.Read();
    const auto resizeList = resizes.Read();
    const auto copyList = copies.Read();
    if (!handle.Present() || handle->service == nullptr) return {};
    Service& service = *handle->service;
    WindowBackend* backend = service.Backend();
    if (backend == nullptr || service.Closed()) return {};
    // Inside the OS's modal loop (a live frame) nothing is asked of the window: the next frame outside applies it
    // (master's survey: no operation from a watch).
    if (backend->InLiveFrame()) return {};
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
        if (request.resizable && *request.resizable != applied.resizable) {
            apply(backend->SetResizable(*request.resizable), applied.resizable, *request.resizable);
        }
        if (request.cursorIcon != applied.cursorIcon) apply(backend->SetCursorIcon(request.cursorIcon), applied.cursorIcon, request.cursorIcon);
        if (request.cursorVisible != applied.cursorVisible) {
            apply(backend->SetCursorVisible(request.cursorVisible), applied.cursorVisible, request.cursorVisible);
        }
        if (request.cursorGrab != applied.cursorGrab) apply(backend->SetCursorGrab(request.cursorGrab), applied.cursorGrab, request.cursorGrab);
    }
    for (const Win::ResizeWindow* resize : resizeList) {
        if (!backend->SetSize(resize->size)) service.Refused(backend->LastError());
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

Kernel::SystemOptions Writer(std::string name, const SDLWindowWorldOptions& options, std::string_view beforeSet) {
    Kernel::SystemOptions system;
    system.name = std::move(name);
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.writeStage;
    mount.before.push_back(std::string(beforeSet));
    system.mounts.push_back(std::move(mount));
    return system;
}

O::RunnerExit FailedExit(const O::OrchestrationError& error) {
    if (error.failure) return O::RunnerExit{O::RunnerExitCode::Failure, "the Product ended: " + error.failure->Text()};
    return O::RunnerExit{O::RunnerExitCode::Failure,
                         std::string(O::ToString(error.code)) + ": " + error.message + (error.detail.empty() ? "" : "\n" + error.detail)};
}

// The Worlds lent to threads of their own (O-13; R5; time design 5.5 (3)): each thread runs its World's rounds and
// waits, between two, until its next fixed step (time design 5.5 (1) step 5: the sampled time plus what is left to
// the next step, divided by the virtual time's scale). Stopped, joined and returned on every way out of the runner.
class LentWorlds final {
public:
    explicit LentWorlds(O::Product& product) : product_(product) {}
    LentWorlds(const LentWorlds&) = delete;
    LentWorlds& operator=(const LentWorlds&) = delete;
    ~LentWorlds() { ReturnAll(); }

    std::optional<O::RunnerExit> Lend(const std::vector<std::string>& names, const std::chrono::nanoseconds idle) {
        for (const std::string& name : names) {
            auto lent = product_.Lend(name);
            if (!lent) return FailedExit(lent.error());
            auto lease = std::make_shared<O::WorldLease>(std::move(lent).value());
            auto failure = std::make_shared<std::optional<O::OrchestrationError>>();
            auto started = Engine::Threads::RoleThread::Start(
                {.name = "gyo-world-" + name, .priority = Engine::Threads::ThreadPriority::Interactive},
                [lease, failure, idle](Engine::Threads::RoleContext& context) {
                    while (!context.StopRequested()) {
                        auto ran = lease->RunRound();
                        if (!ran) {
                            *failure = std::move(ran).error();
                            return;
                        }
                        static_cast<void>(context.WaitUntil(Engine::Time::Now() + UntilNextRound(lease->World(), idle)));
                    }
                });
            if (!started) {
                product_.Return(std::move(*lease));
                return O::RunnerExit{O::RunnerExitCode::Failure, "the thread of World " + name + " did not start: " +
                                                                     Engine::Base::Describe(started.error())};
            }
            worlds_.push_back({lease, failure, std::move(started).value()});
        }
        return std::nullopt;
    }

    // Stops every thread, joins it and returns its World (a World that broke there ends the Product: Return keeps it).
    void ReturnAll() {
        for (Lent& lent : worlds_) lent.thread.RequestStop();
        for (Lent& lent : worlds_) {
            lent.thread.Join();
            product_.Return(std::move(*lent.lease));
        }
        worlds_.clear();
    }

private:
    struct Lent {
        std::shared_ptr<O::WorldLease> lease;
        std::shared_ptr<std::optional<O::OrchestrationError>> failure;
        Engine::Threads::RoleThread thread;
    };

    static std::chrono::nanoseconds UntilNextRound(const Kernel::World& world, const std::chrono::nanoseconds idle) {
        // A World without world_time has neither (Resource asks only of a registered type).
        if (!world.Types().ResourceIdOf<Engine::WorldTime::FixedTime>() || !world.Types().ResourceIdOf<Engine::WorldTime::VirtualTime>()) {
            return idle;
        }
        const auto* fixed = world.Resource<Engine::WorldTime::FixedTime>();
        const auto* time = world.Resource<Engine::WorldTime::VirtualTime>();
        if (fixed == nullptr || time == nullptr || time->paused != 0 || !(time->scale > 0.0)) return idle;
        const double left = static_cast<double>(fixed->accumulator.UntilNextStep()) / time->scale;
        return std::chrono::nanoseconds(static_cast<std::int64_t>(left > 0.0 ? left : 0.0));
    }

    O::Product& product_;
    std::vector<Lent> worlds_;
};

// The window's runner (SDL declaration section 9.1; Bevy's winit runner): pump, frame, until the window closed or
// a frame failed. It looks at no event: the close request travels as window's Message and comes back as the game's
// WindowRequest::close, applied by ApplyWindowRequests. A frame the live frame ran inside the pump failing ends it
// once the pump returned (master's pit 4: the pump is never left part way).
O::RunnerExit RunWindow(O::Product& product, Service& service, const SDLWindowOptions& options) {
    using Clock = Engine::Time::MonotonicClock;
    LentWorlds lent(product);
    if (!options.ownThreadWorlds.empty()) {
        // O-1: game initialization runs before a World is lent.
        if (!product.GameInitRan()) {
            if (auto init = product.RunGameInit(); !init) return FailedExit(init.error());
        }
        if (auto refused = lent.Lend(options.ownThreadWorlds, options.ownThreadIdle)) return *refused;
    }
    std::optional<O::OrchestrationError> liveFailure;
    if (options.liveFrame && service.Backend() != nullptr) {
        service.Backend()->SetLiveFrame([&product, &liveFailure] {
            if (liveFailure) return;
            if (auto ran = product.RunFrame(); !ran) liveFailure = std::move(ran).error();
        });
    }
    const auto finish = [&](O::RunnerExit exit) {
        if (service.Backend() != nullptr) service.Backend()->SetLiveFrame({});
        lent.ReturnAll();
        return exit;
    };
    while (!service.Closed()) {
        const Clock::time_point frameStart = Clock::now();
        if (Engine::OS::Console::RequestedShutdown()) service.ShutdownRequested(frameStart);
        service.Pump();
        if (liveFailure) return finish(FailedExit(*liveFailure));
        if (service.Closed()) break;  // a live frame applied the close
        auto ran = product.RunFrame();
        if (!ran) return finish(FailedExit(ran.error()));
        if (options.minFrameInterval.count() > 0 && !service.Closed()) {
            const auto left = options.minFrameInterval - (Clock::now() - frameStart);
            if (left.count() > 0 && service.Backend() != nullptr) {
                service.Backend()->WaitForEvent(std::chrono::duration_cast<std::chrono::nanoseconds>(left).count());
            }
        }
    }
    return finish(O::RunnerExit{});
}

} // namespace

void SDLWindowPump::Pump() { service_->Pump(); }
void SDLWindowPump::SetLiveFrame(std::function<void()> frame) {
    if (service_->Backend() != nullptr) service_->Backend()->SetLiveFrame(std::move(frame));
}
bool SDLWindowPump::Closed() const { return service_->Closed(); }
std::uint64_t SDLWindowPump::DroppedEvents() const { return service_->Queue().DroppedTotal(); }
std::uint64_t SDLWindowPump::RefusedRequests() const { return service_->RefusedRequests(); }
const std::string& SDLWindowPump::LastRefusal() const { return service_->LastRefusal(); }

SDLWindowPlugin::SDLWindowPlugin() = default;
SDLWindowPlugin::~SDLWindowPlugin() = default;

void SDLWindowPlugin::Configure(O::ProductSetup& setup) {
    const SDLWindowOptions* given = setup.Options<SDLWindowOptions>();
    const SDLWindowOptions options = given != nullptr ? *given : SDLWindowOptions{};
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
        service_ = std::make_unique<Service>(nullptr, options.queueCapacity, options.windowQueueCapacity, settings.window);
        return;
    }
    service_ = std::make_unique<Service>(std::move(*backend), options.queueCapacity, options.windowQueueCapacity, settings.window);
    Service& service = *service_;
    setup.Buffer().Create<SDL::NativeWindows>(SDL::NativeWindows{service.Backend()->Native()});
    setup.Buffer().Create<SDLWindowPump>(service);
    if (options.installRunner) {
        setup.InstallRunner("sdl_window", [&service, options](O::Product& product) { return RunWindow(product, service, options); });
    }
    // After every World (O-15): nothing in a World points at the window any more.
    setup.OnDestroy([&service] { service.Shutdown(); });
}

void SDLWindowPlugin::Build(O::WorldSetup& setup) {
    const SDLWindowWorldOptions& options = WorldOptionsOf(setup.Options<SDLWindowWorldOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<SDLWindowHandle>({.name = "SDLWindowHandle", .threadBound = true});
    r.AddSystem<&WriteInputMessages>(Writer("SDLWindowWriteInput", options, In::kInputSet));
    r.AddSystem<&WriteWindowMessages>(Writer("SDLWindowWriteWindow", options, Win::kWindowSet));
    Kernel::SystemOptions apply;
    apply.name = "SDLWindowApplyRequests";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.applyStage;
    apply.mounts.push_back(std::move(mount));
    r.AddSystem<&ApplyWindowRequests>(std::move(apply));
    // Put on the thread that prepares the World (the runner's, the main thread; SDL-4). A new or replaced World's
    // first round writes the present state.
    Service* service = service_.get();
    setup.Prepare([service](Kernel::World& world) -> Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(SDLWindowHandle{service, true, true});
        return {};
    });
}

} // namespace GYO::Plugins::SDLWindow
