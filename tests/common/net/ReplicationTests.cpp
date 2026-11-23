// Replication (declaration D-4, D-5, D-13, C-MS1): judgement G-5 as declared in section 8 (revision 2; not loosened
// after the results), G-8, the protocol hash, K3's refusal, the World generation, NetLinkFailed and Changed<T>.
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"
#include "engine/rollback/Registrations.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

struct Clock {
    std::shared_ptr<N::NetTime> now = std::make_shared<N::NetTime>(0);
    std::function<N::NetTime()> Reader() const {
        auto shared = now;
        return [shared] { return *shared; };
    }
};

// A server and one client of the sample game over a MemoryLink.
struct Game {
    Clock clock;
    std::pair<N::MemoryLink, N::MemoryLink> links;
    End server;
    End client;
    std::unique_ptr<O::Product> serverProduct;
    std::unique_ptr<O::Product> clientProduct;
    std::vector<std::uint64_t> lostPieces = std::vector<std::uint64_t>(2, 0);

    explicit Game(const N::MemoryLinkSettings& link = {}, const GameRules rules = {}, const bool clientExtra = false)
        : links(N::MemoryLink::Pair(link)), server(GameEnd(N::NetRole::Server, rules)),
          client(GameEnd(N::NetRole::Client, rules, clientExtra)) {
        server.net.link = std::make_shared<N::MemoryLink>(links.first);
        server.net.clock = clock.Reader();
        server.net.seed = 11;
        client.net.link = std::make_shared<N::MemoryLink>(links.second);
        client.net.serverEndpoint = N::MemoryLink::EndpointA();
        client.net.userId = 5;
        client.net.clock = clock.Reader();
        auto& pieces = lostPieces;
        links.first.SetInjectionObserver([&pieces](N::MemoryLink::Injection kind, std::span<const std::byte> datagram) {
            if (kind != N::MemoryLink::Injection::Loss) return;
            const auto summary = N::Connection::Inspect(datagram);
            if (!summary) return;
            for (std::size_t c = 0; c < summary->piecesPerChannel.size() && c < pieces.size(); ++c) pieces[c] += summary->piecesPerChannel[c];
        });
    }
    void Build() {
        auto s = O::Product::Build(Describe(server), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(s), Reported(s));
        serverProduct = std::move(*s);
        auto c = O::Product::Build(Describe(client), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(c), Reported(c));
        clientProduct = std::move(*c);
    }
    void ServerFrame() { REQUIRE(static_cast<bool>(serverProduct->RunFrame())); }
    void ClientFrame() {
        REQUIRE(static_cast<bool>(clientProduct->RunFrame()));
        links.first.Advance();
        *clock.now += Ms(16);
    }
    K::World& ServerWorld() { return serverProduct->World("Net").GetWorld(); }
    K::World& ClientWorld() { return clientProduct->World("Net").GetWorld(); }
    std::uint64_t ServerTick() { return ServerWorld().Resource<W::Tick>()->value; }
    const N::NetReplicaState& Replica() { return *ClientWorld().Resource<N::NetReplicaState>(); }
    N::PeerLedger LedgerOfFirstPeer() {
        for (const K::EntityId id : ServerWorld().Entities()) {
            if (const auto* ledger = ServerWorld().Get<N::PeerLedger>(id)) return *ledger;
        }
        return {};
    }
    N::NetService& ServerService() { return *serverProduct->Buffer().Find<N::NetService>(); }
    N::NetService& ClientService() { return *clientProduct->Buffer().Find<N::NetService>(); }
};

N::MemoryLinkSettings Lossy() {
    N::MemoryLinkSettings settings;
    settings.seed = 0x5EED0005u;
    settings.aToB = {2000, 1000, 2500, 3, 0};
    settings.bToA = {2000, 1000, 2500, 3, 0};
    return settings;
}

std::string Drops(const N::NetReplicaState& state) {
    const char* names[] = {"StaleGeneration", "StaleEpoch", "Malformed", "SpawnRejected", "MissingTarget", "StaleValue", "BeyondDepth"};
    std::string out;
    for (std::size_t i = 0; i < N::kReplicaDropCount; ++i) out += std::string(i ? " " : "") + names[i] + "=" + std::to_string(state.drops[i]);
    return out;
}

} // namespace

