// The connection layer (net Module declaration D-6, D-8): judgements G-3 (the handshake) and G-4 (liveness), as
// declared in section 8. The client session and the server host are pure state machines: the test carries the
// datagrams and gives the time, so each case decides exactly what arrives when.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "NetTestSupport.hpp"
#include "engine/net/ClientSession.hpp"
#include "engine/net/ServerHost.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

constexpr N::Endpoint kClient{5};

N::HandshakeFacts ServerFacts() {
    N::HandshakeFacts facts;
    facts.protocolHash = 0x1234;
    facts.stepNanoseconds = 15'625'000;
    facts.idRange = {1, 1000};
    facts.fingerprint = std::string("1:server-fingerprint");
    return facts;
}

// A client World with an authority strategy (K2, K3): the same facts but its own range.
N::HandshakeFacts ClientFacts() {
    N::HandshakeFacts facts = ServerFacts();
    facts.idRange = {1001, 2000};
    return facts;
}

N::SessionSettings Settings() {
    N::SessionSettings settings;
    settings.transport = TwoChannels(0);
    return settings;
}

struct Wire {
    N::ServerHost& host;
    N::ClientSession& client;

    // One frame: the client's datagrams to the server, then the server's to the client.
    void Round(const N::NetTime now, const N::Stamp stamp = {}) {
        for (const auto& datagram : client.Collect(now, stamp)) host.Receive(kClient, datagram, now);
        for (const auto& [to, datagram] : host.Collect(now, stamp)) {
            if (to == kClient) client.Receive(datagram, now);
        }
    }
};

// The rejection a client with these facts gets from a server with ServerFacts() (nullopt: it connects).
std::optional<N::Rejection> Shake(const N::HandshakeFacts& client, const N::HandshakeFacts& server = ServerFacts(),
                                  N::ServerHost::Admission admission = {}) {
    N::ServerHost host(Settings(), server, 77, 99, std::move(admission));
    N::ClientSession session(Settings(), N::ConnectRequest{client, 42, {}, 0}, Ms(0));
    Wire wire{host, session};
    for (int i = 0; i < 4; ++i) wire.Round(Ms(i));
    if (session.State() == N::ClientState::Connected) return std::nullopt;
    REQUIRE(session.State() == N::ClientState::Disconnected);
    return session.Ended();
}

N::PacketKind KindOf(const std::vector<std::byte>& datagram) { return N::DecodePacketHeader(datagram)->kind; }

} // namespace

TEST_CASE("handshake: matching facts connect, and the accept carries the server's tick, step and range") {
    N::ServerHost host(Settings(), ServerFacts(), 77, 99);
    N::ClientSession client(Settings(), N::ConnectRequest{ClientFacts(), 42, Bytes({9, 9}), 0}, Ms(0));
    Wire wire{host, client};
    wire.Round(Ms(0), {500, 3});
    // After the request the server keeps nothing: the challenge is a hash it can recompute.
    CHECK(host.ConnectionCount() == 0);
    CHECK(client.State() == N::ClientState::Connecting);
    wire.Round(Ms(1), {501, 3});
    REQUIRE(client.State() == N::ClientState::Connected);
    REQUIRE(client.Accepted().has_value());
    CHECK(client.Accepted()->connectionId != 0);
    CHECK(client.Accepted()->tick == 501);
    CHECK(client.Accepted()->worldGeneration == 3);
    CHECK(client.Accepted()->stepNanoseconds == 15'625'000);
    CHECK(client.Accepted()->idRange == N::IdRange{1, 1000});
    CHECK(host.ConnectionCount() == 1);
    const auto events = host.TakeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == N::HostEventKind::Connected);
    CHECK(events[0].userId == 42);
    CHECK(events[0].connectionId == client.Accepted()->connectionId);
}

namespace {

// The reason a client with these facts is rejected for, or Connected (no DisconnectReason is 0) when it connects.
N::DisconnectReason ReasonOf(const N::HandshakeFacts& client, const N::HandshakeFacts& server = ServerFacts()) {
    const auto rejected = Shake(client, server);
    return rejected ? rejected->reason : N::DisconnectReason{};
}
constexpr N::DisconnectReason kConnected{};

} // namespace

