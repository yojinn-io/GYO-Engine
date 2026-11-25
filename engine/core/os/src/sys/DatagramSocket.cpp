// The UDP socket shared by macOS and Linux: BSD sockets, non-blocking through
// fcntl. The two platforms take the same calls, so no step goes through
// Detail::Platform.
#include "engine/os/DatagramSocket.hpp"

#include "engine/base/Assert.hpp"

#include <cerrno>
#include <cstring>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace GYO::Engine::OS {
namespace {

sockaddr_in ToNative(const SocketAddress& address) noexcept {
    sockaddr_in native{};
    native.sin_family = AF_INET;
    native.sin_port = htons(address.port);
    std::uint32_t host = 0;
    for (const std::uint8_t octet : address.octets) host = (host << 8) | octet;
    native.sin_addr.s_addr = htonl(host);
    return native;
}

SocketAddress FromNative(const sockaddr_in& native) noexcept {
    SocketAddress address;
    const std::uint32_t host = ntohl(native.sin_addr.s_addr);
    address.octets = {static_cast<std::uint8_t>(host >> 24), static_cast<std::uint8_t>(host >> 16),
        static_cast<std::uint8_t>(host >> 8), static_cast<std::uint8_t>(host)};
    address.port = ntohs(native.sin_port);
    return address;
}

OSError OpenError(const char* call, const int error) {
    OSErrorCode code = OSErrorCode::IOFailed;
    if (error == EADDRINUSE) code = OSErrorCode::AddressInUse;
    else if (error == EADDRNOTAVAIL) code = OSErrorCode::InvalidAddress;
    else if (error == EACCES || error == EPERM) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, std::string(call) + " failed", {NativeErrorDomain::Errno, error});
}

OSError TransferError(const char* call, const int error) {
    OSErrorCode code = OSErrorCode::IOFailed;
    if (error == EAGAIN || error == EWOULDBLOCK || error == ENOBUFS) code = OSErrorCode::WouldBlock;
    else if (error == EMSGSIZE) code = OSErrorCode::TooLarge;
    else if (error == ECONNREFUSED || error == EHOSTUNREACH || error == ENETUNREACH || error == ENETDOWN ||
             error == EHOSTDOWN)
        code = OSErrorCode::Unreachable;
    else if (error == EACCES || error == EPERM) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, std::string(call) + " failed", {NativeErrorDomain::Errno, error});
}

} // namespace

struct DatagramSocket::Impl final {
    int fd{-1};
    ~Impl() {
        if (fd >= 0) close(fd);
    }
};

Base::Result<DatagramSocket, OSError> DatagramSocket::Open(const SocketAddress& local) {
    auto impl = std::make_unique<Impl>();
    impl->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (impl->fd < 0) return Base::Err(OpenError("socket()", errno));
    const int flags = fcntl(impl->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(impl->fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return Base::Err(OpenError("fcntl(O_NONBLOCK)", errno));
    const int closeOnExec = fcntl(impl->fd, F_GETFD, 0);
    if (closeOnExec < 0 || fcntl(impl->fd, F_SETFD, closeOnExec | FD_CLOEXEC) < 0)
        return Base::Err(OpenError("fcntl(FD_CLOEXEC)", errno));
    const sockaddr_in native = ToNative(local);
    if (bind(impl->fd, reinterpret_cast<const sockaddr*>(&native), sizeof native) < 0)
        return Base::Err(OpenError("bind()", errno));
    return DatagramSocket(std::move(impl));
}

DatagramSocket::DatagramSocket(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
DatagramSocket::DatagramSocket(DatagramSocket&& other) noexcept = default;
DatagramSocket& DatagramSocket::operator=(DatagramSocket&& other) noexcept = default;
DatagramSocket::~DatagramSocket() = default;

Base::Result<SocketAddress, OSError> DatagramSocket::LocalAddress() const {
    GYO_ASSERT(impl_ != nullptr);
    sockaddr_in native{};
    socklen_t length = sizeof native;
    if (getsockname(impl_->fd, reinterpret_cast<sockaddr*>(&native), &length) < 0)
        return Base::Err(MakeOSError(OSErrorCode::IOFailed, "getsockname() failed", {NativeErrorDomain::Errno, errno}));
    return FromNative(native);
}

Base::Result<void, OSError> DatagramSocket::Send(const SocketAddress& to, const std::span<const std::byte> data) {
    GYO_ASSERT(impl_ != nullptr);
    const sockaddr_in native = ToNative(to);
    for (;;) {
        const ssize_t sent = sendto(impl_->fd, data.data(), data.size(), 0, reinterpret_cast<const sockaddr*>(&native),
            sizeof native);
        if (sent >= 0) return {};
        const int error = errno;
        if (error == EINTR) continue;
        return Base::Err(TransferError("sendto()", error));
    }
}

Base::Result<std::optional<ReceivedDatagram>, OSError> DatagramSocket::Receive(const std::span<std::byte> buffer) {
    GYO_ASSERT(impl_ != nullptr);
    for (;;) {
        sockaddr_in native{};
        iovec part{buffer.data(), buffer.size()};
        msghdr message{};
        message.msg_name = &native;
        message.msg_namelen = sizeof native;
        message.msg_iov = &part;
        message.msg_iovlen = 1;
        const ssize_t received = recvmsg(impl_->fd, &message, 0);
        if (received >= 0) {
            if ((message.msg_flags & MSG_TRUNC) != 0)
                return Base::Err(OSError::Make(OSErrorCode::TooLarge, "datagram longer than the buffer was dropped",
                    "buffer_bytes=" + std::to_string(buffer.size())));
            return std::optional<ReceivedDatagram>(ReceivedDatagram{static_cast<std::size_t>(received),
                FromNative(native)});
        }
        const int error = errno;
        if (error == EINTR) continue;
        if (error == EAGAIN || error == EWOULDBLOCK) return std::optional<ReceivedDatagram>();
        return Base::Err(TransferError("recvmsg()", error));
    }
}

} // namespace GYO::Engine::OS
