#pragma once

// Product-owned diagnostics (v7.1 hitbox-fields, pv7 §6 "影子量測", D-2): the
// Match's shadow record of every action decision, match-shots.jsonl version 1.
// Header-only, like MatchIngressTrace.hpp. Before a1-shot-step no decision
// reads E, S or the view tick: the host hands each request's copy to this
// track, and for each decision the track writes what A1 would have done with
// it (its path and step tick, the cooldown it would have to hold) and the
// recorded view tick check (pv7 §7). Which command the step tick resolved
// (Actual, Held, Neutral) and whether it arrived later are not repeated here:
// match-commands.jsonl (resolved, by player, epoch and sequence) and
// match-ingress.jsonl (substitution records with their first late copy) hold
// them, joined offline by (player, E, life, S) in time order (D127): time_ns
// is the clock of both files, MovementTraceNowNs, so a reconnection (the Match
// resets, ticks and sequences start over) cannot pair a record with another
// connection's events.
//
// Records are buffered under the host's lock up to MatchShotRecordCapacity
// between drains; one that finds the buffer full is counted as dropped. The
// Match's main thread drains them with the statistics window and writes them
// (MatchShotTraceFile). Neither fails the Match; an I/O failure does.
#include "RetroFPS/Pvp/Combat.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fps::pvp {

inline constexpr int MatchShotTraceVersion = 1;
inline constexpr std::size_t MatchShotRecordCapacity = 4096;
// W of pv7 §6: the furthest step a request may claim, MaxPendingCommands + 1.
inline constexpr std::uint64_t MaxShotStepLead = MaxPendingCommands + 1;

// The path A1 would take for a request at its handoff tick A (pv7 §6).
enum class ShotStepPath : std::uint8_t { OnTime, Late, Epoch, Life, HoldLimit };

[[nodiscard]] inline constexpr std::string_view ShotStepPathName(ShotStepPath path) noexcept {
    switch (path) {
    case ShotStepPath::OnTime: return "on_time";
    case ShotStepPath::Late: return "late";
    case ShotStepPath::Epoch: return "epoch";
    case ShotStepPath::Life: return "life";
    case ShotStepPath::HoldLimit: return "hold_limit";
    }
    return "unknown";
}

struct ShotStep final {
    ShotStepPath path{};
    std::uint64_t tick{}; // D, the tick A1 would decide at
};

// pv7 §6 at tick A, with Ecur, L and life of the shooter after A's movement:
// another epoch (E = 0 included) or life decides at A; an epoch that resolved
// nothing yet, or an S at least W ahead of L, waits to A + W; S <= L is late
// (A); otherwise on time at T(S) = A + (S - L).
[[nodiscard]] inline constexpr ShotStep ClassifyShotStep(std::uint64_t a, std::uint64_t epoch, std::uint64_t sequence,
                                                         std::uint64_t requestLife, std::uint64_t currentEpoch,
                                                         std::uint64_t lastResolved, std::uint64_t life) noexcept {
    if (epoch != currentEpoch) return {ShotStepPath::Epoch, a};
    if (requestLife != life) return {ShotStepPath::Life, a};
    if (lastResolved == 0) return {ShotStepPath::HoldLimit, a + MaxShotStepLead};
    if (sequence <= lastResolved) return {ShotStepPath::Late, a};
    if (sequence - lastResolved >= MaxShotStepLead) return {ShotStepPath::HoldLimit, a + MaxShotStepLead};
    return {ShotStepPath::OnTime, a + (sequence - lastResolved)};
}

// pv7 §7, a record-only check: a Shot's view tick is 0 or within
// [65536, observed x 65536]; a Reload's is 0. An invalid one counts as missing.
[[nodiscard]] inline constexpr bool ValidViewTick(ActionKind kind, std::uint64_t observedTick, std::uint64_t viewTickQ16) noexcept {
    if (viewTickQ16 == 0) return true;
    if (kind != ActionKind::Shot || viewTickQ16 < 65536) return false;
    return observedTick > (std::numeric_limits<std::uint64_t>::max)() / 65536 || viewTickQ16 <= observedTick * 65536;
}

struct ShotShadowRecord final {
    // When the host recorded the decision (after tick A), on the movement
    // trace's clock (MovementTraceNowNs), the order key of the offline join.
    std::int64_t timeNs{};
    PlayerId playerId{};
    ActionId actionId{};
    ActionKind kind{ActionKind::Shot};
    std::uint64_t handoffTick{};  // A: the decision's tick in v7.1
    std::uint64_t epoch{};        // E
    std::uint64_t sequence{};     // S
    std::uint64_t requestLife{};
    std::uint64_t currentEpoch{}; // Ecur(A)
    std::uint64_t lastResolved{}; // L(A)
    std::uint64_t life{};         // life(A)
    ShotStep step{};
    // Shot only: ticks A1 would hold for the cooldown, nextAllowedShotTick
    // before this decision minus D (0 when none).
    std::optional<std::uint64_t> cooldownHoldTicks;
    ShotRejection rejection{ShotRejection::None};
    std::uint64_t viewTickQ16{};
    bool viewValid{true};
    // A - view tick, in Tick x 65536, for a valid nonzero view tick (pv7 §7
    // view_lag; v8's rewind reference).
    std::optional<std::int64_t> viewLagQ16;
};

