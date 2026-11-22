#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "engine/net/Link.hpp"

// The in-memory Link (declaration D-2): two ends in one process that deliver to each other, with losses,
// duplicates and reordering a test can reproduce. Every decision comes from a fixed-seed generator of its own
// (splitmix64, not <random>'s distributions), so a seed gives the same datagrams on every platform. It counts
// what it injected, so a test can match every datagram sent against what arrived and what was dropped (AGENTS
// section 14, the count reconciliation).
//
// Time on the link is a step counter, not a clock: a delayed datagram is held back for a number of Advance
// calls. The ends are not thread-safe; both ends of one pair are used from one thread.

namespace GYO::Engine::Net {

// The impairment of one direction. Rates are in parts per 10 000.
struct MemoryLinkImpairment final {
    // A datagram is lost (never delivered).
    std::uint32_t lossPer10000 = 0;
    // A delivered datagram is delivered twice. A lost datagram is never duplicated, so each datagram sent is
    // either lost once or delivered at least once.
    std::uint32_t duplicatePer10000 = 0;
    // A delivered copy is held back for 1..maxDelaySteps Advance calls, so later datagrams overtake it.
    std::uint32_t delayPer10000 = 0;
    std::uint32_t maxDelaySteps = 0;
};

struct MemoryLinkSettings final {
    std::uint64_t seed = 1;
    MemoryLinkImpairment aToB;
    MemoryLinkImpairment bToA;
};

// What one direction did, counted at the sending end.
struct MemoryLinkCounts final {
    // Datagrams the sending end accepted.
    std::uint64_t sent = 0;
    // Of those, lost on purpose.
    std::uint64_t injectedLosses = 0;
    // Extra copies made on purpose.
    std::uint64_t injectedDuplicates = 0;
    // Copies held back on purpose.
    std::uint64_t delayed = 0;
    // Copies the receiving end handed out.
    std::uint64_t delivered = 0;
};

namespace Detail {
struct MemoryLinkState;
}

class MemoryLink final : public Link {
public:
    enum class Injection : std::uint8_t { Loss = 1, Duplicate };
    // Called with every datagram the link loses or duplicates on purpose (for a loss, before it is
    // discarded): a test can read what the lost datagram or the extra copy carried.
    using InjectionObserver = std::function<void(Injection injection, std::span<const std::byte> datagram)>;

    // A connected pair: first is end A (Endpoint{1}), second is end B (Endpoint{2}).
    [[nodiscard]] static std::pair<MemoryLink, MemoryLink> Pair(const MemoryLinkSettings& settings);

    [[nodiscard]] static constexpr Endpoint EndpointA() noexcept { return Endpoint{1}; }
    [[nodiscard]] static constexpr Endpoint EndpointB() noexcept { return Endpoint{2}; }

    // This end's own Endpoint, and the other end's.
    [[nodiscard]] Endpoint Self() const noexcept;
    [[nodiscard]] Endpoint Remote() const noexcept;

    // Sending to an Endpoint other than Remote() is LinkFailed.
    [[nodiscard]] NetResult<void> Send(Endpoint to, std::span<const std::byte> datagram) override;
    // A datagram longer than buffer is dropped and reported as DatagramTooLarge (as UDP does).
    [[nodiscard]] NetResult<std::optional<ReceivedDatagram>> Receive(std::span<std::byte> buffer) override;

    // One step of the pair's time: held-back copies whose delay is over become available. Shared by both ends;
    // call it once per round on either end.
    void Advance() noexcept;

    // What the direction from this end to the other did.
    [[nodiscard]] const MemoryLinkCounts& Outgoing() const noexcept;
    // Copies on the way to this end, available or held back.
    [[nodiscard]] std::size_t InFlightToSelf() const noexcept;

    // Observes the injections on the direction from this end to the other.
    void SetInjectionObserver(InjectionObserver observer);

private:
    MemoryLink(std::shared_ptr<Detail::MemoryLinkState> state, int side) noexcept;
    std::shared_ptr<Detail::MemoryLinkState> state_;
    int side_ = 0;
};

} // namespace GYO::Engine::Net
