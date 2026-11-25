#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/DatagramSocket.hpp"
#include "engine/os/OSError.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

// A non-blocking IPv4 TCP client connection (docs/architecture/os.md, Stream; the PvP Gateway port's declaration D8,
// games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md: a Client talks to the Gateway, a Match to its services).
// engine/core/os opens it, reports how the connect went, and moves bytes; what the bytes mean (HTTP or anything else),
// when to give up and what to retry stay with the caller (Design Invariant 7). Nothing here blocks: the caller asks
// again on its next frame. The address is the same IPv4 address and port the datagram socket takes.

namespace GYO::Engine::OS {

enum class StreamState : std::uint8_t {
    // The OS is still connecting.
    Connecting = 1,
    // The connection is open: Send and Receive move bytes.
    Connected,
};

class StreamSocket final {
public:
    // Starts connecting to `to`. The connect's outcome comes from State. InvalidAddress, AccessDenied or IOFailed when
    // the OS refuses to start it.
    [[nodiscard]] static Base::Result<StreamSocket, OSError> Connect(const DatagramAddress& to);

    StreamSocket(StreamSocket&& other) noexcept;
    StreamSocket& operator=(StreamSocket&& other) noexcept;
    StreamSocket(const StreamSocket&) = delete;
    StreamSocket& operator=(const StreamSocket&) = delete;
    // Closes the connection.
    ~StreamSocket();

    // Connecting or Connected; a connect that failed is Unreachable (refused, no route, timed out) or IOFailed, and it
    // stays failed.
    [[nodiscard]] Base::Result<StreamState, OSError> State();

    // Sends what the OS takes now: the number of bytes taken, 0 when it has no room. Before Connected, and after the
    // peer reset the connection, it fails (IOFailed with the native error).
    [[nodiscard]] Base::Result<std::size_t, OSError> Send(std::span<const std::byte> data);

    // Takes the bytes waiting into buffer: their number; 0 when the peer closed its side (nothing more will come);
    // nullopt when nothing is waiting now.
    [[nodiscard]] Base::Result<std::optional<std::size_t>, OSError> Receive(std::span<std::byte> buffer);

private:
    struct Impl;
    explicit StreamSocket(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Engine::OS
