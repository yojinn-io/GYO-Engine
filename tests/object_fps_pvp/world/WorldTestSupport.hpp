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
// every run (the store's results still reach a World at its apply point).
class InlineJobs final : public GYO::Engine::Asset::AssetExecutor {
public:
    void Submit(Job job) override { job(); }
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
                                              const std::uint32_t startDeadlineSeconds = 60) {
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
    description.worlds.push_back(Stepped(PW::MatchWorld({.content = std::move(content), .arena = arena, .seats = seats,
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
                                               const std::size_t arena = 0) {
    O::ProductDescription description;
    description.name = "pvp-client";
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    net.link = std::move(link);
    net.clock = std::move(clock);
    description.options.Set(net);
    ReadShippedMaps(description);
    O::WorldDescription world = Stepped(PW::ClientWorld({.content = std::move(content), .arena = arena, .localUser = userId}));
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

} // namespace object_fps_pvp::test
