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
#include <string>
#include <string_view>
#include <type_traits>

namespace fps::pvp {

inline constexpr int IngressStatisticsVersion = 1;

// Each command of an admitted input falls in exactly one class (I2).
enum class IngressCommandClass : std::uint8_t {
    AcceptedNew, PendingCopy, LateFirst, LateCopy, ResolvedCopy, ResolvedUntracked, Count
};
// Why the host refused a whole input; all its commands count under the reason.
enum class IngressInputRejection : std::uint8_t {
    Resetting, UnknownPlayer, Malformed, EpochOld, EpochFuture, LifeOld, LifeFuture,
    BeyondWindow, ConflictQueued, ConflictStaged, StagedOverWindow, Count
};
// Commands the host admitted and later discarded unresolved.
enum class IngressStagedDiscard : std::uint8_t { HandoffRejected, RotationDiscarded, LeaveDiscarded, Count };
// Why an action batch was refused: OverBatch and Malformed at the wire, the
// rest by the host.
enum class IngressActionRejection : std::uint8_t {
    OverBatch, Malformed, Resetting, InvalidPlayer, InvalidBatch, Conflict, OutsideWindow, Full, Count
};
// How a substitution record of match-ingress.jsonl closed.
enum class IngressSubstitutionClose : std::uint8_t { Aged, Epoch, Life, Removed, Reset, Overflow, End, Count };
enum class IngressUnit : std::uint8_t { Inputs, Commands, Batches, Shots };

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
struct MatchIngressCounts final {
    IngressInputCount received;
    std::uint64_t acceptedInputs{};
    std::array<std::uint64_t, IngressCount<IngressCommandClass>> classified{};
    std::uint64_t lateOnlyInputs{}; // admitted inputs whose every command was late
    std::array<IngressInputCount, IngressCount<IngressInputRejection>> rejected{};
    std::array<std::uint64_t, IngressCount<IngressStagedDiscard>> discarded{};
    IngressActionCount receivedActions;
    IngressActionCount acceptedActions;
    std::array<IngressActionCount, IngressCount<IngressActionRejection>> rejectedActions{};
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

} // namespace fps::pvp
