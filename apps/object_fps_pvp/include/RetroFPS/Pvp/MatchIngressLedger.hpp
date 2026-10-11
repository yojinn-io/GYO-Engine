#pragma once

// Product-owned diagnostics (v7 batch 08c): the Match host's record of every
// movement sequence it substituted, so that copies arriving after resolution
// can be classified and timed. Host layer and header-only: it never reads or
// changes PvpMatch, snapshot fields or the wire, and it is not the slack
// sample track (MatchRuntimeHost's 64-entry SlackTrack stays as it is).
//
// One record per substituted (player, epoch, life, sequence), opened when the
// tick resolved the sequence without a receipt. A record stays open while
// cursor - sequence <= IngressLedgerRetentionSequences (one sequence resolves
// per tick, so 600 sequences are the 600-tick connection-quality window) and
// at most IngressLedgerCapacity records are open per player. With one record
// per sequence, at most retention + 1 records are open, so overflow should
// stay 0 with the product constants. It closes as
//   aged      the cursor moved past the retention,
//   epoch     the player's movement epoch changed (same life),
//   life      the player's life changed,
//   removed   the player left or was evicted,
//   reset     the match was reset (ClearState),
//   overflow  the oldest record of a player at capacity,
//   end       the process ended with the record open.
// Each closed record is handed to the caller once (IngressName gives the
// spelling of its close reason for match-ingress.jsonl).
#include "RetroFPS/Pvp/IngressStatistics.hpp"
#include "RetroFPS/Pvp/Movement.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace fps::pvp {

inline constexpr std::uint64_t IngressLedgerRetentionSequences = ConnectionQualityWindowTicks;
inline constexpr std::size_t IngressLedgerCapacity = 1024;

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
    explicit MatchIngressLedger(std::uint64_t retention = IngressLedgerRetentionSequences,
                                std::size_t capacity = IngressLedgerCapacity)
        : retention_(retention), capacity_(capacity) {}

    // Before the sequences a tick resolved for this player: a new player or a
    // new identity starts an empty record set at the current cursor, closing
    // the previous identity's records (life when the life changed, else epoch).
    template <class Closed>
    void Observe(PlayerId playerId, std::uint64_t epoch, std::uint64_t life, std::uint64_t cursor, Closed&& closed) {
        auto found = players_.find(playerId);
        if (found != players_.end() && found->second.epoch == epoch && found->second.life == life) return;
        if (found != players_.end())
            CloseAll(found->second, found->second.life != life ? IngressSubstitutionClose::Life
                                                               : IngressSubstitutionClose::Epoch, closed);
        players_.insert_or_assign(playerId, Player{.epoch = epoch, .life = life, .watchedFrom = cursor + 1,
                                                   .lastResolved = cursor});
    }

    // One sequence this tick resolved for the observed identity, in order.
    // substitutedAt is set when the host had no receipt for it: a record opens.
    // Records beyond the retention of the new cursor close as aged.
    template <class Closed>
    void Resolved(PlayerId playerId, std::uint64_t sequence,
                  std::optional<std::chrono::steady_clock::time_point> substitutedAt, std::uint64_t tick,
                  Closed&& closed) {
        const auto found = players_.find(playerId);
        if (found == players_.end()) return;
        auto& player = found->second;
        // A gap means sequences resolved unseen: none of them can be vouched for.
        if (sequence != player.lastResolved + 1) player.watchedFrom = sequence;
        player.lastResolved = sequence;
        if (substitutedAt) {
            player.open.insert_or_assign(sequence, IngressSubstitution{.playerId = playerId, .epoch = player.epoch,
                .life = player.life, .sequence = sequence, .substitutedAt = *substitutedAt, .substitutedTick = tick});
            if (player.open.size() > capacity_) {
                const auto oldest = player.open.begin();
                player.overflowed.insert(oldest->first);
                Close(player, oldest, IngressSubstitutionClose::Overflow, closed);
            }
        }
        while (!player.open.empty() && player.open.begin()->first + retention_ < sequence)
            Close(player, player.open.begin(), IngressSubstitutionClose::Aged, closed);
        while (!player.overflowed.empty() && *player.overflowed.begin() + retention_ < sequence)
            player.overflowed.erase(player.overflowed.begin());
    }

    // Classifies one command of an admitted input at or below the cursor.
    // now() is called only for a late_first (the first copy of an open
    // record); a pure retransmission reads no clock. referenceAge() is called
    // with it, after now().
    template <class Now, class ReferenceAge>
    [[nodiscard]] IngressCommandClass Arrive(PlayerId playerId, std::uint64_t epoch, std::uint64_t life,
                                             std::uint64_t sequence, std::uint64_t cursor, Now&& now,
                                             ReferenceAge&& referenceAge) {
        const auto found = players_.find(playerId);
        if (found == players_.end() || found->second.epoch != epoch || found->second.life != life)
            return IngressCommandClass::ResolvedUntracked;
        auto& player = found->second;
        if (const auto record = player.open.find(sequence); record != player.open.end()) {
            if (record->second.firstArrival) {
                ++record->second.copiesAfterFirst;
                return IngressCommandClass::LateCopy;
            }
            record->second.firstArrival = now();
            record->second.referenceAgeMicros = referenceAge();
            return IngressCommandClass::LateFirst;
        }
        // Seen resolved, within the retention, with no record and no overflow:
        // the Match executed this sequence, so this is a copy of it.
        const bool executed = sequence >= player.watchedFrom && sequence <= player.lastResolved &&
            sequence + retention_ >= cursor && !player.overflowed.contains(sequence);
        return executed ? IngressCommandClass::ResolvedCopy : IngressCommandClass::ResolvedUntracked;
    }

    // The player left or was evicted.
    template <class Closed>
    void Remove(PlayerId playerId, Closed&& closed) {
        const auto found = players_.find(playerId);
        if (found == players_.end()) return;
        CloseAll(found->second, IngressSubstitutionClose::Removed, closed);
        players_.erase(found);
    }

    // Every player at once: reset (ClearState) or end (process end).
    template <class Closed>
    void Clear(IngressSubstitutionClose reason, Closed&& closed) {
        for (auto& [playerId, player] : players_) {
            static_cast<void>(playerId);
            CloseAll(player, reason, closed);
        }
        players_.clear();
    }

private:
    struct Player final {
        std::uint64_t epoch{};
        std::uint64_t life{};
        // Sequences in [watchedFrom, lastResolved] were resolved while observed.
        std::uint64_t watchedFrom{};
        std::uint64_t lastResolved{};
        std::map<std::uint64_t, IngressSubstitution> open;
        // Substituted sequences closed as overflow while still in retention.
        std::set<std::uint64_t> overflowed;
    };
    using Record = std::map<std::uint64_t, IngressSubstitution>::iterator;

    template <class Closed>
    static void Close(Player& player, Record record, IngressSubstitutionClose reason, Closed& closed) {
        auto value = std::move(record->second);
        player.open.erase(record);
        value.close = reason;
        closed(std::as_const(value));
    }
    template <class Closed>
    static void CloseAll(Player& player, IngressSubstitutionClose reason, Closed& closed) {
        while (!player.open.empty()) Close(player, player.open.begin(), reason, closed);
        player.overflowed.clear();
    }

    std::uint64_t retention_;
    std::size_t capacity_;
    std::map<PlayerId, Player> players_;
};

} // namespace fps::pvp
