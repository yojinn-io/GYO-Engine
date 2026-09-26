#pragma once

#include <chrono>
#include <optional>

namespace fps::pvp {

// Application-owned sampling at the prediction call site. Runtime frame start
// times can precede event/asset/network work, so they are not movement samples.
class PredictionElapsedTime final {
public:
    using Clock = std::chrono::steady_clock;

    void Reset() noexcept { previous_.reset(); }

    [[nodiscard]] double Sample(Clock::time_point now) noexcept {
        const double elapsed = previous_ ? std::chrono::duration<double>(now - *previous_).count() : 0;
        previous_ = now;
        return elapsed;
    }

private:
    std::optional<Clock::time_point> previous_;
};

} // namespace fps::pvp
