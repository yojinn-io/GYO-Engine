#pragma once

#include <cstdint>

// The trace Module (docs/architecture/target/v2/trace-declaration.zh-Hant.md): installs the receiver of the
// Base trace facade (engine/base/Trace.hpp). Spans are begun through the facade by the kernel, the
// orchestration layer, Modules and products; this Module keeps the stack, the ids and the timing, and writes
// them as logging records:
//
//   one record per span      category "span.<category>", level Trace, text = name, the World; fields span_id,
//                            parent_id, begin_ns, duration_ns, and when they apply error, exception, unbalanced
//   one record per path      category "span.path", level Trace, text = the error; fields path, span_id;
//                            written where an error is produced (D4 (i)) or by the first span an exception unwinds
//
// Records are written only while a Logger is installed and some sink takes them; otherwise a span reads no
// clock and allocates nothing, and only the stack (for the path) is kept. The path keeps its deepest end.

namespace GYO::Engine::Trace {

struct TraceOptions final {
    // The time base of begin_ns and duration_ns; nullptr is Engine::Time::NowNs (the logging Module's mono_ns).
    std::int64_t (*monoNs)() noexcept = nullptr;
};

// Uninstalls on Close or destruction: the facade has no receiver again. Spans still open end without effect.
class TraceHandle final {
public:
    TraceHandle(TraceHandle&& other) noexcept;
    TraceHandle& operator=(TraceHandle&& other) noexcept;
    TraceHandle(const TraceHandle&) = delete;
    TraceHandle& operator=(const TraceHandle&) = delete;
    ~TraceHandle();

    void Close() noexcept;

private:
    friend TraceHandle Install(TraceOptions options);
    explicit TraceHandle(bool installed) noexcept : installed_(installed) {}
    bool installed_ = false;
};

// One per process: a second Install while one is installed is a Programmer Error. Install before other threads
// begin spans, uninstall after they stopped.
[[nodiscard]] TraceHandle Install(TraceOptions options);

} // namespace GYO::Engine::Trace
