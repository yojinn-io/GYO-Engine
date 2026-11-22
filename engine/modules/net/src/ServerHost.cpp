#include "engine/net/ServerHost.hpp"

#include <string>
#include <utility>

#include "ByteCursor.hpp"

namespace GYO::Engine::Net {

ServerHost::ServerHost(SessionSettings settings, HandshakeFacts facts, const std::uint64_t secret,
                       const std::uint64_t idSeed, Admission admission)
    : settings_(std::move(settings)), facts_(std::move(facts)), secret_(secret), idState_(idSeed),
      admission_(std::move(admission)) {}

std::uint32_t ServerHost::NewConnectionId() {
    while (true) {
        // splitmix64 (the same generator as MemoryLink): the ids need only be unpredictable enough to tell an old
        // connection's datagrams from a new one's, not secret.
        std::uint64_t z = (idState_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        const auto id = static_cast<std::uint32_t>(z ^ (z >> 31));
        if (id != 0 && peers_.count(id) == 0) return id;
    }
}

void ServerHost::Reject(const Endpoint to, Rejection rejection) {
    Reply reply{to, PacketKind::Rejected, 0, {}};
    Detail::EncodeRejection(rejection, reply.payload);
    replies_.push_back(std::move(reply));
}

void ServerHost::Respond(const Endpoint from, const std::span<const std::byte> body, const NetTime now) {
    std::size_t used = 0;
    const auto request = Detail::DecodeRequestBody(body, used);
    if (!request) {
        Count(DropReason::DecodeFailed);
        return;
    }
    Detail::ByteReader reader(body.subspan(used));
    std::uint32_t low = 0, high = 0;
    if (!reader.U32(low) || !reader.U32(high)) {
        Count(DropReason::DecodeFailed);
        return;
    }
    const std::uint64_t challenge = (static_cast<std::uint64_t>(high) << 32) | low;
    if (challenge != Detail::ChallengeOf(secret_, from, body.first(used))) {
        Reject(from, Rejection{DisconnectReason::ChallengeFailed, "the response does not carry this server's challenge"});
        return;
    }
    // A second response of the same handshake (its first Accepted was lost): the same answer again. A newer
    // handshake from that endpoint (a larger salt: the client reconnected) ends the old connection first; a late copy
    // of an older one (a smaller salt) is dropped and counted, and the connection stays (S2).
    if (const auto known = byEndpoint_.find(from.value); known != byEndpoint_.end()) {
        auto& peer = peers_.at(known->second);
        if (peer.salt == request->salt && peer.userId == request->userId && !peer.closing) {
            Reply reply{from, PacketKind::Accepted, known->second, {}};
            replies_.push_back(std::move(reply));
            return;
        }
        if (request->salt < peer.salt) {
            Count(DropReason::Unexpected);
            return;
        }
        End(peers_.find(known->second), DisconnectReason::ClosedByRemote);
    }
    if (auto mismatch = CompareFacts(request->facts, facts_)) {
        Reject(from, std::move(*mismatch));
        return;
    }
    if (admission_) {
        if (auto admitted = admission_(*request, from); !admitted) {
            Reject(from, Rejection{DisconnectReason::Refused, admitted.error().message});
            return;
        }
    }
    const std::uint32_t id = NewConnectionId();
    ConnectionSettings transport = settings_.transport;
    transport.connectionId = id;
    peers_.emplace(id, Peer{from, request->userId, request->salt, Connection(std::move(transport)), now, now, false, 0});
    byEndpoint_[from.value] = id;
    events_.push_back(HostEvent{HostEventKind::Connected, id, request->userId, from, DisconnectReason::Closed});
    replies_.push_back(Reply{from, PacketKind::Accepted, id, {}});
}

void ServerHost::Receive(const Endpoint from, const std::span<const std::byte> datagram, const NetTime now) {
    const auto header = DecodePacketHeader(datagram);
    if (!header) {
        Count(DropReason::DecodeFailed);
        return;
    }
    const auto body = datagram.subspan(kPacketHeaderSize);
    switch (header->kind) {
    case PacketKind::ConnectRequest: {
        if (datagram.size() < Detail::kMinRequestDatagram) {
            Count(DropReason::Unexpected);
            return;
        }
        std::size_t used = 0;
        if (!Detail::DecodeRequestBody(body, used)) {
            Count(DropReason::DecodeFailed);
            return;
        }
        // Stateless: the answer depends only on the request's bytes and where it came from.
        Reply reply{from, PacketKind::Challenge, 0, {}};
        Detail::ByteWriter writer(reply.payload);
        const std::uint64_t challenge = Detail::ChallengeOf(secret_, from, body.first(used));
        writer.U32(static_cast<std::uint32_t>(challenge));
        writer.U32(static_cast<std::uint32_t>(challenge >> 32));
        replies_.push_back(std::move(reply));
        return;
    }
    case PacketKind::ConnectResponse: Respond(from, body, now); return;
    case PacketKind::Challenge:
    case PacketKind::Accepted:
    case PacketKind::Rejected: Count(DropReason::Unexpected); return;
    default: break;
    }
    const auto known = byEndpoint_.find(from.value);
    if (known == byEndpoint_.end()) {
        Count(DropReason::UnknownSource);
        return;
    }
    const auto peer = peers_.find(known->second);
    // A datagram of this connection proves the client is alive; another connection's is the Connection's to count.
    if (header->connectionId == peer->first) peer->second.lastReceived = now;
    (void)peer->second.connection.Receive(datagram, now);
    if (peer->second.connection.RemoteClosed()) End(peer, DisconnectReason::ClosedByRemote);
}

void ServerHost::End(const std::map<std::uint32_t, Peer>::iterator peer, const DisconnectReason reason) {
    events_.push_back(HostEvent{HostEventKind::Disconnected, peer->first, peer->second.userId, peer->second.endpoint, reason});
    byEndpoint_.erase(peer->second.endpoint.value);
    peers_.erase(peer);
}

void ServerHost::Update(const NetTime now) {
    for (auto it = peers_.begin(); it != peers_.end();) {
        const auto next = std::next(it);
        if (!it->second.closing && now - it->second.lastReceived > settings_.timeoutAfter) End(it, DisconnectReason::Timeout);
        it = next;
    }
}

std::vector<std::pair<Endpoint, std::vector<std::byte>>> ServerHost::Collect(const NetTime now, const Stamp stamp) {
    Update(now);
    std::vector<std::pair<Endpoint, std::vector<std::byte>>> out;
    while (!replies_.empty()) {
        Reply reply = std::move(replies_.front());
        replies_.pop_front();
        if (reply.kind == PacketKind::Accepted) {
            const auto peer = peers_.find(reply.connectionId);
            if (peer == peers_.end()) continue; // ended before its answer went out
            Detail::EncodeAccepted(ConnectAccepted{reply.connectionId, peer->second.salt, stamp.tick, facts_.stepNanoseconds,
                                                   stamp.worldGeneration, facts_.idRange},
                                   reply.payload);
        }
        out.emplace_back(reply.to, Detail::HandshakeDatagram(reply.kind, reply.connectionId, stamp, reply.payload));
    }
    for (auto it = peers_.begin(); it != peers_.end();) {
        const auto next = std::next(it);
        Peer& peer = it->second;
        auto datagrams = peer.connection.Collect(now, stamp);
        if (datagrams.empty() && !peer.closing && now - peer.lastSent > settings_.keepaliveInterval) {
            auto keepalive = peer.connection.KeepAlive(now, stamp);
            if (!keepalive.empty()) datagrams.push_back(std::move(keepalive));
        }
        if (!datagrams.empty()) peer.lastSent = now;
        for (auto& datagram : datagrams) {
            peer.bytesSent += datagram.size();
            out.emplace_back(peer.endpoint, std::move(datagram));
        }
        if (peer.closing) End(it, DisconnectReason::Closed);
        it = next;
    }
    return out;
}

void ServerHost::Close(const std::uint32_t connectionId) {
    const auto peer = peers_.find(connectionId);
    if (peer == peers_.end() || peer->second.closing) return;
    peer->second.closing = true;
    peer->second.connection.Close();
}

std::vector<HostEvent> ServerHost::TakeEvents() {
    std::vector<HostEvent> taken;
    taken.swap(events_);
    return taken;
}

std::vector<std::uint32_t> ServerHost::ConnectionIds() const {
    std::vector<std::uint32_t> ids;
    for (const auto& [id, peer] : peers_) ids.push_back(id);
    return ids;
}

std::uint64_t ServerHost::UserIdOf(const std::uint32_t connectionId) const {
    const auto peer = peers_.find(connectionId);
    return peer == peers_.end() ? 0 : peer->second.userId;
}

std::uint64_t ServerHost::BytesSentTo(const std::uint32_t connectionId) const {
    const auto peer = peers_.find(connectionId);
    return peer == peers_.end() ? 0 : peer->second.bytesSent;
}

Connection* ServerHost::Find(const std::uint32_t connectionId) noexcept {
    const auto peer = peers_.find(connectionId);
    return peer == peers_.end() ? nullptr : &peer->second.connection;
}

} // namespace GYO::Engine::Net