TEST_CASE("replication: G-5 a display client follows the server through losses, duplicates and reordering") {
    Game game(Lossy());
    game.Build();
    std::map<std::uint64_t, GameState> record;
    std::optional<std::uint32_t> observed;
    std::uint64_t observations = 0, mismatches = 0, firstTick = 0;
    std::string firstMismatch;
    const std::uint64_t lastTick = GameRules{}.lastTick;
    while (game.ServerTick() < lastTick) {
        game.ServerFrame();
        record[game.ServerTick()] = StateOf(game.ServerWorld());
        game.ClientFrame();
        const auto& replica = game.Replica();
        if (firstTick == 0 && replica.latestConfirmed) firstTick = *replica.latestConfirmed;
        // (1) D-7's two conditions hold for T, and nothing newer than T was written yet.
        if (replica.latestConfirmed && *replica.latestConfirmed == replica.maxAppliedTick && observed != replica.latestConfirmed) {
            observed = replica.latestConfirmed;
            ++observations;
            const auto expected = record.find(*observed);
            REQUIRE(expected != record.end());
            const std::string difference = Difference(StateOf(game.ClientWorld()), expected->second);
            if (!difference.empty()) {
                ++mismatches;
                if (firstMismatch.empty()) firstMismatch = "tick " + std::to_string(*observed) + ": " + difference;
            }
        }
    }
    const std::uint64_t ticks = lastTick - firstTick + 1;
    MESSAGE("G-5 (1): observed ticks=" << observations << " of " << ticks << " ("
                                       << (100.0 * static_cast<double>(observations) / static_cast<double>(ticks))
                                       << " %), mismatches=" << mismatches << " " << firstMismatch);
    REQUIRE_MESSAGE(observations * 10 >= ticks, "G-5 (1): the observation holds at 10 % of the ticks or more");
    CHECK_MESSAGE(mismatches == 0, "G-5 (1): at each observed tick T the client equals the server's tick T");

    // (2) The server stops changing; the link drains and everything is confirmed; compare again.
    int drain = 0;
    for (; drain < 2000; ++drain) {
        game.ServerFrame();
        game.ClientFrame();
        const auto ledger = game.LedgerOfFirstPeer();
        const bool settled = game.links.first.InFlightToSelf() == 0 && game.links.second.InFlightToSelf() == 0 &&
                             ledger.confirmationsReceived == game.Replica().confirmationsSent &&
                             StateOf(game.ClientWorld()) == StateOf(game.ServerWorld());
        if (settled && drain > 20) break;
    }
    const GameState final = StateOf(game.ServerWorld());
    MESSAGE("G-5 (2): drain frames=" << drain << " server entities=" << final.entities.size()
                                     << " positions=" << final.positions.size() << " healths=" << final.healths.size()
                                     << " targets=" << final.targets.size() << " doors=" << final.doors.size());
    CHECK_MESSAGE(Difference(StateOf(game.ClientWorld()), final).empty(), "G-5 (2): after the drain the client equals the server");
    CHECK(final.targets.size() > 0);

    // (3) Server messages: every one arrived after the entity it points to.
    const auto& log = *game.ClientWorld().Resource<ClientLog>();
    MESSAGE("G-5 (3): server messages handed out=" << log.messages << " before their entity=" << log.messagesBeforeEntity);
    CHECK(log.messages > 0);
    CHECK_MESSAGE(log.messagesBeforeEntity == 0, "G-5 (3): a server message is handed out after the entity it points to exists");
    CHECK_MESSAGE(log.positionsChanged > 0, "the game reads the replicated writes with Changed<T>");

    // (4) The counts.
    const auto ledger = game.LedgerOfFirstPeer();
    const auto& replica = game.Replica();
    MESSAGE("G-5 (4): structures sent=" << ledger.structuresSent << " applied=" << replica.structuresApplied
                                        << " | server messages sent=" << ledger.serverMessagesSent << " received="
                                        << replica.serverMessagesReceived << " delivered=" << replica.serverMessagesDelivered
                                        << " | value parts sent=" << ledger.valuePartsSent << " received=" << replica.valuePartsReceived
                                        << " processed=" << replica.valuePartsProcessed << " pending=" << replica.pendingValues.size()
                                        << " lost with datagrams=" << game.lostPieces[N::kValuesChannel]
                                        << " | confirmations sent=" << replica.confirmationsSent << " received="
                                        << ledger.confirmationsReceived << " | confirmed ticks=" << replica.confirmedTicks
                                        << " | drops: " << Drops(replica));
    CHECK_MESSAGE(ledger.structuresSent == replica.structuresApplied, "G-5 (4): structures sent == applied");
    CHECK_MESSAGE(ledger.serverMessagesSent == replica.serverMessagesReceived, "G-5 (4): server messages sent == received");
    CHECK(replica.serverMessagesReceived == replica.serverMessagesDelivered + replica.drops[static_cast<std::size_t>(N::ReplicaDrop::Malformed)]);
    CHECK_MESSAGE(ledger.valuePartsSent == replica.valuePartsReceived + game.lostPieces[N::kValuesChannel],
                  "G-5 (4): value parts sent == received + lost with the injected losses");
    CHECK_MESSAGE(replica.valuePartsReceived ==
                      replica.valuePartsProcessed + replica.pendingValues.size() +
                          replica.drops[static_cast<std::size_t>(N::ReplicaDrop::StaleGeneration)] +
                          replica.drops[static_cast<std::size_t>(N::ReplicaDrop::StaleEpoch)],
                  "G-5 (4): value parts received == processed + pending + dropped");
    CHECK_MESSAGE(replica.confirmationsSent == ledger.confirmationsReceived, "G-5 (4): confirmations sent == received");
    CHECK(replica.drops[static_cast<std::size_t>(N::ReplicaDrop::SpawnRejected)] == 0);
    CHECK_MESSAGE(replica.drops[static_cast<std::size_t>(N::ReplicaDrop::Malformed)] == 0, "no replication message is malformed");
}

