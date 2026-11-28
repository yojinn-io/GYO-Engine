#pragma once

// Products of the game for its World tests: a lone Match (its players put in and steered by the test, one fixed
// step a frame) and a Match with Clients over memory links (the net Module's MemoryLink; the server's end of several
// links is HubLink, as the net Module's scenario test).

#include <doctest/doctest.h>

#include "object_fps_pvp/world/PvpWorld.hpp"

#include "engine/net/Link.hpp"
#include "engine/net/MemoryLink.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/Replication.hpp"
#include "engine/world_time/Sampling.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <random>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace object_fps_pvp::test {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace N = GYO::Engine::Net;
namespace WT = GYO::Engine::WorldTime;
namespace Math = GYO::Engine::Math;
namespace PW = object_fps_pvp::world;

// One fixed step of 1/64 s a frame, whatever the clock.
inline constexpr std::int64_t kStep = 15'625'000;

// The asset store's jobs run at once, on the thread that submits them: a World generates in the same number of frames
// every run (the store's results still reach a World at its apply point). A Submit made from inside a job is queued and
// run after it, never recursively (AssetExecutor's contract).
class InlineJobs final : public GYO::Engine::Asset::AssetExecutor {
public:
    void Submit(Job job) override {
        queued_.push_back(std::move(job));
        if (running_) return;
        running_ = true;
        while (!queued_.empty()) {
            Job next = std::move(queued_.front());
            queued_.pop_front();
            next();
        }
        running_ = false;
    }

private:
    std::deque<Job> queued_;
    bool running_ = false;
};

// Jobs that wait until the test runs them: the oldest (RunOne) or a chosen one (RunAt, by submission order among the
// waiting).
class HeldJobs final : public GYO::Engine::Asset::AssetExecutor {
public:
    void Submit(Job job) override {
        const std::lock_guard lock(mutex_);
        jobs_.push_back(std::move(job));
    }
    bool RunOne() { return RunAt(0); }
    bool RunAt(const std::size_t index) {
        Job job;
        {
            const std::lock_guard lock(mutex_);
            if (index >= jobs_.size()) return false;
            job = std::move(jobs_[index]);
            jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(index));
        }
        job();
        return true;
    }
    std::size_t Waiting() {
        const std::lock_guard lock(mutex_);
        return jobs_.size();
    }

private:
    std::mutex mutex_;
    std::deque<Job> jobs_;
};

inline GYO::Engine::Asset::AssetExecutor& TestJobs() {
    static InlineJobs jobs;
    return jobs;
}

// A Product that reads the shipped content's maps (the asset store on the content root, the game's loaders).
inline void ReadShippedMaps(O::ProductDescription& description) {
    GYO::Engine::WorldAsset::AssetOptions assets;
    assets.contentRoot = PVP_CONTENT_ROOT;
    assets.executor = &TestJobs();
    description.options.Set(assets);
    description.configure = [](O::ProductSetup& setup) { PW::ConfigureGame(setup); };
}

// Runs the Product's frames until its World has generated (the map read, the walls built): a few frames.
inline void Generate(O::Product& product, const std::string_view world = "Game") {
    for (int frame = 0; frame < 64 && product.Generating(world); ++frame) REQUIRE(static_cast<bool>(product.RunFrame()));
    const PW::MapLoad* load = product.World(world).GetWorld().Resource<PW::MapLoad>();
    REQUIRE_MESSAGE(!product.Generating(world), (load != nullptr ? load->failure : std::string("no MapLoad")));
}

inline std::shared_ptr<const PW::GameContent> ShippedContent() {
    static const std::shared_ptr<const PW::GameContent> content = [] {
        std::string error;
        auto loaded = PW::LoadGameContent(PVP_CONTENT_ROOT, error);
        REQUIRE_MESSAGE(loaded.has_value(), error);
        return std::make_shared<const PW::GameContent>(std::move(*loaded));
    }();
    return content;
}

// The server's end of several memory links: client i is reached at Endpoint{100 + i}.
class HubLink final : public N::Link {
public:
    explicit HubLink(std::vector<N::MemoryLink> ends) : ends_(std::move(ends)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        const std::size_t index = static_cast<std::size_t>(to.value - 100);
        if (to.value < 100 || index >= ends_.size()) {
            return GYO::Engine::Base::Err(N::NetFailure::Make(N::NetFailureCode::LinkFailed, "no such client"));
        }
        return ends_[index].Send(ends_[index].Remote(), datagram);
    }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(const std::span<std::byte> buffer) override {
        for (std::size_t i = 0; i < ends_.size(); ++i) {
            auto received = ends_[i].Receive(buffer);
            if (!received) return received;
            if (received->has_value()) {
                return std::optional<N::ReceivedDatagram>{N::ReceivedDatagram{(*received)->size, N::Endpoint{100 + i}}};
            }
        }
        return std::optional<N::ReceivedDatagram>{};
    }

private:
    std::vector<N::MemoryLink> ends_;
};

