#pragma once

#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "engine/logging/Level.hpp"
#include "engine/logging/Record.hpp"

// A length of time as one record (section 5.13 of docs/architecture/target/v2/logging-declaration.zh-Hant.md,
// Design Invariant LG-9; question 6, approved: the hand-made span is implemented now):
//
//     {
//         Logging::Span span("trace.net", "send");
//         ...                                   // the work being measured
//     }                                         // one record: kind "span", begin_ns, duration_ns
//
// When it ends (End or the destructor) it writes one record of kind Span on the engine time base: begin_ns
// when it was made, duration_ns the time since. A span no sink takes when it is made reads no clock and
// writes nothing. No parent and no child: spans are not nested in the record (no Feature needs it). The
// category and the text are views: they must outlive the span (string literals usually). Not copyable, not
// movable; one thread.

namespace GYO::Engine::Logging {

class Span final {
public:
    Span(std::string_view category, std::string_view text, Level level = Level::Info) noexcept;
    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;
    ~Span();

    // Writes the record now, with these fields; later calls and the destructor do nothing.
    void End(std::initializer_list<Field> fields = {}) noexcept;

    // Whether a sink took the span when it was made (and the clock was read).
    [[nodiscard]] bool Active() const noexcept { return active_; }

private:
    std::string_view category_;
    std::string_view text_;
    Level level_;
    bool active_;
    bool ended_ = false;
    std::int64_t beginNs_ = 0;
};

} // namespace GYO::Engine::Logging
