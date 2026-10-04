#pragma once

// Product acceptance only. Late-wake distribution of an empty 60 Hz loop that
// sleeps the same way the probe does, measured once before a probe connects.
// It records how precisely this host wakes a sleeping thread, so a slow frame
// can be read against the host's own timer behaviour. It is interpretation
// only: no verdict, threshold or denominator may read it, and nothing branches
// on the operating system's name.
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct TimerBaseline final {
    // Three seconds (180 wakes) keeps the probes' bounded startup deadlines.
    static constexpr double DefaultSeconds = 3;
    static constexpr double Hz = 60;
    static constexpr double SlowIntervalMilliseconds = 18;

    // How the probe's frame loop schedules its sleep, reproduced exactly:
    // Absolute keeps fixed deadlines (deadline += period, re-anchored only if
    // already behind before sleeping); FrameRelative sleeps the rest of one
    // period measured from the start of the frame.
    enum class Schedule { Absolute, FrameRelative };

    std::string sleeper, schedule;
    double seconds{};
    std::size_t samples{};
    double lateP50Ms{}, lateP99Ms{}, lateMaxMs{};
    double intervalP50Ms{}, intervalP99Ms{}, intervalMaxMs{}, intervalOverSlowFraction{};

    // sleepUntil(deadline) must sleep exactly as the probe's own frame loop does.
    template<class SleepUntil>
    static TimerBaseline Measure(std::string sleeper, Schedule schedule, SleepUntil&& sleepUntil,
                                 double seconds = DefaultSeconds) {
        using Clock = std::chrono::steady_clock;
        const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1 / Hz));
        std::vector<double> late, intervals;
        late.reserve(static_cast<std::size_t>(seconds * Hz) + 1);
        intervals.reserve(late.capacity());
        const auto started = Clock::now();
        auto deadline = started, previous = started;
        while (Clock::now() - started < std::chrono::duration<double>(seconds)) {
            if (schedule == Schedule::FrameRelative) deadline = previous + period;
            else {
                deadline += period;
                if (deadline < Clock::now()) deadline = Clock::now();
            }
            sleepUntil(deadline);
            const auto woke = Clock::now();
            late.push_back(std::chrono::duration<double, std::milli>(woke - deadline).count());
            intervals.push_back(std::chrono::duration<double, std::milli>(woke - previous).count());
            previous = woke;
        }
        TimerBaseline result;
        result.sleeper = std::move(sleeper);
        result.schedule = schedule == Schedule::FrameRelative ? "frame_relative" : "absolute";
        result.seconds = seconds;
        result.samples = late.size();
        result.lateP50Ms = Rank(late, .5);
        result.lateP99Ms = Rank(late, .99);
        result.lateMaxMs = late.empty() ? 0 : *std::max_element(late.begin(), late.end());
        result.intervalP50Ms = Rank(intervals, .5);
        result.intervalP99Ms = Rank(intervals, .99);
        result.intervalMaxMs = intervals.empty() ? 0 : *std::max_element(intervals.begin(), intervals.end());
        const auto slow = std::count_if(intervals.begin(), intervals.end(),
            [](double value) { return value > SlowIntervalMilliseconds; });
        result.intervalOverSlowFraction = intervals.empty() ? 0 : double(slow) / double(intervals.size());
        return result;
    }

    nlohmann::json Json() const {
        return {{"use", "interpretation only; never a verdict, threshold or denominator"},
            {"sleeper", sleeper}, {"schedule", schedule}, {"loop_hz", Hz}, {"seconds", seconds}, {"samples", samples},
            {"late_p50_ms", lateP50Ms}, {"late_p99_ms", lateP99Ms}, {"late_max_ms", lateMaxMs},
            {"interval_p50_ms", intervalP50Ms}, {"interval_p99_ms", intervalP99Ms}, {"interval_max_ms", intervalMaxMs},
            {"slow_interval_ms", SlowIntervalMilliseconds}, {"interval_over_slow_fraction", intervalOverSlowFraction}};
    }

private:
    // Nearest rank, the convention of the acceptance analyzers.
    static double Rank(std::vector<double> values, double quantile) {
        if (values.empty()) return 0;
        std::sort(values.begin(), values.end());
        const auto index = static_cast<std::size_t>(std::max(0.0, std::ceil(values.size() * quantile) - 1));
        return values[std::min(index, values.size() - 1)];
    }
};
