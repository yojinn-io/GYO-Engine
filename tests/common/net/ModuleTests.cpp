// The net Module in a Product (declaration D-3, D-10; judgements G-1 and the Peer entity's life with a MemoryLink).
#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ModuleTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

template <class Span>
std::vector<std::string> Names(const Span& infos) {
    std::vector<std::string> names;
    for (const auto& info : infos) names.push_back(info.name);
    return names;
}

std::vector<std::string> SystemNames(const K::FrozenRegistry& registry) {
    std::vector<std::string> names;
    for (const auto& system : registry.Systems()) names.push_back(system.name);
    return names;
}

// A manual clock both Products of a case read (the IO layer's time only).
struct Clock {
    std::shared_ptr<N::NetTime> now = std::make_shared<N::NetTime>(0);
    std::function<N::NetTime()> Reader() const {
        auto shared = now;
        return [shared] { return *shared; };
    }
};

struct Pair {
    Clock clock;
    std::pair<N::MemoryLink, N::MemoryLink> links = N::MemoryLink::Pair({});
    End server;
    End client;
    std::unique_ptr<O::Product> serverProduct;
    std::unique_ptr<O::Product> clientProduct;

    Pair() {
        server.net.role = N::NetRole::Server;
        server.net.link = std::make_shared<N::MemoryLink>(links.first);
        server.net.clock = clock.Reader();
        server.net.seed = 7;
        server.role = N::NetWorldOptions{N::NetRole::Server, std::nullopt};
        server.range = {1, 1000};
        client.net.role = N::NetRole::Client;
        client.net.link = std::make_shared<N::MemoryLink>(links.second);
        client.target.server = N::MemoryLink::EndpointA();
        client.target.userId = 42;
        client.net.clock = clock.Reader();
        client.role = N::NetWorldOptions{N::NetRole::Client, std::nullopt};
        client.range = {1001, 2000};
    }
    void Build() {
        auto s = O::Product::Build(Describe(server), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(s), Reported(s));
        serverProduct = std::move(*s);
        auto c = O::Product::Build(Describe(client), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(c), Reported(c));
        clientProduct = std::move(*c);
    }
    void Frames(const int count) {
        for (int i = 0; i < count; ++i) {
            REQUIRE(static_cast<bool>(serverProduct->RunFrame()));
            REQUIRE(static_cast<bool>(clientProduct->RunFrame()));
            links.first.Advance();
            *clock.now += Ms(16);
        }
    }
    K::World& ServerWorld() { return serverProduct->World("Net").GetWorld(); }
    K::World& ClientWorld() { return clientProduct->World("Net").GetWorld(); }
    N::NetService& ClientService() { return *clientProduct->Buffer().Find<N::NetService>(); }
};

} // namespace

TEST_CASE("module: without a role or settings the net Module adds nothing (G-1)") {
    const K::FrozenRegistry* with = nullptr;
    const K::FrozenRegistry* without = nullptr;
    End plain;
    plain.productOptions = false;
    auto withNet = O::Product::Build(Describe(plain), Units(true, &with));
    REQUIRE_MESSAGE(static_cast<bool>(withNet), Reported(withNet));
    auto withoutNet = O::Product::Build(Describe(plain), Units(false, &without));
    REQUIRE_MESSAGE(static_cast<bool>(withoutNet), Reported(withoutNet));
    REQUIRE(with != nullptr);
    REQUIRE(without != nullptr);
    CHECK_MESSAGE(SystemNames(*with) == SystemNames(*without), "a World without a role has the same Systems as one without the net Module");
    CHECK(Names(with->Resources()) == Names(without->Resources()));
    CHECK_MESSAGE(Names(with->Components()) == Names(without->Components()),
                  "a World without a role has the same components as one without the net Module");
    CHECK(Names(with->Messages()) == Names(without->Messages()));
    CHECK_MESSAGE((*withNet)->Buffer().Find<N::NetService>() == nullptr,
                  "a Product without net settings has no net object (no Link, no socket)");
    CHECK(static_cast<bool>((*withNet)->RunFrame()));
}

