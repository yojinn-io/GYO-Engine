#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

// The failure of the net Module (E2 in docs/architecture/target/error-handling.md, "Error levels": this
// request did not succeed, everything else is intact). Every failure goes back to the caller, which decides
// what follows; the Module writes no log line of its own.

namespace GYO::Engine::Net {

// Zero is not a valid code; the numeric values are not a data contract.
enum class NetFailureCode : std::uint16_t {
    // A datagram shorter than the packet header, or than an entry it announces.
    Truncated = 1,
    // A datagram that does not start with the protocol's magic number.
    NotOurProtocol,
    // A datagram of another protocol version.
    ProtocolVersionMismatch,
    // A packet kind this version does not know (the handshake kinds are reserved, not known yet).
    UnknownPacketKind,
    // The payload after the header cannot be read: an unknown channel, a fragment index past its count, a
    // message larger than the limit, or bytes left over.
    MalformedPayload,
    // The message cannot be queued: the channel's send queue is at its limit, or the message is larger than
    // the limit.
    MessageRejected,
    // The Link cannot take a datagram now without blocking (the datagram is not sent).
    LinkBusy,
    // The Link reports that the remote endpoint or the network cannot be reached.
    Unreachable,
    // A waiting datagram was longer than the receive buffer and was dropped by the Link.
    DatagramTooLarge,
    // Any other failure of the Link; the detail carries the cause.
    LinkFailed,
};

[[nodiscard]] const char* ToString(NetFailureCode code) noexcept;

using NetFailure = Base::Error<NetFailureCode>;
static_assert(Base::CodedError<NetFailure>);

template <class T>
using NetResult = Base::Result<T, NetFailure>;

} // namespace GYO::Engine::Net
