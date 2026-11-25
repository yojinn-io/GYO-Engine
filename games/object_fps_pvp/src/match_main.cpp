// The Match: one headless Product with the game's World as the server, listening on UDP, run frame by frame until
// it is stopped (or --ticks steps have run). Clients connect with the game's Client.
//
//   gyo_object_fps_pvp                       listens on port 40000, plays the first installed arena
//   gyo_object_fps_pvp --port P              listens on port P
//   gyo_object_fps_pvp --arena ID            plays the installed arena whose id is ID
//   gyo_object_fps_pvp --ticks N             stops after N fixed steps (the tests)
//   gyo_object_fps_pvp --content DIR         reads the content from DIR (default: next to the executable)
//   gyo_object_fps_pvp --verbose             logs who joins and leaves and every kill
//   gyo_object_fps_pvp --capacity N          seats N players (default 4)
//
// Started by the matches service (gateway declaration D5, D6):
//   --match-id ID --admission-key-stdin      admits only a Client whose admission token this Match's key signed for
//                                            ID; the key is the first line of the standard input (hex), so it is in
//                                            neither the command line nor the environment
//   --saves A.B.C.D:P                        reads a player's save from the save service when they join and writes it
//                                            back, settled, when they leave (with --match-id)
//   --expect-users ID,ID,...                 the room's members: the match starts when each one's Client is ready
//                                            (before it, each player sees only itself and nobody moves); none: it is
//                                            played from the first step
//   --start-deadline-seconds N               starts anyway N seconds after the first player came (default 60)
//   --idle-seconds N                         ends once no player has been in the match for N seconds, from the start
//                                            or since the last one left (a player who drops may come back within
//                                            it); the matches service then frees the port

#include "object_fps_pvp/data/Hex.hpp"
#include "object_fps_pvp/match/AdmissionToken.hpp"
#include "object_fps_pvp/world/MatchSaves.hpp"
#include "object_fps_pvp/world/PvpWorld.hpp"

#include "gyo/GameConfig.hpp"

#include "engine/base/Log.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/Process.hpp"
#include "engine/time/Waiter.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

// "A.B.C.D:P" (port 1 to 65535).
std::optional<GYO::Engine::OS::DatagramAddress> ParseAddress(const std::string_view text) {
    const std::size_t colon = text.rfind(':');
    std::uint16_t port = 0;
    if (colon == std::string_view::npos || !ParseNumber(text.substr(colon + 1), port) || port == 0) return std::nullopt;
    auto address = GYO::Engine::OS::DatagramAddress::Parse(text.substr(0, colon), port);
    if (!address) return std::nullopt;
    return *address;
}

} // namespace

