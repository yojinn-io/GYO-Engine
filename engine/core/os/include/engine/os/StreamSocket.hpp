#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OSError.hpp"
#include "engine/os/SocketAddress.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

// A non-blocking IPv4 TCP client connection (docs/architecture/os.md, Stream). engine/core/os opens it, reports how the
// connect went, and moves bytes; what the bytes mean (HTTP or anything else), when to give up and what to retry stay
// with the caller (Design Invariant 7). Nothing here blocks: the caller asks again on its next frame. The address is
// a SocketAddress, the same type the datagram socket takes. The OS's own connect timeout (tens of seconds) is the only
// one here; a caller that wants a shorter one drops the socket.

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
    [[nodiscard]] static Base::Result<StreamSocket, OSError> Connect(const SocketAddress& to);

    StreamSocket(StreamSocket&& other) noexcept;
    StreamSocket& operator=(StreamSocket&& other) noexcept;
    StreamSocket(const StreamSocket&) = delete;
    StreamSocket& operator=(const StreamSocket&) = delete;
    // Closes the connection.
    ~StreamSocket();

    // Connecting or Connected; a connect that failed is Unreachable (refused, no route, timed out) or IOFailed, and it
    // stays failed.
    [[nodiscard]] Base::Result<StreamState, OSError> State();

    // Sends what the OS takes now: the number of bytes taken, 0 when it has no room or the connect has not finished.
    // A connect that failed is State's error; a connection the peer reset is IOFailed with the native error. It never
    // raises a signal.
    [[nodiscard]] Base::Result<std::size_t, OSError> Send(std::span<const std::byte> data);

    // Takes the bytes waiting into buffer: their number; 0 when the peer closed its side (nothing more will come; an
    // empty buffer also reads 0, so give it room); nullopt when nothing is waiting now or the connect has not finished.
    // A connect that failed is State's error.
    [[nodiscard]] Base::Result<std::optional<std::size_t>, OSError> Receive(std::span<std::byte> buffer);

private:
    struct Impl;
    explicit StreamSocket(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Engine::OS
