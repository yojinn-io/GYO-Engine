// The Connection's unit cases (net Module declaration D-2, D-12): acknowledgement, resending, AckOnly,
// connection ids, fragments, closing. Datagrams are carried by hand, so each case decides exactly what arrives.
#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

#include "AssertTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/Connection.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

using Datagrams = std::vector<std::vector<std::byte>>;

void Deliver(N::Connection& to, const Datagrams& datagrams, const N::NetTime now) {
    for (const auto& datagram : datagrams) REQUIRE(to.Receive(datagram, now).has_value());
}

N::PacketHeader HeaderOf(const std::vector<std::byte>& datagram) { return *N::DecodePacketHeader(datagram); }

std::uint64_t Dropped(const N::Connection& connection, const N::DropReason reason) {
    return connection.Stats().drops[static_cast<std::size_t>(reason)];
}

std::vector<std::vector<std::byte>> PopAll(N::Connection& connection, const std::size_t channel) {
    std::vector<std::vector<std::byte>> out;
    while (auto message = connection.Pop(channel)) out.push_back(std::move(*message));
    return out;
}

} // namespace

TEST_CASE("connection: the ack bitfield covers the 32 packets before the newest") {
    N::Connection a(TwoChannels());
    N::Connection b(TwoChannels());
    // Packets 1..40 from a, one message each; 10 and 20 never arrive.
    for (std::uint32_t i = 1; i <= 40; ++i) {
        REQUIRE(a.Send(kUnreliable, NumberedMessage(i, 8)).has_value());
        const auto datagrams = a.Collect(Ms(i), {});
        REQUIRE(datagrams.size() == 1);
        REQUIRE(HeaderOf(datagrams[0]).packetId == i);
        if (i != 10 && i != 20) Deliver(b, datagrams, Ms(i));
    }
    const auto reply = b.Collect(Ms(41), {});
    REQUIRE(reply.size() == 1);
    const auto header = HeaderOf(reply[0]);
    CHECK(header.kind == N::PacketKind::AckOnly);
    CHECK(header.ack == 40);
    std::uint32_t expected = 0;
    for (std::uint32_t i = 0; i < 32; ++i) {
        const std::uint32_t id = 39 - i;
        if (id != 10 && id != 20) expected |= 1u << i;
    }
    CHECK_MESSAGE(header.ackBits == expected, "the ack bitfield covers the 32 packets before ack");
}

TEST_CASE("connection: an acknowledged piece is not sent again; an unacknowledged one is, after the delay") {
    auto settings = TwoChannels();
    settings.resendMinDelay = Ms(100);
    N::Connection a(settings);
    N::Connection b(settings);
    REQUIRE(a.Send(kReliable, NumberedMessage(1, 8)).has_value());
    REQUIRE(a.Send(kReliable, NumberedMessage(2, 8)).has_value());

    // Both go out in one packet, which is lost.
    REQUIRE(a.Collect(Ms(0), {}).size() == 1);
    CHECK(a.Collect(Ms(50), {}).empty());
    // After the delay both are resent; this time the packet arrives and b acknowledges it.
    const auto resent = a.Collect(Ms(100), {});
    REQUIRE(resent.size() == 1);
    Deliver(b, resent, Ms(110));
    CHECK(PopAll(b, kReliable).size() == 2);
    Deliver(a, b.Collect(Ms(120), {}), Ms(130));
    CHECK(a.PendingPieces() == 0);
    CHECK_MESSAGE(a.Collect(Ms(1000), {}).empty(), "an acknowledged piece is not sent again");
}

TEST_CASE("connection: the resend delay is max(1.5 x RTT, the floor)") {
    auto settings = TwoChannels();
    settings.resendMinDelay = Ms(10);
    N::Connection a(settings);
    N::Connection b(settings);
    // One round trip of 100 ms gives the first RTT sample.
    REQUIRE(a.Send(kReliable, NumberedMessage(1, 8)).has_value());
    Deliver(b, a.Collect(Ms(0), {}), Ms(50));
    Deliver(a, b.Collect(Ms(50), {}), Ms(100));
    REQUIRE(a.SmoothedRtt() == Ms(100));

    REQUIRE(a.Send(kReliable, NumberedMessage(2, 8)).has_value());
    REQUIRE(a.Collect(Ms(1000), {}).size() == 1); // lost
    CHECK_MESSAGE(a.Collect(Ms(1149), {}).empty(), "a piece is not resent before 1.5 x RTT");
    CHECK(a.Collect(Ms(1150), {}).size() == 1);
}

