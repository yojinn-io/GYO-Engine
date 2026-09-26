#pragma once

#include "RetroFPS/Pvp/MovementTrace.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <thread>

namespace fps::pvp {

// Optional product diagnostic export. Simulation/render/network producers only
// append to the bounded memory sink; this worker owns all file I/O.
class MovementTraceWriter final {
public:
    explicit MovementTraceWriter(const std::filesystem::path& path) {
        if (path.empty()) return;
        output_.open(path);
        if (!output_) throw std::runtime_error("Cannot open movement trace: " + path.string());
        output_ << std::setprecision(17);
        trace_ = std::make_shared<MovementTrace>();
        SetMovementTrace(trace_);
        worker_ = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                WriteEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            WriteEvents();
            output_ << "{\"schema_version\":2,\"kind\":\"trace_end\",\"events\":" << written_
                    << ",\"dropped\":" << trace_->Dropped() << "}\n";
            output_.flush();
            if (!output_) failed_.store(true);
        });
    }
    ~MovementTraceWriter() { Finish(); }
    MovementTraceWriter(const MovementTraceWriter&) = delete;
    MovementTraceWriter& operator=(const MovementTraceWriter&) = delete;
    void Finish() {
        if (!trace_) return;
        SetMovementTrace({});
        if (worker_.joinable()) { worker_.request_stop(); worker_.join(); }
    }
    [[nodiscard]] bool Good() const {
        return !failed_.load() && (!trace_ || trace_->Dropped() == 0);
    }
private:
    void WriteEvents() {
        for (const auto& e : trace_->Drain()) {
            output_ << "{\"schema_version\":2,\"kind\":\"" << TraceKindName(e.kind)
                    << "\",\"time_ns\":" << e.timeNs
                    << ",\"player_id\":" << e.playerId
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
                    << "\",\"started_ns\":" << e.startedNs << "}\n";
            ++written_;
        }
        if (!output_) failed_.store(true);
    }
    std::ofstream output_;
    std::shared_ptr<MovementTrace> trace_;
    std::jthread worker_;
    std::atomic<bool> failed_{};
    std::uint64_t written_{};
};
} // namespace fps::pvp