TEST_CASE("module: a role adds the two apply-point Systems, and a role without settings is reported") {
    const K::FrozenRegistry* registry = nullptr;
    Pair pair;
    auto built = O::Product::Build(Describe(pair.server), Units(true, &registry));
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const auto names = SystemNames(*registry);
    CHECK(std::count(names.begin(), names.end(), "ReceiveNet") == 1);
    CHECK(std::count(names.begin(), names.end(), "SendNet") == 1);
    const auto* receive = registry->SystemNamed("ReceiveNet");
    REQUIRE(receive != nullptr);
    CHECK(receive->mounts.front().schedule == "Main");
    CHECK(receive->mounts.front().stage == "First");
    CHECK(registry->SystemNamed("SendNet")->mounts.front().stage == "Last");

    End orphan = pair.server;
    orphan.productOptions = false;
    const auto refused = O::Product::Build(Describe(orphan), Units(true));
    REQUIRE_FALSE(static_cast<bool>(refused));
    CHECK(Reported(refused).find("NT-E1") != std::string::npos);
}

TEST_CASE("module: a client connects, both Worlds get a Peer, and closing ends the server's with ClosedByRemote") {
    Pair pair;
    pair.Build();
    pair.Frames(4);
    auto serverPeers = PeersOf(pair.ServerWorld());
    REQUIRE(serverPeers.size() == 1);
    CHECK(serverPeers[0].userId == 42);
    CHECK(serverPeers[0].connectionId != 0);
    const auto connected = SeenIn(pair.ServerWorld()).connected;
    REQUIRE(connected.size() == 1);
    CHECK(connected[0].userId == 42);
    CHECK(pair.ServerWorld().Get<N::Peer>(connected[0].peer) != nullptr);
    const auto clientPeers = PeersOf(pair.ClientWorld());
    REQUIRE(clientPeers.size() == 1);
    CHECK(clientPeers[0].connectionId == serverPeers[0].connectionId);

    pair.ClientService().Close();
    pair.Frames(3);
    const auto ended = SeenIn(pair.ServerWorld()).disconnected;
    REQUIRE(ended.size() == 1);
    CHECK_MESSAGE(ended[0].reason == N::DisconnectReason::ClosedByRemote, "the server sees the client's close as ClosedByRemote");
    CHECK(ended[0].peer == connected[0].peer);
    CHECK_MESSAGE(PeersOf(pair.ServerWorld()).empty(), "the server's Peer entity is despawned when its connection ends");
    CHECK(PeersOf(pair.ClientWorld()).empty());
    const auto clientEnded = SeenIn(pair.ClientWorld()).disconnected;
    REQUIRE(clientEnded.size() == 1);
    CHECK(clientEnded[0].reason == N::DisconnectReason::Closed);
}

TEST_CASE("module: a client that stops answering ends with Timeout on the server") {
    Pair pair;
    pair.Build();
    pair.Frames(4);
    REQUIRE(PeersOf(pair.ServerWorld()).size() == 1);
    // Only the server runs from now on; its clock passes the timeout.
    for (int i = 0; i < 3; ++i) {
        *pair.clock.now += std::chrono::seconds(4);
        REQUIRE(static_cast<bool>(pair.serverProduct->RunFrame()));
    }
    const auto ended = SeenIn(pair.ServerWorld()).disconnected;
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == N::DisconnectReason::Timeout);
    CHECK(PeersOf(pair.ServerWorld()).empty());
}

namespace {

struct Health {
    std::int32_t hp = 0;
    friend bool operator==(const Health&, const Health&) = default;
};
inline void HashValue(K::Hasher& h, const Health& v) { HashValue(h, v.hp); }
struct Armor {
    std::int32_t value = 0;
    friend bool operator==(const Armor&, const Armor&) = default;
};
inline void HashValue(K::Hasher& h, const Armor& v) { HashValue(h, v.value); }

void DecidesHealth(O::WorldSetup& s) { A::AuthorityEntry(s).Component<Health>("Health"); }
void DecidesHealthAndArmor(O::WorldSetup& s) {
    A::AuthorityEntry(s).Component<Health>("Health");
    A::AuthorityEntry(s).Component<Armor>("Armor");
}

N::DisconnectReason EndOf(Pair& pair) {
    const auto ended = SeenIn(pair.ClientWorld()).disconnected;
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].peer == 0);
    return ended[0].reason;
}

} // namespace

