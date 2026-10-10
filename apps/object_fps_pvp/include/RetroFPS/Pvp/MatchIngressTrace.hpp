#pragma once

// Product-owned diagnostics (v7 batch 08c): the Match's detail file
// match-ingress.jsonl, version 1. Header-only, so the targets that build the
// host from sources need no new file. Spelling, fields and rules are in
// tests/object_fps_pvp/fixtures/ingress_v1/README.zh-Hant.md; the golden
// sample is match-ingress.jsonl there.
//
// The host keeps a bounded buffer of records (MatchIngressRecordBuffer, under
// its lock); the Match's main thread drains it with the statistics window and
// writes it (MatchIngressTraceWriter). Rejection records are kept for the
// first MatchIngressRejectionsPerWindow refusals of each (player, reason) in
// a statistics window and only counted as suppressed after that; a record
// that finds the buffer full is counted as dropped. Neither fails the Match;
// an I/O failure does (the caller checks Good()).
#include "RetroFPS/Pvp/IngressStatistics.hpp"
#include "RetroFPS/Pvp/MatchIngressLedger.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace fps::pvp {

inline constexpr int MatchIngressTraceVersion = 1;
inline constexpr std::size_t MatchIngressRejectionsPerWindow = 16;
// Per statistics window at most 2 x (retention + 1) substitution records
// close per player (records opened in this and the previous window) next to
// at most 16 rejections per reason; 16384 leaves room for several players.
inline constexpr std::size_t MatchIngressRecordCapacity = 16384;

// One input the host refused, as it arrived. Sequences are absent for an
// input without commands; cursor and current identity are 0 for a player the
// Match does not hold.
struct IngressRejectionRecord final {
    std::int64_t timeNs{};
    PlayerId playerId{};
    IngressInputRejection reason{IngressInputRejection::Malformed};
    std::uint64_t epoch{};
    std::uint64_t life{};
    std::optional<std::uint64_t> firstSequence;
    std::optional<std::uint64_t> lastSequence;
    std::uint64_t commands{};
    std::uint64_t cursor{};
    std::uint64_t currentEpoch{};
    std::uint64_t currentLife{};
};

using IngressRecord = std::variant<IngressRejectionRecord, IngressSubstitution>;

// What one drain hands over: the records in the order they were kept, and
// how many were suppressed or dropped since the previous drain.
struct IngressRecords final {
    std::vector<IngressRecord> records;
    std::uint64_t suppressed{};
    std::uint64_t dropped{};
};

class MatchIngressRecordBuffer final {
public:
    // Other bounds only for tests of the mechanism.
    explicit MatchIngressRecordBuffer(std::size_t capacity = MatchIngressRecordCapacity,
                                      std::size_t rejectionsPerWindow = MatchIngressRejectionsPerWindow)
        : capacity_(capacity), rejectionsPerWindow_(rejectionsPerWindow) {}

    // Whether the caller should build a rejection record for this refusal of
    // the bucket's player (player 0 for players the Match does not hold) and
    // Add it: false once the bucket's reason used its window, or when full.
    [[nodiscard]] bool AdmitRejection(PlayerId bucket, IngressInputRejection reason) {
        auto& used = window_[{bucket, reason}];
        if (used >= rejectionsPerWindow_) {
            ++drained_.suppressed;
            return false;
        }
        ++used;
        if (drained_.records.size() >= capacity_) {
            ++drained_.dropped;
            return false;
        }
        return true;
    }
    void Add(IngressRecord record) {
        if (drained_.records.size() >= capacity_) {
            ++drained_.dropped;
            return;
        }
        drained_.records.push_back(std::move(record));
    }
    // A new statistics window: every (player, reason) may keep records again.
    void NewWindow() { window_.clear(); }
    [[nodiscard]] IngressRecords Drain() { return std::exchange(drained_, {}); }

private:
    std::size_t capacity_;
    std::size_t rejectionsPerWindow_;
    std::map<std::pair<PlayerId, IngressInputRejection>, std::size_t> window_;
    IngressRecords drained_;
};

namespace detail {
inline std::int64_t SteadyNanoseconds(std::chrono::steady_clock::time_point value) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(value.time_since_epoch()).count();
}
template <class T>
void AppendJson(std::string& line, std::string_view name, const std::optional<T>& value) {
    line.append(",\"").append(name).append("\":");
    line.append(value ? std::to_string(*value) : std::string("null"));
}
template <class T>
void AppendJson(std::string& line, std::string_view name, T value) {
    line.append(",\"").append(name).append("\":").append(std::to_string(value));
}
inline void AppendJson(std::string& line, std::string_view name, std::string_view value) {
    line.append(",\"").append(name).append("\":\"").append(value).append("\"");
}
inline std::string MatchIngressHead(std::string_view kind) {
    return std::string("{\"schema\":\"object_fps_pvp.match_ingress\",\"version\":") +
        std::to_string(MatchIngressTraceVersion) + ",\"kind\":\"" + std::string(kind) + "\"";
}
} // namespace detail

