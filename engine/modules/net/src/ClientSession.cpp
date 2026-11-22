#include "engine/net/ClientSession.hpp"

#include <utility>

#include "ByteCursor.hpp"

namespace GYO::Engine::Net {

ClientSession::ClientSession(SessionSettings settings, ConnectRequest request, const NetTime now)
    : settings_(std::move(settings)), request_(std::move(request)), started_(now) {
    Detail::EncodeRequestBody(request_, requestBody_);
}

void ClientSession::End(const DisconnectReason reason, std::string detail) {
    state_ = ClientState::Disconnected;
    ended_ = Rejection{reason, std::move(detail)};
}

void ClientSession::Receive(const std::span<const std::byte> datagram, const NetTime now) {
    if (state_ == ClientState::Disconnected) {
        // A datagram for a connection that has ended (D-12: counted, not silent).
        Count(DropReason::UnknownSource);
        return;
    }
    const auto header = DecodePacketHeader(datagram);
    if (!header) {
        Count(DropReason::DecodeFailed);
        return;
    }
    const auto body = datagram.subspan(kPacketHeaderSize);
    if (IsHandshake(header->kind)) {
        if (state_ != ClientState::Connecting) {
            // A late copy of the accept (the server answers each response), or anything else after the handshake.
            Count(DropReason::Unexpected);
            return;
        }
        switch (header->kind) {
        case PacketKind::Challenge: {
            Detail::ByteReader reader(body);
            std::uint32_t low = 0, high = 0;
            if (!reader.U32(low) || !reader.U32(high)) {
                Count(DropReason::DecodeFailed);
                return;
            }
            challenge_ = (static_cast<std::uint64_t>(high) << 32) | low;
            lastHandshakeSent_.reset(); // answer on the next Collect
            return;
        }
        case PacketKind::Accepted: {
            const auto accepted = Detail::DecodeAccepted(body);
            if (!accepted || accepted->connectionId == 0 || accepted->connectionId != header->connectionId) {
                Count(DropReason::DecodeFailed);
                return;
            }
            accepted_ = *accepted;
            ConnectionSettings transport = settings_.transport;
            transport.connectionId = accepted->connectionId;
            connection_.emplace(std::move(transport));
            state_ = ClientState::Connected;
            lastReceived_ = now;
            lastSent_ = now;
            return;
        }
        case PacketKind::Rejected: {
            const auto rejection = Detail::DecodeRejection(body);
            if (!rejection) {
                Count(DropReason::DecodeFailed);
                return;
            }
            End(rejection->reason, rejection->detail);
            return;
        }
        default: Count(DropReason::Unexpected); return;
        }
    }
    if (!connection_) {
        Count(DropReason::UnknownSource);
        return;
    }
    // A datagram of this connection proves the server is alive, a duplicate included; one of another
    // connection is counted by the Connection (ConnectionMismatch) and proves nothing.
    if (header->connectionId == connection_->Settings().connectionId) {
        lastReceived_ = now;
        if (state_ == ClientState::Interrupted) state_ = ClientState::Connected;
    }
    (void)connection_->Receive(datagram, now);
    if (connection_->RemoteClosed()) End(DisconnectReason::ClosedByRemote, {});
}

void ClientSession::Update(const NetTime now) {
    if (state_ == ClientState::Connecting) {
        if (now - started_ > settings_.handshakeTimeout) End(DisconnectReason::HandshakeTimeout, {});
        return;
    }
    if (state_ != ClientState::Connected && state_ != ClientState::Interrupted) return;
    const NetTime silent = now - lastReceived_;
    if (silent > settings_.timeoutAfter) {
        End(DisconnectReason::Timeout, {});
    } else if (silent > settings_.interruptAfter) {
        state_ = ClientState::Interrupted;
    } else {
        state_ = ClientState::Connected;
    }
}

std::vector<std::vector<std::byte>> ClientSession::Collect(const NetTime now, const Stamp stamp) {
    Update(now);
    std::vector<std::vector<std::byte>> out;
    if (state_ == ClientState::Connecting) {
        if (lastHandshakeSent_ && now - *lastHandshakeSent_ <= settings_.keepaliveInterval) return out;
        std::vector<std::byte> payload = requestBody_;
        PacketKind kind = PacketKind::ConnectRequest;
        if (challenge_) {
            kind = PacketKind::ConnectResponse;
            Detail::ByteWriter writer(payload);
            writer.U32(static_cast<std::uint32_t>(*challenge_));
            writer.U32(static_cast<std::uint32_t>(*challenge_ >> 32));
        }
        auto datagram = Detail::HandshakeDatagram(kind, 0, stamp, payload);
        // A request is never smaller than the challenge it asks for (no amplification).
        if (datagram.size() < Detail::kMinRequestDatagram) datagram.resize(Detail::kMinRequestDatagram);
        out.push_back(std::move(datagram));
        lastHandshakeSent_ = now;
        return out;
    }
    if (!connection_) return out;
    // A connection that timed out or that the server closed sends nothing more.
    if (state_ == ClientState::Disconnected && ended_->reason != DisconnectReason::Closed) return out;
    // Connected, Interrupted, or Disconnected after Close: the Disconnect packets go out once.
    out = connection_->Collect(now, stamp);
    if (out.empty() && state_ != ClientState::Disconnected && now - lastSent_ > settings_.keepaliveInterval) {
        auto keepalive = connection_->KeepAlive(now, stamp);
        if (!keepalive.empty()) out.push_back(std::move(keepalive));
    }
    if (!out.empty()) lastSent_ = now;
    return out;
}

void ClientSession::Close(const NetTime) {
    if (state_ == ClientState::Disconnected) return;
    if (connection_) connection_->Close();
    End(DisconnectReason::Closed, {});
}

} // namespace GYO::Engine::Net