TEST_CASE("handshake: each fact that differs is rejected with its own reason (G-3)") {
    REQUIRE(ReasonOf(ClientFacts()) == kConnected);

    auto version = ClientFacts();
    version.protocolVersion = N::kNetProtocolVersion + 1;
    CHECK_MESSAGE(ReasonOf(version) == N::DisconnectReason::ProtocolMismatch, "a protocol version that differs is ProtocolMismatch");
    CHECK(Shake(version).value_or(N::Rejection{}).detail.find("version") != std::string::npos);

    auto hash = ClientFacts();
    hash.protocolHash = 0x9999;
    CHECK_MESSAGE(ReasonOf(hash) == N::DisconnectReason::ProtocolMismatch, "a protocol hash that differs is ProtocolMismatch");
    CHECK(Shake(hash).value_or(N::Rejection{}).detail.find("protocol hash") != std::string::npos);

    auto step = ClientFacts();
    step.stepNanoseconds = 16'666'667;
    CHECK_MESSAGE(ReasonOf(step) == N::DisconnectReason::StepLengthMismatch, "a step length that differs is StepLengthMismatch");

    auto range = ClientFacts();
    range.idRange = {1000, 2000};
    CHECK_MESSAGE(ReasonOf(range) == N::DisconnectReason::IdRangeOverlap, "overlapping allotter ranges are IdRangeOverlap");
    CHECK(Shake(range).value_or(N::Rejection{}).detail == "client [1000, 2000], server [1, 1000]");

    auto fingerprint = ClientFacts();
    fingerprint.fingerprint = std::string("1:client-fingerprint");
    CHECK_MESSAGE(ReasonOf(fingerprint) == N::DisconnectReason::FingerprintMismatch, "a fingerprint that differs is FingerprintMismatch");
}

TEST_CASE("handshake: a client without an authority strategy is not compared on the fingerprint") {
    auto k1 = ClientFacts();
    k1.fingerprint.reset();
    CHECK_MESSAGE(ReasonOf(k1) == kConnected, "a client without a fingerprint (K1) connects to a server with one");
    // A client with a fingerprint needs the server to have the same.
    auto server = ServerFacts();
    server.fingerprint.reset();
    CHECK(ReasonOf(ClientFacts(), server) == N::DisconnectReason::FingerprintMismatch);
}

TEST_CASE("handshake: a response with another server's challenge is rejected") {
    N::ServerHost first(Settings(), ServerFacts(), 77, 99);
    N::ServerHost second(Settings(), ServerFacts(), 78, 99);
    N::ClientSession client(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 0}, Ms(0));
    // The challenge comes from the first server, the response goes to the second.
    for (const auto& datagram : client.Collect(Ms(0), {})) first.Receive(kClient, datagram, Ms(0));
    for (const auto& [to, datagram] : first.Collect(Ms(0), {})) client.Receive(datagram, Ms(0));
    for (const auto& datagram : client.Collect(Ms(1), {})) {
        REQUIRE(KindOf(datagram) == N::PacketKind::ConnectResponse);
        second.Receive(kClient, datagram, Ms(1));
    }
    for (const auto& [to, datagram] : second.Collect(Ms(1), {})) client.Receive(datagram, Ms(1));
    const N::DisconnectReason reason = client.State() == N::ClientState::Disconnected ? client.Ended()->reason : N::DisconnectReason{};
    CHECK_MESSAGE(reason == N::DisconnectReason::ChallengeFailed, "a wrong challenge is ChallengeFailed");
    CHECK(second.ConnectionCount() == 0);
}