// The World description with a frame of `frame` nanoseconds whatever the clock (by default one fixed step).
inline O::WorldDescription Stepped(O::WorldDescription world, const std::int64_t frame = kStep) {
    world.build = [inner = std::move(world.build), frame](O::WorldSetup& setup) {
        inner(setup);
        setup.Prepare([frame](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            w.SetResource(WT::TimeSampling{frame, WT::SamplingMode::ManualDelta});
            return {};
        });
    };
    return world;
}

template <class Built>
std::string Reported(const Built& built) {
    if (built) return "built";
    std::string out = built.error().message + " " + built.error().detail + "\n";
    for (const O::UnitReportedError& error : built.error().unitErrors) out += error.code + " " + error.message + "\n";
    return out;
}

// A Match Product on a link (an unused memory link end when alone).
inline std::unique_ptr<O::Product> BuildMatch(std::shared_ptr<const PW::GameContent> content, std::shared_ptr<N::Link> link,
                                              std::shared_ptr<PW::Seats> seats, std::function<N::NetTime()> clock,
                                              const std::size_t arena = 0, std::vector<std::uint64_t> expectUsers = {},
                                              const std::uint32_t startDeadlineSeconds = 60, const std::size_t mode = 0) {
    O::ProductDescription description;
    description.name = "pvp-match";
    N::NetProductOptions net;
    net.role = N::NetRole::Server;
    net.link = std::move(link);
    net.clock = std::move(clock);
    net.seed = 7;
    net.admission = PW::SeatAdmission(seats);
    description.options.Set(net);
    ReadShippedMaps(description);
    description.worlds.push_back(Stepped(PW::MatchWorld({.content = std::move(content), .arena = arena, .mode = mode, .seats = seats,
                                                         .expectUsers = std::move(expectUsers),
                                                         .startDeadlineSeconds = startDeadlineSeconds})));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    auto product = std::move(built).value();
    Generate(*product);
    return product;
}

inline std::unique_ptr<O::Product> BuildClient(std::shared_ptr<const PW::GameContent> content, std::shared_ptr<N::Link> link,
                                               const std::uint64_t userId, std::function<N::NetTime()> clock,
                                               const std::size_t arena = 0, const std::size_t mode = 0) {
    O::ProductDescription description;
    description.name = "pvp-client";
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    net.link = std::move(link);
    net.clock = std::move(clock);
    description.options.Set(net);
    ReadShippedMaps(description);
    O::WorldDescription world = Stepped(PW::ClientWorld({.content = std::move(content), .arena = arena, .mode = mode, .localUser = userId}));
    // Whom it connects to (net-connect-target: a client World's NetConnectTarget, put by its prepare function).
    world.prepare.push_back([target = N::NetConnectTarget{N::MemoryLink::EndpointA(), userId, {}}](K::World& w) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        w.SetResource(target);
        return {};
    });
    description.worlds.push_back(std::move(world));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    auto product = std::move(built).value();
    Generate(*product);
    return product;
}

inline K::World& GameWorld(O::Product& product) { return product.World("Game").GetWorld(); }

inline std::uint64_t TickOf(K::World& world) { return world.Resource<WT::Tick>()->value; }

// Every player of a World, by owner.
inline K::EntityId PlayerOf(K::World& world, const std::uint64_t userId) {
    for (const K::EntityId id : world.Entities()) {
        const PW::Owner* owner = world.Get<PW::Owner>(id);
        if (owner != nullptr && owner->userId == userId) return id;
    }
    return 0;
}

inline N::NetTime Ms(const std::int64_t ms) { return std::chrono::milliseconds(ms); }

// A Match and clientCount Clients over memory links (a fixed delay of two Advance calls each way), one frame each a
// step.
struct Session {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::shared_ptr<PW::Seats> seats = std::make_shared<PW::Seats>();
    std::unique_ptr<O::Product> match;
    std::vector<std::unique_ptr<O::Product>> clients;

