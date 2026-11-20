#pragma once

#include <cstdint>
#include <string>

#include "engine/logging/Record.hpp"

// The JSON lines format: a data contract (section 5.12 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md, Design Invariant
// LG-5). One record is one line of strict JSON that GYO::Json's Parse reads
// back. The envelope's keys, in this order:
//
//   schema, kind, level, category, wall, mono_ns,
//   begin_ns, duration_ns            (kind "span" only)
//   product                          (when the record has one: fatal records of a World)
//   world                            (when the record has one)
//   world_interrupted                (only when true)
//   text, fields                     (fields: an object, keys in the order written)
//
// wall is the local date and time with the offset from UTC, as strftime's %z
// writes it: 2026-11-20T10:40:11.007+0900. A double that is not finite is
// written as the string "NaN", "Infinity" or "-Infinity" (question 3,
// approved); negative zero stays -0.0. A record that JSON cannot represent
// (text that is not UTF-8) is replaced by an Error record of category
// "logging" with the same kind and world, whose text names the JSON error
// (not the field). schema is the envelope's version: adding a key keeps it,
// changing the meaning or type of a key raises it. What a product writes in
// fields is versioned inside its own fields.

namespace GYO::Engine::Logging {

inline constexpr std::uint64_t kJsonLinesSchema = 1;

// What a file sink counts (Design Invariant LG-6). Every record handed to
// Write is counted once: handed = written + droppedFull + droppedClosed once
// the sink is closed and no Write is still running. Fatal lines bypass the
// queue and are counted apart, in fatalDirect.
struct JsonLinesCounts final {
    // Lines the writer thread took and handed to its output, the end line not
    // included. Once a write fails (the sink's Good() is false) nothing more
    // reaches the file, but the lines taken are still counted here, so that
    // the counts stay conserved.
    std::uint64_t written{};
    // Refused because the queue was full.
    std::uint64_t droppedFull{};
    // Refused because Close had started.
    std::uint64_t droppedClosed{};
    // Lines the fatal path wrote through the sink's own second output.
    std::uint64_t fatalDirect{};
    // Records replaced by an Error record because JSON could not represent them
    // (counted among the lines above as well).
    std::uint64_t replaced{};
};

// Appends record as one line ending in '\n'. Returns false when the record was
// replaced by the Error record described above.
bool AppendJsonLine(std::string& out, const Record& record);

// Appends the end line, the last line a file sink writes when it is closed:
// {"schema":1,"kind":"end","mono_ns":...,"written":...,"dropped_full":...,
//  "dropped_closed":...,"fatal_direct":...}
void AppendJsonEndLine(std::string& out, std::int64_t monoNs, const JsonLinesCounts& counts);

} // namespace GYO::Engine::Logging