TEST_CASE("handshake: the product's refusal is Refused with its text") {
    const auto rejected = Shake(ClientFacts(), ServerFacts(), [](const N::ConnectRequest& request, N::Endpoint) -> N::NetResult<void> {
        if (request.userId == 42) {
            return GYO::Engine::Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, "the room is full"));
        }
        return {};
    });
    REQUIRE(rejected.has_value());
    CHECK_MESSAGE(rejected->reason == N::DisconnectReason::Refused, "the product's refusal is Refused");
    CHECK(rejected->detail == "the room is full");
}

TEST_CASE("handshake: no answer within the handshake timeout ends the client") {
    N::ClientSession client(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 0}, Ms(0));
    // Requests are resent every keepalive interval while nothing answers.
    CHECK(client.Collect(Ms(0), {}).size() == 1);
    CHECK(client.Collect(Ms(100), {}).empty());
    CHECK(client.Collect(Ms(101), {}).size() == 1);
    client.Update(Ms(15'000));
    CHECK(client.State() == N::ClientState::Connecting);
    client.Update(Ms(15'001));
    REQUIRE(client.State() == N::ClientState::Disconnected);
    CHECK(client.Ended()->reason == N::DisconnectReason::HandshakeTimeout);
}

TEST_CASE("handshake: a request smaller than a challenge gets no answer") {
    N::ServerHost host(Settings(), ServerFacts(), 77, 99);
    N::PacketHeader header;
    header.kind = N::PacketKind::ConnectRequest;
    std::vector<std::byte> tiny;
    N::EncodePacketHeader(header, tiny);
    tiny.push_back(std::byte{1});
    host.Receive(kClient, tiny, Ms(0));
    CHECK_MESSAGE(host.Collect(Ms(0), {}).empty(), "a request smaller than a challenge gets no answer");
    CHECK(host.Drops()[static_cast<std::size_t>(N::DropReason::Unexpected)] == 1);
    // A real request is never smaller than the challenge.
    N::ClientSession client(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 0}, Ms(0));
    const auto request = client.Collect(Ms(0), {});
    REQUIRE(request.size() == 1);
    for (const auto& datagram : request) host.Receive(kClient, datagram, Ms(0));
    const auto challenge = host.Collect(Ms(0), {});
    REQUIRE(challenge.size() == 1);
    CHECK(request[0].size() >= challenge[0].second.size());
}

TEST_CASE("handshake: a new handshake from the same endpoint is a new connection; a repeated response is not") {
    N::ServerHost host(Settings(), ServerFacts(), 77, 99);
    N::ClientSession first(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 1}, Ms(0));
    Wire wire{host, first};
    wire.Round(Ms(0));
    // The response is sent twice (as when its Accepted is lost): one connection, the same id.
    const auto response = first.Collect(Ms(1), {});
    REQUIRE(response.size() == 1);
    host.Receive(kClient, response[0], Ms(1));
    host.Receive(kClient, response[0], Ms(1));
    for (const auto& [to, datagram] : host.Collect(Ms(1), {})) first.Receive(datagram, Ms(1));
    REQUIRE(first.State() == N::ClientState::Connected);
    CHECK(host.ConnectionCount() == 1);
    const std::uint32_t oldId = first.Accepted()->connectionId;
    (void)host.TakeEvents();
    // The same user from the same endpoint shakes hands again (another salt): the old connection ends.
    N::ClientSession second(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 2}, Ms(2));
    Wire again{host, second};
    again.Round(Ms(2));
    again.Round(Ms(3));
    // Given the old connection back, the accept carries the old salt and the newer session does not take it (S2).
    REQUIRE_MESSAGE(second.State() == N::ClientState::Connected,
                    "a reconnect from the same endpoint is a new connection: the newer handshake connects");
    CHECK_MESSAGE(second.Accepted()->connectionId != oldId, "a reconnect from the same endpoint is a new connection");
    CHECK(host.ConnectionCount() == 1);
    const auto events = host.TakeEvents();
    REQUIRE(events.size() == 2);
    CHECK(events[0].kind == N::HostEventKind::Disconnected);
    CHECK(events[0].reason == N::DisconnectReason::ClosedByRemote);
    CHECK(events[1].kind == N::HostEventKind::Connected);
}

