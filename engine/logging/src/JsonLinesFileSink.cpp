#include "engine/logging/JsonLinesFileSink.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/os/Files.hpp"
#include "engine/threads/RoleThread.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace GYO::Engine::Logging {

bool LineOutput::Close() noexcept {
    return true;
}

namespace {

namespace Os = ::Engine::Os;
namespace Threads = ::Engine::Threads;

// The writer hands its bytes to the output at the end of a batch, or once this
// many are waiting, always at the end of a line (master TraceSink).
constexpr std::size_t kWriteChunk = std::size_t{64} * 1024;

// The file through engine/os, opened to append, the bytes as written on every
// platform (golden lines are the same bytes everywhere, declaration 5.12).
class FileOutput final : public LineOutput {
public:
    explicit FileOutput(Os::Files::FileWriter writer) noexcept : writer_(std::move(writer)) {}
    bool Write(const std::string_view bytes) noexcept override {
        if (closed_) return false;
        try {
            return static_cast<bool>(writer_.Write(std::as_bytes(std::span(bytes.data(), bytes.size()))));
        } catch (...) {
            return false;
        }
    }
    bool Close() noexcept override {
        if (closed_) return true;
        closed_ = true;
        try {
            return static_cast<bool>(writer_.Close());
        } catch (...) {
            return false;
        }
    }

private:
    Os::Files::FileWriter writer_;
    bool closed_ = false;
};

LoggingResult<std::shared_ptr<LineOutput>> OpenAppend(const std::string& path) {
    auto writer = Os::Files::FileWriter::Open(path, Os::Files::WriteMode::Append, Os::Files::LineEndings::AsWritten);
    if (!writer) {
        return ::Engine::Base::Err(LoggingError::Make(LoggingErrorCode::OpenFailed, "Cannot open the log file",
                                                      ::Engine::Base::CauseDetail(writer.error(), path)));
    }
    return std::shared_ptr<LineOutput>(std::make_shared<FileOutput>(std::move(writer.value())));
}

} // namespace

struct JsonLinesFileSink::Impl final {
    std::shared_ptr<LineOutput> output;
    std::shared_ptr<LineOutput> fatalOutput;
    std::int64_t (*monoNs)() noexcept = nullptr;
    // Bytes waiting to be written; the writer thread's alone, then the owner's after the join.
    std::string pending;
    std::size_t capacity{};

    // The queue side, shared with every thread that writes a record.
    mutable std::mutex queueMutex;
    std::vector<std::string> queue;
    bool closed{};
    std::uint64_t accepted{};
    std::uint64_t droppedFull{};
    std::uint64_t droppedClosed{};

    // The writer's progress, for Flush and Counts.
    mutable std::mutex progressMutex;
    std::condition_variable progressChanged;
    std::uint64_t written{};
    // Every accepted line up to this count has been handed on.
    std::uint64_t flushedThrough{};
    bool writerDone{};
    std::thread::id writerId;

    std::atomic<bool> failed{};
    std::atomic<std::uint64_t> replaced{};
    std::atomic<std::uint64_t> fatalDirect{};

    // The fatal output: tried, never waited for.
    std::mutex fatalMutex;
    std::atomic<bool> fatalClosed{};

    std::optional<Threads::RoleThread> writer;

    // Owner only.
    bool finished{};
    JsonLinesCounts closeCounts;

    // Hands the waiting bytes on in one write. After a failure nothing more is written.
    void WritePending() noexcept {
        if (pending.empty()) return;
        if (!failed.load(std::memory_order_acquire)) {
            if (!output->Write(pending)) failed.store(true, std::memory_order_release);
        }
        pending.clear();
        // A very long line leaves no large buffer behind.
        if (pending.capacity() > 4 * kWriteChunk) std::string().swap(pending);
    }

    // One line handed on by itself, without copying it.
    void WriteLine(const std::string_view line) noexcept {
        if (failed.load(std::memory_order_acquire)) return;
        if (!output->Write(line)) failed.store(true, std::memory_order_release);
    }

