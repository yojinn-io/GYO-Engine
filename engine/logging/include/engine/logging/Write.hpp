#pragma once

#include <initializer_list>
#include <span>
#include <string_view>

#include "engine/logging/Level.hpp"
#include "engine/logging/Record.hpp"

// Writing one record directly: entrance ④ of section 3 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md. For trace
// records, World-free code, an author's own Systems (question 5, approved) and
// records an author makes of the Kernel diagnostics it took itself. Engine
// Modules report warnings from a System with a Message instead (Design
// Invariant LG-1).
//
// Any thread, concurrently; never throws. A record no sink takes is not
// composed: nothing is allocated and no clock is read (LG-7). Its views need
// to live only until the call returns.

namespace GYO::Engine::Logging {

// What the caller hands in; the Logger adds the time.
struct Entry final {
    Level level{Level::Info};
    std::string_view category;
    std::string_view text;
    std::span<const Field> fields;
    std::string_view world;
    bool worldInterrupted{false};
};

// Whether some sink of the installed Logger would take a record of this level
// and category (always true with no Logger installed).
[[nodiscard]] bool Enabled(Level level, std::string_view category) noexcept;

void Write(const Entry& entry) noexcept;

inline void Write(const Level level, const std::string_view category, const std::string_view text,
                  const std::initializer_list<Field> fields = {}) noexcept {
    Write(Entry{.level = level,
                .category = category,
                .text = text,
                .fields = std::span<const Field>(fields.begin(), fields.size())});
}

} // namespace GYO::Engine::Logging
