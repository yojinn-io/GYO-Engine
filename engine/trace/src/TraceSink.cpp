#include "engine/trace/TraceSink.hpp"

#include "engine/base/Assert.hpp"
#include "engine/os/Files.hpp"
#include "engine/threads/RoleThread.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace Engine::Trace {

namespace {

// The writer hands its bytes to the file at the end of a batch, or once this
// many are waiting (os-subsystem D58: a C++ stream wrote at its own buffer's
// size; the bytes in the file are the same).
constexpr std::size_t kWriteChunk = std::size_t{64} * 1024;

} // namespace

struct TraceSink::Impl final {
    // The file through engine/os, in the platform's line endings ("\r\n" on
    // Windows), as the text-mode C++ stream it replaced wrote it.
    std::optional<Os::Files::FileWriter> file;
    // Bytes waiting to be written; the writer thread's alone, then the owner's after the join.
    std::string pending;
    std::size_t capacity{};
    bool flushEachLine{};

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

    // Hands the waiting bytes to the file in one write. After a failure
    // nothing more is written, as a failed stream wrote nothing more.
    void WritePending() {
        if (pending.empty()) return;
        if (!failed.load(std::memory_order_acquire)) {
            const auto bytes = std::as_bytes(std::span(pending.data(), pending.size()));
            if (!file->Write(bytes)) failed.store(true, std::memory_order_release);
        }
        pending.clear();
        // A very long line leaves no large buffer behind, as the stream's fixed buffer did not.
        if (pending.capacity() > 4 * kWriteChunk) std::string().swap(pending);
    }

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
            for (const auto& line : batch) {
                pending += line;
                pending += '\n';
                if (flushEachLine || pending.size() >= kWriteChunk) WritePending();
            }
            WritePending();
            {
                std::scoped_lock lock(progressMutex);
                written += batch.size();
                flushedThrough = through;
            }
            progressChanged.notify_all();
            batch.clear();
            if (stopping) break;
            // Only a Push into an empty queue, FlushNow or the stop wakes it; a
            // notify latched while writing makes this return at once. Never a
            // timer (docs/architecture/trace.md, Design Invariant 2).
            const Time::Wake wake = context.Wait();
            GYO_ASSERT(wake.reason == Time::WakeReason::Notified);
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
    impl->flushEachLine = options.flushEachLine;
    // Engine paths are UTF-8 (docs/architecture/os.md), not path::string()'s ANSI code page.
    auto path = Os::Files::PathToUtf8(options.path);
    if (!path)
        return Base::Err(TraceError::Make(TraceErrorCode::OpenFailed, "Cannot open trace file",
                                          Base::CauseDetail(path.error())));
    auto file = Os::Files::FileWriter::Open(
        path.value(), options.append ? Os::Files::WriteMode::Append : Os::Files::WriteMode::Truncate,
        Os::Files::LineEndings::Native);
    if (!file)
        return Base::Err(TraceError::Make(TraceErrorCode::OpenFailed, "Cannot open trace file",
                                          Base::CauseDetail(file.error(), path.value())));
    impl->file.emplace(std::move(file.value()));
    auto* state = impl.get();
    auto writer = Threads::RoleThread::Start({.name = std::move(options.threadName)},
                                             [state](Threads::RoleContext& context) { state->RunWriter(context); });
    if (!writer)
        return Base::Err(TraceError::Make(TraceErrorCode::StartFailed, "Cannot start the trace writer",
                                          Base::CauseDetail(writer.error(), path.value())));
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
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    // try_lock: a crash path may run on a thread that already holds the queue
    // (an exception escaping a Push into terminate); waiting would hang it.
    std::unique_lock queue(impl_->queueMutex, std::try_to_lock);
    while (!queue.owns_lock()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
        static_cast<void>(queue.try_lock());
    }
    if (impl_->closed) return false;
    const std::uint64_t target = impl_->accepted;
    queue.unlock();
    impl_->writer->Notify();
    std::unique_lock lock(impl_->progressMutex);
    impl_->progressChanged.wait_until(lock, deadline,
        [&] { return impl_->flushedThrough >= target || impl_->writerDone; });
    return impl_->flushedThrough >= target && !impl_->failed.load(std::memory_order_acquire);
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
    if (endLine) {
        if (const auto line = endLine(counts)) {
            impl_->pending += *line;
            impl_->pending += '\n';
        }
    }
    impl_->WritePending();
    if (!impl_->file->Close()) impl_->failed.store(true, std::memory_order_release);
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
