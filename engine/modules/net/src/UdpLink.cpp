#include "engine/net/UdpLink.hpp"

#include <utility>

namespace GYO::Engine::Net {
namespace {

NetFailure FromOS(const OS::OSError& error, const NetFailureCode code, const char* what) {
    return NetFailure::Make(code, what, Base::CauseDetail(error));
}

} // namespace

UdpLink::UdpLink(OS::DatagramSocket socket) noexcept : socket_(std::move(socket)) {}

NetResult<UdpLink> UdpLink::Open(const OS::DatagramAddress& local) {
    auto socket = OS::DatagramSocket::Open(local);
    if (!socket) {
        return Base::Err(FromOS(socket.error(), NetFailureCode::LinkFailed, "the UDP socket could not be opened"));
    }
    return UdpLink(std::move(*socket));
}

NetResult<OS::DatagramAddress> UdpLink::LocalAddress() const {
    auto address = socket_.LocalAddress();
    if (!address) {
        return Base::Err(FromOS(address.error(), NetFailureCode::LinkFailed, "the UDP socket has no local address"));
    }
    return *address;
}

NetResult<void> UdpLink::Send(const Endpoint to, const std::span<const std::byte> datagram) {
    auto sent = socket_.Send(ToAddress(to), datagram);
    if (sent) return {};
    switch (sent.error().code) {
    case OS::OSErrorCode::WouldBlock:
        return Base::Err(FromOS(sent.error(), NetFailureCode::LinkBusy, "the UDP socket cannot send now"));
    case OS::OSErrorCode::Unreachable:
        return Base::Err(FromOS(sent.error(), NetFailureCode::Unreachable, "the remote endpoint cannot be reached"));
    default:
        return Base::Err(FromOS(sent.error(), NetFailureCode::LinkFailed, "the UDP send failed"));
    }
}

NetResult<std::optional<ReceivedDatagram>> UdpLink::Receive(const std::span<std::byte> buffer) {
    auto received = socket_.Receive(buffer);
    if (!received) {
        if (received.error().code == OS::OSErrorCode::TooLarge) {
            return Base::Err(FromOS(received.error(), NetFailureCode::DatagramTooLarge,
                                    "a datagram longer than the receive buffer was dropped"));
        }
        return Base::Err(FromOS(received.error(), NetFailureCode::LinkFailed, "the UDP receive failed"));
    }
    if (!received->has_value()) return std::optional<ReceivedDatagram>{};
    return std::optional<ReceivedDatagram>{ReceivedDatagram{(*received)->size, ToEndpoint((*received)->from)}};
}

} // namespace GYO::Engine::Net
