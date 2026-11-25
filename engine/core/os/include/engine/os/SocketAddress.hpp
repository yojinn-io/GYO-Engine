#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OSError.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

// The address a socket binds to, sends to or connects to (docs/architecture/os.md, Datagram and Stream): the UDP
// socket and the TCP stream take the same type. It is not the net Module's Endpoint, which names a peer on a Link.

namespace GYO::Engine::OS {

// An IPv4 address and a port, both in host order.
struct SocketAddress final {
    std::array<std::uint8_t, 4> octets{};
    std::uint16_t port = 0;

    // 127.0.0.1.
    [[nodiscard]] static constexpr SocketAddress Loopback(const std::uint16_t port) noexcept {
        return {{127, 0, 0, 1}, port};
    }
    // 0.0.0.0: every local interface (for binding).
    [[nodiscard]] static constexpr SocketAddress Any(const std::uint16_t port) noexcept {
        return {{0, 0, 0, 0}, port};
    }
    // Four decimal numbers 0..255 separated by dots ("192.168.0.10"); anything
    // else is InvalidAddress.
    [[nodiscard]] static Base::Result<SocketAddress, OSError> Parse(std::string_view text, std::uint16_t port);

    // "a.b.c.d:port".
    [[nodiscard]] std::string ToString() const;

    friend constexpr bool operator==(const SocketAddress&, const SocketAddress&) noexcept = default;
};

} // namespace GYO::Engine::OS