    void RunWriter(Threads::RoleContext& context) {
        {
            std::scoped_lock lock(progressMutex);
            writerId = std::this_thread::get_id();
        }
        std::vector<std::string> batch;
        for (;;) {
            // Read before taking the queue: Close marks the queue closed before
            // it requests the stop, so once a stop is seen this take holds
            // every line that will ever be accepted.
            const bool stopping = context.StopRequested();
            std::uint64_t through{};
            {
                std::scoped_lock lock(queueMutex);
                batch.swap(queue);
                through = accepted;
            }
            for (const std::string& line : batch) {
                try {
                    pending += line;
                } catch (...) {
                    // No memory to gather it: what is gathered goes first, then
                    // this line as it is (a record is never worth ending the process).
                    WritePending();
                    WriteLine(line);
                    continue;
                }
                if (pending.size() >= kWriteChunk) WritePending();
            }
            WritePending();
            {
                std::scoped_lock lock(progressMutex);
                // Counted as taken by the writer, failed or not, so that the
                // counts stay conserved; a failure shows in Good() (master TraceSink).
                written += batch.size();
                flushedThrough = through;
            }
            progressChanged.notify_all();
            batch.clear();
            if (stopping) break;
            // Only a push into an empty queue, Flush or the stop wakes it; a
            // notify latched while writing makes this return at once. Never a timer.
            const auto wake = context.Wait();
            GYO_ASSERT(wake.reason == ::Engine::Time::WakeReason::Notified);
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

    void Push(std::string line) {
        bool wake{};
        {
            std::scoped_lock lock(queueMutex);
            if (closed) {
                ++droppedClosed;
                return;
            }
            if (queue.size() >= capacity) {
                ++droppedFull;
                return;
            }
            wake = queue.empty();
            queue.push_back(std::move(line));
            ++accepted;
        }
        // A non-empty queue already has a wake pending since the writer last took it.
        if (wake) writer->Notify();
    }

    [[nodiscard]] JsonLinesCounts CurrentCounts() const {
        JsonLinesCounts counts;
        {
            std::scoped_lock lock(queueMutex);
            counts.droppedFull = droppedFull;
            counts.droppedClosed = droppedClosed;
        }
        {
            std::scoped_lock lock(progressMutex);
            counts.written = written;
        }
        counts.fatalDirect = fatalDirect.load(std::memory_order_relaxed);
        counts.replaced = replaced.load(std::memory_order_relaxed);
        return counts;
    }
};

LoggingResult<std::shared_ptr<JsonLinesFileSink>> JsonLinesFileSink::Open(JsonLinesFileOptions options) {
    GYO_ASSERT(options.capacity > 0);
    GYO_ASSERT(!options.threadName.empty());
    GYO_ASSERT((options.output == nullptr) == (options.fatalOutput == nullptr));
    auto impl = std::make_unique<Impl>();
    impl->capacity = options.capacity;
    impl->monoNs = options.monoNs;
    if (options.output != nullptr) {
        impl->output = std::move(options.output);
        impl->fatalOutput = std::move(options.fatalOutput);
    } else {
        if (options.truncate) {
            auto truncated = Os::Files::FileWriter::Open(options.path, Os::Files::WriteMode::Truncate,
                                                         Os::Files::LineEndings::AsWritten);
            if (!truncated || !truncated.value().Close()) {
                return ::Engine::Base::Err(LoggingError::Make(
                    LoggingErrorCode::OpenFailed, "Cannot open the log file",
                    truncated ? options.path : ::Engine::Base::CauseDetail(truncated.error(), options.path)));
            }
        }
        auto output = OpenAppend(options.path);
        if (!output) return ::Engine::Base::Err(std::move(output).error());
        auto fatalOutput = OpenAppend(options.path);
        if (!fatalOutput) {
            output.value()->Close();
            return ::Engine::Base::Err(std::move(fatalOutput).error());
        }
        impl->output = std::move(output.value());
        impl->fatalOutput = std::move(fatalOutput.value());
    }
    Impl* state = impl.get();
    auto writer = Threads::RoleThread::Start({.name = std::move(options.threadName)},
                                             [state](Threads::RoleContext& context) { state->RunWriter(context); });
    if (!writer) {
        impl->output->Close();
        impl->fatalOutput->Close();
        return ::Engine::Base::Err(LoggingError::Make(LoggingErrorCode::StartFailed, "Cannot start the log writer",
                                                      ::Engine::Base::CauseDetail(writer.error(), options.path)));
    }
    impl->writer.emplace(std::move(writer.value()));
    return std::shared_ptr<JsonLinesFileSink>(new JsonLinesFileSink(std::move(impl)));
}

JsonLinesFileSink::JsonLinesFileSink(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

JsonLinesFileSink::~JsonLinesFileSink() {
    Close();
}

void JsonLinesFileSink::Write(const Record& record) noexcept {
    try {
        std::string line;
        line.reserve(256 + record.text.size() + 32 * record.fields.size());
        if (!AppendJsonLine(line, record)) impl_->replaced.fetch_add(1, std::memory_order_relaxed);
        impl_->Push(std::move(line));
    } catch (...) {
        // No memory for the line: counted as refused for want of room.
        std::scoped_lock lock(impl_->queueMutex);
        ++impl_->droppedFull;
    }
}

bool JsonLinesFileSink::WriteFatal(const Record&, const std::string_view jsonLine, std::string_view) noexcept {
    // Tried, never waited for: a second fatal path on another thread at the
    // same moment writes without the lock rather than wait (section 5.6).
    std::unique_lock lock(impl_->fatalMutex, std::try_to_lock);
    if (impl_->fatalClosed.load(std::memory_order_acquire)) return false;
    if (!impl_->fatalOutput->Write(jsonLine)) return false;
    impl_->fatalDirect.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool JsonLinesFileSink::Flush(const std::chrono::steady_clock::time_point deadline) noexcept {
    try {
        if (impl_->finished || impl_->OnWriterThread()) return false;
        // try_lock: a crash path may run on a thread that already holds the
        // queue; waiting would hang it.
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
    } catch (...) {
        return false;
    }
}

void JsonLinesFileSink::Close() noexcept {
    if (impl_->finished) return;
    try {
        // Close belongs to the owner; on the writer thread it would join itself.
        if (impl_->OnWriterThread()) return;
        {
            std::scoped_lock lock(impl_->queueMutex);
            impl_->closed = true;
        }
        impl_->writer->RequestStop();
        impl_->writer->Join();
        // The writer has returned: the output is the owner's alone from here.
        const JsonLinesCounts counts = impl_->CurrentCounts();
        AppendJsonEndLine(impl_->pending, impl_->monoNs ? impl_->monoNs() : ::Engine::Time::NowNs(), counts);
        impl_->WritePending();
        if (!impl_->output->Close()) impl_->failed.store(true, std::memory_order_release);
        {
            std::scoped_lock lock(impl_->fatalMutex);
            impl_->fatalClosed.store(true, std::memory_order_release);
            impl_->fatalOutput->Close();
        }
        impl_->closeCounts = counts;
    } catch (...) {
        impl_->failed.store(true, std::memory_order_release);
    }
    impl_->finished = true;
}

bool JsonLinesFileSink::Good() const noexcept {
    return !impl_->failed.load(std::memory_order_acquire);
}

JsonLinesCounts JsonLinesFileSink::Counts() const noexcept {
    try {
        return impl_->CurrentCounts();
    } catch (...) {
        return {};
    }
}

} // namespace GYO::Engine::Logging