TEST_CASE("connection: an AckOnly packet does not ask for an acknowledgement") {
    N::Connection a(TwoChannels());
    N::Connection b(TwoChannels());
    REQUIRE(a.Send(kReliable, NumberedMessage(1, 8)).has_value());
    Deliver(b, a.Collect(Ms(0), {}), Ms(1));
    const auto ack = b.Collect(Ms(2), {});
    REQUIRE(ack.size() == 1);
    CHECK(HeaderOf(ack[0]).kind == N::PacketKind::AckOnly);
    CHECK(b.Collect(Ms(3), {}).empty());
    Deliver(a, ack, Ms(4));
    CHECK_MESSAGE(a.Collect(Ms(5), {}).empty(), "an AckOnly packet does not ask for an acknowledgement");
}

TEST_CASE("connection: a datagram of another connection is dropped and counted") {
    N::Connection a(TwoChannels(1));
    N::Connection b(TwoChannels(2));
    REQUIRE(a.Send(kUnreliable, NumberedMessage(1, 8)).has_value());
    const auto datagrams = a.Collect(Ms(0), {});
    REQUIRE(datagrams.size() == 1);
    Deliver(b, datagrams, Ms(1));
    CHECK_MESSAGE(Dropped(b, N::DropReason::ConnectionMismatch) == 1,
                  "a datagram of another connection is dropped and counted");
    CHECK(b.Pop(kUnreliable) == std::nullopt);
    CHECK(b.Stats().datagramsAccepted == 0);
    CHECK(b.Collect(Ms(2), {}).empty());
}

TEST_CASE("connection: a reliable piece received twice is handed out once, in order") {
    auto settings = TwoChannels();
    settings.resendMinDelay = Ms(100);
    N::Connection a(settings);
    N::Connection b(settings);
    REQUIRE(a.Send(kReliable, NumberedMessage(0, 8)).has_value());
    REQUIRE(a.Collect(Ms(0), {}).size() == 1); // message 0: lost
    REQUIRE(a.Send(kReliable, NumberedMessage(1, 8)).has_value());
    // Message 0 is not due yet, so this packet carries message 1 only: b buffers it behind the gap.
    Deliver(b, a.Collect(Ms(10), {}), Ms(11));
    CHECK(b.Pop(kReliable) == std::nullopt);
    // Both are resent; message 1 arrives a second time while it still waits for message 0.
    Deliver(b, a.Collect(Ms(200), {}), Ms(201));
    const auto messages = PopAll(b, kReliable);
    REQUIRE_MESSAGE(messages.size() == 2, "a reliable piece received twice is handed out once");
    CHECK(IndexOf(messages[0]) == 0);
    CHECK(IndexOf(messages[1]) == 1);
    CHECK(Dropped(b, N::DropReason::DuplicateMessage) == 1);
}

TEST_CASE("connection: an incomplete fragmented message is not handed out and is dropped after the timeout") {
    auto settings = TwoChannels();
    settings.fragmentTimeout = Ms(500);
    N::Connection a(settings);
    N::Connection b(settings);
    const auto big = NumberedMessage(9, 3000); // three pieces at the default mtu
    REQUIRE(a.Send(kUnreliable, big).has_value());
    const auto datagrams = a.Collect(Ms(0), {});
    REQUIRE(datagrams.size() == 3);
    Deliver(b, {datagrams[0], datagrams[2]}, Ms(1));
    CHECK_MESSAGE(b.Pop(kUnreliable) == std::nullopt, "an incomplete fragmented message is not handed out");
    CHECK(b.PendingPieces() == 2);
    (void)b.Collect(Ms(501), {});
    CHECK(b.PendingPieces() == 0);
    CHECK(Dropped(b, N::DropReason::FragmentTimeout) == 2);

    // The same message with every piece arrives whole.
    N::Connection c(settings);
    REQUIRE(a.Send(kUnreliable, big).has_value());
    Deliver(c, a.Collect(Ms(600), {}), Ms(601));
    const auto whole = c.Pop(kUnreliable);
    REQUIRE(whole.has_value());
    CHECK(*whole == big);
}

TEST_CASE("connection: a reliable fragmented message arrives whole and in order") {
    N::Connection a(TwoChannels());
    N::Connection b(TwoChannels());
    const auto big = NumberedMessage(1, 5000);
    REQUIRE(a.Send(kReliable, big).has_value());
    REQUIRE(a.Send(kReliable, NumberedMessage(2, 8)).has_value());
    auto datagrams = a.Collect(Ms(0), {});
    REQUIRE(datagrams.size() == 5);
    std::reverse(datagrams.begin(), datagrams.end());
    Deliver(b, datagrams, Ms(1));
    const auto messages = PopAll(b, kReliable);
    REQUIRE(messages.size() == 2);
    CHECK(messages[0] == big);
    CHECK(IndexOf(messages[1]) == 2);
}

