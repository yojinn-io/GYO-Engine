#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchIngressLedger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;
using TimePoint = std::chrono::steady_clock::time_point;

struct LedgerFixture final {
    MatchIngressLedger ledger;
    std::vector<IngressSubstitution> closed;
    TimePoint now = TimePoint{} + 1s;
    std::size_t clockReads{};

    explicit LedgerFixture(MatchIngressLedger value = MatchIngressLedger{}) : ledger(value) {}

    void Observe(PlayerId player, std::uint64_t epoch, std::uint64_t life, std::uint64_t cursor) {
        ledger.Observe(player, epoch, life, cursor, [this](const IngressSubstitution& record) { closed.push_back(record); });
    }
    // Resolves first..last of one player: the listed sequences substituted at `now`.
    void Resolve(PlayerId player, std::uint64_t first, std::uint64_t last, std::vector<std::uint64_t> substituted = {}) {
        for (auto sequence = first; sequence <= last; ++sequence) {
            const bool missing = std::find(substituted.begin(), substituted.end(), sequence) != substituted.end();
            ledger.Resolved(player, sequence, missing ? std::optional<TimePoint>{now} : std::nullopt, sequence,
                [this](const IngressSubstitution& record) { closed.push_back(record); });
        }
    }
    IngressCommandClass Arrive(PlayerId player, std::uint64_t sequence, std::uint64_t cursor,
                               std::uint64_t epoch = 1, std::uint64_t life = 1, std::optional<std::int64_t> age = {}) {
        return ledger.Arrive(player, epoch, life, sequence, cursor, [this] { ++clockReads; return now; },
            [age] { return age; });
    }
};
}

TEST_CASE("PvP ingress ledger keeps a substitution through the retention and closes it aged after") {
    LedgerFixture fixture;
    fixture.Observe(1, 1, 1, 0);
    fixture.Resolve(1, 1, 602, {2});
    // cursor 602: sequence 2 is exactly IngressLedgerRetentionSequences behind.
    const bool retainedAtBoundary = fixture.closed.empty();
    CHECK(retainedAtBoundary);
    fixture.now += 5ms;
    const auto lateAtBoundary = fixture.Arrive(1, 2, 602, 1, 1, 1234);
    CHECK(lateAtBoundary == IngressCommandClass::LateFirst);
    CHECK(fixture.Arrive(1, 2, 602) == IngressCommandClass::LateCopy);
    const auto copyPastBoundary = fixture.Arrive(1, 1, 602);
    CHECK(copyPastBoundary == IngressCommandClass::ResolvedUntracked);
    const auto ledgerClockReads = fixture.clockReads;
    CHECK(ledgerClockReads == 1); // only the late_first

    fixture.Resolve(1, 603, 603);
    REQUIRE(fixture.closed.size() == 1);
    const auto& aged = fixture.closed[0];
    const auto agedClose = aged.close;
    CHECK(agedClose == IngressSubstitutionClose::Aged);
    CHECK(aged.playerId == 1);
    CHECK(aged.sequence == 2);
    CHECK(aged.substitutedTick == 2);
    CHECK(aged.copiesAfterFirst == 1);
    CHECK(aged.referenceAgeMicros == std::optional<std::int64_t>{1234});
    const auto agedLateMicros = aged.LateMicros();
    CHECK(agedLateMicros == std::optional<std::int64_t>{5000});
    const auto copyAtBoundary = fixture.Arrive(1, 3, 603);
    CHECK(copyAtBoundary == IngressCommandClass::ResolvedCopy);
    CHECK(fixture.Arrive(1, 2, 603) == IngressCommandClass::ResolvedUntracked);
    CHECK(fixture.clockReads == 1);
}

TEST_CASE("PvP ingress ledger closes the oldest record over capacity and never overflows within the retention") {
    LedgerFixture small{MatchIngressLedger{IngressLedgerRetentionSequences, 2}};
    small.Observe(1, 1, 1, 0);
    small.Resolve(1, 1, 2, {1, 2});
    const auto closedAtCapacity = small.closed.size();
    CHECK(closedAtCapacity == 0);
    small.Resolve(1, 3, 3, {3});
    REQUIRE(small.closed.size() == 1);
    const auto overflowClose = small.closed[0].close;
    CHECK(overflowClose == IngressSubstitutionClose::Overflow);
    CHECK(small.closed[0].sequence == 1);
    // The overflowed sequence was substituted, so its copy is not a copy of an executed command.
    const auto overflowedCopy = small.Arrive(1, 1, 3);
    CHECK(overflowedCopy == IngressCommandClass::ResolvedUntracked);
    CHECK(small.Arrive(1, 2, 3) == IngressCommandClass::LateFirst);

    // Product bounds: a whole retention of substitutions stays open.
    LedgerFixture full;
    full.Observe(1, 1, 1, 0);
    std::vector<std::uint64_t> all;
    for (std::uint64_t sequence = 1; sequence <= IngressLedgerRetentionSequences + 1; ++sequence) all.push_back(sequence);
    full.Resolve(1, 1, IngressLedgerRetentionSequences + 1, all);
    const auto overflowsWithinRetention = full.closed.size();
    CHECK(overflowsWithinRetention == 0);
    CHECK(full.Arrive(1, 1, IngressLedgerRetentionSequences + 1) == IngressCommandClass::LateFirst);
}