// One line of match-ingress.jsonl, without the newline. Fields in README order.
[[nodiscard]] inline std::string MatchIngressRecordLine(const IngressRecord& record) {
    if (const auto* rejection = std::get_if<IngressRejectionRecord>(&record)) {
        auto line = detail::MatchIngressHead("rejection");
        detail::AppendJson(line, "time_ns", rejection->timeNs);
        detail::AppendJson(line, "player_id", rejection->playerId);
        detail::AppendJson(line, "reason", IngressName(rejection->reason));
        detail::AppendJson(line, "epoch", rejection->epoch);
        detail::AppendJson(line, "life", rejection->life);
        detail::AppendJson(line, "first_sequence", rejection->firstSequence);
        detail::AppendJson(line, "last_sequence", rejection->lastSequence);
        detail::AppendJson(line, "commands", rejection->commands);
        detail::AppendJson(line, "cursor", rejection->cursor);
        detail::AppendJson(line, "current_epoch", rejection->currentEpoch);
        detail::AppendJson(line, "current_life", rejection->currentLife);
        return line.append("}");
    }
    const auto& substitution = std::get<IngressSubstitution>(record);
    auto line = detail::MatchIngressHead("substitution");
    detail::AppendJson(line, "player_id", substitution.playerId);
    detail::AppendJson(line, "epoch", substitution.epoch);
    detail::AppendJson(line, "life", substitution.life);
    detail::AppendJson(line, "sequence", substitution.sequence);
    detail::AppendJson(line, "substituted_ns", detail::SteadyNanoseconds(substitution.substitutedAt));
    detail::AppendJson(line, "substituted_tick", substitution.substitutedTick);
    std::optional<std::int64_t> firstArrival;
    if (substitution.firstArrival) firstArrival = detail::SteadyNanoseconds(*substitution.firstArrival);
    detail::AppendJson(line, "first_arrival_ns", firstArrival);
    detail::AppendJson(line, "late_us", substitution.LateMicros());
    detail::AppendJson(line, "copies_after_first", substitution.copiesAfterFirst);
    detail::AppendJson(line, "reference_age_us", substitution.referenceAgeMicros);
    detail::AppendJson(line, "close", IngressName(substitution.close));
    return line.append("}");
}

[[nodiscard]] inline std::string MatchIngressTraceEndLine(std::uint64_t records, std::uint64_t suppressed,
                                                          std::uint64_t dropped) {
    auto line = detail::MatchIngressHead("trace_end");
    detail::AppendJson(line, "records", records);
    detail::AppendJson(line, "suppressed", suppressed);
    detail::AppendJson(line, "dropped", dropped);
    return line.append("}");
}

// The detail file's path for the Match's command line. Without a movement
// trace there is none (an explicit ingressTrace is then a usage error). The
// default sits next to the movement trace: "<x>-commands.jsonl" becomes
// "<x>-ingress.jsonl", any other name gains ".ingress.jsonl". A name ending in
// "commands.jsonl" is refused: the acceptance tools glob *commands.jsonl for
// movement traces. On a usage error the result is empty and error says why.
[[nodiscard]] inline std::filesystem::path MatchIngressTracePath(const std::filesystem::path& movementTrace,
                                                                 const std::filesystem::path& ingressTrace,
                                                                 std::string& error) {
    static constexpr std::string_view commands = "commands.jsonl";
    static constexpr std::string_view movementSuffix = "-commands.jsonl";
    if (movementTrace.empty()) {
        if (!ingressTrace.empty()) error = "--ingress-trace needs --movement-trace";
        return {};
    }
    auto path = ingressTrace;
    if (path.empty()) {
        const auto name = movementTrace.filename().string();
        path = movementTrace;
        path.replace_filename(name.ends_with(movementSuffix)
            ? name.substr(0, name.size() - movementSuffix.size()) + "-ingress.jsonl"
            : name + ".ingress.jsonl");
    }
    if (path.filename().string().ends_with(commands)) {
        error = "The ingress trace name must not end in commands.jsonl: " + path.string();
        return {};
    }
    if (path.lexically_normal() == movementTrace.lexically_normal()) {
        error = "The ingress trace must not be the movement trace: " + path.string();
        return {};
    }
    return path;
}

// Writes drained records to a stream and ends it with trace_end. Finish takes
// the last drain so that trace_end follows every record, and throws if any
// write failed (the Match then exits 1); suppressed and dropped records are
// counted in trace_end, never failures.
class MatchIngressTraceWriter final {
public:
    explicit MatchIngressTraceWriter(std::ostream& output) : output_(output) {}
    MatchIngressTraceWriter(const MatchIngressTraceWriter&) = delete;
    MatchIngressTraceWriter& operator=(const MatchIngressTraceWriter&) = delete;

    void Write(const IngressRecords& drained) {
        if (finished_) throw std::logic_error("The ingress trace already ended");
        for (const auto& record : drained.records) {
            output_ << MatchIngressRecordLine(record) << '\n';
            ++records_;
        }
        suppressed_ += drained.suppressed;
        dropped_ += drained.dropped;
        output_.flush();
        if (!output_) failed_ = true;
    }
    void Finish(const IngressRecords& last) {
        Write(last);
        output_ << MatchIngressTraceEndLine(records_, suppressed_, dropped_) << '\n';
        output_.flush();
        if (!output_) failed_ = true;
        finished_ = true;
        if (failed_) throw std::runtime_error("Ingress trace failed to write");
    }
    // False once any write failed.
    [[nodiscard]] bool Good() const noexcept { return !failed_ && static_cast<bool>(output_); }

private:
    std::ostream& output_;
    std::uint64_t records_{};
    std::uint64_t suppressed_{};
    std::uint64_t dropped_{};
    bool failed_{};
    bool finished_{};
};

// The Match's file: opening and writing fail loudly (the Match then exits 1).
class MatchIngressTraceFile final {
public:
    explicit MatchIngressTraceFile(const std::filesystem::path& path) : file_(path), writer_(file_) {
        if (!file_) throw std::runtime_error("Cannot open ingress trace: " + path.string());
    }
    void Write(const IngressRecords& drained) { writer_.Write(drained); }
    void Finish(const IngressRecords& last) {
        writer_.Finish(last);
        file_.close();
        if (!file_) throw std::runtime_error("Ingress trace failed to close");
    }

private:
    std::ofstream file_;
    MatchIngressTraceWriter writer_;
};

} // namespace fps::pvp
