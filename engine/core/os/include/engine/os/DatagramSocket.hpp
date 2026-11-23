#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OSError.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// A non-blocking UDP socket (docs/architecture/os.md, Datagram; the net Module's
// declaration D-1, v8-arch-gyo2-net-module). engine/core/os sends and receives
// one datagram at a time and reports failures in neutral codes; which of them
// are transient, what a lost datagram means and when to retry stay with the
// caller (Design Invariant 7). Nothing here blocks.

namespace GYO::Engine::OS {

// An IPv4 address and a port, both in host order.
struct DatagramAddress final {
    std::array<std::uint8_t, 4> octets{};
    std::uint16_t port = 0;

    // 127.0.0.1.
    [[nodiscard]] static constexpr DatagramAddress Loopback(const std::uint16_t port) noexcept {
        return {{127, 0, 0, 1}, port};
    }
    // 0.0.0.0: every local interface (for binding).
    [[nodiscard]] static constexpr DatagramAddress Any(const std::uint16_t port) noexcept {
        return {{0, 0, 0, 0}, port};
    }
    // Four decimal numbers 0..255 separated by dots ("192.168.0.10"); anything
    // else is InvalidAddress.
    [[nodiscard]] static Base::Result<DatagramAddress, OSError> Parse(std::string_view text, std::uint16_t port);

    // "a.b.c.d:port".
    [[nodiscard]] std::string ToString() const;

    friend constexpr bool operator==(const DatagramAddress&, const DatagramAddress&) noexcept = default;
};

struct ReceivedDatagram final {
    std::size_t size = 0;
    DatagramAddress from;
};

class DatagramSocket final {
public:
    // Binds a non-blocking UDP socket to local (port 0: the OS picks one).
    // AddressInUse, InvalidAddress (not a local address), AccessDenied, or
    // IOFailed with the native error.
    [[nodiscard]] static Base::Result<DatagramSocket, OSError> Open(const DatagramAddress& local);

    DatagramSocket(DatagramSocket&& other) noexcept;
    DatagramSocket& operator=(DatagramSocket&& other) noexcept;
    DatagramSocket(const DatagramSocket&) = delete;
    DatagramSocket& operator=(const DatagramSocket&) = delete;
    ~DatagramSocket();

    // The address the socket is bound to, with the port the OS picked.
    [[nodiscard]] Base::Result<DatagramAddress, OSError> LocalAddress() const;

    // Sends one datagram. WouldBlock (the OS has no room now), TooLarge,
    // Unreachable, AccessDenied, or IOFailed. A datagram the OS accepted may
    // still be lost on the way.
    [[nodiscard]] Base::Result<void, OSError> Send(const DatagramAddress& to, std::span<const std::byte> data);

    // Takes one waiting datagram into buffer, or nullopt when none is waiting.
    // A datagram longer than buffer is dropped and reported as TooLarge. An
    // ICMP "port unreachable" from an earlier send does not end up here on any
    // platform (Windows: SIO_UDP_CONNRESET is turned off at Open).
    [[nodiscard]] Base::Result<std::optional<ReceivedDatagram>, OSError> Receive(std::span<std::byte> buffer);

private:
    struct Impl;
    explicit DatagramSocket(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Engine::OS
