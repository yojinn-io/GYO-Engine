#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#if __has_include("RetroFPS/Pvp/MovementTraceWriter.hpp")
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#define PVP_PROBE_MOVEMENT_TRACE 1
#endif
#include "platform/sdl/SdlPlatform.hpp"
#include "render/Renderer.hpp"
#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "acceptance_protocol.hpp"
#include "timer_baseline.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Control = Engine::Runtime::RuntimeControl;
struct Options {
    std::filesystem::path assetRoot;
    std::filesystem::path output;
    std::string gateway{"127.0.0.1:8080"};
    std::string role{"create"};
    std::string gpu{"auto"};
    double duration{5};
    bool move{};
    bool latency{};
    bool latencyShort{};
    bool combat{};
    bool phaseStalls{};
    bool nativeWindow{};
    bool playerShort{};
    bool playerCapture{};
    bool actionShort{};
    bool actionCapture{};
    unsigned events{200};
    double fps{60};
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
double Seconds(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double>(a - b).count();
}
Options Parse(int argc, char* argv[]) {
    Options options;
    bool explicitDuration{};
    bool explicitEvents{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--move") { options.move = true; continue; }
        if (argument == "--latency") { options.latency = true; continue; }
        if (argument == "--latency-short") { options.latencyShort = true; continue; }
        if (argument == "--combat") { options.combat = true; continue; }
        if (argument == "--phase-stalls") { options.phaseStalls = true; continue; }
        if (argument == "--native-window") { options.nativeWindow = true; continue; }
        if (argument == "--player-short") { options.playerShort = true; continue; }
        if (argument == "--player-capture") { options.playerCapture = true; continue; }
        if (argument == "--action-short") { options.actionShort = true; continue; }
        if (argument == "--action-capture") { options.actionCapture = true; continue; }
        Require(i + 1 < argc, "Missing value for " + std::string(argument));
        const std::string value = argv[++i];
        if (argument == "--arena-root") options.assetRoot = value;
        else if (argument == "--gateway") options.gateway = value;
        else if (argument == "--role") options.role = value;
        else if (argument == "--duration") { options.duration = std::stod(value); explicitDuration = true; }
        else if (argument == "--events") { options.events = static_cast<unsigned>(std::stoul(value)); explicitEvents = true; }
        else if (argument == "--fps") options.fps = std::stod(value);
        else if (argument == "--output") options.output = value;
        else if (argument == "--gpu-driver") options.gpu = value;
        else throw std::runtime_error("Unknown option: " + std::string(argument));
    }
    Require(!options.assetRoot.empty(), "--arena-root must name the deployed PvP assets directory");
    Require(!options.output.empty(), "--output must name the capture directory");
    Require(options.role == "create" || options.role == "join", "--role must be create or join");
    Require(unsigned(options.latency) + unsigned(options.latencyShort) + unsigned(options.phaseStalls) +
        unsigned(options.nativeWindow) +
        unsigned(options.playerShort) + unsigned(options.playerCapture) +
        unsigned(options.actionShort) + unsigned(options.actionCapture) <= 1,
        "Choose one explicit probe mode");
    Require(!options.combat || options.latency || options.latencyShort, "--combat requires an explicit latency mode");
    if (options.latency && !explicitDuration) options.duration = 120;
    if (options.latencyShort && !explicitDuration) options.duration = 16;
    if (options.latencyShort && !explicitEvents) options.events = 20;
    if (options.nativeWindow && !explicitDuration) options.duration = 180;
    if ((options.playerShort || options.playerCapture) && !explicitDuration) options.duration = 12;
    if ((options.actionShort || options.actionCapture) && !explicitDuration) options.duration = 18;
    Require(std::isfinite(options.duration) && options.duration >= (options.latency ? 120 : 2) && options.duration <= 3600,
        "--duration must be 120..3600 seconds for latency, 2..3600 otherwise");
    Require(options.events >= (options.latencyShort ? 20U : 200U) && options.events <= 6000,
        "--events must be 200..6000 (20..6000 for explicit short latency)");
    Require(!(options.latency || options.latencyShort) || options.duration / options.events >= .6,
        "Each latency event needs at least 600 ms");
    Require(!options.latencyShort || (options.duration >= 16 && options.duration <= 25),
        "Short latency duration must be 16..25 seconds");
    Require(!(options.playerShort || options.playerCapture) || options.duration == 12,
        "Player presentation probes use an explicit bounded 12-second schedule");
    Require(!(options.actionShort || options.actionCapture) || (options.duration >= 18 && options.duration <= 25),
        "Action probes need 18..25 seconds for their fixed schedule, the target respawn and the rejoin");
    Require(std::isfinite(options.fps) && options.fps >= 30 && options.fps <= 144, "--fps must be 30..144");
    return options;
}

void SaveCapture(Engine::Render::SceneCapture& capture, const std::filesystem::path& destination) {
    Require(capture.width > 0 && capture.height > 0 &&
        capture.rgba8.size() == static_cast<std::size_t>(capture.width) * capture.height * 4,
        "Renderer returned an invalid capture");
    SDL_Surface* surface = SDL_CreateSurfaceFrom(static_cast<int>(capture.width),
        static_cast<int>(capture.height), SDL_PIXELFORMAT_RGBA32, capture.rgba8.data(),
        static_cast<int>(capture.width * 4));
    Require(surface != nullptr, SDL_GetError());
    const bool saved = SDL_SaveBMP(surface, destination.string().c_str());
    SDL_DestroySurface(surface);
    Require(saved, SDL_GetError());
}

// Scripted events enter the same SDL adapter and 60 Hz sampling path as a user.
// Keep each probe in a separate process: SdlPlatform drains SDL's global queue.
void PushMovement(SDL_Window* window, bool held, SDL_Scancode direction = SDL_SCANCODE_W) {
    const auto id = SDL_GetWindowID(window);
    if (held) {
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        focus.window.windowID = id;
        Require(SDL_PushEvent(&focus), "Could not enqueue probe focus event");
        if (!SDL_GetWindowRelativeMouseMode(window)) {
            SDL_Event click{};
            click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            click.button.windowID = id;
            click.button.button = SDL_BUTTON_LEFT;
            click.button.down = true;
            Require(SDL_PushEvent(&click), "Could not enqueue probe capture click");
            click.type = SDL_EVENT_MOUSE_BUTTON_UP;
            click.button.down = false;
            Require(SDL_PushEvent(&click), "Could not enqueue probe capture release");
        }
    }
    SDL_Event keyboard{};
    keyboard.type = held ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    keyboard.key.windowID = id;
    keyboard.key.scancode = direction;
    keyboard.key.key = direction == SDL_SCANCODE_W ? SDLK_W : SDLK_S;
    keyboard.key.down = held;
    keyboard.key.repeat = false;
    Require(SDL_PushEvent(&keyboard), "Could not enqueue probe movement event");
}

void PushKey(SDL_Window* window, SDL_Scancode key, bool held) {
    SDL_Event event{};
    event.type = held ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.scancode = key;
    event.key.down = held;
    Require(SDL_PushEvent(&event), "Could not enqueue Lobby key");
}

const fps::pvp::PlayerState* FindSelf(const fps::pvp::ClientConnectionState& state) {
    if (!state.snapshot) return nullptr;
    const auto found = std::find_if(state.snapshot->players.begin(), state.snapshot->players.end(),
        [&](const auto& player) { return player.playerId == state.playerId; });
    return found == state.snapshot->players.end() ? nullptr : &*found;
}

template<class Observation>
std::uint64_t EpochOf(const Observation& observation) {
    if constexpr (requires { observation.movementEpoch; }) return observation.movementEpoch;
    return 1; // The same probe also measures the saved v2 baseline.
}

template<class Presented>
void WriteLatencyDiagnostics(std::ostream& stream, const Presented& presented) {
    const auto& local = presented.local;
    if constexpr (requires { local.previousCommand; local.currentCommand; local.interpolationAlpha; })
        stream << ',' << local.previousCommand << ',' << local.currentCommand << ',' << local.interpolationAlpha;
    else stream << ",0,0,0";
    if constexpr (requires { presented.remote->lowerTick; presented.remote->lowerResolvedCommand; }) {
        if (const auto& remote = presented.remote) {
            stream << ',' << remote->lowerTick << ',' << remote->upperTick << ',' << remote->presentationTick
                   << ',' << remote->lowerResolvedCommand << ',' << remote->upperResolvedCommand
                   << ',' << remote->interpolationAlpha << ',' << remote->latestReceiveAgeSeconds
                   << ',' << remote->holdSeconds << ',' << remote->totalHoldSeconds
                   << ',' << remote->holdCount << ',' << remote->gapCount << ',' << remote->historySize
                   << ',' << remote->ingressHistoryDrops << ',' << remote->phaseReanchors << ',' << remote->holding;
        } else for (int field = 0; field < 15; ++field) stream << ",0";
    } else for (int field = 0; field < 15; ++field) stream << ",0";
    if constexpr (requires { presented.connectionGeneration; }) stream << ',' << presented.connectionGeneration;
    else stream << ",0";
    if constexpr (requires { presented.remote->missingFutureSnapshot; })
        stream << ',' << (presented.remote && presented.remote->missingFutureSnapshot);
    else stream << ",0";
}

void WriteLatencyPresentation(std::ostream& stream,
    const fps::pvp::PresentedMovementObservation& presented) {
    const auto& local = presented.local;
    const auto& remote = presented.remote;
    stream << presented.hostSteadySeconds << ',' << presented.localPlayerId << ','
           << local.renderPosition.x << ',' << local.renderPosition.z << ','
           << (remote ? remote->playerId : 0) << ',' << (remote ? remote->renderPosition.x : 0) << ','
           << (remote ? remote->renderPosition.z : 0) << ',' << presented.frameId << ','
           << EpochOf(local) << ',' << (remote ? EpochOf(*remote) : 0) << ','
           << local.authorityTick << ',' << local.lastResolvedCommand << ',' << local.latestCommand << ','
           << local.pendingCommands << ',' << local.correctionOffset.x << ',' << local.correctionOffset.z << ','
           << presented.skippedFrames;
    WriteLatencyDiagnostics(stream, presented);
    stream << ',' << local.predictedPosition.x << ',' << local.predictedPosition.z << '\n';
}

#include "platform_fingerprint.hpp"
#include "gui_input.hpp"
#include "player_short.hpp"
#include "action_short.hpp"
#include "native_window.hpp"
#include "combat_latency.hpp"
#include "start_phase_record.hpp"

// Latency-mode window evidence. SDL calls event watchers synchronously while it
// queues an event, so OS window events are counted without a second drain of
// the product's queue; the callback only increments counters. Probe-pushed
// focus (PushMovement) is counted separately via the synthetic flag. Moved and
// resized are counted because the product releases its pointer on them. The
// watcher is added after placement: the placement's own MOVED (queued while
// SDL_SyncWindow pumps events) and the window's creation events are never
// counted; a MOVED that arrives later still lands in window_os_events, but only
// counts as interference inside the measurement window.
struct LatencyWindow {
    static constexpr std::array<std::pair<Uint32, const char*>, 8> Kinds{{
        {SDL_EVENT_WINDOW_OCCLUDED, "occluded"}, {SDL_EVENT_WINDOW_EXPOSED, "exposed"},
        {SDL_EVENT_WINDOW_HIDDEN, "hidden"}, {SDL_EVENT_WINDOW_MINIMIZED, "minimized"},
        {SDL_EVENT_WINDOW_FOCUS_GAINED, "focus_gained"}, {SDL_EVENT_WINDOW_FOCUS_LOST, "focus_lost"},
        {SDL_EVENT_WINDOW_MOVED, "moved"}, {SDL_EVENT_WINDOW_RESIZED, "resized"}}};
    SDL_Window* window{};
    SDL_WindowID id{};
    std::string placement{"platform_default"};
    std::string placementError;
    std::optional<SDL_Rect> usable;
    std::optional<std::array<int, 2>> requested, afterSync;
    std::optional<bool> synced;
    std::optional<SDL_WindowFlags> startFlags;
    std::atomic<bool> measuring{}, synthetic{};
    std::array<std::atomic<unsigned>, Kinds.size()> total{}, during{};
    std::atomic<unsigned> syntheticEvents{};
    // The mover raises itself once both players are present. That raise's own
    // asynchronous completion (focus gained by the mover, focus lost by the
    // observer) may be delivered late on some platforms; the first such event
    // after the raise is the probe's own action, so it is recorded apart and is
    // never interference. A completion is expected only when the focus state at
    // the raise says one is due (the mover lacked focus, or the observer had
    // it); otherwise nothing is set aside. Any further focus event still counts.
    const bool raisesItself;
    const Uint32 raiseCompletionType;
    std::atomic<bool> raiseCompletionPending{};
    std::atomic<unsigned> raiseCompletionEvents{};
    LatencyWindow(SDL_Window* native, bool mover) : window(native), id(SDL_GetWindowID(native)), raisesItself(mover),
        raiseCompletionType(mover ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST) {
        // Both probes otherwise open at one platform-default spot and the mover
        // raises itself over the observer: macOS throttles a fully occluded
        // window and a Wayland compositor may report it OCCLUDED. Opposite
        // corners of the usable bounds keep part of each visible on every
        // platform that lets a program position its windows; the 1280x720
        // render size is unchanged. SDL positions name the client area, and the
        // reported borders (zero on macOS) keep the title bar on screen.
        SDL_Rect bounds{};
        const auto display = SDL_GetDisplayForWindow(window);
        int width{}, height{}, top{}, left{}, bottom{}, right{};
        if (display && SDL_GetDisplayUsableBounds(display, &bounds) && SDL_GetWindowSize(window, &width, &height)) {
            SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right); // Zeros if unsupported.
            const int x = mover ? bounds.x + left : std::max(bounds.x + left, bounds.x + bounds.w - right - width);
            const int y = mover ? bounds.y + top : std::max(bounds.y + top, bounds.y + bounds.h - bottom - height);
            usable = bounds;
            if (SDL_SetWindowPosition(window, x, y)) {
                synced = SDL_SyncWindow(window); // False: timed out before the requested state.
                int syncedX{}, syncedY{};
                if (SDL_GetWindowPosition(window, &syncedX, &syncedY)) afterSync = std::array{syncedX, syncedY};
                requested = std::array{x, y};
                placement = mover ? "usable_bounds_top_left" : "usable_bounds_bottom_right";
            } else {
                // Wayland does not let a client place its window: keep the default
                // position and name why, so an overlap is attributable.
                const char* reason = SDL_GetError();
                placementError = reason && *reason ? reason : "SDL_SetWindowPosition failed";
            }
        }
        Require(SDL_AddEventWatch(&Watch, this), SDL_GetError());
    }
    ~LatencyWindow() { SDL_RemoveEventWatch(&Watch, this); }
    void ExpectRaiseCompletion() {
        const bool focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        raiseCompletionPending.store(raisesItself ? !focused : focused, std::memory_order_relaxed);
    }
    LatencyWindow(const LatencyWindow&) = delete;
    LatencyWindow& operator=(const LatencyWindow&) = delete;
    static bool SDLCALL Watch(void* self, SDL_Event* event) {
        auto& evidence = *static_cast<LatencyWindow*>(self);
        if (event->type < SDL_EVENT_WINDOW_FIRST || event->type > SDL_EVENT_WINDOW_LAST || event->window.windowID != evidence.id) return true;
        if (event->type == evidence.raiseCompletionType && !evidence.synthetic.load(std::memory_order_relaxed) &&
            evidence.raiseCompletionPending.exchange(false, std::memory_order_relaxed)) {
            evidence.raiseCompletionEvents.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        for (std::size_t kind = 0; kind < Kinds.size(); ++kind) {
            if (event->type != Kinds[kind].first) continue;
            if (evidence.synthetic.load(std::memory_order_relaxed)) evidence.syntheticEvents.fetch_add(1, std::memory_order_relaxed);
            else {
                evidence.total[kind].fetch_add(1, std::memory_order_relaxed);
                if (evidence.measuring.load(std::memory_order_relaxed)) evidence.during[kind].fetch_add(1, std::memory_order_relaxed);
            }
            break;
        }
        return true;
    }
    void Report(std::ostream& report) const {
        int x{}, y{}, width{}, height{}, top{}, left{}, bottom{}, right{};
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
        const auto flags = [](std::optional<SDL_WindowFlags> value) -> std::string {
            if (!value) return "unobserved";
            return "occluded:" + std::to_string(int(bool(*value & SDL_WINDOW_OCCLUDED))) + ",hidden:" +
                std::to_string(int(bool(*value & SDL_WINDOW_HIDDEN))) + ",minimized:" +
                std::to_string(int(bool(*value & SDL_WINDOW_MINIMIZED))) + ",input_focus:" +
                std::to_string(int(bool(*value & SDL_WINDOW_INPUT_FOCUS)));
        };
        const auto counts = [](const auto& values) {
            std::string text;
            for (std::size_t kind = 0; kind < Kinds.size(); ++kind)
                text += (kind ? "," : "") + std::string(Kinds[kind].second) + ':' + std::to_string(values[kind].load());
            return text;
        };
        report << "window_placement=" << placement;
        if (!placementError.empty()) report << "\nwindow_placement_error=" << placementError;
        report << "\nwindow_usable_bounds=";
        if (usable) report << usable->x << ',' << usable->y << ',' << usable->w << ',' << usable->h; else report << "unavailable";
        report << "\nwindow_requested_position=";
        if (requested) report << (*requested)[0] << ',' << (*requested)[1]; else report << "none";
        report << "\nwindow_sync=" << (!synced ? "not_requested" : *synced ? "succeeded" : "timed_out")
               << "\nwindow_position_after_sync=";
        if (afterSync) report << (*afterSync)[0] << ',' << (*afterSync)[1]; else report << "none";
        report << "\nwindow_final_position=" << x << ',' << y << "\nwindow_size=" << width << ',' << height
               << "\nwindow_borders=" << top << ',' << left << ',' << bottom << ',' << right
               << "\nwindow_flags_at_measurement_start=" << flags(startFlags)
               << "\nwindow_flags_at_end=" << flags(SDL_GetWindowFlags(window))
               << "\nwindow_os_events=" << counts(total) << "\nwindow_os_events_during_measurement=" << counts(during)
               << "\nwindow_synthetic_events=" << syntheticEvents.load()
               << "\nwindow_raise_completion_events=" << raiseCompletionEvents.load() << '\n';
    }
};

// Long latency runs use ordinary input/connection APIs and successful renderer
// submissions. No readback, capture, production test switch or clock sync exists.
void RunLatency(const Options& options) {
    struct LatencyEvent {
        unsigned id{};
        double actualStartSeconds{};
        fps::pvp::PlayerId playerId{};
        std::uint64_t movementEpoch{}, afterSequence{};
        float originX{}, originZ{};
        double directionX{}, directionZ{};
    };
    std::filesystem::create_directories(options.output);
#ifdef PVP_PROBE_MOVEMENT_TRACE
    fps::pvp::MovementTraceWriter traceWriter(options.output / (options.role + "-commands.jsonl"));
#endif
    fps::pvp::PvpApplication application;
    CombatLatencyEvidence combat(options);
    std::string error;
    Require(application.InitializeContent(options.assetRoot, error), error);
    fps::pvp::PvpApplicationOptions graphics;
    graphics.title = "Object FPS PVP latency / " + options.role;
    graphics.gateway = options.gateway;
    graphics.gpuDriver = options.gpu;
    Require(application.InitializeGraphics(graphics, error), error);
    auto& connection = application.Connection();
    const bool mover = options.role == "create";
    // Placed before connecting: its MOVED event is drained while still in Lobby.
    LatencyWindow windowEvidence(application.Platform().NativeWindow(), mover);
    const auto platform = PlatformFingerprint::Capture(application, application.Platform().NativeWindow());
    StartPhaseRecorder startPhase;
    if (mover) connection.CreateAndJoin(options.gateway);
    else connection.Refresh(options.gateway);
    bool joined = mover;
    const auto started = Clock::now();
    auto previous = started, refreshed = started;
    std::optional<Clock::time_point> togetherSince, measurementStart;
    std::optional<SDL_Scancode> held;
    std::optional<unsigned> activeEvent;
    std::uint64_t frameIndex{}, presentedCount{}, firstTick{}, finalTick{};
    fps::pvp::PlayerId playerId{};
    double maximumFrameGap{};
    const double period = options.duration / options.events;
    const auto finishedPath = options.output / "latency-finished.txt";
    if (mover) std::filesystem::remove(finishedPath);
    // Reserve for the full run deadline, with explicit scheduling slack. CSV
    // formatting and writes happen only after measurement and trailing drain;
    // no observation is discarded if the bound is unexpectedly exhausted.
    const auto maximumPresentations = static_cast<std::size_t>(
        std::ceil((options.duration + 45) * options.fps)) + 1024;
    std::vector<fps::pvp::PresentedMovementObservation> presentationSamples;
    presentationSamples.reserve(maximumPresentations);
    std::vector<LatencyEvent> eventSamples;
    if (mover) eventSamples.reserve(options.events);
    for (;;) {
        const auto now = Clock::now();
        if (!mover && togetherSince && std::filesystem::exists(finishedPath)) break;
        Require(Seconds(now, started) < options.duration + 45, "Timed out during long GUI latency run");
        const auto state = connection.State();
        Require(state.error.empty(), "GUI latency connection failed: " + state.error);
        if (!joined && state.phase == fps::pvp::ConnectionPhase::Lobby) {
            if (!state.rooms.empty()) { connection.Join(options.gateway, state.rooms.front().id); joined = true; }
            else if (Seconds(now, refreshed) >= .5) { connection.Refresh(options.gateway); refreshed = now; }
        }
        const auto* self = FindSelf(state);
        const bool both = state.phase == fps::pvp::ConnectionPhase::Playing && self && state.snapshot->players.size() == 2;
        if (both && !togetherSince) {
            togetherSince = now;
            firstTick = state.snapshot->tick;
            playerId = state.playerId;
            measurementStart = now + std::chrono::seconds(2);
            windowEvidence.ExpectRaiseCompletion();
            if (mover) {
                // Write the complete denominator before dispatching any event.
                std::ofstream plan(options.output / "latency-plan.csv");
                Require(bool(plan), "Cannot create predeclared latency event plan");
                plan << "event_id,scheduled_host_seconds,end_host_seconds,sign,threshold_units,duration_seconds,event_count\n" << std::setprecision(17);
                const double epoch = std::chrono::duration<double>(measurementStart->time_since_epoch()).count();
                for (unsigned event = 0; event < options.events; ++event)
                    plan << event << ',' << epoch + event * period << ',' << epoch + (event + 1) * period
                         << ',' << (event % 2 ? -1 : 1) << ",0.25," << options.duration << ',' << options.events << '\n';
                plan.flush();
                Require(bool(plan), "Cannot write predeclared latency event plan");
                SDL_RaiseWindow(application.Platform().NativeWindow());
            }
        }
        if (togetherSince) Require(both, "A GUI client left during latency measurement");
        if (both) finalTick = state.snapshot->tick;
        const double elapsed = measurementStart ? Seconds(now, *measurementStart) : -1;
        windowEvidence.measuring.store(elapsed >= 0 && elapsed <= options.duration + 2, std::memory_order_relaxed);
        if (elapsed >= 0 && !windowEvidence.startFlags) windowEvidence.startFlags = SDL_GetWindowFlags(application.Platform().NativeWindow());
        if (mover && elapsed >= 0) {
            if (elapsed >= options.duration + 2) break;
            const auto event = static_cast<unsigned>(elapsed / period);
            const double phase = elapsed - event * period;
            if (held && (event != activeEvent || phase >= .3 || event >= options.events)) {
                PushMovement(application.Platform().NativeWindow(), false, *held);
                held.reset();
            }
            if (event < options.events && activeEvent != event) {
                activeEvent = event;
                // A stalled producer never replays missed scripted events.
                // Their predeclared entries remain unmatched in the denominator.
                if (phase < .3) {
                    const auto& local = application.LocalMovement();
                    const auto sign = event % 2 ? -1.0 : 1.0;
                    if (eventSamples.size() == options.events)
                        throw std::runtime_error("Latency event observation buffer overflow");
                    eventSamples.push_back({event, std::chrono::duration<double>(now.time_since_epoch()).count(),
                        state.playerId, EpochOf(local), local.latestCommand, local.renderPosition.x, local.renderPosition.z,
                        std::sin(self->yaw) * sign, std::cos(self->yaw) * sign});
                    held = event % 2 ? SDL_SCANCODE_S : SDL_SCANCODE_W;
                    windowEvidence.synthetic.store(true, std::memory_order_relaxed);
                    PushMovement(application.Platform().NativeWindow(), true, *held);
                    windowEvidence.synthetic.store(false, std::memory_order_relaxed);
                }
            }
        }
        const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
        const auto beforeWeapon = application.WeaponFeedback();
        if (options.combat) combat.BeforeUpdate(elapsed, application);
        previous = now;
        if (elapsed >= 0 && elapsed <= options.duration) maximumFrameGap = std::max(maximumFrameGap, frame.deltaSeconds);
        Require(application.ProcessEvents(frame) == Control::Continue, "Latency window was closed");
        Require(application.Update(frame) == Control::Continue, application.LastError());
        {
            // `state` predates this Update, which may have joined and activated
            // the prediction while `state` still had no player id. Pair the
            // Client observation with the connection state after Update.
            const auto updated = connection.State();
            startPhase.Observe(frame.frameIndex, std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(),
                updated.playerId, FindSelf(updated), application.LocalMovement());
        }
        if (options.combat) combat.AfterUpdate(frame.frameIndex, beforeWeapon, application);
        Require(application.Render(frame) == Control::Continue, application.LastError());
        if (options.combat) combat.AfterRender(frame.frameIndex, application);
        if (const auto& submitted = application.PresentedMovement()) {
            Require(submitted->frameId == frame.frameIndex, "Stale Presented observation");
            if (presentationSamples.size() == maximumPresentations)
                throw std::runtime_error("Latency presentation observation buffer overflow");
            presentationSamples.push_back(*submitted);
            ++presentedCount;
        }
        // Independent measurement pacing; never try to replay missed frames.
        const auto remaining = 1.0 / options.fps - Seconds(Clock::now(), now);
        if (remaining > 0) SDL_DelayNS(static_cast<Uint64>(remaining * 1e9));
    }
    if (held) PushMovement(application.Platform().NativeWindow(), false, *held);
    if (options.combat) combat.Save(application);
    std::ofstream presentation(options.output / (options.role + "-presentation.csv"));
    Require(bool(presentation), "Cannot create latency presentation trace");
    presentation << "host_steady_seconds,local_id,local_x,local_z,remote_id,remote_x,remote_z,frame_id,local_epoch,remote_epoch,authority_tick,resolved,latest,pending,correction_x,correction_z,skipped_frames,local_previous,local_current,local_alpha,remote_lower_tick,remote_upper_tick,remote_tick,remote_lower_command,remote_upper_command,remote_alpha,remote_receive_age,remote_hold_seconds,remote_total_hold_seconds,remote_hold_count,remote_gap_count,remote_history_size,remote_ingress_drops,remote_phase_reanchors,remote_holding,connection_generation,remote_missing_future_snapshot,local_predicted_x,local_predicted_z\n"
                 << std::setprecision(17);
    for (const auto& sample : presentationSamples) WriteLatencyPresentation(presentation, sample);
    presentation.flush();
    Require(bool(presentation), "Cannot write latency presentation trace");
    std::ofstream phase(options.output / (options.role + "-start-phase.json"));
    auto phaseRecord = startPhase.Json();
    phaseRecord["role"] = options.role; phaseRecord["player_id"] = playerId;
    phase << phaseRecord.dump(2) << '\n';
    phase.flush(); Require(bool(phase), "Cannot write start-phase record");
    if (mover) {
        std::ofstream events(options.output / "latency-events.csv");
        Require(bool(events), "Cannot create latency event trace");
        events << "event_id,actual_start_host_seconds,local_id,movement_epoch,after_sequence,origin_x,origin_z,direction_x,direction_z\n" << std::setprecision(17);
        for (const auto& event : eventSamples)
            events << event.id << ',' << event.actualStartSeconds << ',' << event.playerId << ','
                   << event.movementEpoch << ',' << event.afterSequence << ',' << event.originX << ',' << event.originZ
                   << ',' << event.directionX << ',' << event.directionZ << '\n';
        events.flush(); Require(bool(events), "Cannot write latency events");
    }
    std::ofstream report(options.output / (options.role + "-report.txt"));
    Require(bool(report), "Cannot create latency report");
    report << "role=" << options.role << "\nplayer_id=" << playerId
           << "\nmode=" << (options.latencyShort ? "latency-short" : "latency")
           << "\nplanned_events=" << options.events << "\nmeasurement_seconds=" << options.duration
           << "\nnominal_fps=" << options.fps << "\npresentation_frames=" << presentedCount
           << "\npresentation_buffer_capacity=" << maximumPresentations << "\nbuffered_event_count=" << eventSamples.size()
           << "\nobservation_csv_write_phase=after_measurement_and_trailing_drain\nbuffer_overflow=false"
           << "\nmaximum_frame_gap_seconds=" << maximumFrameGap
           << "\nskipped_frames=" << application.SkippedPresentationFrames()
           << "\nfirst_authority_tick=" << firstTick << "\nfinal_authority_tick=" << finalTick
           << "\ncapture=none\nclock_scope=same-host monotonic submission timestamps; not physical scanout\n"
           << "start_phase_record=" << options.role << "-start-phase.json\n";
    platform.Report(report);
    windowEvidence.Report(report);
    report.flush();
    Require(bool(report), "Cannot write latency report");
    if (mover) {
        std::ofstream finished(finishedPath);
        finished << "Measurement and two-second trailing drain complete\n";
        finished.flush();
        Require(bool(finished), "Cannot signal observer completion");
    }
    connection.Leave();
    const auto leaveDeadline = Clock::now() + std::chrono::seconds(5);
    while (connection.State().phase != fps::pvp::ConnectionPhase::Lobby) {
        Require(Clock::now() < leaveDeadline, "Latency client did not leave cleanly");
        SDL_Delay(2);
    }
    Require(connection.State().error.empty(), "Latency leave cleanup failed");
#ifdef PVP_PROBE_MOVEMENT_TRACE
    traceWriter.Finish();
    Require(traceWriter.Good(), "Movement diagnostic trace lost events or could not be written");
#endif
    std::cout << options.role << ": latency trace completed with " << presentedCount << " submitted frames\n";
}

// Fault injection stays in this acceptance executable. The production app sees
// ordinary event/update/render calls and network snapshots, never a test flag.
void RunPhaseStalls(const Options& options) {
    struct Fault {
        bool beforeUpdate{};
        unsigned milliseconds{};
        std::optional<Clock::time_point> released, healthySince;
        std::uint64_t epoch{};
        std::size_t maxPending{};
        std::uint32_t maxServerPending{};
        double recoverySeconds{};
        // Lower bound of the elapsed time the product itself sampled across the
        // stalled frame: SDL_Delay can oversleep, so the nominal milliseconds
        // do not say whether the product really saw a stall of 100 ms or more.
        double productElapsedLowerBound{};
        bool recovered{};
    };
    std::array<Fault, 6> faults{{{true, 64}, {false, 64}, {true, 83},
                               {false, 83}, {true, 250}, {false, 250}}};
    std::filesystem::create_directories(options.output);
#ifdef PVP_PROBE_MOVEMENT_TRACE
    fps::pvp::MovementTraceWriter traceWriter(options.output / (options.role + "-commands.jsonl"));
#endif
    fps::pvp::PvpApplication application;
    std::string error;
    Require(application.InitializeContent(options.assetRoot, error), error);
    fps::pvp::PvpApplicationOptions graphics;
    graphics.title = "Object FPS PVP phase stalls / " + options.role;
    graphics.gateway = options.gateway;
    graphics.gpuDriver = options.gpu;
    graphics.vsync = options.fps <= 60;
    Require(application.InitializeGraphics(graphics, error), error);
    auto& connection = application.Connection();
    const bool mover = options.role == "create";
    if (mover) connection.CreateAndJoin(options.gateway); else connection.Refresh(options.gateway);
    bool joined = mover;
    const auto started = Clock::now();
    auto previous = started, refreshed = started;
    std::optional<Clock::time_point> togetherSince, previousUpdate, previousUpdateStart;
    std::optional<fps::pvp::LocalMovementObservation> previousMovement;
    std::optional<SDL_Scancode> held;
    unsigned nextFault{};
    std::optional<unsigned> activeFault;
    std::uint64_t frameIndex{}, presentedCount{}, measuredFrames{};
    double maximumFrameGap{};
    const auto finishedPath = options.output / "phase-stalls-finished.txt";
    if (mover) std::filesystem::remove(finishedPath);
    std::ofstream samples(options.output / (options.role + "-phase-stalls.csv"));
    Require(bool(samples), "Cannot create phase stall observations");
    samples << "host_steady_seconds,frame_id,frame_seconds,update_interval_seconds,epoch,resolved,latest,pending,server_pending,frozen,presented,case_id\n"
            << std::setprecision(17);
    std::ofstream report(options.output / (options.role + "-phase-stalls-report.txt"));
    Require(bool(report), "Cannot create phase stall report");
    for (;;) {
        const auto now = Clock::now();
        if (!mover && togetherSince && std::filesystem::exists(finishedPath)) break;
        Require(Seconds(now, started) < 60, "Timed out during GUI phase stall acceptance");
        const auto state = connection.State();
        Require(state.error.empty(), "GUI phase stall connection failed: " + state.error);
        if (!joined && state.phase == fps::pvp::ConnectionPhase::Lobby) {
            if (!state.rooms.empty()) { connection.Join(options.gateway, state.rooms.front().id); joined = true; }
            else if (Seconds(now, refreshed) >= .5) { connection.Refresh(options.gateway); refreshed = now; }
        }
        const auto* self = FindSelf(state);
        const bool both = state.phase == fps::pvp::ConnectionPhase::Playing && self && state.snapshot->players.size() == 2;
        if (both && !togetherSince) togetherSince = now;
        if (togetherSince) Require(both, "A client left during phase stall acceptance");
        const double together = togetherSince ? Seconds(now, *togetherSince) : 0;
        if (mover && together >= 21) break;
        if (mover && together >= 1) {
            // Alternate non-neutral inputs inside the same small area.
            const auto direction = static_cast<int>(together * 2) % 2 ? SDL_SCANCODE_S : SDL_SCANCODE_W;
            if (!held || *held != direction) {
                if (held) PushMovement(application.Platform().NativeWindow(), false, *held);
                held = direction;
                PushMovement(application.Platform().NativeWindow(), true, direction);
            }
        }
        const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
        previous = now;
        if (both) { ++measuredFrames; maximumFrameGap = std::max(maximumFrameGap, frame.deltaSeconds); }
        Require(application.ProcessEvents(frame) == Control::Continue, "Phase stall window was closed");
        Fault* injecting = nullptr;
        if (mover && nextFault < faults.size() && together >= 2 + nextFault * 3) {
            if (activeFault) Require(faults[*activeFault].recovered, "Prior phase stall failed to recover");
            activeFault = nextFault++;
            injecting = &faults[*activeFault];
            injecting->epoch = application.LocalMovement().movementEpoch;
        }
        if (injecting && injecting->beforeUpdate) {
            SDL_Delay(injecting->milliseconds);
            injecting->released = Clock::now();
        }
        const auto updateStarted = Clock::now();
        const std::optional<double> previousLowerBound = previousUpdate ?
            std::optional<double>(Seconds(updateStarted, *previousUpdate)) : std::nullopt;
        Require(application.Update(frame) == Control::Continue, application.LastError());
        const auto updateFinished = Clock::now();
        const auto& local = application.LocalMovement();
        const double updateInterval = previousUpdate ? Seconds(updateFinished, *previousUpdate) : 0;
        if (local.active) {
            Require(local.pendingCommands <= fps::pvp::MaxPendingCommands, "Phase stall exceeded pending command capacity");
            if (previousMovement && previousMovement->active &&
                local.movementEpoch == previousMovement->movementEpoch &&
                local.lastResolvedCommand <= previousMovement->latestCommand &&
                local.latestCommand >= previousMovement->latestCommand) {
                // The product samples its own clock inside Update, so its elapsed
                // time since the previous sample is bracketed by this Update's end
                // and the previous Update's start: an upper bound with no timing
                // tolerance. A new authoritative seed is excluded above.
                const auto possibleSteps = static_cast<std::uint64_t>(std::floor(
                    Seconds(updateFinished, *previousUpdateStart) / fps::pvp::MovementTickSeconds)) + 1;
                Require(local.latestCommand - previousMovement->latestCommand <= possibleSteps,
                    "Command generation charged a previous frame's already-covered stall again");
            }
            previousUpdate = updateFinished;
            previousUpdateStart = updateStarted;
            previousMovement = local;
        } else { previousUpdate.reset(); previousUpdateStart.reset(); previousMovement.reset(); }
        if (injecting && !injecting->beforeUpdate) {
            SDL_Delay(injecting->milliseconds);
            injecting->released = Clock::now();
        }
        Require(application.Render(frame) == Control::Continue, application.LastError());
        const bool presented = application.PresentedMovement().has_value();
        presentedCount += presented;
        samples << std::chrono::duration<double>(updateFinished.time_since_epoch()).count() << ','
                << frame.frameIndex << ',' << frame.deltaSeconds << ',' << updateInterval << ','
                << local.movementEpoch << ',' << local.lastResolvedCommand << ',' << local.latestCommand << ','
                << local.pendingCommands << ',' << local.serverPendingCommands << ',' << local.frozen << ','
                << presented << ',' << (activeFault ? static_cast<int>(*activeFault) : -1) << '\n';
        if (mover && activeFault) {
            auto& fault = faults[*activeFault];
            const auto observedAt = Clock::now();
            // The product's sample in this Update and the previous one lie at least
            // as far apart as the previous Update's end and this Update's start.
            if (previousLowerBound) fault.productElapsedLowerBound = std::max(fault.productElapsedLowerBound, *previousLowerBound);
            fault.maxPending = std::max(fault.maxPending, local.pendingCommands);
            fault.maxServerPending = std::max(fault.maxServerPending, local.serverPendingCommands);
            // A stall the product provably saw as 100 ms or more is a long stall,
            // whatever was requested; every shorter one must keep its epoch.
            if (fault.milliseconds < 100 && fault.productElapsedLowerBound < .1)
                Require(local.movementEpoch == fault.epoch,
                    "A short GUI phase stall produced persistent backlog/epoch reset");
            const bool healthy = local.active && !local.frozen && local.pendingCommands < fps::pvp::MaxPendingCommands &&
                local.serverPendingCommands <= 3 && application.RemoteMovement() &&
                application.RemoteMovement()->latestReceiveAgeSeconds < .1;
            if (healthy) {
                if (!fault.healthySince) fault.healthySince = observedAt;
                if (Seconds(observedAt, *fault.healthySince) >= .25 && !fault.recovered) {
                    fault.recoverySeconds = Seconds(*fault.healthySince, *fault.released);
                    Require(fault.recoverySeconds <= 1.5, "Phase stall recovery started after 1.5 seconds");
                    fault.recovered = true;
                }
            } else fault.healthySince.reset();
            Require(fault.recovered || Seconds(observedAt, *fault.released) <= 1.75,
                "Phase stall did not regain a stable bounded command queue");
            if (fault.recovered && Seconds(observedAt, *fault.released) >= 1.75)
                Require(healthy, "Command queue accumulated again after phase stall recovery");
        }
        const auto remaining = 1.0 / options.fps - Seconds(Clock::now(), now);
        if (remaining > 0) SDL_DelayNS(static_cast<Uint64>(remaining * 1e9));
    }
    if (held) PushMovement(application.Platform().NativeWindow(), false, *held);
    samples.flush(); Require(bool(samples), "Cannot write phase stall observations");
    if (mover) {
        Require(nextFault == faults.size(), "Not all predeclared phase stalls ran");
        for (std::size_t index = 0; index < faults.size(); ++index) {
            const auto& fault = faults[index];
            Require(fault.recovered, "Final phase stall did not recover");
            report << "case=" << index << ",phase=" << (fault.beforeUpdate ? "after-events-before-update" : "after-update-before-render")
                   << ",stall_ms=" << fault.milliseconds
                   << ",product_elapsed_lower_bound_ms=" << fault.productElapsedLowerBound * 1000
                   << ",recovery_seconds=" << fault.recoverySeconds
                   << ",max_pending=" << fault.maxPending << ",max_server_pending=" << fault.maxServerPending << '\n';
        }
    }
    report << "passed=true\nnominal_fps=" << options.fps << "\nmeasured_frames=" << measuredFrames
           << "\npresented_frames=" << presentedCount << "\nmaximum_frame_gap_seconds=" << maximumFrameGap
           << "\nno_gpu_readback=true\n";
    report.flush(); Require(bool(report), "Cannot write phase stall report");
    if (mover) {
        std::ofstream finished(finishedPath); finished << "all six phase stalls recovered\n";
        finished.flush(); Require(bool(finished), "Cannot signal phase stall completion");
    }
    connection.Leave();
    const auto leaveDeadline = Clock::now() + std::chrono::seconds(5);
    while (connection.State().phase != fps::pvp::ConnectionPhase::Lobby) {
        Require(Clock::now() < leaveDeadline, "Phase stall client did not leave cleanly");
        SDL_Delay(2);
    }
    Require(connection.State().error.empty(), "Phase stall leave cleanup failed");
#ifdef PVP_PROBE_MOVEMENT_TRACE
    traceWriter.Finish();
    Require(traceWriter.Good(), "Phase stall diagnostics were incomplete");
#endif
    std::cout << options.role << ": GUI phase stall observations passed\n";
}

void Run(const Options& options) {
    std::filesystem::create_directories(options.output);
    fps::pvp::PvpApplication application;
    std::string error;
    Require(application.InitializeContent(options.assetRoot, error), error);
    fps::pvp::PvpApplicationOptions graphics;
    graphics.title = "Object FPS PVP acceptance / " + options.role;
    graphics.gateway = options.gateway;
    graphics.gpuDriver = options.gpu;
    Require(application.InitializeGraphics(graphics, error), error);

    auto& connection = application.Connection();
    const auto started = Clock::now();
    auto previous = started;
    auto lastRefresh = started;
    std::optional<Clock::time_point> togetherSince;
    std::optional<Clock::time_point> captureFinished;
    std::optional<Clock::time_point> movementStarted;
    std::optional<Engine::Math::Vec3> startingPosition;
    std::optional<Engine::Math::Vec3> pointerReleasePosition;
    std::optional<Clock::time_point> pointerReleasedAt;
    bool submittedJoin = options.role == "create";
    bool beforeCaptured = false;
    bool afterCaptured = false;
    bool movementPressed = false;
    bool movementReleased = false;
    bool testedWindowMove = false, testedCursorToggle = false, testedHeldInputRelease = false;
    std::uint64_t frameIndex = 0;
    std::uint64_t firstTick = 0;
    std::uint64_t finalTick = 0;
    double maximumDisplacement = 0;
    std::size_t movingPresentationFrames{}, movingWithoutSnapshot{}, stableFrames{}, stableMovingFrames{};
    double maximumCorrection{}, maximumRenderStep{};
    double firstWorldUpdateMs{}, firstWorldRenderMs{}, maximumNormalRenderMs{};
    bool sawWorldFrame{};
    std::optional<fps::pvp::LocalMovementObservation> previousObservation;
    std::optional<double> previousPresentedTime;
    std::optional<std::uint64_t> previousPresentedFrame;
    std::ofstream trace(options.output / (options.role + "-movement.csv"));
    Require(bool(trace), "Cannot create local movement trace");
    trace << "seconds,frame_seconds,authority_tick,resolved,latest,pending,predicted_x,predicted_z,render_x,render_z,correction_x,correction_z,frozen\n";
    trace << std::setprecision(10);
    std::ofstream presentation(options.output / (options.role + "-presentation.csv"));
    Require(bool(presentation), "Cannot create presentation latency trace");
    presentation << "host_steady_seconds,local_id,local_x,local_z,remote_id,remote_x,remote_z\n";
    presentation << std::setprecision(17);
    fps::pvp::ClientConnectionState finalState;
    // The Lobby's initial focus is Refresh. Navigate the actual UI actions,
    // rather than calling the network connection's join methods directly.
    std::deque<SDL_Scancode> lobbyKeys;
    if (submittedJoin) lobbyKeys.push_back(SDL_SCANCODE_DOWN);
    lobbyKeys.push_back(SDL_SCANCODE_RETURN);
    std::optional<SDL_Scancode> releasedNextFrame;

    for (;;) {
        const auto now = Clock::now();
        // Give the other process time to finish its capture before leaving.
        if (captureFinished && Seconds(now, *captureFinished) >= 1.0) break;
        Require(Seconds(now, started) < options.duration + 35,
            "Timed out waiting for two GUI clients and GPU capture");
        auto state = connection.State();
        if (!submittedJoin && lobbyKeys.empty() && state.phase == fps::pvp::ConnectionPhase::Lobby) {
            if (!state.rooms.empty()) {
                lobbyKeys = {SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN};
                submittedJoin = true;
            } else if (Seconds(now, lastRefresh) >= .5) {
                lobbyKeys.push_back(SDL_SCANCODE_RETURN);
                lastRefresh = now;
            }
        }
        if (submittedJoin && state.phase == fps::pvp::ConnectionPhase::Lobby && !state.error.empty())
            throw std::runtime_error("Gateway / Match rejected GUI probe: " + state.error);

        const auto* self = FindSelf(state);
        const bool both = state.phase == fps::pvp::ConnectionPhase::Playing && self &&
            state.snapshot->players.size() == 2;
        if (both && !togetherSince) {
            togetherSince = now;
            startingPosition = self->position;
            firstTick = state.snapshot->tick;
        }
        const double together = togetherSince ? Seconds(now, *togetherSince) : 0;
        if (togetherSince && !afterCaptured)
            Require(both, "A GUI client left before the acceptance capture completed");
        if (togetherSince && both && !afterCaptured) {
            finalState = state;
            finalTick = state.snapshot->tick;
            const double dx = self->position.x - startingPosition->x;
            const double dz = self->position.z - startingPosition->z;
            maximumDisplacement = (std::max)(maximumDisplacement, std::sqrt(dx * dx + dz * dz));
        }
        if (options.move && beforeCaptured && !movementPressed && together >= .8) {
            // GPU readback can stall a frame past the original movement window.
            // Measure the hold from the actual injected press, not join time.
            SDL_RaiseWindow(application.Platform().NativeWindow());
            PushMovement(application.Platform().NativeWindow(), true);
            movementPressed = true;
            movementStarted = now;
        }
        if (movementPressed && !movementReleased && Seconds(now, *movementStarted) >= .7) {
            pointerReleasePosition = application.LocalMovement().predictedPosition;
            pointerReleasedAt = now;
            movementReleased = true;
            SDL_Event moved{};
            moved.type = SDL_EVENT_WINDOW_MOVED;
            moved.window.windowID = SDL_GetWindowID(application.Platform().NativeWindow());
            Require(SDL_PushEvent(&moved), "Could not enqueue window move event");
            testedWindowMove = true;
        }

        const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
        previous = now;
        if (releasedNextFrame) {
            PushKey(application.Platform().NativeWindow(), *releasedNextFrame, false);
            releasedNextFrame.reset();
        }
        if (!lobbyKeys.empty()) {
            const auto key = lobbyKeys.front();
            lobbyKeys.pop_front();
            PushKey(application.Platform().NativeWindow(), key, true);
            releasedNextFrame = key;
        }
        Require(application.ProcessEvents(frame) == Control::Continue, "Acceptance window was closed");
        const auto updateStarted = Clock::now();
        Require(application.Update(frame) == Control::Continue, application.LastError());
        const auto updateFinished = Clock::now();
        if (testedWindowMove && !testedCursorToggle && Seconds(now, *pointerReleasedAt) >= .05) {
            const auto stoppedPrediction = application.LocalMovement().predictedPosition;
            Require(std::hypot(stoppedPrediction.x - pointerReleasePosition->x,
                               stoppedPrediction.z - pointerReleasePosition->z) < .01,
                "Held W kept predicting movement after window move released capture");
            testedHeldInputRelease = true;
            PushMovement(application.Platform().NativeWindow(), false);
            Require(!SDL_GetWindowRelativeMouseMode(application.Platform().NativeWindow()),
                "Moving the window retained gameplay mouse capture");
            Require(connection.State().phase == fps::pvp::ConnectionPhase::Playing,
                "Releasing mouse capture left the match");
            SDL_Event focus{};
            focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            focus.window.windowID = SDL_GetWindowID(application.Platform().NativeWindow());
            Require(SDL_PushEvent(&focus), "Could not enqueue focus loss");
            focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
            Require(SDL_PushEvent(&focus), "Could not enqueue focus regain");
            Require(application.ProcessEvents(frame) == Control::Continue, "Focus events failed");
            Require(application.Update({frame.frameIndex, 0}) == Control::Continue, application.LastError());
            Require(!SDL_GetWindowRelativeMouseMode(application.Platform().NativeWindow()),
                "Focus regain recaptured the pointer without a content click");
            // Tab resumes capture, then Tab releases it without leaving.
            PushKey(application.Platform().NativeWindow(), SDL_SCANCODE_TAB, true);
            Require(application.ProcessEvents(frame) == Control::Continue, "Cursor toggle event failed");
            Require(application.Update({frame.frameIndex, 0}) == Control::Continue, application.LastError());
            Require(SDL_GetWindowRelativeMouseMode(application.Platform().NativeWindow()), "Tab did not resume capture");
            PushKey(application.Platform().NativeWindow(), SDL_SCANCODE_TAB, false);
            Require(application.ProcessEvents(frame) == Control::Continue, "Cursor toggle release failed");
            PushKey(application.Platform().NativeWindow(), SDL_SCANCODE_TAB, true);
            Require(application.ProcessEvents(frame) == Control::Continue, "Cursor release event failed");
            Require(application.Update({frame.frameIndex, 0}) == Control::Continue, application.LastError());
            Require(!SDL_GetWindowRelativeMouseMode(application.Platform().NativeWindow()), "Tab did not release capture");
            PushKey(application.Platform().NativeWindow(), SDL_SCANCODE_TAB, false);
            testedCursorToggle = true;
        }
        const auto observation = application.LocalMovement();
        if (observation.active) {
            trace << Seconds(now, started) << ',' << frame.deltaSeconds << ',' << observation.authorityTick
                  << ',' << observation.lastResolvedCommand << ',' << observation.latestCommand << ','
                  << observation.pendingCommands << ',' << observation.predictedPosition.x << ','
                  << observation.predictedPosition.z << ',' << observation.renderPosition.x << ','
                  << observation.renderPosition.z << ',' << observation.correctionOffset.x << ','
                  << observation.correctionOffset.z << ',' << observation.frozen << '\n';
            Require(observation.pendingCommands <= fps::pvp::MaxPendingCommands, "Prediction window exceeded limit");
            maximumCorrection = std::max(maximumCorrection, std::hypot(
                double(observation.correctionOffset.x), double(observation.correctionOffset.z)));
        }
        const bool wantsBefore = both && together >= .35 && !beforeCaptured;
        const bool wantsAfter = both && beforeCaptured && together >= options.duration && !afterCaptured &&
            (!options.move || movementReleased);
        if (wantsBefore || wantsAfter) application.Renderer().RequestSceneCapture();
        const auto renderStarted = Clock::now();
        Require(application.Render(frame) == Control::Continue, application.LastError());
        const auto renderFinished = Clock::now();
        const double renderMs = Seconds(renderFinished, renderStarted) * 1000;
        if (const auto& presented = application.PresentedMovement()) {
            const auto& displayed = presented->local;
            if (previousObservation && previousPresentedTime && movementPressed && !movementReleased &&
                Seconds(renderFinished, *movementStarted) > .15) {
                const double step = std::hypot(double(displayed.renderPosition.x - previousObservation->renderPosition.x),
                    double(displayed.renderPosition.z - previousObservation->renderPosition.z));
                const double submittedInterval = presented->hostSteadySeconds - *previousPresentedTime;
                maximumRenderStep = std::max(maximumRenderStep, step);
                if (step > .00001) {
                    ++movingPresentationFrames;
                    if (displayed.authorityTick == previousObservation->authorityTick) ++movingWithoutSnapshot;
                }
                // Steady means consecutive successful presentations (no skipped or
                // failed frame between them), not a frame-time limit.
                if (submittedInterval > .0001 && previousPresentedFrame && presented->frameId == *previousPresentedFrame + 1 &&
                    !displayed.frozen) {
                    ++stableFrames;
                    if (step > .00001) ++stableMovingFrames;
                }
            }
            previousObservation = displayed;
            previousPresentedTime = presented->hostSteadySeconds;
            previousPresentedFrame = presented->frameId;
            const auto& remote = presented->remote;
            presentation << presented->hostSteadySeconds << ',' << presented->localPlayerId << ','
                         << presented->local.renderPosition.x << ',' << presented->local.renderPosition.z << ','
                         << (remote ? remote->playerId : 0) << ',' << (remote ? remote->renderPosition.x : 0) << ','
                         << (remote ? remote->renderPosition.z : 0) << '\n';
        }
        if (observation.active && !sawWorldFrame) {
            firstWorldUpdateMs = Seconds(updateFinished, updateStarted) * 1000;
            firstWorldRenderMs = renderMs;
            sawWorldFrame = true;
        }
        if (observation.active && !wantsBefore && !wantsAfter)
            maximumNormalRenderMs = std::max(maximumNormalRenderMs, renderMs);
        if (auto capture = application.Renderer().TakeSceneCapture()) {
            if (wantsBefore) {
                SaveCapture(*capture, options.output / (options.role + "-before.bmp"));
                beforeCaptured = true;
            } else if (wantsAfter) {
                SaveCapture(*capture, options.output / (options.role + "-world.bmp"));
                afterCaptured = true;
                captureFinished = now;
            }
        }
        SDL_Delay(1);
    }

    Require(finalTick > firstTick, "Authoritative simulation did not advance during GUI run");
    std::ofstream report(options.output / (options.role + "-report.txt"));
    Require(bool(report), "Cannot create GUI acceptance report");
    report << "role=" << options.role << '\n'
           << "gateway=" << options.gateway << '\n'
           << "player_id=" << finalState.playerId << '\n'
           << "observed_players=" << finalState.snapshot->players.size() << '\n'
           << "first_authority_tick=" << firstTick << '\n'
           << "final_authority_tick=" << finalTick << '\n'
           << "maximum_self_displacement=" << maximumDisplacement << '\n'
           << "presentation_frames=" << frameIndex << '\n'
           << "moving_presentation_frames=" << movingPresentationFrames << '\n'
           << "moving_frames_without_new_snapshot=" << movingWithoutSnapshot << '\n'
           << "stable_movement_frames=" << stableFrames << '\n'
           << "stable_frames_with_render_motion=" << stableMovingFrames << '\n'
           << "maximum_render_step=" << maximumRenderStep << '\n'
           << "first_world_update_ms=" << firstWorldUpdateMs << '\n'
           << "first_world_render_ms=" << firstWorldRenderMs << '\n'
           << "maximum_normal_render_ms=" << maximumNormalRenderMs << '\n'
           << "maximum_correction=" << maximumCorrection << '\n'
           << "local_movement_trace=" << options.role << "-movement.csv\n"
           << "scripted_sdl_movement=" << (options.move ? "true" : "false") << '\n'
           << "window_move_releases_capture=" << (testedWindowMove ? "true" : "not exercised") << '\n'
           << "window_move_stops_held_input=" << (testedHeldInputRelease ? "true" : "not exercised") << '\n'
           << "tab_toggles_capture=" << (testedCursorToggle ? "true" : "not exercised") << '\n'
           << "join_via_lobby_keyboard=true\n"
           << "capture=actual GYO scene readback before UI overlay\n"
           << "network_scope=address specified above; localhost is not physical LAN evidence\n";
    for (const auto& player : finalState.snapshot->players) {
        report << "player=" << player.playerId << " position=" << player.position.x << ','
               << player.position.y << ',' << player.position.z << '\n';
    }
    report.flush();
    Require(bool(report), "Cannot write GUI acceptance report");
    if (options.move) {
        Require(maximumDisplacement > .25, "Scripted W input did not cause observed authoritative movement");
        // Snapshot cadence is now equal to authority cadence; repeated-snapshot
        // frames are no longer guaranteed at 60 FPS. Dedicated prediction tests
        // verify motion without ACKs; this probe verifies continuous real rendering.
        Require(stableFrames >= 5 && stableMovingFrames >= stableFrames * .8,
            "Local camera failed to move continuously during steady presentation frames");
    }
    presentation.flush();
    Require(bool(presentation), "Cannot write presentation latency trace");
    trace.flush();
    Require(bool(trace), "Cannot write local movement trace");
    PushKey(application.Platform().NativeWindow(), SDL_SCANCODE_ESCAPE, true);
    const Engine::Runtime::FrameContext leaveFrame{frameIndex++, 1.0 / 60.0};
    Require(application.ProcessEvents(leaveFrame) == Control::Continue, "ESC event failed");
    Require(application.Update(leaveFrame) == Control::Continue, application.LastError());
    Require(!connection.State().snapshot, "ESC retained the old world while leaving");
    Require(!application.LocalMovement().active && application.LocalMovement().pendingCommands == 0,
        "ESC retained prediction history");
    PushKey(application.Platform().NativeWindow(), SDL_SCANCODE_ESCAPE, false);
    const auto leaveDeadline = Clock::now() + std::chrono::seconds(5);
    for (;;) {
        const auto state = connection.State();
        Require(state.error.empty(), "ESC cleanup failed: " + state.error);
        if (state.phase == fps::pvp::ConnectionPhase::Lobby) break;
        Require(Clock::now() < leaveDeadline, "ESC did not finish remote cleanup");
        const Engine::Runtime::FrameContext waiting{frameIndex++, 1.0 / 60.0};
        Require(application.ProcessEvents(waiting) == Control::Continue, "Leave event processing failed");
        Require(application.Update(waiting) == Control::Continue, application.LastError());
        Require(application.Render(waiting) == Control::Continue, application.LastError());
        SDL_Delay(2);
    }
    Require(!connection.State().rooms.empty(), "ESC did not refresh the retained room list");
    report << "escape_returns_to_lobby=true\n" << "escape_clears_prediction=true\n"
           << "escape_refreshes_room_list=true\n";
    std::cout << options.role << ": two network players rendered; captures in "
              << options.output.string() << '\n';
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "PvP GUI acceptance (run create and join in separate processes)\n"
                  << "--arena-root <deployed assets> --gateway host:port --role create|join\n"
                  << "--duration 5 --output <directory> [--gpu-driver auto|d3d12|vulkan|metal] [--move]\n"
                  << "--latency --duration 120 --events 200 --fps 60 (no GPU readback)\n"
                  << "--combat (latency modes only: concurrent predeclared v5 SDL combat schedule, decisions and per-life state)\n"
                  << "--phase-stalls --fps 60 (six 64/83/250 ms event/update phase stalls)\n"
                  << "--latency-short --duration 16 --events 20 --fps 60 (explicit short regression)\n"
                  << "--player-short --fps 30|60|144 (12 seconds, remote character and displacement phase)\n"
                  << "--player-capture --fps 60 (separate real GPU character captures)\n"
                  << "--native-window --duration 180 (passive native X11 input/lifecycle observer)\n"
                  << "--action-short --fps 30|60|144 (18 seconds, SDL-injected v5 actions, window interactions and rejoin on any platform)\n"
                  << "--action-capture --fps 60 (the same schedule with first-person and remote GPU captures)\n";
        return 0;
    }
    try { const auto options = Parse(argc, argv);
        if (options.latency || options.latencyShort) RunLatency(options);
        else if (options.playerShort || options.playerCapture) RunPlayerShort(options);
        else if (options.actionShort || options.actionCapture) RunActionShort(options);
        else if (options.nativeWindow) RunNativeWindow(options);
        else if (options.phaseStalls) RunPhaseStalls(options);
        else Run(options);
        return 0; }
    catch (const std::exception& exception) {
        std::cerr << "PvP GUI acceptance: " << exception.what() << '\n';
        return 1;
    }
}
