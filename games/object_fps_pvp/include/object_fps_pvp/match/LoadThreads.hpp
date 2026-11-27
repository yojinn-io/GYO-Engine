#pragma once

// How many threads the Match parses the map's region files on (big-map declaration 17.8). By default the machine
// decides: its hardware threads, between kLeastLoadThreads and kMostLoadThreads. --load-threads N asks for N; an N
// outside kFewestAskedLoadThreads to kMostLoadThreads is not clamped: the machine decides, as with no N.

#include <cstddef>
#include <optional>
#include <string_view>

namespace object_fps_pvp::match {

inline constexpr std::size_t kLeastLoadThreads = 2;
inline constexpr std::size_t kMostLoadThreads = 8;
inline constexpr std::size_t kFewestAskedLoadThreads = 1;

struct LoadThreads final {
    std::size_t count = 0;
    // An N was given but was outside the range, so count is the machine's.
    bool askedOutOfRange = false;
};

// The machine's choice: hardware (std::thread::hardware_concurrency; 0: not known) between least and most.
[[nodiscard]] std::size_t MachineLoadThreads(unsigned hardware) noexcept;

// asked: --load-threads' text, nullopt without it. nullopt: the text is not a whole number.
[[nodiscard]] std::optional<LoadThreads> ChooseLoadThreads(std::optional<std::string_view> asked, unsigned hardware) noexcept;

} // namespace object_fps_pvp::match
