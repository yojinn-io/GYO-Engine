#include "engine/time/LateWakeStats.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Engine::Time {

void LateWakeStats::Record(const Duration late) {
    GYO_ASSERT(late >= Duration::zero());
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(late).count();
    const auto bin = static_cast<std::size_t>(
        std::upper_bound(BinUpperMicros.begin(), BinUpperMicros.end(), micros) - BinUpperMicros.begin());
    ++bins_[bin];
    ++count_;
    max_ = std::max(max_, late);
}

void LateWakeStats::Record(const Wake& wake) {
    if (wake.reason == WakeReason::Deadline) Record(wake.late);
}

void LateWakeStats::Reset() noexcept {
    bins_ = {};
    count_ = 0;
    max_ = Duration::zero();
}

Duration LateWakeStats::QuantileUpperBound(const double q) const {
    GYO_ASSERT(q > 0.0 && q <= 1.0);
    if (count_ == 0) return Duration::zero();
    const auto rank = static_cast<std::uint64_t>(std::ceil(q * static_cast<double>(count_)));
    std::uint64_t seen = 0;
    for (std::size_t bin = 0; bin < BinCount; ++bin) {
        seen += bins_[bin];
        if (seen >= rank) {
            if (bin + 1 == BinCount) return max_;
            return std::min<Duration>(max_, std::chrono::microseconds(BinUpperMicros[bin]));
        }
    }
    return max_;
}

} // namespace Engine::Time
