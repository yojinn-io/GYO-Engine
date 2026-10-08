#pragma once

#include "engine/time/MonotonicClock.hpp"
#include "engine/time/Waiter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Engine::Time {

// How late deadline wakes were: a fixed-bin histogram with count and maximum.
// No allocation and no thread of its own; the owner decides when to read and
// reset it (for example once per statistics window).
class LateWakeStats final {
public:
    static constexpr std::size_t BinCount = 10;
    // Upper bounds (exclusive) of bins 0..8 in microseconds; bin 9 holds the rest.
    static constexpr std::array<std::int64_t, BinCount - 1> BinUpperMicros{
        250, 500, 1000, 2000, 4000, 8000, 16000, 32000, 64000};

    // late must not be negative.
    void Record(Duration late);
    // Records deadline wakes only; a notification is not a late wake.
    void Record(const Wake& wake);
    void Reset() noexcept;

    [[nodiscard]] std::uint64_t Count() const noexcept { return count_; }
    [[nodiscard]] Duration Max() const noexcept { return max_; }
    [[nodiscard]] const std::array<std::uint64_t, BinCount>& Bins() const noexcept { return bins_; }
    // An upper bound of the q-quantile (0 < q <= 1): the upper bound of the bin
    // holding it, or Max() for the last bin. Zero when nothing was recorded.
    [[nodiscard]] Duration QuantileUpperBound(double q) const;

private:
    std::array<std::uint64_t, BinCount> bins_{};
    std::uint64_t count_{};
    Duration max_{};
};

} // namespace Engine::Time
