#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/Renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

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
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
double Seconds(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double>(a - b).count();
}
Options Parse(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--move") { options.move = true; continue; }
        Require(i + 1 < argc, "Missing value for " + std::string(argument));
        const std::string value = argv[++i];
        if (argument == "--arena-root") options.assetRoot = value;
        else if (argument == "--gateway") options.gateway = value;
        else if (argument == "--role") options.role = value;
        else if (argument == "--duration") options.duration = std::stod(value);
        else if (argument == "--output") options.output = value;
        else if (argument == "--gpu-driver") options.gpu = value;
        else throw std::runtime_error("Unknown option: " + std::string(argument));
    }
    Require(!options.assetRoot.empty(), "--arena-root must name the deployed PvP assets directory");
    Require(!options.output.empty(), "--output must name the capture directory");
    Require(options.role == "create" || options.role == "join", "--role must be create or join");
    Require(std::isfinite(options.duration) && options.duration >= 2 && options.duration <= 120,
        "--duration must be between 2 and 120 seconds");
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
void PushMovement(SDL_Window* window, bool held) {
    const auto id = SDL_GetWindowID(window);
    if (held) {
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        focus.window.windowID = id;
        Require(SDL_PushEvent(&focus), "Could not enqueue probe focus event");
    }
    SDL_Event keyboard{};
    keyboard.type = held ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    keyboard.key.windowID = id;
    keyboard.key.scancode = SDL_SCANCODE_W;
    keyboard.key.key = SDLK_W;
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
    std::optional<fps::Float3> startingPosition;
    bool submittedJoin = options.role == "create";
    bool beforeCaptured = false;
    bool afterCaptured = false;
    bool movementPressed = false;
    bool movementReleased = false;
    std::uint64_t frameIndex = 0;
    std::uint64_t firstTick = 0;
    std::uint64_t finalTick = 0;
    double maximumDisplacement = 0;
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
            PushMovement(application.Platform().NativeWindow(), false);
            movementReleased = true;
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
        Require(application.Update(frame) == Control::Continue, application.LastError());
        const bool wantsBefore = both && together >= .35 && !beforeCaptured;
        const bool wantsAfter = both && beforeCaptured && together >= options.duration && !afterCaptured &&
            (!options.move || movementReleased);
        if (wantsBefore || wantsAfter) application.Renderer().RequestSceneCapture();
        Require(application.Render(frame) == Control::Continue, application.LastError());
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
           << "scripted_sdl_movement=" << (options.move ? "true" : "false") << '\n'
           << "join_via_lobby_keyboard=true\n"
           << "capture=actual GYO scene readback before UI overlay\n"
           << "network_scope=address specified above; localhost is not physical LAN evidence\n";
    for (const auto& player : finalState.snapshot->players) {
        report << "player=" << player.playerId << " position=" << player.position.x << ','
               << player.position.y << ',' << player.position.z << '\n';
    }
    report.flush();
    Require(bool(report), "Cannot write GUI acceptance report");
    if (options.move) Require(maximumDisplacement > .25,
        "Scripted W input did not cause observed authoritative movement");
    PushKey(application.Platform().NativeWindow(), SDL_SCANCODE_ESCAPE, true);
    const Engine::Runtime::FrameContext leaveFrame{frameIndex++, 1.0 / 60.0};
    Require(application.ProcessEvents(leaveFrame) == Control::Continue, "ESC event failed");
    Require(application.Update(leaveFrame) == Control::Continue, application.LastError());
    Require(connection.State().phase == fps::pvp::ConnectionPhase::Lobby && !connection.State().snapshot,
        "ESC did not return the GUI client to Lobby");
    report << "escape_returns_to_lobby=true\n";
    std::cout << options.role << ": two network players rendered; captures in "
              << options.output.string() << '\n';
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "PvP GUI acceptance (run create and join in separate processes)\n"
                  << "--arena-root <deployed assets> --gateway host:port --role create|join\n"
                  << "--duration 5 --output <directory> [--gpu-driver auto|d3d12|vulkan] [--move]\n";
        return 0;
    }
    try { Run(Parse(argc, argv)); return 0; }
    catch (const std::exception& exception) {
        std::cerr << "PvP GUI acceptance: " << exception.what() << '\n';
        return 1;
    }
}