TEST_CASE("replication: a tick without changes still sends a part, and a confirmed value is not sent again") {
    // The rules stop at tick 10: afterwards nothing changes.
    Game game({}, GameRules{10, true});
    game.Build();
    for (int i = 0; i < 40; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    const auto before = game.LedgerOfFirstPeer();
    REQUIRE(before.valuePartsSent > 0);
    game.ServerFrame();
    game.ClientFrame();
    const auto after = game.LedgerOfFirstPeer();
    CHECK_MESSAGE(after.valuePartsSent == before.valuePartsSent + 1, "every tick sends at least one value part");
    // Everything was confirmed long ago: the part of this tick carries no entity.
    REQUIRE_FALSE(after.pending.empty());
    CHECK_MESSAGE(after.pending.rbegin()->second.entities.empty(), "a confirmed value is not sent again");
    CHECK(StateOf(game.ClientWorld()) == StateOf(game.ServerWorld()));
}

TEST_CASE("replication: two ends that replicate differently do not connect (ProtocolMismatch)") {
    Game game({}, {}, true);
    game.Build();
    for (int i = 0; i < 4; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    const auto& seen = SeenIn(game.ClientWorld()).disconnected;
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].reason == N::DisconnectReason::ProtocolMismatch);
    CHECK(seen[0].detail.find("protocol hash") != std::string::npos);
}

TEST_CASE("replication: a client with a rollback strategy (K3) is built (NT-E5 is gone with step 4)") {
    End client = GameEnd(N::NetRole::Client, {});
    client.net.link = std::make_shared<N::MemoryLink>(N::MemoryLink::Pair({}).second);
    // A registration a rollback strategy takes (the sample game writes DoorOpened in the step and reads it outside:
    // RB-E8); the K3 game itself is in RollbackClientTests.cpp.
    client.build = [](O::WorldSetup& s) {
        s.Registrar().Component<Position>({.name = "Position"});
        N::NetEntry(s).Replicate<Position>("Position", PositionCodec());
        GYO::Engine::Rollback::RollbackEntry(s).StepSchedule();
    };
    auto description = Describe(client);
    GYO::Engine::Rollback::RollbackOptions rollback;
    rollback.strategy = std::make_shared<GYO::Engine::Rollback::StateRollbackStrategy>();
    description.worlds[0].options.Set(rollback);
    const auto built = O::Product::Build(std::move(description), Units(true));
    CHECK_MESSAGE(static_cast<bool>(built), "a client with a rollback strategy is built (K3): " << Reported(built));
}

namespace {
// A Resource that rolls back, replicated through a value type of its own (V != R, Q8 (b)'s shape).
struct Level {
    std::int32_t value = 0;
    friend bool operator==(const Level&, const Level&) = default;
};
inline void HashValue(K::Hasher& h, const Level& v) { HashValue(h, v.value); }
} // namespace

