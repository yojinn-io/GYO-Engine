// The CPU measurement of the net Module (declaration section 6 for step 3b; judgement G-12 of section 19 for step 3c):
// the time the net Module's Systems take per tick at the Match with N Peers and 400 replicated entities. Not a ctest
// (no CPU threshold runs in CI): run by hand on one host, its output kept as evidence.
//
// Method (G-12): one Match Product and N Client Products (K1) of the sample game (GameTestSupport.hpp), over memory
// links without loss, one tick a frame (ManualDelta), the wall clock of the IO layer advancing one 64 Hz step a frame.
// The Match has 400 entities with Position and Health; every Position changes every tick (MoveAll), so every tick sends
// 400 values to each Peer. A trace receiver times each System span the kernel opens ("kernel", System name) with the
// monotonic clock and adds up, per frame, the net Module's Systems: at the Match ReceiveNet, ReceiveInputs,
// ApplyInput:*, CollectServerMessages:*, DeliverServerMessages:*, ReplicateOut, SendNet; at the first Client
// ReceiveNet, ApplyReplication, NotifyReplica, DeliverServerMessages:*, SyncClock, SendInputs, SendNet. After a warm-up
// the per-tick sums are reported as mean, p50, p99 and max in microseconds, with the mean of each System; the other
// Systems seen at the Match are listed by name (not counted).
//
// It also reports the Match's downlink per Peer and tick (datagram bytes, UDP payload) and an FNV-1a digest of every
// datagram the Match sent, in order: the same digest before and after a change of the sending code shows the wire
// bytes did not change (step 3c).
//
// "wire" mode replays G-8's scenario (ReplicationTests.cpp, BytesPerTick: one Client, the busy game to tick 120, 130
// frames, 16 ms a frame) and prints its bytes per tick and the digest of the server's datagrams.
//
// Usage: gyo_net_budget [peers] [frames]   (default 8 Peers, 2000 frames after 300 of warm-up)
//        gyo_net_budget wire

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#define DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>

#include "GameTestSupport.hpp"
#include "engine/base/Trace.hpp"
#include "engine/net/MemoryLink.hpp"
#include "engine/net/ServerHost.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

// FNV-1a over every datagram the Match sent, with its length and destination in front.
struct Digest {
    std::uint64_t value = 14695981039346656037ull;
    void Add(const std::byte* data, const std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) {
            value ^= static_cast<std::uint64_t>(data[i]);
            value *= 1099511628211ull;
        }
    }
    void Add(const std::uint64_t v) {
        std::byte raw[8];
        std::memcpy(raw, &v, 8);
        Add(raw, 8);
    }
};

struct Wire {
    Digest digest;
    std::vector<std::uint64_t> bytes; // per destination, this frame
    std::vector<std::uint64_t> datagrams;
};

class HubLink final : public N::Link {
public:
    HubLink(std::vector<N::MemoryLink> ends, std::shared_ptr<Wire> wire) : ends_(std::move(ends)), wire_(std::move(wire)) {
        wire_->bytes.assign(ends_.size(), 0);
        wire_->datagrams.assign(ends_.size(), 0);
    }
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        const auto index = static_cast<std::size_t>(to.value - 100);
        wire_->digest.Add(to.value);
        wire_->digest.Add(datagram.size());
        wire_->digest.Add(datagram.data(), datagram.size());
        wire_->bytes[index] += datagram.size();
        ++wire_->datagrams[index];
        auto& end = ends_[index];
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
    std::shared_ptr<Wire> wire_;
};

// A link that digests what it sends (the "wire" mode's server end).
class DigestLink final : public N::Link {
public:
    DigestLink(N::MemoryLink end, std::shared_ptr<Wire> wire) : end_(std::move(end)), wire_(std::move(wire)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        wire_->digest.Add(datagram.size());
        wire_->digest.Add(datagram.data(), datagram.size());
        return end_.Send(to, datagram);
    }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(const std::span<std::byte> buffer) override { return end_.Receive(buffer); }

private:
    N::MemoryLink end_;
    std::shared_ptr<Wire> wire_;
};

