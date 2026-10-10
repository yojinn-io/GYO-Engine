#pragma once

// Product-owned diagnostics (v7 batch 08c): what the Match host did with each
// input and action batch it received. Header-only, so the targets that build
// the host from sources need no new file. Counting never changes admission.
//
//   [ObjectFPS/PvP Match] ingress statistics version=1 player=<id> window_ms=<int> final=<0|1> <key>=<int>...
//
// Each line counts what happened since the previous line of the same player;
// player=0 holds unknown players and is written every window; final=1 marks
// the last line. Keys are "<reason>[_<kind>]_<unit>" and new keys are only
// ever appended. The golden lines live in tests/object_fps_pvp/fixtures/ingress_v1.
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace fps::pvp {

inline constexpr int IngressStatisticsVersion = 1;

// Each command of an admitted input falls in exactly one class (I2). Above
// the cursor: accepted_new or pending_copy. At or below it, by the substitution
// records of MatchIngressLedger.hpp: late_first (first copy of an open
// record), late_copy (a later copy), resolved_copy (a copy of a sequence the
// Match executed within the retention) or resolved_untracked (anything else:
// beyond the retention, overflowed, or resolved before the ledger saw it).
enum class IngressCommandClass : std::uint8_t {
    AcceptedNew, PendingCopy, LateFirst, LateCopy, ResolvedCopy, ResolvedUntracked, Count
};
// Why the host refused a whole input; all its commands count under the reason.
enum class IngressInputRejection : std::uint8_t {
    Resetting, UnknownPlayer, Malformed, EpochOld, EpochFuture, LifeOld, LifeFuture,
    BeyondWindow, ConflictQueued, ConflictStaged, StagedOverWindow, Count
};
// Commands the host admitted and later discarded unresolved. A reset's
// discards are counted apart (MatchIngressCounts::resetDiscardedCommands)
// because their key was added after this list's.
enum class IngressStagedDiscard : std::uint8_t { HandoffRejected, RotationDiscarded, LeaveDiscarded, Count };
// Why an action batch was refused: OverBatch and Malformed at the wire, the
// rest by the host. Full should stay 0 (D55): every candidate action id lies
// in (floor, floor + MaxActionWindow], so neither bound can be exceeded.
enum class IngressActionRejection : std::uint8_t {
    OverBatch, Malformed, Resetting, InvalidPlayer, InvalidBatch, Conflict, OutsideWindow, Full, Count
};
// How a substitution record closed (MatchIngressLedger.hpp).
enum class IngressSubstitutionClose : std::uint8_t { Aged, Epoch, Life, Removed, Reset, Overflow, End, Count };
enum class IngressUnit : std::uint8_t { Inputs, Commands, Batches, Shots, Acks, Samples };

// The only spelling of each word, shared by the statistics line and the
// detail file (its reason and close fields).
inline constexpr std::string_view IngressName(IngressCommandClass value) noexcept {
    switch (value) {
    case IngressCommandClass::AcceptedNew: return "accepted_new";
    case IngressCommandClass::PendingCopy: return "pending_copy";
    case IngressCommandClass::LateFirst: return "late_first";
    case IngressCommandClass::LateCopy: return "late_copy";
    case IngressCommandClass::ResolvedCopy: return "resolved_copy";
    case IngressCommandClass::ResolvedUntracked: return "resolved_untracked";
    case IngressCommandClass::Count: break;
    }
    return "invalid";
}
inline constexpr std::string_view IngressName(IngressInputRejection value) noexcept {
    switch (value) {
    case IngressInputRejection::Resetting: return "resetting";
    case IngressInputRejection::UnknownPlayer: return "unknown_player";
    case IngressInputRejection::Malformed: return "malformed";
    case IngressInputRejection::EpochOld: return "epoch_old";
    case IngressInputRejection::EpochFuture: return "epoch_future";
    case IngressInputRejection::LifeOld: return "life_old";
    case IngressInputRejection::LifeFuture: return "life_future";
    case IngressInputRejection::BeyondWindow: return "beyond_window";
    case IngressInputRejection::ConflictQueued: return "conflict_queued";
    case IngressInputRejection::ConflictStaged: return "conflict_staged";
    case IngressInputRejection::StagedOverWindow: return "staged_over_window";
    case IngressInputRejection::Count: break;
    }
    return "invalid";
}
inline constexpr std::string_view IngressName(IngressStagedDiscard value) noexcept {
    switch (value) {
    case IngressStagedDiscard::HandoffRejected: return "handoff_rejected";
    case IngressStagedDiscard::RotationDiscarded: return "rotation_discarded";
    case IngressStagedDiscard::LeaveDiscarded: return "leave_discarded";
    case IngressStagedDiscard::Count: break;
    }
    return "invalid";
}
inline constexpr std::string_view IngressName(IngressActionRejection value) noexcept {
    switch (value) {
    case IngressActionRejection::OverBatch: return "over_batch";
    case IngressActionRejection::Malformed: return "malformed";
    case IngressActionRejection::Resetting: return "resetting";
    case IngressActionRejection::InvalidPlayer: return "invalid_player";
    case IngressActionRejection::InvalidBatch: return "invalid_batch";
    case IngressActionRejection::Conflict: return "conflict";
    case IngressActionRejection::OutsideWindow: return "outside_window";
    case IngressActionRejection::Full: return "full";
    case IngressActionRejection::Count: break;
    }
    return "invalid";
}
inline constexpr std::string_view IngressName(IngressSubstitutionClose value) noexcept {
    switch (value) {
    case IngressSubstitutionClose::Aged: return "aged";
    case IngressSubstitutionClose::Epoch: return "epoch";
    case IngressSubstitutionClose::Life: return "life";
    case IngressSubstitutionClose::Removed: return "removed";
    case IngressSubstitutionClose::Reset: return "reset";
    case IngressSubstitutionClose::Overflow: return "overflow";
    case IngressSubstitutionClose::End: return "end";
    case IngressSubstitutionClose::Count: break;
    }
    return "invalid";
}
inline constexpr std::string_view IngressName(IngressUnit value) noexcept {
    switch (value) {
    case IngressUnit::Inputs: return "inputs";
    case IngressUnit::Commands: return "commands";
    case IngressUnit::Batches: return "batches";
    case IngressUnit::Shots: return "shots";
    case IngressUnit::Acks: return "acks";
    case IngressUnit::Samples: return "samples";
    }
    return "invalid";
}

template <class E>
inline constexpr std::size_t IngressCount = static_cast<std::size_t>(E::Count);
template <class E>
[[nodiscard]] constexpr std::size_t IngressIndex(E value) noexcept { return static_cast<std::size_t>(value); }

struct IngressInputCount final {
    std::uint64_t inputs{};
    std::uint64_t commands{};
};
struct IngressActionCount final {
    std::uint64_t batches{};
    std::uint64_t shots{};
};

// One player's (or the player=0 bucket's) counts.
//   I2: received.commands == sum(classified) + sum(rejected[].commands)
//       received.inputs   == acceptedInputs + sum(rejected[].inputs)
//   receivedActions == acceptedActions + sum(rejectedActions[]), per unit
// Staged discards are outside I2: those commands were classified accepted_new.
// Substitution records (J5a, once every record is closed):
//   substitutedCommands == classified[LateFirst] + sum(unarrived[])
struct MatchIngressCounts final {
    IngressInputCount received;
    std::uint64_t acceptedInputs{};
    std::array<std::uint64_t, IngressCount<IngressCommandClass>> classified{};
    // Admitted inputs whose every command was late_first or late_copy.
    std::uint64_t lateOnlyInputs{};
    std::array<IngressInputCount, IngressCount<IngressInputRejection>> rejected{};
    std::array<std::uint64_t, IngressCount<IngressStagedDiscard>> discarded{};
    IngressActionCount receivedActions;
    IngressActionCount acceptedActions;
    std::array<IngressActionCount, IngressCount<IngressActionRejection>> rejectedActions{};
    // Appended keys (batch 08c F2b-1), in line order.
    // Substitution records opened, and records closed before any copy arrived, by close reason.
    std::uint64_t substitutedCommands{};
    std::array<std::uint64_t, IngressCount<IngressSubstitutionClose>> unarrived{};
    // Admitted commands and accepted shots a reset (ClearState) discarded
    // unresolved, and shots a Leave or eviction discarded before handoff.
    std::uint64_t resetDiscardedCommands{};
    std::uint64_t resetDiscardedActionShots{};
    std::uint64_t leaveDiscardedActionShots{};
    // Players whose pending action acknowledgement a reset discarded.
    std::uint64_t resetDiscardedActionAcks{};
    // Appended keys (batch 08c F2b-2): the movement slack samples of this
    // player on their way to the IPC (known gap 6, host side). A candidate is
    // an executed sequence's first receipt or a late first arrival still in
    // the 64-entry substitution track. Each publication carries only the
    // smallest candidate since the previous one; the others are merged.
    //   S1: slackExecuted + slackLate == slackMerged + slackDiscarded + slackPublished
    //   S2: slackPublished == slackOverwritten + slackTaken + slackUnclaimed
    // (exact once nothing is pending: after a reset or the final window).
    // slackPublishedNegative is part of slackPublished; slackCoalesced is part
    // of slackTaken (samples the IPC replaced, latest wins, before writing them).
    std::uint64_t slackExecutedSamples{};
    std::uint64_t slackLateSamples{};
    std::uint64_t slackMergedSamples{};
    // Candidates dropped before a publication: identity change, Leave,
    // eviction, reset or the end of the process.
    std::uint64_t slackDiscardedSamples{};
    std::uint64_t slackPublishedSamples{};
    std::uint64_t slackPublishedNegativeSamples{};
    // Published snapshots replaced before the IPC took them.
    std::uint64_t slackOverwrittenSamples{};
    std::uint64_t slackTakenSamples{};
    std::uint64_t slackCoalescedSamples{};
    // Published samples still waiting for the IPC at a reset or the end.
    std::uint64_t slackUnclaimedSamples{};
};

struct MatchIngressWindow final {
    std::uint64_t player{};
    std::uint64_t windowMs{};
    bool final{};
    MatchIngressCounts counts;
};

[[nodiscard]] inline std::string IngressKey(std::string_view reason, std::string_view kind, IngressUnit unit) {
    std::string key(reason);
    if (!kind.empty()) key.append("_").append(kind);
    return key.append("_").append(IngressName(unit));
}

// Visits every counter in line order as field(key, value), where value is a
// reference into counts (const when counts is). The line, its golden test and
// any summing over windows share this one order.
template <class Counts, class Field>
    requires std::is_same_v<std::remove_const_t<Counts>, MatchIngressCounts>
void ForEachMatchIngressField(Counts& counts, Field&& field) {
    constexpr std::string_view none;
    constexpr std::string_view actions = "actions";
    field(IngressKey("received", none, IngressUnit::Inputs), counts.received.inputs);
    field(IngressKey("received", none, IngressUnit::Commands), counts.received.commands);
    field(IngressKey("accepted", none, IngressUnit::Inputs), counts.acceptedInputs);
    for (std::size_t i = 0; i < counts.classified.size(); ++i)
        field(IngressKey(IngressName(static_cast<IngressCommandClass>(i)), none, IngressUnit::Commands), counts.classified[i]);
    field(IngressKey("late_only", none, IngressUnit::Inputs), counts.lateOnlyInputs);
    for (std::size_t i = 0; i < counts.rejected.size(); ++i) {
        const auto reason = IngressName(static_cast<IngressInputRejection>(i));
        field(IngressKey(reason, none, IngressUnit::Inputs), counts.rejected[i].inputs);
        field(IngressKey(reason, none, IngressUnit::Commands), counts.rejected[i].commands);
    }
    for (std::size_t i = 0; i < counts.discarded.size(); ++i)
        field(IngressKey(IngressName(static_cast<IngressStagedDiscard>(i)), none, IngressUnit::Commands), counts.discarded[i]);
    field(IngressKey("received", actions, IngressUnit::Batches), counts.receivedActions.batches);
    field(IngressKey("received", actions, IngressUnit::Shots), counts.receivedActions.shots);
    field(IngressKey("accepted", actions, IngressUnit::Batches), counts.acceptedActions.batches);
    field(IngressKey("accepted", actions, IngressUnit::Shots), counts.acceptedActions.shots);
    for (std::size_t i = 0; i < counts.rejectedActions.size(); ++i) {
        const auto reason = IngressName(static_cast<IngressActionRejection>(i));
        field(IngressKey(reason, actions, IngressUnit::Batches), counts.rejectedActions[i].batches);
        field(IngressKey(reason, actions, IngressUnit::Shots), counts.rejectedActions[i].shots);
    }
    field(IngressKey("substituted", none, IngressUnit::Commands), counts.substitutedCommands);
    for (std::size_t i = 0; i < counts.unarrived.size(); ++i)
        field(IngressKey("unarrived_" + std::string(IngressName(static_cast<IngressSubstitutionClose>(i))), none,
                         IngressUnit::Commands), counts.unarrived[i]);
    field(IngressKey("reset_discarded", none, IngressUnit::Commands), counts.resetDiscardedCommands);
    field(IngressKey("reset_discarded", actions, IngressUnit::Shots), counts.resetDiscardedActionShots);
    field(IngressKey("leave_discarded", actions, IngressUnit::Shots), counts.leaveDiscardedActionShots);
    field(IngressKey("reset_discarded", actions, IngressUnit::Acks), counts.resetDiscardedActionAcks);
    field(IngressKey("slack_executed", none, IngressUnit::Samples), counts.slackExecutedSamples);
    field(IngressKey("slack_late", none, IngressUnit::Samples), counts.slackLateSamples);
    field(IngressKey("slack_merged", none, IngressUnit::Samples), counts.slackMergedSamples);
    field(IngressKey("slack_discarded", none, IngressUnit::Samples), counts.slackDiscardedSamples);
    field(IngressKey("slack_published", none, IngressUnit::Samples), counts.slackPublishedSamples);
    field(IngressKey("slack_published_negative", none, IngressUnit::Samples), counts.slackPublishedNegativeSamples);
    field(IngressKey("slack_overwritten", none, IngressUnit::Samples), counts.slackOverwrittenSamples);
    field(IngressKey("slack_taken", none, IngressUnit::Samples), counts.slackTakenSamples);
    field(IngressKey("slack_coalesced", none, IngressUnit::Samples), counts.slackCoalescedSamples);
    field(IngressKey("slack_unclaimed", none, IngressUnit::Samples), counts.slackUnclaimedSamples);
}

[[nodiscard]] inline std::string MatchIngressStatisticsLine(const MatchIngressWindow& window) {
    std::string line = "[ObjectFPS/PvP Match] ingress statistics version=" + std::to_string(IngressStatisticsVersion) +
        " player=" + std::to_string(window.player) + " window_ms=" + std::to_string(window.windowMs) +
        " final=" + (window.final ? "1" : "0");
    ForEachMatchIngressField(window.counts, [&](const std::string& key, std::uint64_t value) {
        line.append(" ").append(key).append("=").append(std::to_string(value));
    });
    return line;
}

// The lines of one window from the counts the Match host handed over (keyed
// by player). Every window has a player=0 line. The final window also closes
// each player written before with a final=1 line, zero counts included, so
// every (process, player) ends with exactly one final=1 line. Lines are in
// player order.
class MatchIngressStatisticsWriter final {
public:
    [[nodiscard]] std::vector<std::string> Lines(std::map<std::uint64_t, MatchIngressCounts> taken,
                                                 std::uint64_t windowMs, bool final) {
        taken.try_emplace(0);
        for (const auto& [player, counts] : taken) written_.insert(player);
        if (final)
            for (const auto player : written_) taken.try_emplace(player);
        std::vector<std::string> lines;
        lines.reserve(taken.size());
        for (const auto& [player, counts] : taken)
            lines.push_back(MatchIngressStatisticsLine({.player = player, .windowMs = windowMs, .final = final, .counts = counts}));
        return lines;
    }

private:
    std::set<std::uint64_t> written_;
};

} // namespace fps::pvp
