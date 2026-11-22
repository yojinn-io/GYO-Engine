// The CPU measurement of declaration section 6 (net Module, step 3b): the time the net Module's Systems take per tick
// with 4 Peers and 400 replicated entities. Not a ctest and not a judgement (the declaration names no CPU threshold):
// run by hand on one host, its output kept as evidence.
//
// Method: one Match Product and four Client Products (K1) of the sample game (GameTestSupport.hpp), over memory links
// without loss, one tick a frame (ManualDelta), the wall clock of the IO layer advancing one 64 Hz step a frame. The
// Match has 400 entities with Position and Health that all move every tick (so every tick sends 400 values to each
// Peer). A trace receiver times each System span the kernel opens ("kernel", System name) with the monotonic clock and
// adds up, per frame, the net Module's Systems: at the Match ReceiveNet, ReceiveInputs, ReplicateOut, SendNet; at one
// Client ReceiveNet, ApplyReplication, SyncClock, SendInputs, SendNet. After a warm-up the per-tick sums are reported as
// mean, p50, p99 and max in microseconds.
//
// Usage: gyo_net_budget [frames]   (default 2000 after 300 of warm-up)

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

#define DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>

#include "GameTestSupport.hpp"
#include "engine/base/Trace.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

class HubLink final : public N::Link {
public:
    explicit HubLink(std::vector<N::MemoryLink> ends) : ends_(std::move(ends)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        auto& end = ends_[static_cast<std::size_t>(to.value - 100)];
        return end.Send(end.Remote(), datagram);
    }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(const std::span<std::byte> buffer) override {
        for (std::size_t i = 0; i < ends_.size(); ++i) {
            auto received = ends_[i].Receive(buffer);
            if (!received) return received;
            if (received->has_value()) return std::optional<N::ReceivedDatagram>{N::ReceivedDatagram{(*received)->size, N::Endpoint{100 + i}}};
        }
        return std::optional<N::ReceivedDatagram>{};
    }

private:
    std::vector<N::MemoryLink> ends_;
};

// Which Product runs now, and the per-frame sum of the net Systems' spans for it.
struct Timing {
    int side = 0; // 1: Match, 2: the measured Client, 0: not measured
    std::vector<std::chrono::steady_clock::time_point> starts;
    std::vector<bool> measured;
    std::vector<std::string> names;
    double matchNs = 0, clientNs = 0;
    // Per System over the measured frames (Match and Client apart).
    std::map<std::string, double> matchBySystem, clientBySystem;
    bool counting = false;
};
Timing& Times() {
    static Timing timing;
    return timing;
}
const std::set<std::string> kMatchSystems{"ReceiveNet", "ReceiveInputs", "ReplicateOut", "SendNet"};
const std::set<std::string> kClientSystems{"ReceiveNet", "ApplyReplication", "SyncClock", "SendInputs", "SendNet"};

GYO::Engine::Base::TraceToken SpanBegin(std::string_view category, std::string_view name, std::string_view) noexcept {
    Timing& t = Times();
    const bool net = category == "kernel" &&
                     ((t.side == 1 && kMatchSystems.count(std::string(name)) != 0) ||
                      (t.side == 2 && kClientSystems.count(std::string(name)) != 0));
    t.measured.push_back(net);
    t.names.emplace_back(name);
    t.starts.push_back(std::chrono::steady_clock::now());
    return {t.starts.size(), 1, static_cast<std::uint32_t>(t.starts.size())};
}
void SpanEnd(GYO::Engine::Base::TraceToken, bool) noexcept {
    Timing& t = Times();
    const auto elapsed = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t.starts.back()).count();
    if (t.measured.back()) {
        (t.side == 1 ? t.matchNs : t.clientNs) += elapsed;
        if (t.counting) (t.side == 1 ? t.matchBySystem : t.clientBySystem)[t.names.back()] += elapsed;
    }
    t.starts.pop_back();
    t.measured.pop_back();
    t.names.pop_back();
}
void SpanError(GYO::Engine::Base::TraceToken, std::string_view, bool) noexcept {}
void SpanMark(std::string_view, bool) noexcept {}
std::size_t SpanPath(std::span<char>) noexcept { return 0; }
constexpr GYO::Engine::Base::TraceReceiver kReceiver{&SpanBegin, &SpanEnd, &SpanError, &SpanMark, &SpanPath};