TEST_CASE("replication: K3 refuses a rolling-back Resource whose value type is not the Resource (NT-E6, B2)") {
    End client = GameEnd(N::NetRole::Client, {});
    client.net.link = std::make_shared<N::MemoryLink>(N::MemoryLink::Pair({}).second);
    client.build = [](O::WorldSetup& s) {
        s.Registrar().Component<Position>({.name = "Position"});
        s.Registrar().Resource<Level>({.name = "Level"});
        N::NetEntry net(s);
        net.Replicate<Position>("Position", PositionCodec());
        N::NetEntry::ResourceOptions<Level, std::int32_t> options;
        options.codec = {[](N::NetWriter& w, const std::int32_t& v) { w.Value(v); }, [](N::NetReader& r, std::int32_t& v) { return r.Value(v); }, 1};
        options.read = [](const Level& level) { return std::optional<std::int32_t>{level.value}; };
        options.apply = [](N::ReplicaWorld& world, const std::int32_t& v) -> K::SystemResult {
            if (auto set = world.SetResource(Level{v}); !set) return GYO::Engine::Base::Err(std::move(set).error());
            return {};
        };
        net.ReplicateResource<Level, std::int32_t>("Level", std::move(options));
        GYO::Engine::Rollback::RollbackEntry rollback(s);
        rollback.StepSchedule();
        rollback.Resource<Level>();
    };
    auto description = Describe(client);
    GYO::Engine::Rollback::RollbackOptions rollback;
    rollback.strategy = std::make_shared<GYO::Engine::Rollback::StateRollbackStrategy>();
    description.worlds[0].options.Set(rollback);
    const auto refused = O::Product::Build(std::move(description), Units(true));
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(refused), "B2: K3 would write a rolling-back Resource in First through its apply");
    CHECK_MESSAGE(Reported(refused).find("NT-E6") != std::string::npos, "B2: the refusal is NT-E6: " << Reported(refused));
}

namespace {

struct Score {
    std::int32_t value = 0;
    friend bool operator==(const Score&, const Score&) = default;
};
inline void HashValue(K::Hasher& h, const Score& v) { HashValue(h, v.value); }

K::SystemResult RaiseScore(K::Query<K::Write<Score>> scores) {
    scores.ForEach([](K::EntityId, K::Mut<Score> score) { ++score->value; });
    return {};
}

} // namespace

