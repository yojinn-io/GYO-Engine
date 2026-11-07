#pragma once

// v8-product-pm-detect-diag: optional movement diagnostics, one JSON line per
// movement step of the Match (every resolved step) or of a Client (every first
// prediction, not the replays). Diagnostics only: nothing reads them back, and
// they enter no digest and no wire. A producer copies a small event into a
// bounded queue; this writer's own thread formats the lines, and the Engine
// trace sink's thread writes the file, so the Match's authority lock only
// holds the copy.
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"

#include "engine/base/Error.hpp"
#include "engine/os/Files.hpp"
#include "engine/trace/TraceSink.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fps::pvp {

enum class MovementDiagRole : std::uint8_t { Match, Client };

struct MovementDiagEvent final {
    MovementDiagRole role{};
    std::int64_t timeNs{};
    std::uint64_t playerId{};
    std::uint64_t lifeGeneration{};
    std::uint64_t epoch{};
    std::uint64_t sequence{};
    std::uint64_t authorityTick{}; // the Match's; 0 on a Client
    bool grounded{};               // after the step
    float verticalVelocity{};      // after the step
    bool crouched{};               // after the step
    MovementStepDiagnostics step{};
};

inline constexpr int MovementDiagVersion = 1;

[[nodiscard]] inline std::string MovementDiagLine(const MovementDiagEvent& e) {
    std::string line;
    line.reserve(512);
    const auto number = [&](const char* key, double value) {
        line.append(",\"").append(key).append("\":");
        detail::AppendTraceDouble(line, value);
    };
    const auto integer = [&](const char* key, std::uint64_t value) {
        line.append(",\"").append(key).append("\":").append(std::to_string(value));
    };
    const auto flag = [&](const char* key, bool value) {
        line.append(",\"").append(key).append("\":").append(value ? "true" : "false");
    };
    const auto vec3 = [&](const char* key, const Engine::Math::Vec3& v) {
        line.append(",\"").append(key).append("\":[");
        detail::AppendTraceDouble(line, v.x);
        line.push_back(',');
        detail::AppendTraceDouble(line, v.y);
        line.push_back(',');
        detail::AppendTraceDouble(line, v.z);
        line.push_back(']');
    };
    line.append("{\"schema\":\"gyo.pvp.movement_diag\",\"version\":").append(std::to_string(MovementDiagVersion));
    line.append(",\"kind\":\"step\",\"role\":\"").append(e.role == MovementDiagRole::Match ? "match" : "client").append("\"");
    line.append(",\"time_ns\":").append(std::to_string(e.timeNs));
    integer("player_id", e.playerId);
    integer("life_generation", e.lifeGeneration);
    integer("epoch", e.epoch);
    integer("sequence", e.sequence);
    integer("authority_tick", e.authorityTick);
    vec3("before", e.step.before);
    flag("grounded_before", e.step.groundedBefore);
    flag("supported_before", e.step.supportedBefore);
    number("desired_y", e.step.desiredY);
    vec3("after", e.step.after);
    flag("grounded", e.grounded);
    number("vertical_velocity", e.verticalVelocity);
    flag("crouched", e.crouched);
    integer("penetration_pushes", static_cast<std::uint64_t>(e.step.move.penetrationPushes));
    vec3("deepest_normal", e.step.move.deepestNormal);
    number("deepest_depth", e.step.move.deepestDepth);
    integer("sweep_hits", static_cast<std::uint64_t>(e.step.move.sweepHits));
    vec3("sweep_normal", e.step.move.sweepNormal);
    number("sweep_fraction", e.step.move.sweepFraction);
    line.push_back('}');
    return line;
}

// The writer. Open it before the producers start; Finish after they stop.
class MovementDiagWriter final {
public:
    static constexpr std::size_t Capacity = 65536;

    explicit MovementDiagWriter(const std::filesystem::path& path) {
        auto sink = Engine::Trace::TraceSink::Open(
            {.path = path, .capacity = Capacity, .threadName = "pvp-movement-diag"});
        if (!sink) throw std::runtime_error("Cannot open movement diagnostics: " + Engine::Os::Files::PathForMessage(path) +
                                            " (" + Engine::Base::Describe(sink.error()) + ")");
        sink_ = std::make_unique<Engine::Trace::TraceSink>(std::move(*sink));
        thread_ = std::thread([this] { Run(); });
    }
    ~MovementDiagWriter() { Finish(); }
    MovementDiagWriter(const MovementDiagWriter&) = delete;
    MovementDiagWriter& operator=(const MovementDiagWriter&) = delete;

