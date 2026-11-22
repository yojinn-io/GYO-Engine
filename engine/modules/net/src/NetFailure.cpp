#include "engine/net/NetFailure.hpp"

namespace GYO::Engine::Net {

const char* ToString(const NetFailureCode code) noexcept {
    switch (code) {
    case NetFailureCode::Truncated: return "Truncated";
    case NetFailureCode::NotOurProtocol: return "NotOurProtocol";
    case NetFailureCode::ProtocolVersionMismatch: return "ProtocolVersionMismatch";
    case NetFailureCode::UnknownPacketKind: return "UnknownPacketKind";
    case NetFailureCode::MalformedPayload: return "MalformedPayload";
    case NetFailureCode::MessageRejected: return "MessageRejected";
    case NetFailureCode::LinkBusy: return "LinkBusy";
    case NetFailureCode::Unreachable: return "Unreachable";
    case NetFailureCode::DatagramTooLarge: return "DatagramTooLarge";
    case NetFailureCode::LinkFailed: return "LinkFailed";
    }
    return "Unknown";
}

} // namespace GYO::Engine::Net
