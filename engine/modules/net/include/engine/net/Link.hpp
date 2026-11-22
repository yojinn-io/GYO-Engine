#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "engine/net/NetFailure.hpp"

// The Link (declaration D-3): the narrow edge between the one protocol and a transport. It sends one datagram
// to an Endpoint and takes one waiting datagram; it holds bytes only, and knows nothing of packets, channels
// or game state (Design Invariant draft NT-2). UDP is UdpLink; MemoryLink is the in-process pair the tests
// use. Another transport (WebSocket, a relay) is another Link, and nothing above it changes.

namespace GYO::Engine::Net {

// An opaque address on one Link. Its meaning belongs to the Link that made it (UdpLink packs an IPv4 address
// and a port); the layers above only compare and copy it.
struct Endpoint final {
    std::uint64_t value = 0;

    friend constexpr bool operator==(const Endpoint&, const Endpoint&) noexcept = default;
};

struct ReceivedDatagram final {
    std::size_t size = 0;
    Endpoint from;
};

class Link {
public:
    virtual ~Link() = default;

    // Sends one datagram. A datagram the Link accepted may still be lost on the way.
    [[nodiscard]] virtual NetResult<void> Send(Endpoint to, std::span<const std::byte> datagram) = 0;

    // Takes one waiting datagram into buffer, or nullopt when none is waiting. Never blocks.
    [[nodiscard]] virtual NetResult<std::optional<ReceivedDatagram>> Receive(std::span<std::byte> buffer) = 0;

protected:
    Link() = default;
    Link(const Link&) = default;
    Link(Link&&) noexcept = default;
    Link& operator=(const Link&) = default;
    Link& operator=(Link&&) noexcept = default;
};

} // namespace GYO::Engine::Net
