#include "engine/trace/TraceSink.hpp"

#include "engine/base/Assert.hpp"
#include "engine/threads/RoleThread.hpp"

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace Engine::Trace {

struct TraceSink::Impl final {
    std::ofstream file;
    std::size_t capacity{};

    // The queue side, shared with every producer.
    mutable std::mutex queueMutex;
    std::vector<std::string> queue;
    bool closed{};
    std::uint64_t accepted{};
    std::uint64_t droppedFull{};
    std::uint64_t droppedClosed{};

    // The writer's progress, for FlushNow and Counts.
    mutable std::mutex progressMutex;
    std::condition_variable progressChanged;
    std::uint64_t written{};
    // Every accepted line up to this count is written and flushed.
    std::uint64_t flushedThrough{};
    bool writerDone{};
    std::thread::id writerId;

    std::atomic<bool> failed{};
    std::optional<Threads::RoleThread> writer;

    // Owner only.
    bool finished{};
    TraceCounts closeCounts;

    void RunWriter(Threads::RoleContext& context) {
        {
            std::scoped_lock lock(progressMutex);
            writerId = std::this_thread::get_id();
        }
        std::vector<std::string> batch;
        for (;;) {
            // Read before taking the queue: Close marks the queue closed before
            // it requests the stop (release/acquire), so once a stop is seen,
            // this take holds every line that will ever be accepted.
            const bool stopping = context.StopRequested();
            std::uint64_t through{};
            {
                std::scoped_lock lock(queueMutex);
                batch.swap(queue);
                through = accepted;
            }
            for (const auto& line : batch) file << line << '\n';
            if (!batch.empty()) file.flush();
            if (!file) failed.store(true, std::memory_order_release);
            {
                std::scoped_lock lock(progressMutex);
                written += batch.size();
                flushedThrough = through;
            }
            progressChanged.notify_all();
            batch.clear();
            if (stopping) break;
            // Only a Push into an empty queue, FlushNow or the stop wakes it; a
            // notify latched while writing makes this return at once.
            static_cast<void>(context.Wait());
        }
        {
            std::scoped_lock lock(progressMutex);
            writerDone = true;
        }
        progressChanged.notify_all();
    }

    [[nodiscard]] bool OnWriterThread() const {
        std::scoped_lock lock(progressMutex);
        return writerId == std::this_thread::get_id();
    }
};

Base::Result<TraceSink, TraceError> TraceSink::Open(TraceSinkOptions options) {
    GYO_ASSERT(options.capacity > 0);
    GYO_ASSERT(!options.threadName.empty());
    auto impl = std::make_unique<Impl>();
    impl->capacity = options.capacity;
    impl->file.open(options.path, options.append ? std::ios::out | std::ios::app : std::ios::out | std::ios::trunc);
    if (!impl->file)
        return Base::Err(TraceError::Make(TraceErrorCode::OpenFailed, "Cannot open trace file", options.path.string()));
    auto* state = impl.get();
    auto writer = Threads::RoleThread::Start({.name = std::move(options.threadName)},
                                             [state](Threads::RoleContext& context) { state->RunWriter(context); });
    if (!writer)
        return Base::Err(TraceError::Make(TraceErrorCode::StartFailed, "Cannot start the trace writer",
                                          Base::CauseDetail(writer.error(), options.path.string())));
    impl->writer.emplace(std::move(*writer));
    return TraceSink(std::move(impl));
}

TraceSink::TraceSink(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
TraceSink::TraceSink(TraceSink&& other) noexcept = default;

TraceSink::~TraceSink() {
    if (impl_) static_cast<void>(Close());
}

bool TraceSink::Push(std::string line) {
    GYO_ASSERT(impl_);
    bool wake{};
    {
        std::scoped_lock lock(impl_->queueMutex);
        if (impl_->closed) {
            ++impl_->droppedClosed;
            return false;
        }
        if (impl_->queue.size() >= impl_->capacity) {
            ++impl_->droppedFull;
            return false;
        }
        wake = impl_->queue.empty();
        impl_->queue.push_back(std::move(line));
        ++impl_->accepted;
    }
    // A non-empty queue already has a wake pending since the writer last took it.
    if (wake) impl_->writer->Notify();
    return true;
}

bool TraceSink::FlushNow(const std::chrono::nanoseconds timeout) {
    GYO_ASSERT(impl_);
    if (impl_->OnWriterThread()) return false;
    std::uint64_t target{};
    {
        std::scoped_lock lock(impl_->queueMutex);
        if (impl_->closed) return false;
        target = impl_->accepted;
    }
    impl_->writer->Notify();
    std::unique_lock lock(impl_->progressMutex);
    impl_->progressChanged.wait_for(lock, timeout,
        [&] { return impl_->flushedThrough >= target || impl_->writerDone; });
    return impl_->flushedThrough >= target;
}

TraceCounts TraceSink::Close(const EndLine& endLine) {
    GYO_ASSERT(impl_);
    if (impl_->finished) return impl_->closeCounts;
    GYO_ASSERT(!impl_->OnWriterThread());
    {
        std::scoped_lock lock(impl_->queueMutex);
        impl_->closed = true;
    }
    impl_->writer->RequestStop();
    impl_->writer->Join();
    // The writer has returned: the file is the owner's alone from here.
    const TraceCounts counts = Counts();
    if (endLine)
        if (const auto line = endLine(counts)) impl_->file << *line << '\n';
    impl_->file.flush();
    if (!impl_->file) impl_->failed.store(true, std::memory_order_release);
    impl_->file.close();
    if (impl_->file.fail()) impl_->failed.store(true, std::memory_order_release);
    impl_->finished = true;
    impl_->closeCounts = counts;
    return counts;
}

bool TraceSink::Good() const {
    GYO_ASSERT(impl_);
    return !impl_->failed.load(std::memory_order_acquire);
}

TraceCounts TraceSink::Counts() const {
    GYO_ASSERT(impl_);
    TraceCounts counts;
    {
        std::scoped_lock lock(impl_->queueMutex);
        counts.droppedFull = impl_->droppedFull;
        counts.droppedClosed = impl_->droppedClosed;
    }
    std::scoped_lock lock(impl_->progressMutex);
    counts.written = impl_->written;
    return counts;
}

} // namespace Engine::Trace