TEST_CASE("PvP ingress ledger names why each record closed") {
    LedgerFixture fixture;
    fixture.Observe(1, 1, 1, 0);
    fixture.Observe(2, 1, 1, 0);
    fixture.Resolve(1, 1, 2, {2});
    fixture.Resolve(2, 1, 1, {1});
    fixture.Observe(1, 1, 1, 2); // same identity: nothing closes
    CHECK(fixture.closed.empty());
    fixture.Observe(1, 2, 1, 0); // rotation without a new life
    REQUIRE(fixture.closed.size() == 1);
    const auto epochClose = fixture.closed[0].close;
    CHECK(epochClose == IngressSubstitutionClose::Epoch);
    CHECK(fixture.closed[0].epoch == 1);
    CHECK_FALSE(fixture.closed[0].firstArrival);
    CHECK(fixture.Arrive(1, 2, 0, 1) == IngressCommandClass::ResolvedUntracked); // old identity

    fixture.Resolve(1, 1, 1, {1});
    fixture.Observe(1, 3, 2, 0); // respawn: new life and epoch
    REQUIRE(fixture.closed.size() == 2);
    const auto lifeClose = fixture.closed[1].close;
    CHECK(lifeClose == IngressSubstitutionClose::Life);

    fixture.Resolve(1, 1, 1, {1});
    fixture.ledger.Remove(1, [&](const IngressSubstitution& record) { fixture.closed.push_back(record); });
    REQUIRE(fixture.closed.size() == 3);
    const auto removedClose = fixture.closed[2].close;
    CHECK(removedClose == IngressSubstitutionClose::Removed);
    CHECK(fixture.Arrive(1, 1, 1, 3, 2) == IngressCommandClass::ResolvedUntracked);

    fixture.ledger.Clear(IngressSubstitutionClose::Reset, [&](const IngressSubstitution& record) { fixture.closed.push_back(record); });
    REQUIRE(fixture.closed.size() == 4);
    const auto resetClose = fixture.closed[3].close;
    CHECK(resetClose == IngressSubstitutionClose::Reset);
    CHECK(fixture.closed[3].playerId == 2);

    fixture.Observe(2, 1, 1, 0);
    fixture.Resolve(2, 1, 1, {1});
    fixture.ledger.Clear(IngressSubstitutionClose::End, [&](const IngressSubstitution& record) { fixture.closed.push_back(record); });
    REQUIRE(fixture.closed.size() == 5);
    CHECK(fixture.closed[4].close == IngressSubstitutionClose::End);
    CHECK(fixture.clockReads == 0);
}

TEST_CASE("PvP ingress ledger vouches only for sequences it saw resolved") {
    LedgerFixture fixture;
    fixture.Observe(1, 1, 1, 5); // first seen at cursor 5
    fixture.Resolve(1, 6, 7);
    const auto unwatchedCopy = fixture.Arrive(1, 3, 7);
    CHECK(unwatchedCopy == IngressCommandClass::ResolvedUntracked);
    const auto watchedCopy = fixture.Arrive(1, 6, 7);
    CHECK(watchedCopy == IngressCommandClass::ResolvedCopy);
    CHECK(fixture.Arrive(1, 6, 7, 2) == IngressCommandClass::ResolvedUntracked); // another epoch
    CHECK(fixture.Arrive(1, 6, 7, 1, 2) == IngressCommandClass::ResolvedUntracked); // another life
    CHECK(fixture.Arrive(9, 6, 7) == IngressCommandClass::ResolvedUntracked); // never observed
    fixture.Resolve(1, 10, 10); // 8 and 9 resolved unseen
    const auto gapCopy = fixture.Arrive(1, 8, 10);
    CHECK(gapCopy == IngressCommandClass::ResolvedUntracked);
    CHECK(fixture.Arrive(1, 10, 10) == IngressCommandClass::ResolvedCopy);
    CHECK(fixture.clockReads == 0);
}
