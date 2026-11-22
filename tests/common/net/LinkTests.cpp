// The Links (net Module declaration D-3): UdpLink over loopback, and the Endpoint packing.
#include <doctest/doctest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "NetTestSupport.hpp"
#include "engine/net/UdpLink.hpp"

namespace N = GYO::Engine::Net;
namespace OS = GYO::Engine::OS;
using GYO::Test::Net::Bytes;

TEST_CASE("link: an Endpoint packs an IPv4 address and a port and gives them back") {
    const OS::DatagramAddress address{{192, 168, 0, 10}, 40123};
    const N::Endpoint endpoint = N::UdpLink::ToEndpoint(address);
    CHECK(endpoint.value == 0xC0A8000A9CBBull);
    CHECK(N::UdpLink::ToAddress(endpoint) == address);
}

TEST_CASE("link: a datagram goes over UDP loopback with its sender") {
    auto a = N::UdpLink::Open(OS::DatagramAddress::Loopback(0));
    auto b = N::UdpLink::Open(OS::DatagramAddress::Loopback(0));
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    const auto aAddress = a->LocalAddress();
    const auto bAddress = b->LocalAddress();
    REQUIRE(aAddress.has_value());
    REQUIRE(bAddress.has_value());

    std::vector<std::byte> buffer(64);
    auto nothing = b->Receive(buffer);
    REQUIRE(nothing.has_value());
    CHECK_FALSE(nothing->has_value());

    const auto payload = Bytes({1, 2, 3, 4, 5});
    REQUIRE(a->Send(N::UdpLink::ToEndpoint(*bAddress), payload).has_value());
    std::optional<N::ReceivedDatagram> got;
    for (int attempt = 0; attempt < 2000 && !got; ++attempt) {
        auto received = b->Receive(buffer);
        REQUIRE(received.has_value());
        got = *received;
        if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(got.has_value());
    CHECK(std::vector<std::byte>(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(got->size)) == payload);
    CHECK(got->from == N::UdpLink::ToEndpoint(*aAddress));
}
