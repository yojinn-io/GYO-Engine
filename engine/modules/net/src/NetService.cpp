#include "engine/net/NetService.hpp"

#include <algorithm>
#include <utility>

#include "engine/net/UdpLink.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace GYO::Engine::Net {
namespace {

// The receive buffer: larger than any datagram this protocol sends (the mtu), so a longer one is not ours.
constexpr std::size_t kReceiveBuffer = 2048;
// Datagrams taken per Pump at most, so a flood cannot hold a frame forever; the rest wait for the next frame.
constexpr std::size_t kMaxDatagramsPerPump = 4096;

} // namespace

NetService::NetService(NetProductOptions options, std::shared_ptr<Link> link, const std::uint64_t secret,
                       const std::uint64_t idSeed)
    : options_(std::move(options)), link_(std::move(link)), secret_(secret), idSeed_(idSeed) {}

NetTime NetService::Now() const {
    if (options_.clock) return options_.clock();
    return NetTime{Time::NowNs()};
}

void NetService::SetFacts(const HandshakeFacts& facts, const NetTime now, const bool newWorld) {
    if (options_.role == NetRole::Server) {
        if (server_) {
            server_->SetFacts(facts);
        } else {
            server_.emplace(options_.session, facts, secret_, idSeed_, options_.admission);
        }
        return;
    }
    static_cast<void>(newWorld);  // a client's new World is SetTarget's (its first frame)
    if (!target_) return;  // a client World that connects to nothing (D-8)
    // A session that ended stays until the next client World's SetTarget (D-8: no reconnect within a World).
    if (!client_) {
        // A salt per handshake, larger than the one before (D-8, S2): the IO layer's monotonic time in nanoseconds,
        // or one more than the last when the clock has not moved. Limit: the monotonic clock is comparable only within
        // one process, so a client started again may draw a smaller salt than its previous process; its handshake is
        // then taken as an old one and dropped until the old connection times out (timeoutAfter, 10 s, shorter than
        // the handshake timeout of 15 s).
        lastSalt_ = std::max(lastSalt_ + 1, static_cast<std::uint64_t>(std::max<NetTime::rep>(now.count(), 0)));
        // The epoch of the client World that asked for it goes with the connection (R2): given with the target, so a
        // World replaced again before its first frame does not shift it.
        ConnectRequest request{facts, target_->userId, target_->token, lastSalt_, targetEpoch_};
        client_.emplace(options_.session, std::move(request), now);
    }
}

void NetService::EndClient(const NetTime now) {
    if (!client_) return;
    client_->Close(now);
    if (target_) {
        for (auto& datagram : client_->Collect(now, {})) {
            if (auto sent = link_->Send(target_->endpoint, datagram); !sent) Failed(std::move(sent).error());
        }
    }
    client_.reset();
}

void NetService::SetTarget(const NetConnectTarget* target, const NetTime now, const std::uint32_t epoch) {
    targetEpoch_ = epoch;
    std::optional<Target> wanted;
    if (target != nullptr) {
        // The server of this transport's kind: an address over the UDP socket the Module opened, an Endpoint over a
        // Link the product gave.
        if (const auto* address = std::get_if<OS::DatagramAddress>(&target->server); address != nullptr && !options_.link) {
            wanted = Target{UdpLink::ToEndpoint(*address), target->userId, target->token};
        } else if (const auto* endpoint = std::get_if<Endpoint>(&target->server); endpoint != nullptr && options_.link) {
            wanted = Target{*endpoint, target->userId, target->token};
        } else if (!std::holds_alternative<std::monostate>(target->server)) {
            Failed(NetFailure::Make(NetFailureCode::LinkFailed,
                                    options_.link ? "a NetConnectTarget over a given Link names an address, not an Endpoint"
                                                  : "a NetConnectTarget over UDP names an Endpoint, not an address"));
        }
    }
    // A client's UDP socket opens with its first target (a World without one never opens it; net-connect-target 4.5).
    if (wanted && !link_) {
        auto opened = UdpLink::Open(options_.bind);
        if (!opened) {
            Failed(std::move(opened).error());
            wanted.reset();
        } else {
            link_ = std::make_shared<UdpLink>(std::move(*opened));
        }
    }
    const bool live = client_ && client_->State() != ClientState::Disconnected;
    if (wanted && target_ && *wanted == *target_ && live) return;  // the same connection goes on into the new World
    // Not kept: its last datagrams go to the old target now (a live one's Disconnect; one closed just before the
    // replacement still has its Disconnect packets to send), and the next SetFacts opens a new one.
    EndClient(now);
    target_ = std::move(wanted);
}

void NetService::Failed(NetFailure failure) {
    ++linkFailures_;
    lastLinkFailure_ = std::move(failure);
}

void NetService::Pump(const NetTime now) {
    std::vector<std::byte> buffer(kReceiveBuffer);
    // A client's UDP socket opens with its first target (SetTarget): until then nothing can arrive.
    for (std::size_t i = 0; link_ && i < kMaxDatagramsPerPump; ++i) {
        auto received = link_->Receive(buffer);
        if (!received) {
            // A datagram too large was dropped and the next may be fine; any other failure lasts, and this frame's
            // receiving stops (small thing 3: a broken socket is not tried kMaxDatagramsPerPump times a frame).
            const bool tooLarge = received.error().code == NetFailureCode::DatagramTooLarge;
            Failed(std::move(received).error());
            if (tooLarge) continue;
            break;
        }
        if (!received->has_value()) break;
        const auto datagram = std::span<const std::byte>(buffer).first((*received)->size);
        if (server_) {
            server_->Receive((*received)->from, datagram, now);
        } else if (client_) {
            if (!target_ || (*received)->from != target_->endpoint) {
                ++drops_[static_cast<std::size_t>(DropReason::UnknownSource)];
                continue;
            }
            client_->Receive(datagram, now);
        } else {
            ++drops_[static_cast<std::size_t>(DropReason::UnknownSource)];
        }
    }
    if (server_) server_->Update(now);
    if (client_) client_->Update(now);
}

void NetService::Flush(const NetTime now, Stamp stamp) {
    if (!link_) return;  // a client that has had no target: no socket, nothing to send
    stamp.worldGeneration = generation_;
    if (server_) {
        for (auto& [to, datagram] : server_->Collect(now, stamp)) {
            if (auto sent = link_->Send(to, datagram); !sent) Failed(std::move(sent).error());
        }
    }
    if (client_ && target_) {
        for (auto& datagram : client_->Collect(now, stamp)) {
            if (auto sent = link_->Send(target_->endpoint, datagram); !sent) Failed(std::move(sent).error());
        }
    }
}

void NetService::Close() {
    if (client_) client_->Close(Now());
    // A server closes every open connection (their Disconnect packets go out on the next Flush).
    if (server_) {
        for (const std::uint32_t id : server_->ConnectionIds()) server_->Close(id);
    }
}

std::optional<OS::DatagramAddress> NetService::UdpAddress() const {
    if (options_.link || !link_) return std::nullopt;  // a given Link, or a client's socket not opened yet
    const auto* udp = static_cast<const UdpLink*>(link_.get());
    auto address = udp->LocalAddress();
    if (!address) return std::nullopt;
    return *address;
}

bool NetService::Bind(const std::string& world) {
    if (boundWorld_ && *boundWorld_ != world) return false;
    boundWorld_ = world;
    return true;
}

} // namespace GYO::Engine::Net
