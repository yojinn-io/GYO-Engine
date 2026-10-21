#pragma once

#include "RetroFPS/Pvp/MovementTrace.hpp"

#include "engine/base/Error.hpp"
#include "engine/trace/TraceSink.hpp"

#include <filesystem>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace fps::pvp {

namespace detail {
// What std::ostream with setprecision(17) writes for a double (%.17g), without
// building a stream: MovementTraceLine runs on the producer's thread, some of
// them under the Match's authority lock.
inline void AppendTraceDouble(std::string& line, double value) {
    char text[32];
    const int length = std::snprintf(text, sizeof text, "%.17g", value);
    line.append(text, static_cast<std::size_t>(length > 0 ? length : 0));
}
} // namespace detail

// One line of the movement trace (schema version 3, v7.1 D152: version 2 plus
// the phase_decision kind and its four keys), without the newline.
[[nodiscard]] inline std::string MovementTraceLine(const MovementTraceEvent& e) {
    std::string line;
    line.reserve(420);
    line.append("{\"schema_version\":3,\"kind\":\"").append(TraceKindName(e.kind));
    line.append("\",\"time_ns\":").append(std::to_string(e.timeNs));
    line.append(",\"player_id\":").append(std::to_string(e.playerId));
    line.append(",\"life_generation\":").append(std::to_string(e.lifeGeneration));
    line.append(",\"epoch\":").append(std::to_string(e.epoch));
    line.append(",\"sequence\":").append(std::to_string(e.sequence));
    line.append(",\"authority_tick\":").append(std::to_string(e.authorityTick));
    line.append(",\"source\":\"").append(TraceSourceName(e.source));
    line.append("\",\"queued\":").append(std::to_string(e.queued));
    line.append(",\"pending\":").append(std::to_string(e.pending));
    line.append(",\"seeded_neutral\":").append(e.seededNeutral ? "true" : "false");
    line.append(",\"dropped_seconds\":");
    detail::AppendTraceDouble(line, e.droppedSeconds);
    line.append(",\"frame_seconds\":");
    detail::AppendTraceDouble(line, e.frameSeconds);
    line.append(",\"count\":").append(std::to_string(e.count));
    line.append(",\"age_seconds\":");
    detail::AppendTraceDouble(line, e.ageSeconds);
    line.append(",\"reset_reason\":\"").append(TraceResetReasonName(e.resetReason));
    line.append("\",\"started_ns\":").append(std::to_string(e.startedNs));
    if (e.kind == MovementTraceKind::PhaseDecision) {
        line.append(",\"phase_path\":\"").append(TracePhasePathName(e.phasePath));
        line.append("\",\"phase_error_seconds\":");
        detail::AppendTraceDouble(line, e.phaseErrorSeconds);
        line.append(",\"phase_shift_seconds\":");
        detail::AppendTraceDouble(line, e.phaseShiftSeconds);
        line.append(",\"phase_deadband_seconds\":");
        detail::AppendTraceDouble(line, e.phaseDeadbandSeconds);
    }
    line.append("}");
    return line;
}

// The closing line: events written and events lost (a full queue, or a
// producer still recording after Finish).
[[nodiscard]] inline std::string MovementTraceEndLine(const Engine::Trace::TraceCounts& counts) {
    return "{\"schema_version\":3,\"kind\":\"trace_end\",\"events\":" + std::to_string(counts.written) +
           ",\"dropped\":" + std::to_string(counts.Dropped()) + "}";
}

// Optional product diagnostic export. Simulation, render and network
// producers serialize their event and hand the line to the Engine trace sink;
// its writer thread owns all file I/O.
class MovementTraceWriter final {
public:
    explicit MovementTraceWriter(const std::filesystem::path& path) {
        if (path.empty()) return;
        auto sink = Engine::Trace::TraceSink::Open(
            {.path = path, .capacity = MovementTrace::Capacity, .threadName = "pvp-movement-trace"});
        if (!sink) throw std::runtime_error("Cannot open movement trace: " + path.string() + " (" +
                                            Engine::Base::Describe(sink.error()) + ")");
        file_ = std::make_shared<File>(std::move(*sink));
        SetMovementTrace(file_);
    }
    ~MovementTraceWriter() { Finish(); }
    MovementTraceWriter(const MovementTraceWriter&) = delete;
    MovementTraceWriter& operator=(const MovementTraceWriter&) = delete;
    // Stops the trace and ends the file with trace_end. Producers should be
    // stopped first; an event recorded after this is counted as lost.
    void Finish() {
        if (!file_) return;
        SetMovementTrace({});
        static_cast<void>(file_->sink.Close(MovementTraceEndLine));
    }
    // No write failed and no event was lost.
    [[nodiscard]] bool Good() const {
        return !file_ || (file_->sink.Good() && file_->sink.Counts().Dropped() == 0);
    }
private:
    struct File final : MovementTraceSink {
        explicit File(Engine::Trace::TraceSink traceSink) : sink(std::move(traceSink)) {}
        void Record(const MovementTraceEvent& event) override { static_cast<void>(sink.Push(MovementTraceLine(event))); }
        Engine::Trace::TraceSink sink;
    };
    std::shared_ptr<File> file_;
};
} // namespace fps::pvp
