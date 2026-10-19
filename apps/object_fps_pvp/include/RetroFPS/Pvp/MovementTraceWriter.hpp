#pragma once

#include "RetroFPS/Pvp/MovementTrace.hpp"

#include "engine/base/Error.hpp"
#include "engine/trace/TraceSink.hpp"

#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fps::pvp {

// One line of the movement trace (schema version 2), without the newline.
[[nodiscard]] inline std::string MovementTraceLine(const MovementTraceEvent& e) {
    std::ostringstream line;
    line << std::setprecision(17);
    line << "{\"schema_version\":2,\"kind\":\"" << TraceKindName(e.kind)
         << "\",\"time_ns\":" << e.timeNs
         << ",\"player_id\":" << e.playerId
         << ",\"life_generation\":" << e.lifeGeneration
         << ",\"epoch\":" << e.epoch
         << ",\"sequence\":" << e.sequence
         << ",\"authority_tick\":" << e.authorityTick
         << ",\"source\":\"" << TraceSourceName(e.source)
         << "\",\"queued\":" << e.queued
         << ",\"pending\":" << e.pending
         << ",\"seeded_neutral\":" << (e.seededNeutral ? "true" : "false")
         << ",\"dropped_seconds\":" << e.droppedSeconds
         << ",\"frame_seconds\":" << e.frameSeconds
         << ",\"count\":" << e.count
         << ",\"age_seconds\":" << e.ageSeconds
         << ",\"reset_reason\":\"" << TraceResetReasonName(e.resetReason)
         << "\",\"started_ns\":" << e.startedNs << '}';
    return line.str();
}

// The closing line: events written and events lost (a full queue, or a
// producer still recording after Finish).
[[nodiscard]] inline std::string MovementTraceEndLine(const Engine::Trace::TraceCounts& counts) {
    return "{\"schema_version\":2,\"kind\":\"trace_end\",\"events\":" + std::to_string(counts.written) +
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
