// Judgement G-2 of the net Module declaration (docs/architecture/target/v2/net-module-declaration.zh-Hant.md,
// section 8), as declared before the results and not loosened after them (AGENTS section 14.1):
//
//   A test Link loses 20 %, duplicates 10 % and reorders (fixed seed, 10 000 messages):
//   - Ordered Reliable hands out every message exactly once, in order;
//   - Unreliable hands out no duplicate;
//   - a fragmented message is handed out only when every piece arrived.
//   Count reconciliation: sent = handed out + the drop count of every reason + the losses the test Link
//   injected, item by item.
//
// The reconciliation is done in two units, both exact:
//   datagrams, per direction:  sent + injected duplicates = accepted + dropped (each packet-level reason)
//                              + injected losses;
//   message pieces, per direction and channel (an unfragmented message is one piece, a fragmented one has a
//   piece per fragment; resends count as sent):
//                              pieces sent + pieces in injected duplicates = pieces handed out + pieces
//                              dropped (each reason) + pieces in injected losses.
// The link never duplicates a datagram it lost, so every datagram sent has exactly one fate. Time is driven by
// the test: every round is 16 ms and one link step.
#include <doctest/doctest.h>

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "NetTestSupport.hpp"
#include "engine/net/Connection.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

constexpr std::uint32_t kMessages = 10000;
constexpr std::uint32_t kReturnMessages = 2000;
constexpr std::uint32_t kPerRound = 10;
constexpr std::int64_t kRoundMs = 16;
constexpr int kMaxRounds = 6000;

// Every 50th message is larger than one packet: 3000 bytes is three pieces at the default mtu.
std::size_t SizeOf(const std::uint32_t index) { return index % 50 == 49 ? 3000 : 16 + index % 40; }

struct Injected final {
    std::vector<std::uint64_t> lostPieces = std::vector<std::uint64_t>(2, 0);
    std::vector<std::uint64_t> duplicatePieces = std::vector<std::uint64_t>(2, 0);
    std::uint64_t unreadable = 0;
};

void Observe(N::MemoryLink& link, Injected& injected) {
    link.SetInjectionObserver([&injected](const N::MemoryLink::Injection kind, const std::span<const std::byte> datagram) {
        const auto summary = N::Connection::Inspect(datagram);
        if (!summary) {
            ++injected.unreadable;
            return;
        }
        auto& pieces = kind == N::MemoryLink::Injection::Loss ? injected.lostPieces : injected.duplicatePieces;
        for (std::size_t c = 0; c < summary->piecesPerChannel.size() && c < pieces.size(); ++c) {
            pieces[c] += summary->piecesPerChannel[c];
        }
    });
}

void SendAll(N::Connection& from, N::MemoryLink& link, const N::NetTime now) {
    for (const auto& datagram : from.Collect(now, {})) REQUIRE(link.Send(link.Remote(), datagram).has_value());
}

void ReceiveAll(N::Connection& to, N::MemoryLink& link, const N::NetTime now) {
    std::vector<std::byte> buffer(2048);
    while (true) {
        auto received = link.Receive(buffer);
        REQUIRE(received.has_value());
        if (!received->has_value()) return;
        REQUIRE(to.Receive(std::span<const std::byte>(buffer).first((*received)->size), now).has_value());
    }
}

std::uint64_t Sum(const std::array<std::uint64_t, N::kDropReasonCount>& counts) {
    std::uint64_t total = 0;
    for (const auto count : counts) total += count;
    return total;
}

std::uint64_t PacketLevelDrops(const N::ConnectionStats& stats) {
    std::uint64_t total = 0;
    for (const auto reason : {N::DropReason::DecodeFailed, N::DropReason::ConnectionMismatch,
                              N::DropReason::DuplicatePacket, N::DropReason::TooOld,
                              N::DropReason::ReliableWindowFull}) {
        total += stats.drops[static_cast<std::size_t>(reason)];
    }
    return total;
}

std::string Reasons(const std::array<std::uint64_t, N::kDropReasonCount>& counts) {
    std::string text;
    for (std::size_t r = 0; r < N::kDropReasonCount; ++r) {
        text += std::string(r == 0 ? "" : " ") + N::ToString(static_cast<N::DropReason>(r)) + "=" +
                std::to_string(counts[r]);
    }
    return text;
}

