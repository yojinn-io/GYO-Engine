// Product-owned GUI participant of the room-capacity acceptance: the real
// application creates a room, waits for headless bots (quad_main.cpp) to fill
// it and plays with them. The two-Client GUI probe (gui_main.cpp) is unchanged.
#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "acceptance_capacity.hpp"
#include "acceptance_protocol.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Control = Engine::Runtime::RuntimeControl;
using Json = nlohmann::json;

struct Options {
    std::filesystem::path assetRoot;
    std::filesystem::path output;
    std::string gateway;
    std::string gpu{"auto"};
    double duration{30};
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
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        Require(i + 1 < argc, "Missing value for " + std::string(argument));
        const std::string value = argv[++i];
        if (argument == "--arena-root") options.assetRoot = value;
        else if (argument == "--gateway") options.gateway = value;
        else if (argument == "--output") options.output = value;
        else if (argument == "--gpu-driver") options.gpu = value;
        else if (argument == "--duration") options.duration = std::stod(value);
        else if (argument == "--fps") options.fps = std::stod(value);
        else throw std::runtime_error("Unknown option: " + std::string(argument));
    }
    Require(!options.assetRoot.empty() && !options.output.empty() && !options.gateway.empty(),
        "--arena-root, --gateway and --output are required");
    Require(std::isfinite(options.duration) && options.duration >= 10 && options.duration <= 600,
        "--duration (the minimum full-room seconds) must be 10..600");
    Require(std::isfinite(options.fps) && options.fps >= 30 && options.fps <= 144, "--fps must be 30..144");
    return options;
}