struct ShotRecords final {
    std::vector<ShotShadowRecord> records;
    std::uint64_t dropped{};
    std::uint64_t viewTickInvalid{};
};

[[nodiscard]] inline std::string_view ShotRejectionName(ShotRejection rejection) noexcept {
    switch (rejection) {
    case ShotRejection::None: return "none";
    case ShotRejection::InvalidReference: return "invalid_reference";
    case ShotRejection::Expired: return "expired";
    case ShotRejection::Cooldown: return "cooldown";
    case ShotRejection::Dead: return "dead";
    case ShotRejection::StaleLife: return "stale_life";
    case ShotRejection::InvalidLife: return "invalid_life";
    case ShotRejection::Reloading: return "reloading";
    case ShotRejection::EmptyMagazine: return "empty_magazine";
    case ShotRejection::MagazineFull: return "magazine_full";
    }
    return "unknown";
}

// One line of match-shots.jsonl, without the newline.
[[nodiscard]] inline std::string MatchShotRecordLine(const ShotShadowRecord& r) {
    const auto optional = [](const auto& value) { return value ? std::to_string(*value) : std::string("null"); };
    return "{\"version\":" + std::to_string(MatchShotTraceVersion) + ",\"kind\":\"decision\",\"time_ns\":" +
        std::to_string(r.timeNs) + ",\"player_id\":" +
        std::to_string(r.playerId) + ",\"action_id\":" + std::to_string(r.actionId) + ",\"action_kind\":\"" +
        (r.kind == ActionKind::Shot ? "shot" : "reload") + "\",\"a\":" + std::to_string(r.handoffTick) +
        ",\"e\":" + std::to_string(r.epoch) + ",\"s\":" + std::to_string(r.sequence) +
        ",\"request_life\":" + std::to_string(r.requestLife) + ",\"epoch_at_a\":" + std::to_string(r.currentEpoch) +
        ",\"last_resolved_at_a\":" + std::to_string(r.lastResolved) + ",\"life_at_a\":" + std::to_string(r.life) +
        ",\"path\":\"" + std::string(ShotStepPathName(r.step.path)) + "\",\"step_tick\":" + std::to_string(r.step.tick) +
        ",\"cooldown_hold_ticks\":" + optional(r.cooldownHoldTicks) + ",\"rejection\":\"" +
        std::string(ShotRejectionName(r.rejection)) + "\",\"view_tick_q16\":" + std::to_string(r.viewTickQ16) +
        ",\"view_valid\":" + (r.viewValid ? "true" : "false") + ",\"view_lag_q16\":" + optional(r.viewLagQ16) + "}";
}

[[nodiscard]] inline std::string MatchShotTraceEndLine(std::uint64_t records, std::uint64_t dropped,
                                                       std::uint64_t viewTickInvalid) {
    return "{\"version\":" + std::to_string(MatchShotTraceVersion) + ",\"kind\":\"trace_end\",\"records\":" +
        std::to_string(records) + ",\"dropped\":" + std::to_string(dropped) + ",\"view_tick_invalid\":" +
        std::to_string(viewTickInvalid) + "}";
}

// The host's side: a copy of every request handed to the Match, until its
// decision. Under the host's lock; bounded by the Match's action window.
class ShotShadowTrack final {
public:
    void Handed(PlayerId playerId, const ShotRequest& request) { requests_[playerId][request.actionId] = request; }
    [[nodiscard]] bool Empty() const noexcept { return requests_.empty(); }
    void Remove(PlayerId playerId) { requests_.erase(playerId); }
    void Clear() noexcept { requests_.clear(); }

