// DatagramSocket (docs/architecture/os.md, Datagram; net Module declaration D-1
// and judgment G-7): loopback only, so it runs on every L1 runner.
#include <doctest/doctest.h>

#include "engine/os/DatagramSocket.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace GYO::Engine::OS;

namespace {

std::vector<std::byte> Bytes(std::size_t count, std::uint8_t first) {
    std::vector<std::byte> bytes(count);
    for (std::size_t i = 0; i < count; ++i) bytes[i] = static_cast<std::byte>(static_cast<std::uint8_t>(first + i));
    return bytes;
}

DatagramSocket OpenLoopback() {
    auto socket = DatagramSocket::Open(DatagramAddress::Loopback(0));
    REQUIRE(socket.has_value());
    return std::move(socket).value();
}

DatagramAddress Local(const DatagramSocket& socket) {
    auto address = socket.LocalAddress();
    REQUIRE(address.has_value());
    return *address;
}

// Loopback delivery is not instantaneous on every OS: polls for up to two seconds.
std::optional<ReceivedDatagram> ReceiveWithin(DatagramSocket& socket, std::span<std::byte> buffer) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < until) {
        auto received = socket.Receive(buffer);
        REQUIRE(received.has_value());
        if (*received) return *received;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("datagram address: dotted IPv4 text parses, anything else is InvalidAddress") {
    const auto parsed = DatagramAddress::Parse("192.168.0.10", 7000);
    REQUIRE(parsed.has_value());
    CHECK(*parsed == DatagramAddress{{192, 168, 0, 10}, 7000});
    CHECK(parsed->ToString() == "192.168.0.10:7000");
    CHECK(DatagramAddress::Parse("0.0.0.0", 1)->octets == std::array<std::uint8_t, 4>{0, 0, 0, 0});
    CHECK(DatagramAddress::Parse("255.255.255.255", 1)->octets == std::array<std::uint8_t, 4>{255, 255, 255, 255});
    for (const std::string bad : {"", "1.2.3", "1.2.3.4.5", "256.0.0.1", "01.2.3.4", "1..3.4", "1.2.3.4 ", "a.b.c.d",
             "1.2.3.1000"}) {
        CAPTURE(bad);
        const auto result = DatagramAddress::Parse(bad, 1);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == OSErrorCode::InvalidAddress);
    }
}

TEST_CASE("datagram socket: a datagram sent on loopback arrives whole, with the sender's address") {
    DatagramSocket sender = OpenLoopback();
    DatagramSocket receiver = OpenLoopback();
    const DatagramAddress to = Local(receiver);
    CHECK(to.port != 0);
    CHECK(to.octets == std::array<std::uint8_t, 4>{127, 0, 0, 1});

    const auto payload = Bytes(300, 7);
    REQUIRE(sender.Send(to, payload).has_value());
    std::array<std::byte, 2048> buffer{};
    const auto received = ReceiveWithin(receiver, buffer);
    REQUIRE(received.has_value());
    CHECK(received->size == payload.size());
    CHECK(std::equal(payload.begin(), payload.end(), buffer.begin()));
    CHECK(received->from == Local(sender));
}

TEST_CASE("datagram socket: receive with nothing waiting returns no datagram and does not block") {
    DatagramSocket socket = OpenLoopback();
    std::array<std::byte, 64> buffer{};
    const auto received = socket.Receive(buffer);
    REQUIRE(received.has_value());
    CHECK_FALSE(received->has_value());
}

TEST_CASE("datagram socket: a datagram longer than the buffer is TooLarge and the next one still arrives") {
    DatagramSocket sender = OpenLoopback();
    DatagramSocket receiver = OpenLoopback();
    const DatagramAddress to = Local(receiver);
    REQUIRE(sender.Send(to, Bytes(100, 1)).has_value());
    REQUIRE(sender.Send(to, Bytes(8, 50)).has_value());

    std::array<std::byte, 16> small{};
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::optional<OSError> failure;
    while (!failure && std::chrono::steady_clock::now() < until) {
        auto received = receiver.Receive(small);
        if (!received.has_value()) failure = received.error();
        else REQUIRE_FALSE(received->has_value()); // the long one comes first
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(failure.has_value());
    CHECK(failure->code == OSErrorCode::TooLarge);

    const auto next = ReceiveWithin(receiver, small);
    REQUIRE(next.has_value());
    CHECK(next->size == 8);
    CHECK(small[0] == std::byte{50});
}

TEST_CASE("datagram socket: a port another socket holds is AddressInUse") {
    DatagramSocket first = OpenLoopback();
    const auto second = DatagramSocket::Open(Local(first));
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code == OSErrorCode::AddressInUse);
}

TEST_CASE("datagram socket: binding an address this machine does not have is InvalidAddress") {
    // 203.0.113.1 is TEST-NET-3 (RFC 5737): never assigned to a local interface.
    const auto socket = DatagramSocket::Open(DatagramAddress{{203, 0, 113, 1}, 0});
    REQUIRE_FALSE(socket.has_value());
    CHECK(socket.error().code == OSErrorCode::InvalidAddress);
}

TEST_CASE("datagram socket: sending to a port nobody listens on does not break the next receive") {
    DatagramSocket socket = OpenLoopback();
    DatagramAddress closed;
    {
        DatagramSocket gone = OpenLoopback();
        closed = Local(gone);
    }
    // Windows would answer the ICMP port-unreachable with WSAECONNRESET on the next
    // receive unless SIO_UDP_CONNRESET is off; the other platforms never report it.
    REQUIRE(socket.Send(closed, Bytes(4, 0)).has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::array<std::byte, 64> buffer{};
    const auto nothing = socket.Receive(buffer);
    REQUIRE(nothing.has_value());
    CHECK_FALSE(nothing->has_value());

    DatagramSocket other = OpenLoopback();
    REQUIRE(other.Send(Local(socket), Bytes(4, 9)).has_value());
    const auto received = ReceiveWithin(socket, buffer);
    REQUIRE(received.has_value());
    CHECK(received->size == 4);
}