// Scripted input enters the same SDL adapter as a user; one probe per process.
void PushMovement(SDL_Window* window, bool held, SDL_Scancode direction) {
    const auto id = SDL_GetWindowID(window);
    if (held && !SDL_GetWindowRelativeMouseMode(window)) {
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        focus.window.windowID = id;
        Require(SDL_PushEvent(&focus), "Could not enqueue probe focus event");
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
    Require(SDL_PushEvent(&event), "Could not enqueue key");
}

void Run(const Options& options) {
    std::filesystem::create_directories(options.output);
    fps::pvp::MovementTraceWriter traceWriter(options.output / "gui-commands.jsonl");
    fps::pvp::PvpApplication application;
    std::string error;
    Require(application.InitializeContent(options.assetRoot, error), error);
    fps::pvp::PvpApplicationOptions graphics;
    graphics.title = "Object FPS PVP room capacity / GUI";
    graphics.gateway = options.gateway;
    graphics.gpuDriver = options.gpu;
    Require(application.InitializeGraphics(graphics, error), error);
    auto& connection = application.Connection();
    auto* window = application.Platform().NativeWindow();

    std::uint64_t frameIndex = 0;
    auto previous = Clock::now();
    const auto step = [&](Clock::time_point now) {
        const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
        previous = now;
        Require(application.ProcessEvents(frame) == Control::Continue, "Acceptance window was closed");
        Require(application.Update(frame) == Control::Continue, application.LastError());
        Require(application.Render(frame) == Control::Continue, application.LastError());
        return frame.deltaSeconds;
    };
    const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / options.fps));
    const auto waitFor = [&](auto condition, double seconds, const std::string& why) {
        const auto until = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        for (;;) {
            const auto state = connection.State();
            Require(state.error.empty(), "GUI Client failed: " + state.error);
            if (condition(state)) return;
            Require(Clock::now() < until, why);
            step(Clock::now());
            std::this_thread::sleep_for(period);
        }
    };

    connection.CreateAndJoin(options.gateway);
    waitFor([](const auto& s) { return s.phase == fps::pvp::ConnectionPhase::Playing && s.snapshot.has_value(); },
        10, "GUI Client could not create and join a room");
    const auto playerId = connection.State().playerId;
    {
        std::ofstream ready(options.output / "gui-ready.json");
        ready << Json{{"player_id", playerId}, {"protocol", AcceptanceProtocolVersion}}.dump();
        Require(bool(ready), "Cannot write GUI readiness");
    }
    waitFor([](const auto& s) { return s.snapshot && s.snapshot->players.size() == AcceptanceMaxPlayers; },
        30, "The bots did not fill the room");

    const auto started = Clock::now();
    const auto startNs = fps::pvp::MovementTraceNowNs();
    auto deadline = started;
    std::vector<double> frameSeconds;
    std::vector<std::uint64_t> frameTimes;
    std::size_t minimumPlayers = AcceptanceMaxPlayers, maximumPlayers = 0;
    std::set<std::uint64_t> lives;
    std::uint64_t deaths = 0, presentedFrames = 0;
    bool wasDead = false;
    std::optional<std::uint64_t> lastPresentedFrame;
    bool held = false;
    SDL_Scancode direction = SDL_SCANCODE_W;
    double nextToggle = .5;
    // Plays while the room is full; the bots outlast --duration, then their
    // leaving ends the run. A room that empties earlier fails it.
    bool roomLeft = false;
    while (!roomLeft) {
        const auto now = Clock::now();
        const double age = Seconds(now, started);
        Require(age < options.duration + 30, "The bots did not leave after the run");
        if (age >= nextToggle) {
            // Walk forward and back in 0.8 s holds so the GUI player keeps moving among the bots.
            if (held) PushMovement(window, false, direction);
            direction = direction == SDL_SCANCODE_W ? SDL_SCANCODE_S : SDL_SCANCODE_W;
            PushMovement(window, true, direction);
            held = true;
            nextToggle += .8;
        }
        frameTimes.push_back(fps::pvp::MovementTraceNowNs());
        frameSeconds.push_back(step(now));
        const auto state = connection.State();
        Require(state.error.empty(), "GUI Client failed: " + state.error);
        Require(state.phase == fps::pvp::ConnectionPhase::Playing && state.snapshot, "GUI Client left the match");
        const auto players = state.snapshot->players.size();
        if (players < AcceptanceMaxPlayers) {
            Require(age >= options.duration, "The room lost a player before the GUI run completed");
            roomLeft = true;
            continue;
        }
        minimumPlayers = std::min(minimumPlayers, players);
        maximumPlayers = std::max(maximumPlayers, players);
        for (const auto& player : state.snapshot->players) {
            if (player.playerId != playerId) continue;
            lives.insert(player.lifeGeneration);
            const bool dead = player.lifeState == fps::pvp::LifeState::Dead;
            if (dead && !wasDead) ++deaths;
            wasDead = dead;
        }
        if (const auto& presented = application.PresentedMovement();
            presented && (!lastPresentedFrame || presented->frameId != *lastPresentedFrame)) {
            ++presentedFrames;
            lastPresentedFrame = presented->frameId;
        }
        deadline += period;
        if (deadline < Clock::now()) deadline = Clock::now();
        std::this_thread::sleep_until(deadline);
    }
    const auto endNs = fps::pvp::MovementTraceNowNs();
    if (held) PushMovement(window, false, direction);

    PushKey(window, SDL_SCANCODE_ESCAPE, true);
    step(Clock::now());
    PushKey(window, SDL_SCANCODE_ESCAPE, false);
    waitFor([](const auto& s) { return s.phase == fps::pvp::ConnectionPhase::Lobby; }, 5, "ESC did not leave the match");
    traceWriter.Finish();
    Require(traceWriter.Good(), "GUI diagnostics were incomplete");

    const Json report{{"protocol", AcceptanceProtocolVersion}, {"capacity", AcceptanceMaxPlayers},
        {"player_id", playerId}, {"start_ns", startNs}, {"end_ns", endNs}, {"fps", options.fps},
        {"duration", options.duration}, {"frames", frameSeconds.size()}, {"presented_frames", presentedFrames},
        {"skipped_presentation_frames", application.SkippedPresentationFrames()},
        {"minimum_players", minimumPlayers}, {"maximum_players", maximumPlayers},
        {"life_generations", std::vector<std::uint64_t>(lives.begin(), lives.end())}, {"deaths", deaths},
        {"frame_seconds", frameSeconds}, {"frame_time_ns", frameTimes}};
    std::ofstream file(options.output / "gui-quad.json");
    file << report.dump(2) << '\n';
    Require(bool(file), "Cannot write GUI report");
    std::cout << "GUI participant: " << frameSeconds.size() << " frames with " << maximumPlayers << " players\n";
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "PvP room-capacity GUI participant (run with quad probe bots joining)\n"
                  << "--arena-root <deployed assets> --gateway host:port --output <directory>\n"
                  << "[--duration 30] [--fps 60] [--gpu-driver auto|d3d12|vulkan|metal]\n";
        return 0;
    }
    try {
        Run(Parse(argc, argv));
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "PvP GUI participant: " << exception.what() << '\n';
        return 1;
    }
}
