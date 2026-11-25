// The TCP client connection on Windows: Winsock 2, non-blocking through FIONBIO, the connect's outcome through select
// (writable: connected; exception: failed, SO_ERROR says why). select and not WSAPoll: WSAPoll does not report a
// failed connect before Windows 10 2004. Each socket holds one WSAStartup reference, as the datagram socket does.
#include "engine/os/StreamSocket.hpp"

#include "engine/base/Assert.hpp"

#include <climits>
#include <string>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace GYO::Engine::OS {
namespace {

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
    if (error == WSAEADDRNOTAVAIL || error == WSAEAFNOSUPPORT) code = OSErrorCode::InvalidAddress;
    else if (error == WSAEACCES) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, std::string(call) + " failed", {NativeErrorDomain::Win32, error});
}

OSError ConnectError(const int error) {
    OSErrorCode code = OSErrorCode::IOFailed;
    if (error == WSAECONNREFUSED || error == WSAEHOSTUNREACH || error == WSAENETUNREACH || error == WSAENETDOWN ||
        error == WSAEHOSTDOWN || error == WSAETIMEDOUT)
        code = OSErrorCode::Unreachable;
    else if (error == WSAEACCES) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, "connect() failed", {NativeErrorDomain::Win32, error});
}

} // namespace

struct StreamSocket::Impl final {
    bool started{false};
    SOCKET handle{INVALID_SOCKET};
    bool connected{false};
    std::optional<OSError> failed;
    ~Impl() {
        if (handle != INVALID_SOCKET) closesocket(handle);
        if (started) WSACleanup();
    }
};

Base::Result<StreamSocket, OSError> StreamSocket::Connect(const DatagramAddress& to) {
    auto impl = std::make_unique<Impl>();
    WSADATA data{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup != 0) return Base::Err(OpenError("WSAStartup()", startup));
    impl->started = true;
    impl->handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
    if (impl->handle == INVALID_SOCKET) return Base::Err(OpenError("WSASocketW()", WSAGetLastError()));
    u_long nonBlocking = 1;
    if (ioctlsocket(impl->handle, FIONBIO, &nonBlocking) != 0)
        return Base::Err(OpenError("ioctlsocket(FIONBIO)", WSAGetLastError()));
    // Small requests and answers: do not hold them back to coalesce.
    const BOOL on = TRUE;
    if (setsockopt(impl->handle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof on) != 0)
        return Base::Err(OpenError("setsockopt(TCP_NODELAY)", WSAGetLastError()));
    const sockaddr_in native = ToNative(to);
    if (connect(impl->handle, reinterpret_cast<const sockaddr*>(&native), sizeof native) == 0) {
        impl->connected = true;
    } else {
        const int error = WSAGetLastError();
        if (error == WSAEADDRNOTAVAIL || error == WSAEAFNOSUPPORT || error == WSAEACCES)
            return Base::Err(OpenError("connect()", error));
        if (error != WSAEWOULDBLOCK) impl->failed = ConnectError(error);
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
    fd_set writable;
    fd_set exceptional;
    FD_ZERO(&writable);
    FD_ZERO(&exceptional);
    FD_SET(impl_->handle, &writable);
    FD_SET(impl_->handle, &exceptional);
    timeval now{0, 0};
    const int ready = select(0, nullptr, &writable, &exceptional, &now);
    if (ready == SOCKET_ERROR)
        return Base::Err(MakeOSError(OSErrorCode::IOFailed, "select() failed", {NativeErrorDomain::Win32, WSAGetLastError()}));
    if (ready == 0) return StreamState::Connecting;
    if (FD_ISSET(impl_->handle, &exceptional)) {
        int error = 0;
        int length = sizeof error;
        if (getsockopt(impl_->handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) != 0)
            error = WSAGetLastError();
        impl_->failed = ConnectError(error == 0 ? WSAECONNREFUSED : error);
        return Base::Err(*impl_->failed);
    }
    impl_->connected = true;
    return StreamState::Connected;
}

Base::Result<std::size_t, OSError> StreamSocket::Send(const std::span<const std::byte> data) {
    GYO_ASSERT(impl_ != nullptr);
    const int size = data.size() > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(data.size());
    const int sent = send(impl_->handle, reinterpret_cast<const char*>(data.data()), size, 0);
    if (sent != SOCKET_ERROR) return static_cast<std::size_t>(sent);
    const int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK) return std::size_t{0};
    return Base::Err(MakeOSError(OSErrorCode::IOFailed, "send() failed", {NativeErrorDomain::Win32, error}));
}

Base::Result<std::optional<std::size_t>, OSError> StreamSocket::Receive(const std::span<std::byte> buffer) {
    GYO_ASSERT(impl_ != nullptr);
    const int capacity = buffer.size() > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(buffer.size());
    const int received = recv(impl_->handle, reinterpret_cast<char*>(buffer.data()), capacity, 0);
    if (received != SOCKET_ERROR) return std::optional<std::size_t>(static_cast<std::size_t>(received));
    const int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK) return std::optional<std::size_t>();
    return Base::Err(MakeOSError(OSErrorCode::IOFailed, "recv() failed", {NativeErrorDomain::Win32, error}));
}

} // namespace GYO::Engine::OS
