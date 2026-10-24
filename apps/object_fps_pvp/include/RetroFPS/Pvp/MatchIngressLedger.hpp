#pragma once

// Product-owned diagnostics (v7 batch 08c; since v7.1 outage-resync a reader of
// the substitution records): classifies every command that arrives at or
// below the cursor and turns each substitution record that closes into a
// match-ingress.jsonl record. The records themselves (substituted, first
// arrival, retention, close reasons) belong to MatchSubstitutionRecords.hpp,
// the slack sample path's record; this ledger never changes them and nothing
// the Match or the samples decide reads this ledger (network-architecture §7
// "被替代序號的單一紀錄"). It keeps only what no decision needs: the copies
// after the first, the reference age of the first copy, which sequences it
// saw resolved, and the sequences closed as overflow.
#include "RetroFPS/Pvp/IngressStatistics.hpp"
#include "RetroFPS/Pvp/MatchSubstitutionRecords.hpp"
#include "RetroFPS/Pvp/Movement.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace fps::pvp {

// The substitution fields of match-ingress.jsonl. Times are steady_clock,
// the scale of the movement trace.
struct IngressSubstitution final {
    PlayerId playerId{};
    std::uint64_t epoch{};
    std::uint64_t life{};
    std::uint64_t sequence{};
    std::chrono::steady_clock::time_point substitutedAt;
    std::uint64_t substitutedTick{};
    // The first copy that arrived after the substitution, if any.
    std::optional<std::chrono::steady_clock::time_point> firstArrival;
    std::uint64_t copiesAfterFirst{};
    // Reference age of the input that brought the first copy, when that
    // input named a snapshot the host still knew (or one older than all).
    std::optional<std::int64_t> referenceAgeMicros;
    IngressSubstitutionClose close{IngressSubstitutionClose::End};

    [[nodiscard]] std::optional<std::int64_t> LateMicros() const {
        if (!firstArrival) return std::nullopt;
        return std::chrono::duration_cast<std::chrono::microseconds>(*firstArrival - substitutedAt).count();
    }
};

class MatchIngressLedger final {
public:
    // Other bounds only for tests of the mechanism.
    explicit MatchIngressLedger(std::uint64_t retention = SubstitutionRetentionSequences) : retention_(retention) {}

    // After MatchSubstitutionRecords::Observe: a new identity watches from the
    // current cursor on.
    void Watch(PlayerId playerId, std::uint64_t cursor, bool newIdentity) {
        if (!newIdentity && players_.contains(playerId)) return;
        players_.insert_or_assign(playerId, Player{.watchedFrom = cursor + 1, .lastResolved = cursor});
    }

    // After MatchSubstitutionRecords::Resolved, for the same sequence.
    void Resolved(PlayerId playerId, std::uint64_t sequence) {
        const auto found = players_.find(playerId);
        if (found == players_.end()) return;
        auto& player = found->second;
        // A gap means sequences resolved unseen: none of them can be vouched for.
        if (sequence != player.lastResolved + 1) player.watchedFrom = sequence;
        player.lastResolved = sequence;
        while (!player.overflowed.empty() && *player.overflowed.begin() + retention_ < sequence)
            player.overflowed.erase(player.overflowed.begin());
    }

    // A record MatchSubstitutionRecords closed, as its match-ingress.jsonl record.
    [[nodiscard]] IngressSubstitution Closed(PlayerId playerId, std::uint64_t epoch, std::uint64_t life,
                                             const SubstitutionRecord& record, IngressSubstitutionClose reason) {
        IngressSubstitution result{.playerId = playerId, .epoch = epoch, .life = life, .sequence = record.sequence,
            .substitutedAt = record.substitutedAt, .substitutedTick = record.substitutedTick,
            .firstArrival = record.firstArrival, .close = reason};
        if (const auto found = players_.find(playerId); found != players_.end()) {
            auto& player = found->second;
            if (const auto annex = player.annexes.find(record.sequence); annex != player.annexes.end()) {
                result.copiesAfterFirst = annex->second.copiesAfterFirst;
                result.referenceAgeMicros = annex->second.referenceAgeMicros;
                player.annexes.erase(annex);
            }
            if (reason == IngressSubstitutionClose::Overflow) player.overflowed.insert(record.sequence);
        }
        return result;
    }

    // Classifies one command of an admitted input at or below the cursor from
    // what MatchSubstitutionRecords::Arrive found. referenceAge() is called
    // only for a late_first.
    template <class ReferenceAge>
    [[nodiscard]] IngressCommandClass Classify(PlayerId playerId, std::uint64_t sequence, std::uint64_t cursor,
                                               const SubstitutionArrival& arrival, ReferenceAge&& referenceAge) {
        const auto found = players_.find(playerId);
        if (found == players_.end() || !arrival.identity) return IngressCommandClass::ResolvedUntracked;
        auto& player = found->second;
        if (arrival.open) {
            auto& annex = player.annexes[sequence];
            if (!arrival.first) {
                ++annex.copiesAfterFirst;
                return IngressCommandClass::LateCopy;
            }
            annex.referenceAgeMicros = referenceAge();
            return IngressCommandClass::LateFirst;
        }
        // Seen resolved, within the retention, with no record and no overflow:
        // the Match executed this sequence, so this is a copy of it.
        const bool executed = sequence >= player.watchedFrom && sequence <= player.lastResolved &&
            sequence + retention_ >= cursor && !player.overflowed.contains(sequence);
        return executed ? IngressCommandClass::ResolvedCopy : IngressCommandClass::ResolvedUntracked;
    }

    // After MatchSubstitutionRecords::Remove or Clear closed the records.
    void Remove(PlayerId playerId) { players_.erase(playerId); }
    void Clear() { players_.clear(); }

private:
    struct Annex final {
        std::uint64_t copiesAfterFirst{};
        std::optional<std::int64_t> referenceAgeMicros;
    };
    struct Player final {
        // Sequences in [watchedFrom, lastResolved] were resolved while observed.
        std::uint64_t watchedFrom{};
        std::uint64_t lastResolved{};
        std::map<std::uint64_t, Annex> annexes;
        // Substituted sequences closed as overflow while still in retention.
        std::set<std::uint64_t> overflowed;
    };

    std::uint64_t retention_;
    std::map<PlayerId, Player> players_;
};

} // namespace fps::pvp