// The reconciliation of one direction, printed whatever the result.
void Reconcile(const std::string& direction, const N::Connection& from, const N::Connection& to, const N::MemoryLink& fromLink,
               const Injected& injected) {
    const auto& link = fromLink.Outgoing();
    const auto& sent = from.Stats();
    const auto& got = to.Stats();
    MESSAGE(direction << " datagrams: sent=" << sent.datagramsSent << " link_sent=" << link.sent
                      << " injected_duplicates=" << link.injectedDuplicates << " injected_losses="
                      << link.injectedLosses << " delayed=" << link.delayed << " accepted=" << got.datagramsAccepted
                      << " | drops (packet reasons in datagrams, the others in pieces): " << Reasons(got.drops));
    CHECK(sent.datagramsSent == link.sent);
    CHECK(injected.unreadable == 0);
    const std::uint64_t datagramsIn = link.sent + link.injectedDuplicates;
    const std::uint64_t datagramsOut = got.datagramsAccepted + PacketLevelDrops(got) + link.injectedLosses;
    CHECK_MESSAGE(datagramsIn == datagramsOut,
                  "datagrams: sent + injected duplicates == accepted + dropped + injected losses");
    for (std::size_t c = 0; c < 2; ++c) {
        const auto& out = sent.channels[c];
        const auto& in = got.channels[c];
        MESSAGE(direction << " channel " << c << std::string(c == kReliable ? " (OrderedReliable)" : " (Unreliable)")
                          << ": messages_sent=" << out.messagesSent << " messages_handed_out=" << in.messagesDelivered
                          << " pieces_sent=" << out.piecesSent << " pieces_in_duplicates="
                          << injected.duplicatePieces[c] << " pieces_handed_out=" << in.piecesDelivered
                          << " pieces_in_losses=" << injected.lostPieces[c] << " | pieces dropped: "
                          << Reasons(in.piecesDropped));
        const std::uint64_t piecesIn = out.piecesSent + injected.duplicatePieces[c];
        const std::uint64_t piecesOut = in.piecesDelivered + Sum(in.piecesDropped) + injected.lostPieces[c];
        if (c == kReliable) {
            CHECK_MESSAGE(piecesIn == piecesOut, "reliable pieces: sent + in duplicates == handed out + dropped + in losses");
            CHECK(out.messagesSent == in.messagesDelivered);
        } else {
            CHECK_MESSAGE(piecesIn == piecesOut,
                          "unreliable pieces: sent + in duplicates == handed out + dropped + in losses");
        }
    }
}

} // namespace