TEST_CASE("handshake: a newer handshake from the same endpoint that is refused leaves the live connection as it was") {
    // The product admits a token {1} only (a stale or forged one is refused).
    N::ServerHost host(Settings(), ServerFacts(), 77, 99, [](const N::ConnectRequest& request, N::Endpoint) -> N::NetResult<void> {
        if (request.token == Bytes({1})) return {};
        return GYO::Engine::Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, "token refused"));
    });
    N::ClientSession first(Settings(), N::ConnectRequest{ClientFacts(), 42, Bytes({1}), 1}, Ms(0));
    Wire wire{host, first};
    wire.Round(Ms(0));
    wire.Round(Ms(1));
    REQUIRE(first.State() == N::ClientState::Connected);
    const std::uint32_t liveId = first.Accepted()->connectionId;
    (void)host.TakeEvents();

    // From the same endpoint, with a larger salt: first a refused token, then facts that differ.
    auto stale = N::ConnectRequest{ClientFacts(), 42, Bytes({2}), 2};
    auto differs = N::ConnectRequest{ClientFacts(), 42, Bytes({1}), 3};
    differs.facts.protocolHash = 0x9999;
    for (const auto& request : {stale, differs}) {
        N::ClientSession later(Settings(), request, Ms(2));
        Wire again{host, later};
        again.Round(Ms(2));
        again.Round(Ms(3));
        REQUIRE(later.State() == N::ClientState::Disconnected);
        CHECK_MESSAGE(host.ConnectionCount() == 1, "[nt-refused-keeps-live] a refused handshake does not end the live connection");
        CHECK(host.ConnectionIds() == std::vector<std::uint32_t>{liveId});
        CHECK_MESSAGE(host.TakeEvents().empty(), "[nt-refused-keeps-live] a refused handshake ends nothing and connects nothing");
    }
    // The live connection still carries datagrams both ways.
    wire.Round(Ms(4));
    CHECK(first.State() == N::ClientState::Connected);
    CHECK(host.ConnectionIds() == std::vector<std::uint32_t>{liveId});
}

namespace {

// A connected pair at time 0 (the handshake takes the rounds at 0 and 1 ms... all before 2 ms).
struct Connected {
    N::ServerHost host{Settings(), ServerFacts(), 77, 99};
    N::ClientSession client{Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 0}, Ms(0)};
    Wire wire{host, client};
    std::uint32_t id = 0;
    Connected() {
        wire.Round(Ms(0));
        wire.Round(Ms(0));
        REQUIRE(client.State() == N::ClientState::Connected);
        id = client.Accepted()->connectionId;
        (void)host.TakeEvents();
    }
};

std::size_t KeepAlives(const std::vector<std::vector<std::byte>>& datagrams) {
    std::size_t count = 0;
    for (const auto& datagram : datagrams) count += KindOf(datagram) == N::PacketKind::AckOnly ? 1 : 0;
    return count;
}

} // namespace

TEST_CASE("liveness: no keepalive while both ends send data (G-4)") {
    Connected pair;
    std::size_t keepalives = 0;
    for (int frame = 1; frame <= 200; ++frame) {
        const N::NetTime now = Ms(frame * 16);
        REQUIRE(pair.client.Transport()->Send(kUnreliable, NumberedMessage(static_cast<std::uint32_t>(frame), 8)).has_value());
        REQUIRE(pair.host.Find(pair.id)->Send(kUnreliable, NumberedMessage(static_cast<std::uint32_t>(frame), 8)).has_value());
        const auto up = pair.client.Collect(now, {});
        keepalives += KeepAlives(up);
        for (const auto& datagram : up) pair.host.Receive(kClient, datagram, now);
        for (const auto& [to, datagram] : pair.host.Collect(now, {})) {
            keepalives += KindOf(datagram) == N::PacketKind::AckOnly ? 1 : 0;
            pair.client.Receive(datagram, now);
        }
    }
    CHECK_MESSAGE(keepalives == 0, "no keepalive while both ends send data");
    CHECK(pair.client.State() == N::ClientState::Connected);

    // Data more than the interval apart: the frame that sends data sends no keepalive beside it.
    std::size_t beside = 0;
    for (int frame = 1; frame <= 20; ++frame) {
        const N::NetTime now = Ms(3200 + frame * 150);
        REQUIRE(pair.client.Transport()->Send(kUnreliable, NumberedMessage(static_cast<std::uint32_t>(frame), 8)).has_value());
        const auto up = pair.client.Collect(now, {});
        beside += KeepAlives(up);
        for (const auto& datagram : up) pair.host.Receive(kClient, datagram, now);
        for (const auto& [to, datagram] : pair.host.Collect(now, {})) pair.client.Receive(datagram, now);
    }
    CHECK_MESSAGE(beside == 0, "a frame with data carries no keepalive");
}

