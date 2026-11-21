#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string_view>

// The trace facade (docs/architecture/target/v2/trace-declaration.zh-Hant.md, D1 and D2): the one entrance
// through which the kernel, the orchestration layer, Modules and products say which step they are in. It
// tells; it never handles: nothing here changes a return value, the flow or an exception.
//
// A receiver (the trace Module installs one) gets the calls; with none installed every call reads one atomic
// pointer and returns. The facade holds no state of its own: the span stack, ids, timing and the path belong
// to the receiver.
//
//     Engine::Base::TraceSpan span;            // declared where the step's scope starts
//     if (runs) span.Begin("kernel", name, world);   // started only when the step runs
//     ...                                       // ended by the destructor, also on an exception
//
// The string views given to Begin must outlive the span (the kernel's schedule, System and World names live
// in its registry and World while it runs).

namespace Engine::Base {

// What a receiver hands back for a begun span; id 0 is "no span".
struct TraceToken {
    std::uint64_t id = 0;
    std::uint32_t generation = 0;
    std::uint32_t depth = 0;
};

// Every function may run on any thread and must not throw.
struct TraceReceiver {
    TraceToken (*begin)(std::string_view category, std::string_view name, std::string_view world) noexcept;
    // endedByException: an exception was unwinding the span when it ended.
    void (*end)(TraceToken token, bool endedByException) noexcept;
    // origin: the error is produced here (the path is written); false: it only passes through (declaration D4 (i)).
    void (*error)(TraceToken token, std::string_view text, bool origin) noexcept;
    // Marks the span on top of the calling thread's stack.
    void (*markCurrent)(std::string_view text, bool origin) noexcept;
    // Writes the calling thread's current path into buffer, keeping the deepest end; returns its length.
    std::size_t (*path)(std::span<char> buffer) noexcept;
};

namespace Detail {
inline constinit std::atomic<const TraceReceiver*> traceReceiver{nullptr};
} // namespace Detail

// Installs receiver (nullptr: none) and returns the previous one. The receiver must outlive every span begun
// with it (a span ends at the receiver it began with).
inline const TraceReceiver* SetTraceReceiver(const TraceReceiver* receiver) noexcept {
    return Detail::traceReceiver.exchange(receiver, std::memory_order_acq_rel);
}

// A span: not started until Begin; ended by its destructor. Neither copyable nor movable (it is a scope).
class TraceSpan {
public:
    TraceSpan() noexcept = default;
    TraceSpan(const TraceSpan&) = delete;
    TraceSpan& operator=(const TraceSpan&) = delete;
    ~TraceSpan() {
        if (receiver_ != nullptr) receiver_->end(token_, std::uncaught_exceptions() > exceptions_);
    }

    // Starts the span; a second Begin on the same span is ignored.
    void Begin(const std::string_view category, const std::string_view name, const std::string_view world = {}) noexcept {
        if (receiver_ != nullptr) return;
        const TraceReceiver* receiver = Detail::traceReceiver.load(std::memory_order_acquire);
        if (receiver == nullptr) return;
        exceptions_ = std::uncaught_exceptions();
        token_ = receiver->begin(category, name, world);
        receiver_ = receiver;
    }

    // Marks this span with an error before it ends; origin: the error is produced in this span.
    void MarkError(const std::string_view text, const bool origin = true) noexcept {
        if (receiver_ != nullptr) receiver_->error(token_, text, origin);
    }

    [[nodiscard]] bool Active() const noexcept { return receiver_ != nullptr; }

private:
    const TraceReceiver* receiver_ = nullptr;
    TraceToken token_{};
    int exceptions_ = 0;
};

// Marks the span on top of the calling thread's stack (the kernel's Fail: one place for every error of a Run).
inline void TraceMarkCurrent(const std::string_view text, const bool origin) noexcept {
    if (const TraceReceiver* receiver = Detail::traceReceiver.load(std::memory_order_acquire)) receiver->markCurrent(text, origin);
}

// The calling thread's current path, written into buffer (no allocation, no lock); 0 with no receiver.
inline std::size_t TraceCurrentPath(const std::span<char> buffer) noexcept {
    const TraceReceiver* receiver = Detail::traceReceiver.load(std::memory_order_acquire);
    return receiver != nullptr ? receiver->path(buffer) : 0;
}

} // namespace Engine::Base
