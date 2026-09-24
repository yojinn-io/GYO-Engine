#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace Engine::Runtime {

struct TickSettings final {
    double tickRate{60.0};
    std::uint32_t maximumCatchUpSteps{5};
};

struct TickContext final {
    std::uint64_t tickId{};
    double deltaSeconds{};
};

struct FixedTickAdvance final {
    std::uint32_t steps{};
    double droppedSeconds{};
    double secondsUntilNextTick{};
};

// Caller supplies elapsed time and owns its clock, sleeping and lifecycle.
// Excess whole steps are dropped after bounded catch-up; the fractional
// remainder survives. Tick IDs count simulated steps, never discarded time.
class FixedTickRuntime final {
public:
    explicit FixedTickRuntime(TickSettings settings = {})
        : FixedTickRuntime(settings.tickRate, settings.maximumCatchUpSteps) {}

    explicit FixedTickRuntime(double tickRate, std::uint32_t maximumCatchUpSteps = 5)
        : stepSeconds_(1.0 / tickRate), maximumCatchUpSteps_(maximumCatchUpSteps) {
        if (!std::isfinite(tickRate) || tickRate <= 0.0 ||
            !std::isfinite(stepSeconds_) || stepSeconds_ <= 0.0 || maximumCatchUpSteps == 0) {
            throw std::invalid_argument("FixedTickRuntime requires a finite positive tick rate and catch-up limit");
        }
    }

    template<class Callback>
    FixedTickAdvance Advance(double elapsedSeconds, Callback&& callback) {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0 ||
            !std::isfinite(accumulatedSeconds_ + elapsedSeconds)) {
            throw std::invalid_argument("FixedTickRuntime elapsed time must be finite and non-negative");
        }
        accumulatedSeconds_ += elapsedSeconds;
        FixedTickAdvance result;
        const double tolerance = stepSeconds_ * 1.0e-9;
        while (accumulatedSeconds_ + tolerance >= stepSeconds_ &&
               result.steps < maximumCatchUpSteps_) {
            accumulatedSeconds_ = (std::max)(0.0, accumulatedSeconds_ - stepSeconds_);
            ++tickId_;
            ++result.steps;
            callback(TickContext{tickId_, stepSeconds_});
        }
        if (accumulatedSeconds_ + tolerance >= stepSeconds_) {
            double remainder = std::fmod(accumulatedSeconds_, stepSeconds_);
            if (remainder + tolerance >= stepSeconds_) remainder = 0.0;
            result.droppedSeconds = accumulatedSeconds_ - remainder;
            accumulatedSeconds_ = remainder;
        }
        result.secondsUntilNextTick = stepSeconds_ - accumulatedSeconds_;
        return result;
    }

    void Reset() noexcept { accumulatedSeconds_ = 0.0; tickId_ = 0; }
    [[nodiscard]] std::uint64_t TickId() const noexcept { return tickId_; }
    [[nodiscard]] double StepSeconds() const noexcept { return stepSeconds_; }

private:
    double stepSeconds_{};
    std::uint32_t maximumCatchUpSteps_{};
    double accumulatedSeconds_{};
    std::uint64_t tickId_{};
};

} // namespace Engine::Runtime