TEST_CASE("connection: channels with a higher priority go first") {
    N::ConnectionSettings settings;
    settings.channels = {{N::ChannelKind::Unreliable, 0}, {N::ChannelKind::Unreliable, 5}};
    N::Connection a(settings);
    REQUIRE(a.Send(0, NumberedMessage(0, 1000)).has_value());
    REQUIRE(a.Send(1, NumberedMessage(1, 1000)).has_value());
    const auto datagrams = a.Collect(Ms(0), {});
    REQUIRE(datagrams.size() == 2);
    const auto first = N::Connection::Inspect(datagrams[0]);
    REQUIRE(first.has_value());
    REQUIRE(first->piecesPerChannel.size() == 2);
    CHECK(first->piecesPerChannel[1] == 1);
    CHECK(first->piecesPerChannel[0] == 0);
}

TEST_CASE("connection: the stamp is written and the newest one is read back") {
    N::Connection a(TwoChannels());
    N::Connection b(TwoChannels());
    REQUIRE(a.Send(kUnreliable, NumberedMessage(1, 8)).has_value());
    const auto older = a.Collect(Ms(0), {100, 3});
    REQUIRE(a.Send(kUnreliable, NumberedMessage(2, 8)).has_value());
    const auto newer = a.Collect(Ms(1), {101, 3});
    CHECK(b.RemoteStamp() == std::nullopt);
    Deliver(b, newer, Ms(2));
    Deliver(b, older, Ms(3));
    REQUIRE(b.RemoteStamp().has_value());
    CHECK(*b.RemoteStamp() == N::Stamp{101, 3});
}

TEST_CASE("connection: Close sends the Disconnect packets once") {
    N::Connection a(TwoChannels());
    N::Connection b(TwoChannels());
    a.Close();
    const auto datagrams = a.Collect(Ms(0), {});
    CHECK(datagrams.size() == 10);
    for (const auto& datagram : datagrams) CHECK(HeaderOf(datagram).kind == N::PacketKind::Disconnect);
    CHECK(a.Collect(Ms(1), {}).empty());
    Deliver(b, {datagrams[3]}, Ms(1));
    CHECK(b.RemoteClosed());
    CHECK(b.Collect(Ms(2), {}).empty());
}

TEST_CASE("connection: a message the channel cannot take is refused and counted") {
    auto settings = TwoChannels();
    settings.sendQueueCapacity = 1;
    settings.maxMessageBytes = 100;
    N::Connection a(settings);
    REQUIRE(a.Send(kUnreliable, NumberedMessage(1, 8)).has_value());
    CHECK(CodeOf(a.Send(kUnreliable, NumberedMessage(2, 8))) == N::NetFailureCode::MessageRejected);
    CHECK(CodeOf(a.Send(kReliable, NumberedMessage(3, 101))) == N::NetFailureCode::MessageRejected);
    CHECK(Dropped(a, N::DropReason::SendRejected) == 2);
}

TEST_CASE("connection: a datagram that cannot be read returns its failure and is counted") {
    N::Connection b(TwoChannels());
    CHECK(CodeOf(b.Receive(Bytes({1, 2, 3}), Ms(0))) == N::NetFailureCode::Truncated);
    // A Data packet whose entry names channel 9.
    N::PacketHeader header;
    header.connectionId = 7;
    header.packetId = 1;
    std::vector<std::byte> datagram;
    N::EncodePacketHeader(header, datagram);
    const auto entry = Bytes({9, 0, 0, 0, 1, 0, 0x55});
    datagram.insert(datagram.end(), entry.begin(), entry.end());
    CHECK(CodeOf(b.Receive(datagram, Ms(0))) == N::NetFailureCode::MalformedPayload);
    CHECK(Dropped(b, N::DropReason::DecodeFailed) == 2);
    CHECK(b.Stats().datagramsAccepted == 0);
}

TEST_CASE("connection: the reliable window is a power of two (small thing 4)") {
    N::ConnectionSettings settings = TwoChannels();
    settings.reliableWindow = 100;
    GYO_CHECK_ASSERTS(N::Connection(settings));
    settings.reliableWindow = 128;
    N::Connection fine(settings);
    CHECK(fine.Settings().reliableWindow == 128);
}
