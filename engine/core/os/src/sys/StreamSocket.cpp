// The TCP client connection shared by macOS and Linux: BSD sockets, non-blocking through fcntl, the connect's outcome
// through poll and SO_ERROR. A write to a connection the peer reset must not raise SIGPIPE: MSG_NOSIGNAL on each send
// where the C library has it (Linux; macOS SDKs since 2023), else SO_NOSIGPIPE on the socket (older macOS SDKs). One of
// the two, chosen by the feature macros (no OS macro).
#include "engine/os/StreamSocket.hpp"

#include "engine/base/Assert.hpp"

#include <cerrno>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace GYO::Engine::OS {
namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

sockaddr_in ToNative(const DatagramAddress& address) noexcept {
    sockaddr_in native{};
    native.sin_family = AF_INET;
    native.sin_port = htons(address.port);
    std::uint32_t host = 0;
    for (const std::uint8_t octet : address.octets) host = (host << 8) | octet;
    native.sin_addr.s_addr = htonl(host);
    return native;
}

OSError OpenError(const char* call, const int error) {
    OSErrorCode code = OSErrorCode::IOFailed;
    if (error == EADDRNOTAVAIL || error == EAFNOSUPPORT) code = OSErrorCode::InvalidAddress;
    else if (error == EACCES || error == EPERM) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, std::string(call) + " failed", {NativeErrorDomain::Errno, error});
}

OSError ConnectError(const int error) {
    OSErrorCode code = OSErrorCode::IOFailed;
    if (error == ECONNREFUSED || error == EHOSTUNREACH || error == ENETUNREACH || error == ENETDOWN ||
        error == EHOSTDOWN || error == ETIMEDOUT)
        code = OSErrorCode::Unreachable;
    else if (error == EACCES || error == EPERM) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, "connect() failed", {NativeErrorDomain::Errno, error});
}

} // namespace

struct StreamSocket::Impl final {
    int fd{-1};
    bool connected{false};
    std::optional<OSError> failed;
    ~Impl() {
        if (fd >= 0) close(fd);
    }
};

Base::Result<StreamSocket, OSError> StreamSocket::Connect(const DatagramAddress& to) {
    auto impl = std::make_unique<Impl>();
    impl->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (impl->fd < 0) return Base::Err(OpenError("socket()", errno));
    const int flags = fcntl(impl->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(impl->fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return Base::Err(OpenError("fcntl(O_NONBLOCK)", errno));
    const int closeOnExec = fcntl(impl->fd, F_GETFD, 0);
    if (closeOnExec < 0 || fcntl(impl->fd, F_SETFD, closeOnExec | FD_CLOEXEC) < 0)
        return Base::Err(OpenError("fcntl(FD_CLOEXEC)", errno));
    const int on = 1;
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    if (setsockopt(impl->fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on) < 0)
        return Base::Err(OpenError("setsockopt(SO_NOSIGPIPE)", errno));
#endif
    // Small requests and answers: do not hold them back to coalesce.
    if (setsockopt(impl->fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on) < 0)
        return Base::Err(OpenError("setsockopt(TCP_NODELAY)", errno));
    const sockaddr_in native = ToNative(to);
    for (;;) {
        if (connect(impl->fd, reinterpret_cast<const sockaddr*>(&native), sizeof native) == 0) {
            impl->connected = true;
            break;
        }
        const int error = errno;
        // Interrupted: the connect goes on in the background (calling again would be EALREADY); State reports it.
        if (error == EINPROGRESS || error == EINTR) break;
        if (error == EADDRNOTAVAIL || error == EAFNOSUPPORT || error == EACCES || error == EPERM)
            return Base::Err(OpenError("connect()", error));
        // Refused at once (loopback): the outcome State reports.
        impl->failed = ConnectError(error);
        break;
    }
    return StreamSocket(std::move(impl));
}

StreamSocket::StreamSocket(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
StreamSocket::StreamSocket(StreamSocket&& other) noexcept = default;
StreamSocket& StreamSocket::operator=(StreamSocket&& other) noexcept = default;
StreamSocket::~StreamSocket() = default;

Base::Result<StreamState, OSError> StreamSocket::State() {
    GYO_ASSERT(impl_ != nullptr);
    if (impl_->failed) return Base::Err(*impl_->failed);
    if (impl_->connected) return StreamState::Connected;
    pollfd watched{impl_->fd, POLLOUT, 0};
    const int ready = poll(&watched, 1, 0);
    if (ready < 0) {
        if (errno == EINTR) return StreamState::Connecting;
        return Base::Err(MakeOSError(OSErrorCode::IOFailed, "poll() failed", {NativeErrorDomain::Errno, errno}));
    }
    if (ready == 0) return StreamState::Connecting;
    int error = 0;
    socklen_t length = sizeof error;
    if (getsockopt(impl_->fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) error = errno;
    if (error != 0) {
        impl_->failed = ConnectError(error);
        return Base::Err(*impl_->failed);
    }
    impl_->connected = true;
    return StreamState::Connected;
}

Base::Result<std::size_t, OSError> StreamSocket::Send(const std::span<const std::byte> data) {
    GYO_ASSERT(impl_ != nullptr);
    // The connection's own state first, so every platform answers the same before Connected and after a failure.
    if (!impl_->connected) {
        auto state = State();
        if (!state) return Base::Err(state.error());
        if (*state != StreamState::Connected) return std::size_t{0};
    }
    for (;;) {
        const ssize_t sent = send(impl_->fd, data.data(), data.size(), kSendFlags);
        if (sent >= 0) return static_cast<std::size_t>(sent);
        const int error = errno;
        if (error == EINTR) continue;
        if (error == EAGAIN || error == EWOULDBLOCK) return std::size_t{0};
        return Base::Err(MakeOSError(OSErrorCode::IOFailed, "send() failed", {NativeErrorDomain::Errno, error}));
    }
}

Base::Result<std::optional<std::size_t>, OSError> StreamSocket::Receive(const std::span<std::byte> buffer) {
    GYO_ASSERT(impl_ != nullptr);
    if (!impl_->connected) {
        auto state = State();
        if (!state) return Base::Err(state.error());
        if (*state != StreamState::Connected) return std::optional<std::size_t>();
    }
    for (;;) {
        const ssize_t received = recv(impl_->fd, buffer.data(), buffer.size(), 0);
        if (received >= 0) return std::optional<std::size_t>(static_cast<std::size_t>(received));
        const int error = errno;
        if (error == EINTR) continue;
        if (error == EAGAIN || error == EWOULDBLOCK) return std::optional<std::size_t>();
        return Base::Err(MakeOSError(OSErrorCode::IOFailed, "recv() failed", {NativeErrorDomain::Errno, error}));
    }
}

} // namespace GYO::Engine::OS
