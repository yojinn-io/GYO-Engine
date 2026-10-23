#pragma once

// The Match host's substitution records (v7.1 outage-resync, C13 (b);
// network-architecture §7 "被替代序號的單一紀錄"): the only record of "this
// movement sequence was substituted, and has its first copy arrived". The slack
// sample path decides from it; the ingress ledger (diagnostics) only
// reads the result of each arrival and receives each closed record. Host layer
// and header-only: it never reads or changes PvpMatch or the wire.
//
// One record per substituted (player, epoch, life, sequence), opened when the
// tick resolved the sequence without a receipt. A record is open while
// cursor - sequence <= SubstitutionRetentionSequences (one sequence resolves per
// tick, so 600 sequences are the 600-tick connection-quality window) and at
// most SubstitutionRecordCapacity records are open per player. It closes, once,
// as
//   aged      the cursor moved past the retention,
//   epoch     the player's movement epoch changed (same life),
//   life      the player's life changed,
//   removed   the player left or was evicted,
//   reset     the match was reset (ClearState),
//   overflow  the oldest record of a player at capacity,
//   end       the process ended with the record open.
//
// The first copy of an open record decides once (network-architecture §4):
// later than N_o, no slack sample and a Resync; otherwise one slack sample. A
// copy of a closed record decides nothing, so the retention is part of the
// game's rule (v7.1 outage-resync retention-600).
#include "RetroFPS/Pvp/Movement.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

namespace fps::pvp {

// Why a record closed. The name predates the records' move out of the ingress
// ledger (v7.1 outage-resync); the ledger spells it and counts it.
enum class IngressSubstitutionClose : std::uint8_t { Aged, Epoch, Life, Removed, Reset, Overflow, End, Count };

inline constexpr std::uint64_t SubstitutionRetentionSequences = ConnectionQualityWindowTicks;
inline constexpr std::size_t SubstitutionRecordCapacity = 1024;

struct SubstitutionRecord final {
    std::uint64_t sequence{};
    std::chrono::steady_clock::time_point substitutedAt;
    std::uint64_t substitutedTick{};
    // The first copy that arrived after the substitution, if any.
    std::optional<std::chrono::steady_clock::time_point> firstArrival;
};

// What one arriving copy found.
struct SubstitutionArrival final {
    // The player's records describe the input's epoch and life.
    bool identity{};
    // An open record: the first copy (first) or a later one. Only a first
    // copy decides (its lateness: a slack sample or a Resync).
    bool open{};
    bool first{};
    std::chrono::steady_clock::time_point substitutedAt;
};

class MatchSubstitutionRecords final {
public:
    // Other bounds only for tests of the mechanism.
    explicit MatchSubstitutionRecords(std::uint64_t retention = SubstitutionRetentionSequences,
                                      std::size_t capacity = SubstitutionRecordCapacity)
        : retention_(retention), capacity_(capacity) {}

    [[nodiscard]] std::uint64_t Retention() const noexcept { return retention_; }

    // Before the sequences a tick resolved for this player: a new player or a
    // new identity starts an empty record set, closing the previous identity's
    // records (life when the life changed, else epoch). Returns whether the
    // identity is new. closed(player, epoch, life, record, reason).
    template <class Closed>
    bool Observe(PlayerId playerId, std::uint64_t epoch, std::uint64_t life, Closed&& closed) {
        auto found = players_.find(playerId);
        if (found != players_.end() && found->second.epoch == epoch && found->second.life == life) return false;
        if (found != players_.end())
            CloseAll(playerId, found->second, found->second.life != life ? IngressSubstitutionClose::Life
                                                                       : IngressSubstitutionClose::Epoch, closed);
        players_.insert_or_assign(playerId, Player{.epoch = epoch, .life = life});
        return true;
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
        if (substitutedAt) {
            // One record per sequence: within an identity the cursor only
            // advances, so a sequence is never substituted twice.
            player.open.insert_or_assign(sequence, SubstitutionRecord{.sequence = sequence,
                .substitutedAt = *substitutedAt, .substitutedTick = tick});
            if (player.open.size() > capacity_) Close(playerId, player, player.open.begin(), IngressSubstitutionClose::Overflow, closed);
        }
        while (!player.open.empty() && player.open.begin()->first + retention_ < sequence)
            Close(playerId, player, player.open.begin(), IngressSubstitutionClose::Aged, closed);
    }

    // A copy of a sequence at or below the cursor. now() is called only for the
    // first copy of an open record; a later copy reads no clock.
    template <class Now>
    [[nodiscard]] SubstitutionArrival Arrive(PlayerId playerId, std::uint64_t epoch, std::uint64_t life,
                                             std::uint64_t sequence, Now&& now) {
        const auto found = players_.find(playerId);
        if (found == players_.end() || found->second.epoch != epoch || found->second.life != life) return {};
        auto& player = found->second;
        SubstitutionArrival arrival{.identity = true};
        const auto record = player.open.find(sequence);
        if (record == player.open.end()) return arrival;
        arrival.open = true;
        arrival.substitutedAt = record->second.substitutedAt;
        if (record->second.firstArrival) return arrival;
        arrival.first = true;
        record->second.firstArrival = now();
        return arrival;
    }

    // The player left or was evicted.
    template <class Closed>
    void Remove(PlayerId playerId, Closed&& closed) {
        const auto found = players_.find(playerId);
        if (found == players_.end()) return;
        CloseAll(playerId, found->second, IngressSubstitutionClose::Removed, closed);
        players_.erase(found);
    }

    // Every player at once: reset (ClearState) or end (process end).
    template <class Closed>
    void Clear(IngressSubstitutionClose reason, Closed&& closed) {
        for (auto& [playerId, player] : players_) CloseAll(playerId, player, reason, closed);
        players_.clear();
    }

private:
    struct Player final {
        std::uint64_t epoch{};
        std::uint64_t life{};
        std::map<std::uint64_t, SubstitutionRecord> open;
    };
    using Record = std::map<std::uint64_t, SubstitutionRecord>::iterator;

    template <class Closed>
    void Close(PlayerId playerId, Player& player, Record record, IngressSubstitutionClose reason, Closed& closed) {
        auto value = std::move(record->second);
        player.open.erase(record);
        closed(playerId, player.epoch, player.life, std::as_const(value), reason);
    }
    template <class Closed>
    void CloseAll(PlayerId playerId, Player& player, IngressSubstitutionClose reason, Closed& closed) {
        while (!player.open.empty()) Close(playerId, player, player.open.begin(), reason, closed);
    }

    std::uint64_t retention_;
    std::size_t capacity_;
    std::map<PlayerId, Player> players_;
};

} // namespace fps::pvp
