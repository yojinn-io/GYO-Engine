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

// The World description with one fixed step a frame.
inline O::WorldDescription Stepped(O::WorldDescription world) {
    world.build = [inner = std::move(world.build)](O::WorldSetup& setup) {
        inner(setup);
        setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            w.SetResource(WT::TimeSampling{kStep, WT::SamplingMode::ManualDelta});
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
                                              const std::size_t arena = 0) {
    O::ProductDescription description;
    description.name = "pvp-match";
    N::NetProductOptions net;
    net.role = N::NetRole::Server;
    net.link = std::move(link);
    net.clock = std::move(clock);
    net.seed = 7;
    net.admission = PW::SeatAdmission(seats);
    description.options.Set(net);
    description.worlds.push_back(Stepped(PW::MatchWorld({std::move(content), arena, 0, seats})));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    return std::move(built).value();
}

inline std::unique_ptr<O::Product> BuildClient(std::shared_ptr<const PW::GameContent> content, std::shared_ptr<N::Link> link,
                                               const std::uint64_t userId, std::function<N::NetTime()> clock,
                                               const std::size_t arena = 0) {
    O::ProductDescription description;
    description.name = "pvp-client";
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    net.link = std::move(link);
    net.serverEndpoint = N::MemoryLink::EndpointA();
    net.userId = userId;
    net.clock = std::move(clock);
    description.options.Set(net);
    description.worlds.push_back(Stepped(PW::ClientWorld({std::move(content), arena, userId, nullptr})));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    return std::move(built).value();
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
