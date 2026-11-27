#include "object_fps_pvp/match/LoadThreads.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>

namespace object_fps_pvp::match {

std::size_t MachineLoadThreads(const unsigned hardware) noexcept {
    return std::clamp<std::size_t>(hardware == 0 ? kLeastLoadThreads : hardware, kLeastLoadThreads, kMostLoadThreads);
}

std::optional<LoadThreads> ChooseLoadThreads(const std::optional<std::string_view> asked, const unsigned hardware) noexcept {
    const LoadThreads machine{.count = MachineLoadThreads(hardware), .askedOutOfRange = false};
    if (!asked) return machine;
    std::int64_t value = 0;
    const auto parsed = std::from_chars(asked->data(), asked->data() + asked->size(), value);
    // from_chars reads nothing of text that does not start with a number (and of "" reads nothing at all).
    if (asked->empty() || parsed.ptr != asked->data() + asked->size()) return std::nullopt;
    // A whole number too large for 64 bits is a number out of the range, not a malformed one.
    if (parsed.ec == std::errc::result_out_of_range) return LoadThreads{.count = machine.count, .askedOutOfRange = true};
    if (value < static_cast<std::int64_t>(kFewestAskedLoadThreads) || value > static_cast<std::int64_t>(kMostLoadThreads)) {
        return LoadThreads{.count = machine.count, .askedOutOfRange = true};
    }
    return LoadThreads{.count = static_cast<std::size_t>(value), .askedOutOfRange = false};
}

} // namespace object_fps_pvp::match
