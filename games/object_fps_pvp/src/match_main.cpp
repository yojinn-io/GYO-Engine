// The Match: one headless Product with the game's World as the server, listening on UDP, run frame by frame until
// it is stopped (or --ticks steps have run). Clients connect with the game's Client.
//
//   gyo_object_fps_pvp                       listens on port 40000, plays the first installed arena
//   gyo_object_fps_pvp --port P              listens on port P
//   gyo_object_fps_pvp --arena ID            plays the installed arena whose id is ID
//   gyo_object_fps_pvp --ticks N             stops after N fixed steps (the tests)
//   gyo_object_fps_pvp --content DIR         reads the content from DIR (default: next to the executable)
//   gyo_object_fps_pvp --verbose             logs who joins and leaves and every kill

#include "object_fps_pvp/world/PvpWorld.hpp"

#include "gyo/GameConfig.hpp"

#include "engine/base/Log.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/Process.hpp"
#include "engine/time/Waiter.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace N = GYO::Engine::Net;
namespace WT = GYO::Engine::WorldTime;
namespace PW = object_fps_pvp::world;

void Log(const GYO::Engine::Base::LogLevel level, const std::string& text) { GYO::Engine::Base::Log(level, GYO_GAME_ID, text); }

// --verbose: who joins and leaves, and every death (one line each).
struct Seen final {
    std::uint64_t lastKill = 0;
};
void HashValue(K::Hasher&, const Seen&) {}

K::SystemResult LogMatch(K::MessageReader<N::PeerConnected> connected, K::MessageReader<N::PeerDisconnected> disconnected,
                         K::Query<K::Read<PW::Owner>, K::Read<PW::HitRecord>, K::Read<PW::PlayerCombat>> players,
                         K::ResMut<Seen> seen) {
    for (const N::PeerConnected* joined : connected.Read()) {
        Log(GYO::Engine::Base::LogLevel::Info, "user " + std::to_string(joined->userId) + " joined");
    }
    for (const N::PeerDisconnected* gone : disconnected.Read()) {
        Log(GYO::Engine::Base::LogLevel::Info, "user " + std::to_string(gone->userId) + " left (" + gone->detail + ")");
    }
    players.ForEach([&](K::EntityId, const PW::Owner& owner, const PW::HitRecord& hits, const PW::PlayerCombat& combat) {
        if (combat.life == object_fps_pvp::sim::LifeState::Dead && combat.respawnTick > seen.Read().lastKill) {
            seen->lastKill = combat.respawnTick;
            Log(GYO::Engine::Base::LogLevel::Info,
                "user " + std::to_string(owner.userId) + " was killed (" + std::to_string(hits.hits) + " hits)");
        }
    });
    return {};
}

std::optional<std::string> ContentRoot() {
    auto executable = GYO::Engine::OS::Process::ExecutablePath();
    if (!executable) return std::nullopt;
    const std::string& path = *executable;
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return std::nullopt;
    return path.substr(0, slash + 1) + GYO_GAME_ASSETS;
}

template <class T>
bool ParseNumber(const std::string_view text, T& value) {
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

} // namespace