    // Any thread: a copy into the queue, or a count when it is full or closed.
    void Record(const MovementDiagEvent& event) {
        {
            std::lock_guard lock(mutex_);
            if (closed_ || queue_.size() >= Capacity) {
                ++dropped_;
                return;
            }
            queue_.push_back(event);
        }
        wake_.notify_one();
    }

    // Writes what is queued, then diag_end with the lines written and the
    // events lost (a full queue here or in the sink). Idempotent.
    void Finish() {
        {
            std::lock_guard lock(mutex_);
            if (closed_) return;
            closed_ = true;
        }
        wake_.notify_one();
        if (thread_.joinable()) thread_.join();
        std::uint64_t dropped{};
        {
            std::lock_guard lock(mutex_);
            dropped = dropped_;
        }
        static_cast<void>(sink_->Close([dropped](const Engine::Trace::TraceCounts& counts) {
            return std::optional<std::string>(
                "{\"schema\":\"gyo.pvp.movement_diag\",\"version\":" + std::to_string(MovementDiagVersion) +
                ",\"kind\":\"diag_end\",\"records\":" + std::to_string(counts.written) +
                ",\"dropped\":" + std::to_string(dropped + counts.Dropped()) + "}");
        }));
    }

private:
    void Run() {
        std::vector<MovementDiagEvent> batch;
        for (;;) {
            bool closing{};
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [&] { return closed_ || !queue_.empty(); });
                batch.swap(queue_);
                closing = closed_;
            }
            for (const auto& event : batch) static_cast<void>(sink_->Push(MovementDiagLine(event)));
            batch.clear();
            if (closing) {
                std::lock_guard lock(mutex_);
                if (queue_.empty()) return;
            }
        }
    }

    std::unique_ptr<Engine::Trace::TraceSink> sink_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<MovementDiagEvent> queue_;
    std::uint64_t dropped_{};
    bool closed_{};
    std::thread thread_;
};

// One optional writer per product process (installed before the producers
// start, removed before Finish). enabled is checked first so that a process
// without diagnostics pays one relaxed load per step.
inline std::atomic<bool> movementDiagEnabled{false};
inline std::mutex movementDiagMutex;
inline std::shared_ptr<MovementDiagWriter> movementDiagWriter;

inline void SetMovementDiag(std::shared_ptr<MovementDiagWriter> writer) noexcept {
    std::shared_ptr<MovementDiagWriter> previous;
    {
        std::lock_guard lock(movementDiagMutex);
        previous = std::exchange(movementDiagWriter, std::move(writer));
        movementDiagEnabled.store(movementDiagWriter != nullptr, std::memory_order_relaxed);
    }
}
[[nodiscard]] inline bool MovementDiagEnabled() noexcept { return movementDiagEnabled.load(std::memory_order_relaxed); }
inline void RecordMovementDiag(MovementDiagEvent event) {
    std::shared_ptr<MovementDiagWriter> writer;
    {
        std::lock_guard lock(movementDiagMutex);
        writer = movementDiagWriter;
    }
    if (!writer) return;
    if (!event.timeNs) event.timeNs = MovementTraceNowNs();
    writer->Record(event);
}

// The process's diagnostics file for its lifetime: installed when path is not
// empty, removed and finished on destruction. Declare it before the objects
// that record (they stop first).
class MovementDiagFile final {
public:
    explicit MovementDiagFile(const std::filesystem::path& path) {
        if (path.empty()) return;
        writer_ = std::make_shared<MovementDiagWriter>(path);
        SetMovementDiag(writer_);
    }
    ~MovementDiagFile() {
        if (!writer_) return;
        SetMovementDiag({});
        writer_->Finish();
    }
    MovementDiagFile(const MovementDiagFile&) = delete;
    MovementDiagFile& operator=(const MovementDiagFile&) = delete;
private:
    std::shared_ptr<MovementDiagWriter> writer_;
};

} // namespace fps::pvp