TEST_CASE("replication: logic with OnServer() runs at the server only, and both ends keep one fingerprint") {
    Game game;
    game.server.authority = std::make_shared<A::ServerAuthorityStrategy>();
    game.client.authority = std::make_shared<A::ServerAuthorityStrategy>();
    // A decision component written by a System registered at both ends with OnServer() (D-13).
    const auto decide = [](O::WorldSetup& s) {
        RegisterGame(s, GameRules{});
        A::AuthorityEntry(s).Component<Score>("Score");
        s.Registrar().AddSystem<&RaiseScore>(Fixed("RaiseScore", true));
    };
    game.server.build = decide;
    game.client.build = decide;
    game.Build();
    for (int i = 0; i < 12; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    CHECK_MESSAGE(PeersOf(game.ServerWorld()).size() == 1, "the same System with OnServer() at both ends keeps one fingerprint");
    CHECK(StateOf(game.ClientWorld()).doors.size() == 3);
}

TEST_CASE("replication: OnServer() is false at a client") {
    Game game({}, GameRules{1, false});
    game.Build();
    for (int i = 0; i < 8; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    // With the rules over at tick 1, the server spawned one door and two players once; the client has exactly those,
    // which it would not if SpawnMap had run there too.
    const GameState client = StateOf(game.ClientWorld());
    CHECK_MESSAGE(client.entities.size() == 3, "OnServer() logic does not run at the client");
    CHECK(client == StateOf(game.ServerWorld()));
}

namespace {

K::SystemResult KeepReplaced(K::MessageReader<N::ServerWorldReplaced> replaced, K::ResMut<ClientLog> log) {
    for (const N::ServerWorldReplaced* notice : replaced.Read()) log->replacedGeneration = notice->generation;
    return {};
}

} // namespace

TEST_CASE("replication: a replaced server World is noticed, and a replaced client World gets everything again") {
    Game game;
    game.client.build = [](O::WorldSetup& s) {
        RegisterGame(s, GameRules{});
        s.Registrar().AddSystem<&KeepReplaced>(N::Detail::MountedAt("KeepReplaced", "Update"));
    };
    game.Build();
    for (int i = 0; i < 20; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    REQUIRE(StateOf(game.ClientWorld()).doors.size() == 3);
    REQUIRE(static_cast<bool>(game.serverProduct->ReplaceWorld("Net", {})));
    for (int i = 0; i < 6; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    CHECK_MESSAGE(game.ClientWorld().Resource<ClientLog>()->replacedGeneration == 2,
                  "the client is told that the server's World was replaced");
    // The product replaces its World too; the new one asks for everything again.
    REQUIRE(static_cast<bool>(game.clientProduct->ReplaceWorld("Net", {})));
    for (int i = 0; i < 30; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    CHECK_MESSAGE(StateOf(game.ClientWorld()) == StateOf(game.ServerWorld()),
                  "a replaced client World gets the server's World again");
    CHECK(PeersOf(game.ServerWorld()).size() == 1);
}

namespace {

// A Link whose sends fail.
class BrokenLink final : public N::Link {
public:
    N::NetResult<void> Send(N::Endpoint, std::span<const std::byte>) override {
        return GYO::Engine::Base::Err(N::NetFailure::Make(N::NetFailureCode::LinkFailed, "the cable is cut"));
    }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(std::span<std::byte>) override {
        return std::optional<N::ReceivedDatagram>{};
    }
};

K::SystemResult KeepLinkFailures(K::MessageReader<N::NetLinkFailed> failed, K::ResMut<ClientLog> log) {
    log->messages += failed.Read().size();
    return {};
}

} // namespace

TEST_CASE("replication: a failing Link is told with NetLinkFailed and the World goes on") {
    End client = GameEnd(N::NetRole::Client, {});
    client.net.link = std::make_shared<BrokenLink>();
    client.build = [](O::WorldSetup& s) {
        RegisterGame(s, GameRules{});
        s.Registrar().AddSystem<&KeepLinkFailures>(N::Detail::MountedAt("KeepLinkFailures", "Update"));
    };
    auto built = O::Product::Build(Describe(client), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    for (int i = 0; i < 3; ++i) REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE((*built)->World("Net").GetWorld().Resource<ClientLog>()->messages > 0, "a Link failure is told with NetLinkFailed");
    CHECK((*built)->Buffer().Find<N::NetService>()->LinkFailures() > 0);
}

namespace {

std::vector<std::uint64_t> BytesPerTick() {
    Game game({}, GameRules{120, true});
    game.Build();
    std::vector<std::uint64_t> bytes;
    std::uint64_t last = 0;
    for (int i = 0; i < 130; ++i) {
        game.ServerFrame();
        game.ClientFrame();
        auto* server = game.ServerService().Server();
        const auto ids = server->ConnectionIds();
        const std::uint64_t now = ids.empty() ? 0 : server->BytesSentTo(ids.front());
        bytes.push_back(now - last);
        last = now;
    }
    return bytes;
}

} // namespace

TEST_CASE("replication: G-8 the bytes per tick and Peer are the same in two runs") {
    const auto first = BytesPerTick();
    const auto second = BytesPerTick();
    CHECK_MESSAGE(first == second, "G-8: two runs send the same bytes per tick");
    std::uint64_t total = 0, most = 0;
    for (const auto b : first) {
        total += b;
        most = std::max(most, b);
    }
    std::string series;
    for (std::size_t i = 0; i < first.size(); ++i) series += (i ? "," : "") + std::to_string(first[i]);
    MESSAGE("G-8: ticks=" << first.size() << " total bytes=" << total << " max per tick=" << most
                          << " mean=" << (static_cast<double>(total) / static_cast<double>(first.size())) << " series=" << series);
}

TEST_CASE("replication: a value sent again unchanged is not written, so Changed<T> tells only real changes (CCR-8)") {
    // Confirmations get through rarely: the server sends the same values again and again.
    N::MemoryLinkSettings link;
    link.seed = 8;
    link.bToA = {8000, 0, 0, 0, 0};
    Game game(link, GameRules{300, true});
    game.Build();
    // Play until the server's last change (tick 300) is written at the client and the client equals the server.
    for (int i = 0; i < 800 && !(game.Replica().maxAppliedTick >= 300 && StateOf(game.ClientWorld()) == StateOf(game.ServerWorld())); ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    REQUIRE(StateOf(game.ClientWorld()) == StateOf(game.ServerWorld()));
    const std::uint64_t changedWhilePlaying = game.ClientWorld().Resource<ClientLog>()->positionsChanged;
    // From now on nothing changes at the server; what the client gets are the same values sent again.
    const std::uint64_t changedBefore = changedWhilePlaying;
    const std::uint64_t unchangedBefore = game.Replica().unchangedNotWritten;
    const std::uint64_t processedBefore = game.Replica().valuePartsProcessed;
    for (int i = 0; i < 60; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
    const std::uint64_t changedAfter = game.ClientWorld().Resource<ClientLog>()->positionsChanged;
    MESSAGE("CCR-8: Changed<Position> while playing=" << changedWhilePlaying << ", while only resent=" << (changedAfter - changedBefore)
                                                      << "; values not written because unchanged=" << game.Replica().unchangedNotWritten
                                                      << " (" << (game.Replica().unchangedNotWritten - unchangedBefore)
                                                      << " while watching); value parts processed while watching="
                                                      << (game.Replica().valuePartsProcessed - processedBefore));
    CHECK(changedWhilePlaying > 0);
    CHECK(game.Replica().unchangedNotWritten > unchangedBefore);
    CHECK_MESSAGE(changedAfter == changedBefore, "a value sent again unchanged does not make Changed<T> true (CCR-8)");
}

// ---- The fixes of the design review before PR-1's merge (dev_log 2026-11-23 review_fixes). Each case first
// failed on the code before its fix (AGENTS §14.3).

namespace {

std::uint64_t DropOf(const std::array<std::uint64_t, N::kReplicaDropCount>& drops, const N::ReplicaDrop reason) {
    return drops[static_cast<std::size_t>(reason)];
}

void Frames(Game& game, const int count) {
    for (int i = 0; i < count; ++i) {
        game.ServerFrame();
        game.ClientFrame();
    }
}

// A message the test writes on the client's connection, as a client could send it.
std::vector<std::byte> ClientMessage(const std::uint8_t kind, const std::uint32_t generation, const std::uint32_t epoch,
                                     const std::vector<std::uint32_t>& rest = {}) {
    std::vector<std::byte> body;
    N::NetWriter writer(body);
    writer.Value(kind);
    writer.Value(generation);
    writer.Value(epoch);
    for (const std::uint32_t value : rest) writer.Value(value);
    return body;
}

} // namespace

TEST_CASE("replication: a replaced client World resyncs: the server takes its confirmations again, the old epoch's messages are dropped (B1)") {
    Game game;
    game.Build();
    Frames(game, 20);
    for (int world = 2; world <= 3; ++world) {
        CAPTURE(world);
        REQUIRE(static_cast<bool>(game.clientProduct->ReplaceWorld("Net", {})));
        Frames(game, 30);
        const auto before = game.LedgerOfFirstPeer().confirmationsReceived;
        Frames(game, 10);
        CHECK_MESSAGE(game.LedgerOfFirstPeer().confirmationsReceived > before,
                      "B1: after a resync the server takes the client's confirmations again");
        CHECK_MESSAGE(DropOf(game.Replica().drops, N::ReplicaDrop::StaleEpoch) > 0,
                      "B1: the messages of the client's old epoch are dropped and counted");
        CHECK(StateOf(game.ClientWorld()) == StateOf(game.ServerWorld()));
    }
}

namespace {

struct SentMessages {
    std::uint64_t frames = 0;
    std::uint64_t sent = 0;
};
inline void HashValue(K::Hasher&, const SentMessages&) {}

// At the server, Main/Update: one DoorOpened a frame for 40 frames once a client is connected.
K::SystemResult SendEveryFrame(K::Res<N::NetWorldLink> link, K::ResMut<SentMessages> log, K::MessageWriter<DoorOpened> opened) {
    if (link->peers.empty() || log.Read().frames >= 40) return {};
    ++log->frames;
    opened.Send(DoorOpened{0, 1});
    ++log->sent;
    return {};
}

} // namespace

TEST_CASE("replication: a server message written in a frame without a fixed step goes out with the next tick (B2)") {
    // The rules stop at tick 1: no other System writes DoorOpened.
    Game game({}, GameRules{1, false});
    const GameRules rules{1, false};
    game.server.build = [rules](O::WorldSetup& s) {
        RegisterGame(s, rules);
        s.Registrar().Resource<SentMessages>({.name = "SentMessages"});
        K::SystemOptions options = N::Detail::MountedAt("SendEveryFrame", "Update");
        options.mounts.front().conditions.push_back(N::OnServer());
        s.Registrar().AddSystem<&SendEveryFrame>(std::move(options));
        s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            world.SetResource(SentMessages{});
            world.SetResource(W::TimeSampling{7'812'500, W::SamplingMode::ManualDelta});
            return {};
        });
    };
    game.Build();
    Frames(game, 120);
    const auto sent = game.ServerWorld().Resource<SentMessages>()->sent;
    REQUIRE(sent == 40);
    CHECK_MESSAGE(game.LedgerOfFirstPeer().serverMessagesSent == sent, "B2: every server message written goes out, steps or not");
    CHECK(game.ClientWorld().Resource<ClientLog>()->messages == sent);
}

namespace {

// The sample game's replication (the same registrations as RegisterGame, so the protocol hash is the same) with only
// MoveAll: no structure changes after the first.
void RegisterMovesOnly(O::WorldSetup& s, const GameRules rules) {
    auto& r = s.Registrar();
    r.Component<Position>({.name = "Position"});
    r.Component<Health>({.name = "Health"});
    r.Component<Target>({.name = "Target"});
    r.Component<Door>({.name = "Door"});
    r.Message<DoorOpened>({.name = "DoorOpened"});
    r.Resource<GameRules>({.name = "GameRules"});
    r.Resource<ClientLog>({.name = "ClientLog"});
    N::NetEntry net(s);
    net.Replicate<Position>("Position", PositionCodec());
    net.Replicate<Health>("Health", HealthCodec());
    net.Replicate<Target>("Target", TargetCodec());
    net.Replicate<Door>("Door", DoorCodec());
    net.ServerMessage<DoorOpened>("DoorOpened", DoorOpenedCodec());
    r.AddSystem<&MoveAll>(Fixed("MoveAll", true));
    s.Prepare([rules](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(rules);
        world.SetResource(ClientLog{});
        return {};
    });
}

} // namespace

TEST_CASE("replication: a value part needing the structure of tick 0 waits for it (S1)") {
    Game game({}, GameRules{1000, false});
    const GameRules rules{1000, false};
    // Each server World starts with five entities of its own ids (put by a prepare function; the client keeps the
    // first World's when the server's is replaced) and runs no step in its first frame, so its only structure update
    // is the one of tick 0; afterwards only the positions change.
    auto builds = std::make_shared<K::EntityId>(0);
    game.server.build = [rules, builds](O::WorldSetup& s) {
        RegisterMovesOnly(s, rules);
        const K::EntityId base = 50'000 + 10'000 * (*builds)++;
        s.Prepare([base](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            world.SetResource(W::TimeSampling{7'812'500, W::SamplingMode::ManualDelta});
            for (K::EntityId id = base + 1; id <= base + 5; ++id) {
                world.Insert(id, Position{static_cast<std::int32_t>(id), 0});
                world.Insert(id, Health{100});
            }
            return {};
        });
    };
    game.Build();
    Frames(game, 30);
    const std::uint64_t missingBefore = DropOf(game.Replica().drops, N::ReplicaDrop::MissingTarget);
    // The new World's first frame (tick 0): its structure update is lost on the way and resent later; the value part
    // of tick 1, which needs it, arrives first.
    game.links.first.SetOutgoingImpairment({10000, 0, 0, 0, 0});
    REQUIRE(static_cast<bool>(game.serverProduct->ReplaceWorld("Net", {})));
    game.ServerFrame();
    game.ClientFrame();
    game.links.first.SetOutgoingImpairment({0, 0, 0, 0, 0});
    Frames(game, 40);
    CHECK_MESSAGE(DropOf(game.Replica().drops, N::ReplicaDrop::MissingTarget) == missingBefore,
                  "S1: no value is written before the structure of tick 0 it needs");
    CHECK(StateOf(game.ClientWorld()).positions.count(60'001) == 1);
}

TEST_CASE("replication: a reliable send the connection refuses does not move the ledger on (S3)") {
    Game game(Lossy(), GameRules{120, true});
    // One reliable message at a time: a structure update or a server message is refused while another is unacked.
    game.server.net.session.transport.sendQueueCapacity = 1;
    game.Build();
    while (game.ServerTick() < 120) Frames(game, 1);
    for (int drain = 0; drain < 600; ++drain) {
        Frames(game, 1);
        if (drain > 60 && StateOf(game.ClientWorld()) == StateOf(game.ServerWorld())) break;
    }
    const auto ledger = game.LedgerOfFirstPeer();
    MESSAGE("S3: reliable sends refused=" << ledger.reliableRejected);
    REQUIRE_MESSAGE(ledger.reliableRejected > 0, "S3: the queue refused reliable sends");
    CHECK_MESSAGE(Difference(StateOf(game.ClientWorld()), StateOf(game.ServerWorld())).empty(),
                  "S3: a refused structure update is sent again, so the client ends equal to the server");
}

TEST_CASE("replication: what the server drops from a client is counted on its Peer (S4, small thing 2)") {
    Game game({}, GameRules{1, false});
    game.Build();
    Frames(game, 20);
    N::Connection* up = game.ClientService().Client()->Transport();
    REQUIRE(up != nullptr);
    const std::uint32_t generation = game.ServerService().Generation();
    // Cut short; an unknown kind; a confirmation of another epoch; an input message cut short.
    REQUIRE(up->Send(N::kReliableChannel, std::vector<std::byte>{std::byte{2}}).has_value());
    REQUIRE(up->Send(N::kReliableChannel, ClientMessage(99, generation, 0)).has_value());
    REQUIRE(up->Send(N::kReliableChannel, ClientMessage(2, generation, 7, {0})).has_value());
    REQUIRE(up->Send(N::kValuesChannel, std::vector<std::byte>{std::byte{6}}).has_value());
    Frames(game, 5);
    const auto ledger = game.LedgerOfFirstPeer();
    CHECK_MESSAGE(DropOf(ledger.drops, N::ReplicaDrop::Malformed) == 3, "S4: unreadable messages and unknown kinds are counted");
    CHECK_MESSAGE(DropOf(ledger.drops, N::ReplicaDrop::StaleEpoch) == 1, "S4: a confirmation of another epoch is counted");
    // A confirmation of another generation, then an input message of another generation (each checked on its own).
    REQUIRE(up->Send(N::kReliableChannel, ClientMessage(2, generation + 5, 0, {0})).has_value());
    Frames(game, 5);
    const std::uint64_t afterConfirm = DropOf(game.LedgerOfFirstPeer().drops, N::ReplicaDrop::StaleGeneration);
    CHECK_MESSAGE(afterConfirm == 1, "small thing 2: a confirmation of another generation is dropped and counted");
    REQUIRE(up->Send(N::kValuesChannel, ClientMessage(6, generation + 5, 0)).has_value());
    Frames(game, 5);
    CHECK_MESSAGE(DropOf(game.LedgerOfFirstPeer().drops, N::ReplicaDrop::StaleGeneration) - afterConfirm == 1,
                  "small thing 2: an input message of another generation is dropped and counted");
}

TEST_CASE("replication: a structure update for an entity the client does not have is counted (S4)") {
    Game game({}, GameRules{1, false});
    game.Build();
    Frames(game, 20);
    const std::uint64_t before = DropOf(game.Replica().drops, N::ReplicaDrop::MissingTarget);
    const auto ids = game.ServerService().Server()->ConnectionIds();
    REQUIRE(ids.size() == 1);
    // A structure update (as the server sends it) changing an entity the client has not got: one Position set.
    std::vector<std::byte> body;
    N::NetWriter writer(body);
    writer.Value(std::uint8_t{1});
    writer.Value(game.ServerService().Generation());
    writer.Value(game.LedgerOfFirstPeer().epoch);
    writer.Value(static_cast<std::uint32_t>(game.ServerTick()));
    writer.Value(std::uint32_t{1});
    writer.Value(K::EntityId{777'777});
    writer.Value(std::uint8_t{0});
    writer.Value(std::uint8_t{0});
    writer.Value(std::uint8_t{1});
    writer.Value(std::uint16_t{0});
    writer.Value(std::uint32_t{8});
    writer.Value(std::int32_t{1});
    writer.Value(std::int32_t{2});
    REQUIRE(game.ServerService().Server()->Find(ids[0])->Send(N::kReliableChannel, body).has_value());
    Frames(game, 3);
    CHECK_MESSAGE(DropOf(game.Replica().drops, N::ReplicaDrop::MissingTarget) == before + 1,
                  "S4: a structure update's component for a missing entity is counted");
}

// ---- The narrow recheck (R2): the epoch goes with the connection.

TEST_CASE("replication: client and server replace their Worlds within one latency: the confirmations still count (R2 a)") {
    N::MemoryLinkSettings slow;
    slow.aToB.fixedDelaySteps = 3;
    slow.bToA.fixedDelaySteps = 3;
    // The rules stop at tick 60 of each server World, so the client can be compared once the link has drained.
    Game game(slow, GameRules{60, true});
    game.Build();
    Frames(game, 30);
    REQUIRE(static_cast<bool>(game.serverProduct->ReplaceWorld("Net", {})));
    game.ServerFrame();
    REQUIRE(static_cast<bool>(game.clientProduct->ReplaceWorld("Net", {})));
    game.ClientFrame();
    Frames(game, 40);
    const auto before = game.LedgerOfFirstPeer().confirmationsReceived;
    Frames(game, 10);
    CHECK_MESSAGE(game.LedgerOfFirstPeer().confirmationsReceived > before,
                  "R2 a: the server takes the confirmations after both Worlds were replaced close together");
    while (game.ServerTick() < 60) Frames(game, 1);
    Frames(game, 20);
    CHECK(StateOf(game.ClientWorld()) == StateOf(game.ServerWorld()));
    // A later server World: its ledger starts with the epoch the client World set on this connection with its resync.
    REQUIRE(static_cast<bool>(game.serverProduct->ReplaceWorld("Net", {})));
    Frames(game, 30);
    const auto later = game.LedgerOfFirstPeer().confirmationsReceived;
    Frames(game, 10);
    CHECK_MESSAGE(game.LedgerOfFirstPeer().confirmationsReceived > later,
                  "R2 a: a later server World starts with the connection's epoch, so the confirmations count");
}

TEST_CASE("replication: after a reconnect the server's first whole World is taken, not dropped as another epoch (R2 b)") {
    Game game;
    game.Build();
    Frames(game, 20);
    game.ClientService().Close();
    REQUIRE(static_cast<bool>(game.clientProduct->ReplaceWorld("Net", {})));
    Frames(game, 40);
    CHECK_MESSAGE(DropOf(game.Replica().drops, N::ReplicaDrop::StaleEpoch) == 0,
                  "R2 b: the new connection's messages carry the new World's epoch from the start");
    CHECK(StateOf(game.ClientWorld()) == StateOf(game.ServerWorld()));
}