TEST_CASE("liveness: an idle end sends a keepalive when more than the interval passed since its last send (G-4)") {
    Connected pair;
    std::vector<std::int64_t> clientTimes, serverTimes;
    for (int ms = 10; ms <= 2000; ms += 10) {
        const N::NetTime now = Ms(ms);
        const auto up = pair.client.Collect(now, {});
        if (KeepAlives(up) > 0) clientTimes.push_back(ms);
        for (const auto& datagram : up) pair.host.Receive(kClient, datagram, now);
        bool serverSent = false;
        for (const auto& [to, datagram] : pair.host.Collect(now, {})) {
            serverSent = serverSent || KindOf(datagram) == N::PacketKind::AckOnly;
            pair.client.Receive(datagram, now);
        }
        if (serverSent) serverTimes.push_back(ms);
    }
    // Last send at 0 ms; strictly more than 100 ms later is the first 10 ms frame after 100, then every 110 ms.
    REQUIRE(clientTimes.size() >= 3);
    CHECK_MESSAGE(clientTimes[0] == 110, "the first keepalive goes out when more than the interval passed");
    for (std::size_t i = 1; i < clientTimes.size(); ++i) CHECK(clientTimes[i] - clientTimes[i - 1] == 110);
    REQUIRE(serverTimes.size() >= 3);
    CHECK(serverTimes[0] == 110);
    CHECK(pair.client.State() == N::ClientState::Connected);
    CHECK(pair.host.ConnectionCount() == 1);
}

