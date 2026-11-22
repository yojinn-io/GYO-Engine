#include "engine/net/MemoryLink.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace GYO::Engine::Net {
namespace Detail {
namespace {

// splitmix64 (Steele, Lea, Flood 2014; the public-domain reference by Sebastiano Vigna): a fixed function of
// its state, so the same seed gives the same sequence on every platform.
struct SplitMix64 final {
    std::uint64_t state = 0;
    std::uint64_t Next() noexcept {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // True with probability per10000 / 10 000.
    bool Chance(const std::uint32_t per10000) noexcept { return Next() % 10000u < per10000; }
};

struct Copy final {
    std::uint64_t availableAt = 0;
    std::uint64_t order = 0;
    std::vector<std::byte> bytes;
};

} // namespace

struct Direction final {
    MemoryLinkImpairment impairment;
    SplitMix64 random;
    MemoryLinkCounts counts;
    MemoryLink::InjectionObserver observer;
    // Ordered by (availableAt, order): a copy is handed out after every copy that became available before it.
    std::deque<Copy> queue;
};

struct MemoryLinkState final {
    std::uint64_t step = 0;
    std::uint64_t order = 0;
    // Index 0: from A to B; index 1: from B to A.
    std::array<Direction, 2> directions;

    void Enqueue(Direction& direction, std::uint64_t availableAt, std::span<const std::byte> datagram) {
        Copy copy{availableAt, order++, std::vector<std::byte>(datagram.begin(), datagram.end())};
        const auto at = std::upper_bound(direction.queue.begin(), direction.queue.end(), copy,
                                         [](const Copy& a, const Copy& b) {
                                             return a.availableAt != b.availableAt ? a.availableAt < b.availableAt
                                                                                   : a.order < b.order;
                                         });
        direction.queue.insert(at, std::move(copy));
    }

    std::uint64_t DelayOf(Direction& direction) {
        const auto& impairment = direction.impairment;
        if (impairment.maxDelaySteps == 0 || !direction.random.Chance(impairment.delayPer10000)) return 0;
        ++direction.counts.delayed;
        return 1 + direction.random.Next() % impairment.maxDelaySteps;
    }
};

} // namespace Detail

std::pair<MemoryLink, MemoryLink> MemoryLink::Pair(const MemoryLinkSettings& settings) {
    auto state = std::make_shared<Detail::MemoryLinkState>();
    state->directions[0].impairment = settings.aToB;
    state->directions[1].impairment = settings.bToA;
    // Two independent streams from one seed: what one direction draws does not shift the other.
    state->directions[0].random.state = settings.seed;
    state->directions[1].random.state = settings.seed ^ 0xD1B54A32D192ED03ull;
    return {MemoryLink(state, 0), MemoryLink(state, 1)};
}

MemoryLink::MemoryLink(std::shared_ptr<Detail::MemoryLinkState> state, const int side) noexcept
    : state_(std::move(state)), side_(side) {}

Endpoint MemoryLink::Self() const noexcept { return side_ == 0 ? EndpointA() : EndpointB(); }
Endpoint MemoryLink::Remote() const noexcept { return side_ == 0 ? EndpointB() : EndpointA(); }

NetResult<void> MemoryLink::Send(const Endpoint to, const std::span<const std::byte> datagram) {
    if (to != Remote()) {
        return Base::Err(NetFailure::Make(NetFailureCode::LinkFailed, "a memory link reaches only its other end",
                                          "endpoint=" + std::to_string(to.value)));
    }
    auto& direction = state_->directions[static_cast<std::size_t>(side_)];
    ++direction.counts.sent;
    if (direction.random.Chance(direction.impairment.lossPer10000)) {
        ++direction.counts.injectedLosses;
        if (direction.observer) direction.observer(Injection::Loss, datagram);
        return {};
    }
    state_->Enqueue(direction, state_->step + state_->DelayOf(direction), datagram);
    if (direction.random.Chance(direction.impairment.duplicatePer10000)) {
        ++direction.counts.injectedDuplicates;
        if (direction.observer) direction.observer(Injection::Duplicate, datagram);
        state_->Enqueue(direction, state_->step + state_->DelayOf(direction), datagram);
    }
    return {};
}

NetResult<std::optional<ReceivedDatagram>> MemoryLink::Receive(const std::span<std::byte> buffer) {
    auto& direction = state_->directions[static_cast<std::size_t>(1 - side_)];
    if (direction.queue.empty() || direction.queue.front().availableAt > state_->step) {
        return std::optional<ReceivedDatagram>{};
    }
    Detail::Copy copy = std::move(direction.queue.front());
    direction.queue.pop_front();
    ++direction.counts.delivered;
    if (copy.bytes.size() > buffer.size()) {
        return Base::Err(NetFailure::Make(NetFailureCode::DatagramTooLarge,
                                          "a datagram longer than the receive buffer was dropped",
                                          "bytes=" + std::to_string(copy.bytes.size())));
    }
    if (!copy.bytes.empty()) std::memcpy(buffer.data(), copy.bytes.data(), copy.bytes.size());
    return std::optional<ReceivedDatagram>{ReceivedDatagram{copy.bytes.size(), Remote()}};
}

void MemoryLink::Advance() noexcept { ++state_->step; }

const MemoryLinkCounts& MemoryLink::Outgoing() const noexcept {
    return state_->directions[static_cast<std::size_t>(side_)].counts;
}

std::size_t MemoryLink::InFlightToSelf() const noexcept {
    return state_->directions[static_cast<std::size_t>(1 - side_)].queue.size();
}

void MemoryLink::SetInjectionObserver(InjectionObserver observer) {
    state_->directions[static_cast<std::size_t>(side_)].observer = std::move(observer);
}

} // namespace GYO::Engine::Net
