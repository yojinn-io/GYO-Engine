#include "engine/threads/WorkerPool.hpp"

#include "engine/base/Assert.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>

namespace GYO::Engine::Threads {

struct WorkerPool::State final {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Job> queue;
    bool stopping{false};
};

namespace {

// A worker returns once the pool is stopping and nothing is left to take; Drain leaves the queue to be emptied
// first, Discard has already emptied it.
void RunWorker(std::mutex& mutex, std::condition_variable& wake, std::deque<WorkerPool::Job>& queue,
               const bool& stopping) {
    while (true) {
        WorkerPool::Job job;
        {
            std::unique_lock lock(mutex);
            wake.wait(lock, [&] { return stopping || !queue.empty(); });
            if (queue.empty()) return;
            job = std::move(queue.front());
            queue.pop_front();
        }
        job();
    }
}

} // namespace

Base::Result<WorkerPool, ThreadError> WorkerPool::Start(WorkerPoolOptions options) {
    GYO_ASSERT(!options.name.empty());
    GYO_ASSERT(options.threadCount >= 1);
    auto state = std::make_shared<State>();
    std::vector<RoleThread> workers;
    workers.reserve(options.threadCount);
    for (std::size_t index = 0; index < options.threadCount; ++index) {
        auto worker = RoleThread::Start(
            {.name = options.name + " (" + std::to_string(index) + ")", .priority = options.priority},
            [state](RoleContext&) { RunWorker(state->mutex, state->wake, state->queue, state->stopping); });
        if (!worker) {
            // The started workers wait on the pool, not on their RoleThread stop: release them before they join.
            {
                std::lock_guard lock(state->mutex);
                state->stopping = true;
            }
            state->wake.notify_all();
            return Base::Err(std::move(worker.error()));
        }
        workers.push_back(std::move(*worker));
    }
    return WorkerPool(std::move(state), std::move(workers));
}

WorkerPool::WorkerPool(std::shared_ptr<State> state, std::vector<RoleThread> workers) noexcept
    : state_(std::move(state)), workers_(std::move(workers)) {}

WorkerPool::WorkerPool(WorkerPool&& other) noexcept = default;

WorkerPool::~WorkerPool() { Stop(StopMode::Discard); }

bool WorkerPool::Submit(Job job) {
    GYO_ASSERT(static_cast<bool>(job));
    if (!state_) return false;
    {
        std::lock_guard lock(state_->mutex);
        if (state_->stopping) return false;
        state_->queue.push_back(std::move(job));
    }
    state_->wake.notify_one();
    return true;
}

void WorkerPool::Stop(const StopMode mode) {
    if (!state_) return;
    std::deque<Job> dropped;
    {
        std::lock_guard lock(state_->mutex);
        state_->stopping = true;
        if (mode == StopMode::Discard) dropped.swap(state_->queue);
    }
    state_->wake.notify_all();
    for (auto& worker : workers_) worker.Join();
    // The dropped jobs are destroyed here, outside the lock: their captures may be large or take locks.
}

std::size_t WorkerPool::ThreadCount() const { return workers_.size(); }

const std::string& WorkerPool::ThreadName(const std::size_t index) const {
    GYO_ASSERT(index < workers_.size());
    return workers_[index].Name();
}

} // namespace GYO::Engine::Threads