bool StartsWith(const std::string& name, const char* prefix) { return name.rfind(prefix, 0) == 0; }
bool MatchNet(const std::string& name) {
    return name == "ReceiveNet" || name == "ReceiveInputs" || name == "ReplicateOut" || name == "SendNet" ||
           StartsWith(name, "ApplyInput:") || StartsWith(name, "CollectServerMessages:") || StartsWith(name, "DeliverServerMessages:");
}
bool ClientNet(const std::string& name) {
    return name == "ReceiveNet" || name == "ApplyReplication" || name == "NotifyReplica" || name == "SyncClock" ||
           name == "SendInputs" || name == "SendNet" || StartsWith(name, "DeliverServerMessages:");
}

// Which Product runs now, and the per-frame sum of the net Systems' spans for it.
struct Timing {
    int side = 0; // 1: Match, 2: the measured Client, 0: not measured
    std::vector<std::chrono::steady_clock::time_point> starts;
    std::vector<bool> measured;
    std::vector<std::string> names;
    double matchNs = 0, clientNs = 0;
    // Per System over the measured frames (Match and Client apart).
    std::map<std::string, double> matchBySystem, clientBySystem;
    std::set<std::string> matchOthers;
    bool counting = false;
};
Timing& Times() {
    static Timing timing;
    return timing;
}

