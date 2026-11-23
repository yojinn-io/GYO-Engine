// The UDP socket on Windows: Winsock 2, non-blocking through FIONBIO. Each
// socket holds one WSAStartup reference (Winsock counts them) and releases it
// when it closes. SIO_UDP_CONNRESET and SIO_UDP_NETRESET are turned off so an
// ICMP reply to an earlier send does not make the next receive fail; macOS and
// Linux do not report it on an unconnected socket either, so the three
// platforms behave alike.
#include "engine/os/DatagramSocket.hpp"

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
#include <mstcpip.h>

// mstcpip.h declares these only for some SDK targets; the values are the
// documented Winsock IOCTL codes (IOC_VENDOR 12 and 15).
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#ifndef SIO_UDP_NETRESET
#define SIO_UDP_NETRESET _WSAIOW(IOC_VENDOR, 15)
#endif

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

DatagramAddress FromNative(const sockaddr_in& native) noexcept {
    DatagramAddress address;
    const std::uint32_t host = ntohl(native.sin_addr.s_addr);
    address.octets = {static_cast<std::uint8_t>(host >> 24), static_cast<std::uint8_t>(host >> 16),
        static_cast<std::uint8_t>(host >> 8), static_cast<std::uint8_t>(host)};
    address.port = ntohs(native.sin_port);
    return address;
}

OSError OpenError(const char* call, const int error) {
    OSErrorCode code = OSErrorCode::IOFailed;
    if (error == WSAEADDRINUSE) code = OSErrorCode::AddressInUse;
    else if (error == WSAEADDRNOTAVAIL) code = OSErrorCode::InvalidAddress;
    else if (error == WSAEACCES) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, std::string(call) + " failed", {NativeErrorDomain::Win32, error});
}

OSError TransferError(const char* call, const int error) {
    OSErrorCode code = OSErrorCode::IOFailed;
    if (error == WSAEWOULDBLOCK || error == WSAENOBUFS) code = OSErrorCode::WouldBlock;
    else if (error == WSAEMSGSIZE) code = OSErrorCode::TooLarge;
    else if (error == WSAECONNRESET || error == WSAENETRESET || error == WSAEHOSTUNREACH ||
             error == WSAENETUNREACH || error == WSAENETDOWN || error == WSAEHOSTDOWN)
        code = OSErrorCode::Unreachable;
    else if (error == WSAEACCES) code = OSErrorCode::AccessDenied;
    return MakeOSError(code, std::string(call) + " failed", {NativeErrorDomain::Win32, error});
}

} // namespace

struct DatagramSocket::Impl final {
    bool started{false};
    SOCKET handle{INVALID_SOCKET};
    ~Impl() {
        if (handle != INVALID_SOCKET) closesocket(handle);
        if (started) WSACleanup();
    }
};

Base::Result<DatagramSocket, OSError> DatagramSocket::Open(const DatagramAddress& local) {
    auto impl = std::make_unique<Impl>();
    WSADATA data{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup != 0) return Base::Err(OpenError("WSAStartup()", startup));
    impl->started = true;
    impl->handle = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
    if (impl->handle == INVALID_SOCKET) return Base::Err(OpenError("WSASocketW()", WSAGetLastError()));
    u_long nonBlocking = 1;
    if (ioctlsocket(impl->handle, FIONBIO, &nonBlocking) != 0)
        return Base::Err(OpenError("ioctlsocket(FIONBIO)", WSAGetLastError()));
    for (const DWORD control : {static_cast<DWORD>(SIO_UDP_CONNRESET), static_cast<DWORD>(SIO_UDP_NETRESET)}) {
        BOOL report = FALSE;
        DWORD returned = 0;
        if (WSAIoctl(impl->handle, control, &report, sizeof report, nullptr, 0, &returned, nullptr, nullptr) != 0)
            return Base::Err(OpenError("WSAIoctl(SIO_UDP_*RESET)", WSAGetLastError()));
    }
    const sockaddr_in native = ToNative(local);
    if (bind(impl->handle, reinterpret_cast<const sockaddr*>(&native), sizeof native) != 0)
        return Base::Err(OpenError("bind()", WSAGetLastError()));
    return DatagramSocket(std::move(impl));
}

DatagramSocket::DatagramSocket(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
DatagramSocket::DatagramSocket(DatagramSocket&& other) noexcept = default;
DatagramSocket& DatagramSocket::operator=(DatagramSocket&& other) noexcept = default;
DatagramSocket::~DatagramSocket() = default;

Base::Result<DatagramAddress, OSError> DatagramSocket::LocalAddress() const {
    GYO_ASSERT(impl_ != nullptr);
    sockaddr_in native{};
    int length = sizeof native;
    if (getsockname(impl_->handle, reinterpret_cast<sockaddr*>(&native), &length) != 0)
        return Base::Err(
            MakeOSError(OSErrorCode::IOFailed, "getsockname() failed", {NativeErrorDomain::Win32, WSAGetLastError()}));
    return FromNative(native);
}

Base::Result<void, OSError> DatagramSocket::Send(const DatagramAddress& to, const std::span<const std::byte> data) {
    GYO_ASSERT(impl_ != nullptr);
    if (data.size() > static_cast<std::size_t>(INT_MAX))
        return Base::Err(OSError::Make(OSErrorCode::TooLarge, "datagram too large to send",
            "data_bytes=" + std::to_string(data.size())));
    const sockaddr_in native = ToNative(to);
    const int sent = sendto(impl_->handle, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
        reinterpret_cast<const sockaddr*>(&native), sizeof native);
    if (sent != SOCKET_ERROR) return {};
    return Base::Err(TransferError("sendto()", WSAGetLastError()));
}

Base::Result<std::optional<ReceivedDatagram>, OSError> DatagramSocket::Receive(const std::span<std::byte> buffer) {
    GYO_ASSERT(impl_ != nullptr);
    sockaddr_in native{};
    int length = sizeof native;
    const int capacity = buffer.size() > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(buffer.size());
    const int received = recvfrom(impl_->handle, reinterpret_cast<char*>(buffer.data()), capacity, 0,
        reinterpret_cast<sockaddr*>(&native), &length);
    if (received != SOCKET_ERROR)
        return std::optional<ReceivedDatagram>(ReceivedDatagram{static_cast<std::size_t>(received), FromNative(native)});
    const int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK) return std::optional<ReceivedDatagram>();
    if (error == WSAEMSGSIZE)
        return Base::Err(OSError::Make(OSErrorCode::TooLarge, "datagram longer than the buffer was dropped",
            "buffer_bytes=" + std::to_string(buffer.size())));
    return Base::Err(TransferError("recvfrom()", error));
}

} // namespace GYO::Engine::OS
