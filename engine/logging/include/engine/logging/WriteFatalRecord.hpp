#pragma once

#include <cstdint>

#include "engine/logging/Record.hpp"

// The fatal path's last write (section 5.6 of docs/architecture/target/v2/logging-declaration.zh-Hant.md,
// Design Invariant LG-2). WriteFailure (engine/world_logging) and the assertion handler both end here.
// On the calling thread, through no mechanism of a World, no event and no queue:
//
//   1. the record is formatted once as a text line and a JSON line, in the capacity Install reserved
//      (no allocation unless a line is longer);
//   2. standard error: the text line, written and flushed at once (its lock is only tried: a line may
//      interleave with another rather than wait);
//   3. every sink that takes the record's category (the level is not consulted): Sink::WriteFatal, which a
//      JSON lines file sink writes through its own second output at once, not through its queue;
//   4. only then, at best: every sink's Flush, all of them within one deadline (the Logger's
//      fatalFlushTimeout), so that what was queued before reaches the OS too. A sink's Flush on its own
//      writer thread returns false at once.
//
// "Written" means handed to the OS, not to the disk. Never throws. A fatal path that starts again on the
// same thread while one runs (a sink failing inside its WriteFatal, say) writes only standard error and
// returns. Not guaranteed: memory already corrupted, an exhausted stack, signals, and a time limit while a
// writer is stuck inside the OS (a second handle of the same file can be stuck with it). With no Logger
// installed only standard error is written.

namespace GYO::Engine::Logging {

struct FatalWriteReport final {
    // Standard error was written and flushed.
    bool console{false};
    // Sinks whose WriteFatal returned true, and false.
    std::uint32_t sinksWritten{0};
    std::uint32_t sinksFailed{0};
    // Every sink's Flush returned true within the deadline (step 4).
    bool flushed{false};
    // A fatal path was already running on this thread: only standard error was written.
    bool reentered{false};
};

// record.level is written as given (Fatal for WriteFailure and assertions); its time fields are filled
// with the Logger's clock (the defaults with no Logger installed).
FatalWriteReport WriteFatalRecord(const Record& record) noexcept;

} // namespace GYO::Engine::Logging