int main(int argc, char** argv) {
    std::uint16_t port = 40000;
    std::int64_t ticks = -1;
    std::string arenaId;
    std::optional<std::string> contentRoot;
    bool verbose = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const bool hasValue = index + 1 < argc;
        if (argument == "--port" && hasValue) {
            if (!ParseNumber(argv[++index], port)) {
                Log(GYO::Engine::Base::LogLevel::Error, "--port takes a port number");
                return 2;
            }
        } else if (argument == "--ticks" && hasValue) {
            if (!ParseNumber(argv[++index], ticks) || ticks <= 0) {
                Log(GYO::Engine::Base::LogLevel::Error, "--ticks takes a positive whole number");
                return 2;
            }
        } else if (argument == "--arena" && hasValue) {
            arenaId = argv[++index];
        } else if (argument == "--content" && hasValue) {
            contentRoot = argv[++index];
        } else if (argument == "--verbose") {
            verbose = true;
        } else {
            Log(GYO::Engine::Base::LogLevel::Error,
                "unknown argument '" + std::string(argument) + "' (--port P, --arena ID, --ticks N, --content DIR, --verbose)");
            return 2;
        }
    }
    if (!contentRoot) contentRoot = ContentRoot();
    if (!contentRoot) {
        Log(GYO::Engine::Base::LogLevel::Error, "the executable's directory is unknown, so the content cannot be found");
        return 1;
    }
    std::string error;
    auto content = PW::LoadGameContent(*contentRoot, error);
    if (!content) {
        Log(GYO::Engine::Base::LogLevel::Error, "the content did not load: " + error);
        return 1;
    }
    auto shared = std::make_shared<const PW::GameContent>(std::move(*content));
    std::size_t arena = 0;
    if (!arenaId.empty()) {
        const PW::InstalledArena* found = shared->Find(arenaId);
        if (found == nullptr) {
            Log(GYO::Engine::Base::LogLevel::Error, "no installed arena has the id '" + arenaId + "'");
            return 2;
        }
        arena = static_cast<std::size_t>(found - shared->arenas.data());
    }

    auto seats = std::make_shared<PW::Seats>();
    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    N::NetProductOptions net;
    net.role = N::NetRole::Server;
    net.bind = GYO::Engine::OS::DatagramAddress::Any(port);
    net.admission = PW::SeatAdmission(seats);
    description.options.Set(net);
    O::WorldDescription world = PW::MatchWorld({shared, arena, 0, seats});
    if (verbose) {
        world.build = [inner = std::move(world.build)](O::WorldSetup& setup) {
            inner(setup);
            setup.Registrar().Resource<Seen>({.name = "Seen"});
            K::SystemOptions log;
            log.name = "LogMatch";
            K::Mount mount;
            mount.schedule = std::string(O::DefaultSchedules::frame);
            mount.stage = std::string(O::DefaultSchedules::last);
            log.mounts.push_back(std::move(mount));
            setup.Registrar().AddSystem<&LogMatch>(std::move(log));
            setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(Seen{});
                return {};
            });
        };
    }
    description.worlds.push_back(std::move(world));

    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    if (!built) {
        const O::ProductStartupReport& report = built.error();
        std::string text = "the match did not start: " + report.message + (report.detail.empty() ? "" : "\n" + report.detail);
        for (const O::UnitReportedError& unit : report.unitErrors) text += "\n" + unit.unit + ": " + unit.code + ": " + unit.message;
        Log(GYO::Engine::Base::LogLevel::Error, text);
        return 1;
    }
    std::unique_ptr<O::Product> product = std::move(built).value();
    auto waiter = GYO::Engine::Time::Waiter::Create();
    if (!waiter) {
        Log(GYO::Engine::Base::LogLevel::Error, "no timer to pace the frames: " + GYO::Engine::Base::Describe(waiter.error()));
        return 1;
    }
    Log(GYO::Engine::Base::LogLevel::Info, "the match plays " + shared->arenas[arena].content.id + " on UDP port " + std::to_string(port));
    // A frame every 4 ms: the time Module runs the fixed steps the real time has made due.
    constexpr auto kFrame = std::chrono::milliseconds(4);
    auto next = GYO::Engine::Time::MonotonicClock::now();
    while (true) {
        auto ran = product->RunFrame();
        if (!ran) {
            Log(GYO::Engine::Base::LogLevel::Error, "the match stopped: " + ran.error().message + " " + ran.error().detail);
            return 1;
        }
        if (ticks > 0) {
            const WT::Tick* tick = product->World("Game").GetWorld().Resource<WT::Tick>();
            if (tick != nullptr && tick->value >= static_cast<std::uint64_t>(ticks)) break;
        }
        next += kFrame;
        const auto now = GYO::Engine::Time::MonotonicClock::now();
        if (next < now) next = now;
        (void)waiter->WaitUntil(next);
    }
    return 0;
}