void Report(const char* what, std::vector<double> us) {
    std::sort(us.begin(), us.end());
    double sum = 0;
    for (const double v : us) sum += v;
    std::printf("%s: ticks=%zu mean=%.1f us p50=%.1f us p99=%.1f us max=%.1f us\n", what, us.size(), sum / static_cast<double>(us.size()),
                us[us.size() / 2], us[us.size() * 99 / 100], us.back());
}

} // namespace

int main(int argc, char** argv) {
    const int frames = argc > 1 ? std::atoi(argv[1]) : 2000;
    constexpr int kWarmup = 300;
    constexpr int kPeers = 4;
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::vector<N::MemoryLink> serverEnds;
    for (int i = 0; i < kPeers; ++i) {
        N::MemoryLinkSettings settings;
        settings.seed = static_cast<std::uint64_t>(100 + i);
        links.push_back(N::MemoryLink::Pair(settings));
        serverEnds.push_back(links.back().first);
    }
    const GameRules rules{1'000'000, false};
    End server = GameEnd(N::NetRole::Server, rules);
    server.net.link = std::make_shared<HubLink>(serverEnds);
    server.net.clock = reader;
    server.net.seed = 5;
    server.build = [rules](O::WorldSetup& s) {
        RegisterGame(s, rules);
        s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            for (K::EntityId id = 50'001; id <= 50'400; ++id) {
                world.Insert(id, Position{static_cast<std::int32_t>(id), 0});
                world.Insert(id, Health{100});
            }
            return {};
        });
    };
    auto builtServer = O::Product::Build(Describe(server), Units(true));
    if (!builtServer) {
        std::printf("server: %s\n", Reported(builtServer).c_str());
        return 1;
    }
    std::vector<std::unique_ptr<O::Product>> clients;
    for (int i = 0; i < kPeers; ++i) {
        End client = GameEnd(N::NetRole::Client, rules);
        client.range = E::EntityIdsOptions{static_cast<K::EntityId>(200'001 + i * 100'000), static_cast<K::EntityId>(300'000 + i * 100'000)};
        client.net.link = std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second);
        client.net.serverEndpoint = N::MemoryLink::EndpointA();
        client.net.userId = static_cast<std::uint64_t>(i + 1);
        client.net.clock = reader;
        auto built = O::Product::Build(Describe(client), Units(true));
        if (!built) {
            std::printf("client: %s\n", Reported(built).c_str());
            return 1;
        }
        clients.push_back(std::move(*built));
    }
    const GYO::Engine::Base::TraceReceiver* previous = GYO::Engine::Base::SetTraceReceiver(&kReceiver);
    std::vector<double> matchUs, clientUs;
    for (int frame = 0; frame < kWarmup + frames; ++frame) {
        Times().matchNs = Times().clientNs = 0;
        Times().counting = frame >= kWarmup;
        Times().side = 1;
        if (!(*builtServer)->RunFrame()) return 1;
        for (int i = 0; i < kPeers; ++i) {
            Times().side = i == 0 ? 2 : 0;
            if (!clients[static_cast<std::size_t>(i)]->RunFrame()) return 1;
        }
        Times().side = 0;
        for (auto& [a, b] : links) a.Advance();
        *clock += N::NetTime{15'625'000};
        if (frame >= kWarmup) {
            matchUs.push_back(Times().matchNs / 1000.0);
            clientUs.push_back(Times().clientNs / 1000.0);
        }
    }
    GYO::Engine::Base::SetTraceReceiver(previous);
    auto& world = (*builtServer)->World("Net").GetWorld();
    std::size_t peers = 0;
    for (const K::EntityId id : world.Entities()) peers += world.Get<N::Peer>(id) != nullptr ? 1 : 0;
    const auto client0 = StateOf(clients[0]->World("Net").GetWorld());
    std::printf("setup: peers=%zu replicated entities at the Match=%zu at client 0=%zu\n", peers, StateOf(world).entities.size(),
                client0.entities.size());
    Report("Match net Systems per tick (ReceiveNet+ReceiveInputs+ReplicateOut+SendNet)", matchUs);
    Report("Client net Systems per tick (ReceiveNet+ApplyReplication+SyncClock+SendInputs+SendNet)", clientUs);
    for (const auto& [name, ns] : Times().matchBySystem) std::printf("  Match %s: mean %.1f us\n", name.c_str(), ns / 1000.0 / frames);
    for (const auto& [name, ns] : Times().clientBySystem) std::printf("  Client %s: mean %.1f us\n", name.c_str(), ns / 1000.0 / frames);
    return 0;
}