TEST_CASE("module: with authority strategies the fingerprints are compared; a client without one is not") {
    SUBCASE("the same decision part connects") {
        Pair pair;
        pair.server.authority = std::make_shared<A::ServerAuthorityStrategy>();
        pair.client.authority = std::make_shared<A::ServerAuthorityStrategy>();
        pair.server.build = &DecidesHealth;
        pair.client.build = &DecidesHealth;
        pair.Build();
        pair.Frames(4);
        CHECK(PeersOf(pair.ServerWorld()).size() == 1);
    }
    SUBCASE("another decision part is FingerprintMismatch") {
        Pair pair;
        pair.server.authority = std::make_shared<A::ServerAuthorityStrategy>();
        pair.client.authority = std::make_shared<A::ServerAuthorityStrategy>();
        pair.server.build = &DecidesHealth;
        pair.client.build = &DecidesHealthAndArmor;
        pair.Build();
        pair.Frames(4);
        CHECK(PeersOf(pair.ServerWorld()).empty());
        CHECK(EndOf(pair) == N::DisconnectReason::FingerprintMismatch);
    }
    SUBCASE("a client without a strategy (K1) connects to a server with one") {
        Pair pair;
        pair.server.authority = std::make_shared<A::ServerAuthorityStrategy>();
        pair.server.build = &DecidesHealth;
        pair.Build();
        pair.Frames(4);
        CHECK_MESSAGE(PeersOf(pair.ServerWorld()).size() == 1, "a client without an authority strategy is not compared on the fingerprint");
    }
    SUBCASE("overlapping ranges are IdRangeOverlap, the given range counts over RangeAllotter's") {
        Pair pair;
        pair.client.role->idRange = N::IdRange{500, 600};
        pair.Build();
        pair.Frames(4);
        CHECK(EndOf(pair) == N::DisconnectReason::IdRangeOverlap);
    }
}

namespace {

// A Link whose receive fails the way the test says: a lasting failure, or one datagram too large then nothing.
class FailingReceive final : public N::Link {
public:
    explicit FailingReceive(const N::NetFailureCode code) : code_(code) {}
    N::NetResult<void> Send(N::Endpoint, std::span<const std::byte>) override { return {}; }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(std::span<std::byte>) override {
        ++calls;
        if (code_ == N::NetFailureCode::DatagramTooLarge && calls > 1) return std::optional<N::ReceivedDatagram>{};
        return GYO::Engine::Base::Err(N::NetFailure::Make(code_, "the test's failure"));
    }
    std::uint64_t calls = 0;

private:
    N::NetFailureCode code_;
};

} // namespace

TEST_CASE("module: a lasting Link failure stops the frame's receiving; a datagram too large does not (small thing 3)") {
    N::NetProductOptions options;
    options.role = N::NetRole::Server;
    auto failing = std::make_shared<FailingReceive>(N::NetFailureCode::LinkFailed);
    N::NetService lasting(options, failing, 1, 2);
    lasting.Pump(N::NetTime{0});
    CHECK_MESSAGE(failing->calls == 1, "small thing 3: a lasting failure stops this frame's receiving");
    CHECK(lasting.LinkFailures() == 1);
    auto large = std::make_shared<FailingReceive>(N::NetFailureCode::DatagramTooLarge);
    N::NetService once(options, large, 1, 2);
    once.Pump(N::NetTime{0});
    CHECK_MESSAGE(large->calls == 2, "small thing 3: after a datagram too large the frame keeps receiving");
    CHECK(once.LinkFailures() == 1);
}

TEST_CASE("module: each new handshake of a client carries a larger salt (S2's premise, recheck small thing b)") {
    N::NetProductOptions options;
    options.role = N::NetRole::Client;
    auto links = N::MemoryLink::Pair({});
    options.link = std::make_shared<N::MemoryLink>(links.second);
    N::NetService service(options, options.link, 1, 2);
    const N::NetConnectTarget target{N::MemoryLink::EndpointA(), 1, {}};
    N::HandshakeFacts facts;
    std::uint64_t previous = 0;
    for (int i = 0; i < 4; ++i) {
        // The clock moves on for some handshakes and stands still for others. Each round is a new client World: its
        // target (the closed session is dropped), then its first facts open a new session.
        const N::NetTime now = Ms(i < 2 ? 5 : 5 + i);
        service.SetTarget(&target, now, static_cast<std::uint32_t>(i));
        service.SetFacts(facts, now, false);
        REQUIRE(service.Client() != nullptr);
        const std::uint64_t salt = service.Client()->Request().salt;
        CHECK_MESSAGE(salt > previous, "recheck small thing b: a new handshake's salt is larger than the one before");
        CHECK(service.Client()->Request().epoch == static_cast<std::uint32_t>(i));
        previous = salt;
        service.Close();
        REQUIRE(service.Client()->State() == N::ClientState::Disconnected);
    }
}