GYO::Engine::Base::TraceToken SpanBegin(std::string_view category, std::string_view name, std::string_view) noexcept {
    Timing& t = Times();
    std::string text(name);
    const bool kernel = category == "kernel";
    const bool net = kernel && ((t.side == 1 && MatchNet(text)) || (t.side == 2 && ClientNet(text)));
    if (kernel && t.side == 1 && !net && t.counting) t.matchOthers.insert(text);
    t.measured.push_back(net);
    t.names.push_back(std::move(text));
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

// G-8's scenario (ReplicationTests.cpp, Game and BytesPerTick), the server's link digested.
int WireMode() {
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    auto links = N::MemoryLink::Pair({});
    auto wire = std::make_shared<Wire>();
    const GameRules rules{120, true};
    End server = GameEnd(N::NetRole::Server, rules);
    End client = GameEnd(N::NetRole::Client, rules);
    server.net.link = std::make_shared<DigestLink>(links.first, wire);
    server.net.clock = reader;
    server.net.seed = 11;
    client.net.link = std::make_shared<N::MemoryLink>(links.second);
    client.target.server = N::MemoryLink::EndpointA();
    client.target.userId = 5;
    client.net.clock = reader;
    client.net.seed = 7; // G-8 leaves it to std::random_device: fixed here so the digest repeats (the sizes do not depend on it)
    auto s = O::Product::Build(Describe(server), Units(true));
    auto c = O::Product::Build(Describe(client), Units(true));
    if (!s || !c) {
        std::printf("build failed\n");
        return 1;
    }
    std::uint64_t last = 0, total = 0;
    std::string series;
    for (int i = 0; i < 130; ++i) {
        if (!(*s)->RunFrame() || !(*c)->RunFrame()) return 1;
        links.first.Advance();
        *clock += N::NetTime{16'000'000};
        auto* host = (*s)->Buffer().Find<N::NetService>()->Server();
        const auto ids = host->ConnectionIds();
        const std::uint64_t now = ids.empty() ? 0 : host->BytesSentTo(ids.front());
        series += (i ? "," : "") + std::to_string(now - last);
        total += now - last;
        last = now;
    }
    std::printf("G-8 replay: ticks=130 total bytes=%llu series=%s\n", static_cast<unsigned long long>(total), series.c_str());
    std::printf("G-8 replay: server datagram digest=%016llx\n", static_cast<unsigned long long>(wire->digest.value));
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "wire") == 0) return WireMode();
    const int peerCount = argc > 1 ? std::atoi(argv[1]) : 8;
    const int frames = argc > 2 ? std::atoi(argv[2]) : 2000;
    constexpr int kWarmup = 300;
    const auto kPeers = static_cast<std::size_t>(peerCount);
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::vector<N::MemoryLink> serverEnds;
    for (std::size_t i = 0; i < kPeers; ++i) {
        N::MemoryLinkSettings settings;
        settings.seed = static_cast<std::uint64_t>(100 + i);
        links.push_back(N::MemoryLink::Pair(settings));
        serverEnds.push_back(links.back().first);
    }
    auto wire = std::make_shared<Wire>();
    const GameRules rules{1'000'000, false};
    End server = GameEnd(N::NetRole::Server, rules);
    server.net.link = std::make_shared<HubLink>(serverEnds, wire);
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
    for (std::size_t i = 0; i < kPeers; ++i) {
        End client = GameEnd(N::NetRole::Client, rules);
        client.range = E::EntityIdsOptions{static_cast<K::EntityId>(200'001 + i * 100'000), static_cast<K::EntityId>(300'000 + i * 100'000)};
        client.net.link = std::make_shared<N::MemoryLink>(links[i].second);
        client.target.server = N::MemoryLink::EndpointA();
        client.target.userId = static_cast<std::uint64_t>(i + 1);
        client.net.clock = reader;
        client.net.seed = 1000 + i; // fixed so the digest repeats
        auto built = O::Product::Build(Describe(client), Units(true));
        if (!built) {
            std::printf("client: %s\n", Reported(built).c_str());
            return 1;
        }
        clients.push_back(std::move(*built));
    }
    const GYO::Engine::Base::TraceReceiver* previous = GYO::Engine::Base::SetTraceReceiver(&kReceiver);
    std::vector<double> matchUs, clientUs, downBytes;
    std::uint64_t downDatagrams = 0;
    for (int frame = 0; frame < kWarmup + frames; ++frame) {
        Times().matchNs = Times().clientNs = 0;
        Times().counting = frame >= kWarmup;
        std::fill(wire->bytes.begin(), wire->bytes.end(), 0);
        std::fill(wire->datagrams.begin(), wire->datagrams.end(), 0);
        Times().side = 1;
        if (!(*builtServer)->RunFrame()) return 1;
        for (std::size_t i = 0; i < kPeers; ++i) {
            Times().side = i == 0 ? 2 : 0;
            if (!clients[i]->RunFrame()) return 1;
        }
        Times().side = 0;
        for (auto& [a, b] : links) a.Advance();
        *clock += N::NetTime{15'625'000};
        if (frame >= kWarmup) {
            matchUs.push_back(Times().matchNs / 1000.0);
            clientUs.push_back(Times().clientNs / 1000.0);
            for (std::size_t i = 0; i < kPeers; ++i) {
                downBytes.push_back(static_cast<double>(wire->bytes[i]));
                downDatagrams += wire->datagrams[i];
            }
        }
    }
    GYO::Engine::Base::SetTraceReceiver(previous);
    auto& world = (*builtServer)->World("Net").GetWorld();
    std::size_t peers = 0;
    for (const K::EntityId id : world.Entities()) peers += world.Get<N::Peer>(id) != nullptr ? 1 : 0;
    const auto matchState = StateOf(world);
    std::size_t clientsEqual = 0;
    for (auto& c : clients) clientsEqual += StateOf(c->World("Net").GetWorld()).entities.size() == matchState.entities.size() ? 1 : 0;
    std::printf("setup: peers=%zu replicated entities at the Match=%zu clients with as many=%zu/%zu frames=%d warmup=%d\n", peers,
                matchState.entities.size(), clientsEqual, kPeers, frames, kWarmup);
    Report("Match net Systems per tick", matchUs);
    Report("Client 0 net Systems per tick", clientUs);
    for (const auto& [name, ns] : Times().matchBySystem) std::printf("  Match %s: mean %.1f us\n", name.c_str(), ns / 1000.0 / frames);
    for (const auto& [name, ns] : Times().clientBySystem) std::printf("  Client %s: mean %.1f us\n", name.c_str(), ns / 1000.0 / frames);
    std::string others;
    for (const auto& name : Times().matchOthers) others += (others.empty() ? "" : ", ") + name;
    std::printf("  Match other Systems (not counted): %s\n", others.c_str());
    {
        double sum = 0, most = 0;
        for (const double b : downBytes) {
            sum += b;
            most = std::max(most, b);
        }
        const double mean = sum / static_cast<double>(downBytes.size());
        std::printf("downlink per Peer and tick: mean=%.1f bytes max=%.0f bytes datagrams/tick=%.2f (64 Hz: mean %.1f kbps, max %.1f kbps)\n",
                    mean, most, static_cast<double>(downDatagrams) / static_cast<double>(downBytes.size()), mean * 8 * 64 / 1000.0,
                    most * 8 * 64 / 1000.0);
    }
    std::printf("Match datagram digest=%016llx\n", static_cast<unsigned long long>(wire->digest.value));
    return 0;
}