int main(int argc, char** argv) {
    std::uint16_t port = 40000;
    std::int64_t ticks = -1;
    std::string arenaId;
    std::optional<std::string> contentRoot;
    bool verbose = false;
    std::uint32_t capacity = 4;
    std::optional<std::uint64_t> matchId;
    bool keyOnStdin = false;
    std::optional<std::int64_t> idleSeconds;
    std::vector<std::uint64_t> expectUsers;
    std::uint32_t startDeadline = 60;
    std::optional<GYO::Engine::OS::DatagramAddress> saves;
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
        } else if (argument == "--capacity" && hasValue) {
            if (!ParseNumber(argv[++index], capacity) || capacity == 0) {
                Log(GYO::Engine::Base::LogLevel::Error, "--capacity takes a positive whole number");
                return 2;
            }
        } else if (argument == "--match-id" && hasValue) {
            std::uint64_t id = 0;
            if (!ParseNumber(argv[++index], id) || id == 0) {
                Log(GYO::Engine::Base::LogLevel::Error, "--match-id takes a whole number other than 0");
                return 2;
            }
            matchId = id;
        } else if (argument == "--admission-key-stdin") {
            keyOnStdin = true;
        } else if (argument == "--saves" && hasValue) {
            saves = ParseAddress(argv[++index]);
            if (!saves) {
                Log(GYO::Engine::Base::LogLevel::Error, "--saves takes A.B.C.D:P");
                return 2;
            }
        } else if (argument == "--idle-seconds" && hasValue) {
            std::int64_t seconds = 0;
            if (!ParseNumber(argv[++index], seconds) || seconds <= 0) {
                Log(GYO::Engine::Base::LogLevel::Error, "--idle-seconds takes a positive whole number");
                return 2;
            }
            idleSeconds = seconds;
        } else if (argument == "--expect-users" && hasValue) {
            const std::string_view list(argv[++index]);
            for (std::size_t at = 0; at <= list.size();) {
                const std::size_t comma = std::min(list.find(',', at), list.size());
                std::uint64_t user = 0;
                if (!ParseNumber(list.substr(at, comma - at), user) || user == 0) {
                    Log(GYO::Engine::Base::LogLevel::Error, "--expect-users takes user ids separated by commas");
                    return 2;
                }
                expectUsers.push_back(user);
                at = comma + 1;
            }
        } else if (argument == "--start-deadline-seconds" && hasValue) {
            if (!ParseNumber(argv[++index], startDeadline)) {
                Log(GYO::Engine::Base::LogLevel::Error, "--start-deadline-seconds takes a whole number");
                return 2;
            }
        } else {
            Log(GYO::Engine::Base::LogLevel::Error,
                "unknown argument '" + std::string(argument) +
                    "' (--port P, --arena ID, --ticks N, --content DIR, --verbose, --capacity N, --match-id ID, "
                    "--admission-key-stdin, --saves A.B.C.D:P, --idle-seconds N, --expect-users ID,..., --start-deadline-seconds N)");
            return 2;
        }
    }
    if (keyOnStdin != matchId.has_value()) {
        Log(GYO::Engine::Base::LogLevel::Error, "--match-id and --admission-key-stdin go together");
        return 2;
    }
    if (saves && !matchId) {
        Log(GYO::Engine::Base::LogLevel::Error, "--saves needs --match-id (the save keys name the Match)");
        return 2;
    }
    std::optional<PW::AdmissionCheck> admission;
    if (keyOnStdin) {
        std::string line;
        std::getline(std::cin, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto key = object_fps_pvp::data::DecodeHex(line);
        if (!key || key->empty()) {
            Log(GYO::Engine::Base::LogLevel::Error, "the standard input's first line is not a hexadecimal key");
            return 2;
        }
        // The wall clock: tokens expire in Unix seconds (an admission is not decision data, net D-9).
        admission = PW::AdmissionCheck{std::move(*key), *matchId, [] {
                                            return std::chrono::duration_cast<std::chrono::seconds>(
                                                       std::chrono::system_clock::now().time_since_epoch())
                                                .count();
                                        }};
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

    auto seats = std::make_shared<PW::Seats>(capacity);
    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    N::NetProductOptions net;
    net.role = N::NetRole::Server;
    net.bind = GYO::Engine::OS::DatagramAddress::Any(port);
    net.admission = admission ? PW::MatchAdmission(seats, std::move(*admission)) : PW::SeatAdmission(seats);
    description.options.Set(net);
    O::WorldDescription world = PW::MatchWorld({.content = shared, .arena = arena, .seats = seats, .expectUsers = expectUsers,
                                                .startDeadlineSeconds = startDeadline});
    if (saves) world = PW::WithMatchSaves(std::move(world), {*saves, *matchId});
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
    auto emptySince = next;
    std::uint64_t failuresLogged = 0;
    std::optional<GYO::Engine::Time::MonotonicClock::time_point> endingSince;
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
        const PW::MatchSaves* matchSaves = saves ? product->World("Game").GetWorld().Resource<PW::MatchSaves>() : nullptr;
        if (matchSaves != nullptr) {
            const auto report = matchSaves->Report();
            if (report.failures > failuresLogged) {
                failuresLogged = report.failures;
                Log(GYO::Engine::Base::LogLevel::Warning, "a save was not kept: " + report.lastFailure);
            }
        }
        if (idleSeconds) {
            const auto now = GYO::Engine::Time::MonotonicClock::now();
            if (seats->Taken() > 0) {
                emptySince = now;
                endingSince.reset();
            } else if (now - emptySince >= std::chrono::seconds(*idleSeconds)) {
                // The saves still in flight finish first (at most five seconds).
                if (!endingSince) endingSince = now;
                const bool savesBusy = matchSaves != nullptr && matchSaves->Busy();
                if (!savesBusy || now - *endingSince >= std::chrono::seconds(5)) {
                    if (savesBusy) Log(GYO::Engine::Base::LogLevel::Warning, "the match ends with saves not written");
                    Log(GYO::Engine::Base::LogLevel::Info,
                        "no player for " + std::to_string(*idleSeconds) + " s: the match ends");
                    break;
                }
            }
        }
        next += kFrame;
        const auto now = GYO::Engine::Time::MonotonicClock::now();
        if (next < now) next = now;
        (void)waiter->WaitUntil(next);
    }
    return 0;
}
