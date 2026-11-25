#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OSError.hpp"
#include "engine/os/SocketAddress.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

// A non-blocking UDP socket (docs/architecture/os.md, Datagram; the net Module's
// declaration D-1, v8-arch-gyo2-net-module). engine/core/os sends and receives
// one datagram at a time and reports failures in neutral codes; which of them
// are transient, what a lost datagram means and when to retry stay with the
// caller (Design Invariant 7). Nothing here blocks.

namespace GYO::Engine::OS {

struct ReceivedDatagram final {
    std::size_t size = 0;
    SocketAddress from;
};

class DatagramSocket final {
public:
    // Binds a non-blocking UDP socket to local (port 0: the OS picks one).
    // AddressInUse, InvalidAddress (not a local address), AccessDenied, or
    // IOFailed with the native error.
    [[nodiscard]] static Base::Result<DatagramSocket, OSError> Open(const SocketAddress& local);

    DatagramSocket(DatagramSocket&& other) noexcept;
    DatagramSocket& operator=(DatagramSocket&& other) noexcept;
    DatagramSocket(const DatagramSocket&) = delete;
    DatagramSocket& operator=(const DatagramSocket&) = delete;
    ~DatagramSocket();

    // The address the socket is bound to, with the port the OS picked.
    [[nodiscard]] Base::Result<SocketAddress, OSError> LocalAddress() const;

    // Sends one datagram. WouldBlock (the OS has no room now), TooLarge,
    // Unreachable, AccessDenied, or IOFailed. A datagram the OS accepted may
    // still be lost on the way.
    [[nodiscard]] Base::Result<void, OSError> Send(const SocketAddress& to, std::span<const std::byte> data);

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