TEST_CASE("liveness: silence past the first threshold interrupts, past the second ends with Timeout (G-4)") {
    Connected pair;
    // The last datagram from the server arrived at 0 ms.
    pair.client.Update(Ms(1000));
    CHECK_MESSAGE(pair.client.State() == N::ClientState::Connected, "at the first threshold the client is still connected");
    pair.client.Update(Ms(1000) + N::NetTime{1});
    CHECK_MESSAGE(pair.client.State() == N::ClientState::Interrupted, "just past the first threshold the client is interrupted");
    pair.client.Update(Ms(10'000));
    CHECK_MESSAGE(pair.client.State() == N::ClientState::Interrupted, "at the second threshold the client is still interrupted");
    pair.client.Update(Ms(10'000) + N::NetTime{1});
    REQUIRE(pair.client.State() == N::ClientState::Disconnected);
    CHECK_MESSAGE(pair.client.Ended()->reason == N::DisconnectReason::Timeout, "just past the second threshold the client ends with Timeout");
}

TEST_CASE("liveness: any datagram of the connection refreshes it, and an interrupted client comes back") {
    Connected pair;
    pair.client.Update(Ms(1500));
    REQUIRE(pair.client.State() == N::ClientState::Interrupted);
    // A keepalive from the server at 1500 ms.
    for (const auto& [to, datagram] : pair.host.Collect(Ms(1500), {})) pair.client.Receive(datagram, Ms(1500));
    CHECK(pair.client.State() == N::ClientState::Connected);
    pair.client.Update(Ms(2500));
    CHECK_MESSAGE(pair.client.State() == N::ClientState::Connected, "a datagram of the connection refreshes its last received time");
    pair.client.Update(Ms(2501));
    CHECK(pair.client.State() == N::ClientState::Interrupted);
}

TEST_CASE("liveness: the server ends a silent connection with Timeout past the threshold") {
    Connected pair;
    pair.host.Update(Ms(10'000));
    CHECK(pair.host.ConnectionCount() == 1);
    pair.host.Update(Ms(10'000) + N::NetTime{1});
    CHECK(pair.host.ConnectionCount() == 0);
    const auto events = pair.host.TakeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == N::HostEventKind::Disconnected);
    CHECK(events[0].reason == N::DisconnectReason::Timeout);
}

TEST_CASE("liveness: a closed client sends its Disconnect packets and the server ends with ClosedByRemote") {
    Connected pair;
    pair.client.Close(Ms(5));
    REQUIRE(pair.client.State() == N::ClientState::Disconnected);
    CHECK(pair.client.Ended()->reason == N::DisconnectReason::Closed);
    const auto goodbye = pair.client.Collect(Ms(5), {});
    CHECK(goodbye.size() == 10);
    for (const auto& datagram : goodbye) pair.host.Receive(kClient, datagram, Ms(5));
    CHECK(pair.client.Collect(Ms(6), {}).empty());
    const auto events = pair.host.TakeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].reason == N::DisconnectReason::ClosedByRemote);
    // The other nine are of a connection that no longer exists.
    CHECK(pair.host.Drops()[static_cast<std::size_t>(N::DropReason::UnknownSource)] == 9);
}

// ---- Fixes of the design review before PR-1's merge (S2): the salt tells a newer handshake from an older one.

TEST_CASE("handshake: a late response of an older handshake does not end the newer connection (S2 a)") {
    N::ServerHost host(Settings(), ServerFacts(), 77, 99);
    N::ClientSession first(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 1}, Ms(0));
    Wire wire{host, first};
    wire.Round(Ms(0));
    // The first handshake's response, held back on the way (the client resends every 100 ms; this copy is late).
    const auto late = first.Collect(Ms(1), {});
    REQUIRE(late.size() == 1);
    // A newer handshake from the same endpoint (salt 2) connects.
    N::ClientSession second(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 2}, Ms(2));
    Wire again{host, second};
    again.Round(Ms(2));
    again.Round(Ms(3));
    REQUIRE(second.State() == N::ClientState::Connected);
    const std::uint32_t id = second.Accepted()->connectionId;
    (void)host.TakeEvents();
    host.Receive(kClient, late[0], Ms(4));
    (void)host.Collect(Ms(4), {});
    CHECK_MESSAGE(host.TakeEvents().empty(), "S2 a: an older handshake's response neither ends nor opens a connection");
    CHECK(host.ConnectionCount() == 1);
    CHECK(host.Find(id) != nullptr);
    CHECK(host.Drops()[static_cast<std::size_t>(N::DropReason::Unexpected)] == 1);
}

TEST_CASE("handshake: a connecting client does not take the accept of an older handshake (S2 b)") {
    N::ServerHost host(Settings(), ServerFacts(), 77, 99);
    N::ClientSession first(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 1}, Ms(0));
    Wire wire{host, first};
    wire.Round(Ms(0));
    for (const auto& datagram : first.Collect(Ms(1), {})) host.Receive(kClient, datagram, Ms(1));
    // The server's accept for the first handshake, which arrives only after the client started a new one.
    const auto replies = host.Collect(Ms(1), {});
    REQUIRE(replies.size() == 1);
    REQUIRE(KindOf(replies[0].second) == N::PacketKind::Accepted);
    N::ClientSession second(Settings(), N::ConnectRequest{ClientFacts(), 42, {}, 2}, Ms(2));
    (void)second.Collect(Ms(2), {});
    second.Receive(replies[0].second, Ms(3));
    CHECK_MESSAGE(second.State() == N::ClientState::Connecting, "S2 b: the accept of an older handshake is not taken");
    CHECK(second.Drops()[static_cast<std::size_t>(N::DropReason::Unexpected)] == 1);
}
