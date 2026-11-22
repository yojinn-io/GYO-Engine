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

NetService::NetService(NetProductOptions options, std::shared_ptr<Link> link, const Endpoint serverEndpoint,
                       const std::uint64_t secret, const std::uint64_t idSeed)
    : options_(std::move(options)), link_(std::move(link)), serverEndpoint_(serverEndpoint), secret_(secret),
      idSeed_(idSeed) {}

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
    if (!client_ || (newWorld && client_->State() == ClientState::Disconnected)) {
        // The ended session's last datagrams (its Disconnect packets after a Close) go out before the new one starts.
        if (client_) {
            for (auto& datagram : client_->Collect(now, {})) {
                if (auto sent = link_->Send(serverEndpoint_, datagram); !sent) Failed(std::move(sent).error());
            }
        }
        // A salt per handshake, larger than the one before (D-8, S2): the IO layer's monotonic time in nanoseconds,
        // or one more than the last when the clock has not moved; so a client that starts again (a new process on the
        // same machine) keeps going up as well.
        lastSalt_ = std::max(lastSalt_ + 1, static_cast<std::uint64_t>(std::max<NetTime::rep>(now.count(), 0)));
        ConnectRequest request{facts, options_.userId, options_.token, lastSalt_};
        client_.emplace(options_.session, std::move(request), now);
    }
}

void NetService::Failed(NetFailure failure) {
    ++linkFailures_;
    lastLinkFailure_ = std::move(failure);
}

void NetService::Pump(const NetTime now) {
    std::vector<std::byte> buffer(kReceiveBuffer);
    for (std::size_t i = 0; i < kMaxDatagramsPerPump; ++i) {
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
            if ((*received)->from != serverEndpoint_) {
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
    stamp.worldGeneration = generation_;
    if (server_) {
        for (auto& [to, datagram] : server_->Collect(now, stamp)) {
            if (auto sent = link_->Send(to, datagram); !sent) Failed(std::move(sent).error());
        }
    }
    if (client_) {
        for (auto& datagram : client_->Collect(now, stamp)) {
            if (auto sent = link_->Send(serverEndpoint_, datagram); !sent) Failed(std::move(sent).error());
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
    if (options_.link) return std::nullopt;
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