    // After tick A: a record for every handed request the Match decided at A.
    // before is the state before the tick (for the shooter's cooldown), after
    // the state after it (Ecur, L and life after A's movement); timeNs the
    // movement trace's clock now (MovementTraceNowNs).
    void Decided(const PvpMatch& match, std::uint64_t a, const WorldSnapshot& before, const WorldSnapshot& after,
                 std::int64_t timeNs) {
        for (auto player = requests_.begin(); player != requests_.end();) {
            const auto results = match.GetActionResults(player->first);
            const auto state = Find(after.players, player->first);
            if (!results || !state) {
                player = requests_.erase(player);
                continue;
            }
            const auto combat = Find(before.combat, player->first);
            for (const auto& decision : results->decisions) {
                const auto request = player->second.find(decision.actionId);
                if (decision.resolvedTick != a || request == player->second.end()) continue;
                Record(request->second, decision, a, *state, combat, timeNs);
                player->second.erase(request);
            }
            if (player->second.empty()) player = requests_.erase(player);
            else ++player;
        }
    }
    [[nodiscard]] ShotRecords Drain() {
        auto drained = std::move(buffer_);
        buffer_ = {};
        return drained;
    }

private:
    template <class T>
    [[nodiscard]] static const T* Find(const std::vector<T>& values, PlayerId playerId) {
        for (const auto& value : values)
            if (value.playerId == playerId) return &value;
        return nullptr;
    }
    void Record(const ShotRequest& request, const ShotDecision& decision, std::uint64_t a, const PlayerState& state,
                const CombatState* before, std::int64_t timeNs) {
        ShotShadowRecord record{.timeNs = timeNs, .playerId = state.playerId, .actionId = request.actionId, .kind = request.kind,
            .handoffTick = a, .epoch = request.movementEpoch, .sequence = request.commandSequence,
            .requestLife = request.lifeGeneration, .currentEpoch = state.movementEpoch,
            .lastResolved = state.lastResolvedCommand, .life = state.lifeGeneration, .rejection = decision.rejection,
            .viewTickQ16 = request.viewTickQ16};
        record.step = ClassifyShotStep(a, request.movementEpoch, request.commandSequence, request.lifeGeneration,
                                       state.movementEpoch, state.lastResolvedCommand, state.lifeGeneration);
        if (request.kind == ActionKind::Shot) {
            const auto next = before ? before->nextAllowedShotTick : 0;
            record.cooldownHoldTicks = next > record.step.tick ? next - record.step.tick : 0;
        }
        record.viewValid = ValidViewTick(request.kind, request.observedAuthorityTick, request.viewTickQ16);
        if (!record.viewValid) ++buffer_.viewTickInvalid;
        else if (request.viewTickQ16)
            record.viewLagQ16 = static_cast<std::int64_t>(a * 65536) - static_cast<std::int64_t>(request.viewTickQ16);
        if (buffer_.records.size() >= MatchShotRecordCapacity) ++buffer_.dropped;
        else buffer_.records.push_back(record);
    }
    std::map<PlayerId, std::map<ActionId, ShotRequest>> requests_;
    ShotRecords buffer_;
};

// "<x>-commands.jsonl" gives "<x>-shots.jsonl"; any other name gains
// ".shots.jsonl". Empty without a movement trace.
[[nodiscard]] inline std::filesystem::path MatchShotTracePath(const std::filesystem::path& movementTrace) {
    static constexpr std::string_view movementSuffix = "-commands.jsonl";
    if (movementTrace.empty()) return {};
    const auto name = movementTrace.filename().string();
    auto path = movementTrace;
    path.replace_filename(name.ends_with(movementSuffix)
        ? name.substr(0, name.size() - movementSuffix.size()) + "-shots.jsonl"
        : name + ".shots.jsonl");
    return path;
}

// Writes drained records and ends with trace_end; Finish throws if any write
// failed (the Match then exits 1).
class MatchShotTraceWriter final {
public:
    explicit MatchShotTraceWriter(std::ostream& output) : output_(output) {}
    MatchShotTraceWriter(const MatchShotTraceWriter&) = delete;
    MatchShotTraceWriter& operator=(const MatchShotTraceWriter&) = delete;

    void Write(const ShotRecords& drained) {
        if (finished_) throw std::logic_error("The shot trace already ended");
        for (const auto& record : drained.records) {
            output_ << MatchShotRecordLine(record) << '\n';
            ++records_;
        }
        dropped_ += drained.dropped;
        viewTickInvalid_ += drained.viewTickInvalid;
        output_.flush();
        if (!output_) failed_ = true;
    }
    void Finish(const ShotRecords& last) {
        Write(last);
        output_ << MatchShotTraceEndLine(records_, dropped_, viewTickInvalid_) << '\n';
        output_.flush();
        if (!output_) failed_ = true;
        finished_ = true;
        if (failed_) throw std::runtime_error("Shot trace failed to write");
    }

private:
    std::ostream& output_;
    std::uint64_t records_{}, dropped_{}, viewTickInvalid_{};
    bool failed_{}, finished_{};
};

class MatchShotTraceFile final {
public:
    explicit MatchShotTraceFile(const std::filesystem::path& path) : file_(path), writer_(file_) {
        if (!file_) throw std::runtime_error("Cannot open shot trace: " + path.string());
    }
    void Write(const ShotRecords& drained) { writer_.Write(drained); }
    void Finish(const ShotRecords& last) { writer_.Finish(last); }

private:
    std::ofstream file_;
    MatchShotTraceWriter writer_;
};

} // namespace fps::pvp