    // expectUsers, startDeadlineSeconds: the Match's start (none: played from the first step). clientArena: the arena
    // the Clients play (none: the Match's). matchContent: the Match's (none: the shipped content). mode: the mode every
    // end plays (an index into the content's modes).
    Session(const int clientCount, const std::uint32_t capacity, const std::size_t matchArena = 0,
            std::shared_ptr<const PW::GameContent> clientContent = ShippedContent(), std::vector<std::uint64_t> expectUsers = {},
            const std::uint32_t startDeadlineSeconds = 60, const std::optional<std::size_t> clientArena = std::nullopt,
            std::shared_ptr<const PW::GameContent> matchContent = nullptr, const std::size_t mode = 0) {
        seats = std::make_shared<PW::Seats>(capacity);
        std::vector<N::MemoryLink> serverEnds;
        for (int i = 0; i < clientCount; ++i) {
            N::MemoryLinkSettings settings;
            settings.seed = static_cast<std::uint64_t>(60 + i);
            settings.aToB.fixedDelaySteps = 2;
            settings.bToA.fixedDelaySteps = 2;
            links.push_back(N::MemoryLink::Pair(settings));
            serverEnds.push_back(links.back().first);
        }
        auto reader = [c = clock] { return *c; };
        match = BuildMatch(matchContent ? matchContent : ShippedContent(), std::make_shared<HubLink>(serverEnds), seats, reader, matchArena,
                           std::move(expectUsers), startDeadlineSeconds, mode);
        for (int i = 0; i < clientCount; ++i) {
            clients.push_back(BuildClient(clientContent, std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second),
                                          static_cast<std::uint64_t>(i + 1), reader, clientArena.value_or(matchArena), mode));
        }
    }
    K::World& Match() { return GameWorld(*match); }
    K::World& Client(const std::size_t i) { return GameWorld(*clients[i]); }
    // One frame everywhere; inputs[i] (when given) is client i's input for its next step.
    void Frame(const std::vector<std::optional<PW::PlayerInput>>& inputs = {}) {
        for (std::size_t i = 0; i < clients.size(); ++i) {
            if (i < inputs.size() && inputs[i]) {
                if (auto* local = N::LocalInput<PW::PlayerInput>(Client(i))) {
                    const auto target = static_cast<std::uint32_t>(TickOf(Client(i)) + 1);
                    local->Set(target, *inputs[i], N::ViewTime{target, 0});
                }
            }
        }
        REQUIRE(static_cast<bool>(match->RunFrame()));
        for (auto& client : clients) REQUIRE(static_cast<bool>(client->RunFrame()));
        for (auto& link : links) link.first.Advance();
        *clock += Ms(16);
    }
    void Frames(const int n, const std::vector<std::optional<PW::PlayerInput>>& inputs = {}) {
        for (int i = 0; i < n; ++i) Frame(inputs);
    }
};

// A content root of a test's own: the shipped rule tables and the mode files the test writes (modes/, rules/).
struct ModeFolder {
    std::filesystem::path root;
    ModeFolder() {
        std::random_device random;
        root = std::filesystem::temp_directory_path() / ("gyo_pvp_modes_" + std::to_string(random()));
        std::filesystem::create_directories(root / "modes");
        std::filesystem::create_directories(root / "rules");
        for (const char* table : {"movement_rules.json", "damage_rules.json"}) {
            std::filesystem::copy_file(std::filesystem::path(PVP_CONTENT_ROOT) / "rules" / table, root / "rules" / table);
        }
    }
    ~ModeFolder() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    ModeFolder(const ModeFolder&) = delete;
    ModeFolder& operator=(const ModeFolder&) = delete;
    void Write(const std::string& path, const std::string& text) const {
        std::ofstream out(root / path, std::ios::binary);
        out << text;
    }
    // A list of the files, each a mode written as given.
    void Modes(const std::vector<std::pair<std::string, std::string>>& files) const {
        std::string list = R"({"version": 1, "modes": [)";
        for (std::size_t i = 0; i < files.size(); ++i) {
            list += (i == 0 ? "\"" : ", \"") + files[i].first + "\"";
            Write("modes/" + files[i].first, files[i].second);
        }
        Write("modes/modes.json", list + "]}");
    }
};

// The shipped core PvP's rule tables (modes/core_pvp.json's rules).
inline const std::string kShippedRules = R"({"movement": "rules/movement_rules.json", "damage": "rules/damage_rules.json"})";

// A mode file's text.
inline std::string ModeText(const std::string& id, const std::string& rules = kShippedRules,
                            const std::string& sets = "{}", const std::string& arenas = "[]") {
    return R"({"version": 1, "id": ")" + id + R"(", "name": "M", "rules": )" + rules + R"(, "sets": )" + sets + R"(, "arenas": )" + arenas + "}";
}

} // namespace object_fps_pvp::test
