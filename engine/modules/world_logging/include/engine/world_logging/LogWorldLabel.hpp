#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

// The World's name as a Resource the Systems that Forward registers read
// (section 5.3 of docs/architecture/target/v2/logging-declaration.zh-Hant.md):
// a Kernel System has no state of its own and no parameter that gives the
// World's name. Fixed length, no pointer, no padding (the bytes of a Resource
// go into the World summary), as TimeConfig::fixedSchedule. Not data any
// decision reads; never written after Prepare puts it.
//
// One type per descriptor D, all of the same layout: the Kernel has no way to
// ask whether a type is registered yet, and registering one type twice is a
// startup error (R-E2), so each Forward registers its own label type. A second
// Forward of the same descriptor in one World is that startup error.

namespace GYO::Engine::WorldLogging {

inline constexpr std::size_t kMaxWorldNameBytes = 63;

template <class D>
struct LogWorldLabel final {
    // NUL-padded.
    char name[kMaxWorldNameBytes + 1] = {};

    [[nodiscard]] std::string_view Name() const noexcept {
        std::size_t size = 0;
        while (size < sizeof name && name[size] != '\0') ++size;
        return std::string_view(name, size);
    }
};

} // namespace GYO::Engine::WorldLogging
