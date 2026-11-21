#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string_view>

#include "engine/logging/Sink.hpp"

// The console sink (section 5.7 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md): the text form,
// for people, written on the calling thread, one line in one write under a
// lock. A record that reaches it from a World's System is written on the
// thread that runs the World: when the console blocks, so does the World
// (section 12). A product that wants to avoid that keeps such categories out
// of the console's filter.

namespace GYO::Engine::Logging {

class ConsoleSink final : public Sink {
public:
    // Standard error by default; tests pass a stream of their own.
    explicit ConsoleSink(std::FILE* stream = stderr) noexcept;

    void Write(const Record& record) noexcept override;
    // The fatal path writes standard error itself: on standard error this does
    // nothing; on another stream it writes the text line.
    bool WriteFatal(const Record& record, std::string_view jsonLine, std::string_view textLine) noexcept override;

    // Lines that could not be formatted or written.
    [[nodiscard]] std::uint64_t Failed() const noexcept { return failed_.load(std::memory_order_relaxed); }

private:
    std::FILE* stream_;
    std::mutex ownMutex_;
    std::atomic<std::uint64_t> failed_{0};
};

} // namespace GYO::Engine::Logging