TEST_CASE("transport: G-2 losses, duplicates and reordering with a fixed seed") {
    N::MemoryLinkSettings linkSettings;
    linkSettings.seed = 0x5EED0002u;
    linkSettings.aToB = {2000, 1000, 2500, 3};
    linkSettings.bToA = {2000, 1000, 2500, 3};
    auto [linkA, linkB] = N::MemoryLink::Pair(linkSettings);
    Injected aToB, bToA;
    Observe(linkA, aToB);
    Observe(linkB, bToA);

    N::Connection a(TwoChannels());
    N::Connection b(TwoChannels());

    std::uint32_t nextReliable = 0, nextUnreliable = 0, nextReturn = 0;
    std::vector<std::uint32_t> reliableOrder, returnOrder;
    std::set<std::uint32_t> unreliableSeen;
    std::uint64_t unreliableDuplicates = 0, wrongContent = 0, fragmentedHandedOut = 0;

    auto take = [&](N::Connection& at, const std::size_t channel, auto&& onMessage) {
        while (auto message = at.Pop(channel)) {
            const std::uint32_t index = IndexOf(*message);
            if (*message != NumberedMessage(index, SizeOf(index))) ++wrongContent;
            onMessage(index, *message);
        }
    };

    int round = 0;
    for (; round < kMaxRounds; ++round) {
        const N::NetTime now = Ms(round * kRoundMs);
        for (std::uint32_t i = 0; i < kPerRound && nextReliable < kMessages; ++i, ++nextReliable) {
            REQUIRE(a.Send(kReliable, NumberedMessage(nextReliable, SizeOf(nextReliable))).has_value());
        }
        for (std::uint32_t i = 0; i < kPerRound && nextUnreliable < kMessages; ++i, ++nextUnreliable) {
            REQUIRE(a.Send(kUnreliable, NumberedMessage(nextUnreliable, SizeOf(nextUnreliable))).has_value());
        }
        for (std::uint32_t i = 0; i < 2 && nextReturn < kReturnMessages; ++i, ++nextReturn) {
            REQUIRE(b.Send(kReliable, NumberedMessage(nextReturn, SizeOf(nextReturn))).has_value());
        }
        SendAll(a, linkA, now);
        SendAll(b, linkB, now);
        linkA.Advance();
        ReceiveAll(b, linkB, now);
        ReceiveAll(a, linkA, now);
        take(b, kReliable, [&](std::uint32_t index, const auto&) { reliableOrder.push_back(index); });
        take(a, kReliable, [&](std::uint32_t index, const auto&) { returnOrder.push_back(index); });
        take(b, kUnreliable, [&](std::uint32_t index, const auto& message) {
            if (!unreliableSeen.insert(index).second) ++unreliableDuplicates;
            if (message.size() > N::kDefaultMtu) ++fragmentedHandedOut;
        });
        const bool allSent = nextReliable == kMessages && nextUnreliable == kMessages && nextReturn == kReturnMessages;
        if (allSent && a.PendingPieces() == 0 && b.PendingPieces() == 0 && linkA.InFlightToSelf() == 0 &&
            linkB.InFlightToSelf() == 0) {
            break;
        }
    }
    MESSAGE("rounds=" << round << " reliable_handed_out=" << reliableOrder.size() << " return_handed_out="
                      << returnOrder.size() << " unreliable_handed_out=" << unreliableSeen.size()
                      << " unreliable_fragmented_handed_out=" << fragmentedHandedOut << " rtt_a_ns="
                      << a.SmoothedRtt().count());
    REQUIRE_MESSAGE(round < kMaxRounds, "the transport drained within the round limit");

    // Ordered Reliable: every message exactly once, in order (both directions).
    bool reliableInOrder = reliableOrder.size() == kMessages;
    for (std::size_t i = 0; reliableInOrder && i < reliableOrder.size(); ++i) reliableInOrder = reliableOrder[i] == i;
    CHECK_MESSAGE(reliableInOrder, "Ordered Reliable hands out every message exactly once, in order");
    bool returnInOrder = returnOrder.size() == kReturnMessages;
    for (std::size_t i = 0; returnInOrder && i < returnOrder.size(); ++i) returnInOrder = returnOrder[i] == i;
    CHECK_MESSAGE(returnInOrder, "Ordered Reliable hands out every message exactly once, in order (b to a)");

    // Unreliable: no duplicate; fragmented messages only whole (every message handed out matches what was sent).
    CHECK_MESSAGE(unreliableDuplicates == 0, "Unreliable hands out no duplicate");
    CHECK_MESSAGE(wrongContent == 0, "every message handed out is whole and unchanged");
    CHECK(fragmentedHandedOut > 0);

    // The impairments really happened.
    CHECK(linkA.Outgoing().injectedLosses > 0);
    CHECK(linkA.Outgoing().injectedDuplicates > 0);
    CHECK(linkA.Outgoing().delayed > 0);
    CHECK(b.Stats().drops[static_cast<std::size_t>(N::DropReason::DuplicatePacket)] > 0);
    CHECK(b.Stats().drops[static_cast<std::size_t>(N::DropReason::DuplicateMessage)] > 0);
    CHECK(b.Stats().drops[static_cast<std::size_t>(N::DropReason::FragmentTimeout)] > 0);

    Reconcile("a->b", a, b, linkA, aToB);
    Reconcile("b->a", b, a, linkB, bToA);
}

TEST_CASE("transport: the memory link repeats itself for one seed") {
    auto run = [] {
        N::MemoryLinkSettings settings;
        settings.seed = 42;
        settings.aToB = {2000, 1000, 2500, 3};
        auto [a, b] = N::MemoryLink::Pair(settings);
        std::vector<std::uint32_t> order;
        std::vector<std::byte> buffer(16);
        for (std::uint32_t i = 0; i < 200; ++i) {
            const auto message = NumberedMessage(i, 4);
            REQUIRE(a.Send(b.Self(), message).has_value());
            a.Advance();
            while (auto got = b.Receive(buffer)) {
                if (!got->has_value()) break;
                order.push_back(IndexOf(std::vector<std::byte>(buffer.begin(), buffer.begin() + 4)));
            }
        }
        return std::make_pair(order, a.Outgoing().injectedLosses);
    };
    const auto first = run();
    const auto second = run();
    CHECK(first == second);
    CHECK(first.second > 0);
}
