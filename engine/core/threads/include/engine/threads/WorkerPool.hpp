#pragma once

#include "engine/base/Result.hpp"
#include "engine/threads/RoleThread.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace GYO::Engine::Threads {

struct WorkerPoolOptions final {
    // Each thread is named "<name> (<index>)"; non-empty.
    std::string name;
    // At least one.
    std::size_t threadCount{1};
    ThreadPriority priority{ThreadPriority::Normal};
};

// A fixed number of threads taking jobs in the order they were submitted (docs/architecture/threads.md). Whoever
// creates it owns it: there is no shared or global pool (Design Invariant 12). No futures, priorities, work
// stealing or scopes. A job must not let an exception escape (as a RoleThread body, Design Invariant 8). Stop and
// the destructor are not called from one of the pool's own threads, nor from two threads at once.
class WorkerPool final {
public:
    using Job = std::function<void()>;

    enum class StopMode : std::uint8_t {
        // Runs the jobs already queued, then stops.
        Drain,
        // Drops the jobs not started; a job already running finishes.
        Discard,
    };

    [[nodiscard]] static Base::Result<WorkerPool, ThreadError> Start(WorkerPoolOptions options);

    WorkerPool(WorkerPool&& other) noexcept;
    WorkerPool& operator=(WorkerPool&&) = delete;
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    // Stop(StopMode::Discard).
    ~WorkerPool();

    // Any thread: queues the job and returns true; after Stop, takes nothing and returns false (Design
    // Invariant 10). On a moved-from pool, false.
    [[nodiscard]] bool Submit(Job job);
    // Returns once no job is running and every thread has returned (Design Invariant 11). Idempotent.
    void Stop(StopMode mode);

    [[nodiscard]] std::size_t ThreadCount() const;
    [[nodiscard]] const std::string& ThreadName(std::size_t index) const;

private:
    struct State;
    WorkerPool(std::shared_ptr<State> state, std::vector<RoleThread> workers) noexcept;
    std::shared_ptr<State> state_;
    std::vector<RoleThread> workers_;
};

} // namespace GYO::Engine::Threads
