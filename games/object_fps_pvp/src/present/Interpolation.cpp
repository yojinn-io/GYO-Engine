#include "object_fps_pvp/present/Interpolation.hpp"

#include "engine/math/scalar/Angle.hpp"

#include <algorithm>
#include <cmath>

namespace object_fps_pvp::present {

double InterpolationDelay(const TimelineSettings& settings, const double jitterTicks, const double stepSeconds) {
    const double minimum = stepSeconds > 0.0 ? settings.minimumDelaySeconds / stepSeconds : 0.0;
    return std::max(settings.sendIntervalTicks * settings.sendIntervalRatio, minimum) + settings.jitterMultiple * jitterTicks +
           settings.jitterMarginTicks;
}

void Advance(InterpolationTimeline& timeline, const double deltaTicks, const double objective, const TimelineSettings& settings) {
    if (timeline.synced) timeline.tick += deltaTicks * timeline.speed;
    const double error = objective - timeline.tick;
    if (!timeline.synced || std::abs(error) > settings.snapTicks) {
        timeline.tick = objective;
        timeline.synced = true;
        timeline.speed = 1.0;
        timeline.consecutive = 0;
        ++timeline.jumps;
        return;
    }
    if (std::abs(error) > settings.deadbandTicks) {
        const bool behind = error > 0.0;
        timeline.consecutive = behind == timeline.previousBehind ? static_cast<std::uint8_t>(timeline.consecutive + 1) : 1;
        timeline.previousBehind = behind;
        if (timeline.consecutive >= settings.consecutive) timeline.speed = behind ? settings.speedup : 1.0 / settings.speedup;
    } else {
        timeline.consecutive = 0;
        timeline.speed = 1.0;
    }
}

Bracket BracketAt(const std::span<const std::uint64_t> ticks, const double t) {
    Bracket bracket;
    if (ticks.empty() || t < static_cast<double>(ticks.front())) return bracket;
    // The last entry at or before t.
    const auto after = std::upper_bound(ticks.begin(), ticks.end(), t,
                                        [](const double value, const std::uint64_t tick) { return value < static_cast<double>(tick); });
    bracket.start = static_cast<std::size_t>(after - ticks.begin()) - 1;
    if (after == ticks.end()) return bracket;
    bracket.end = bracket.start + 1;
    const double span = static_cast<double>(ticks[*bracket.end] - ticks[bracket.start]);
    bracket.fraction = std::clamp((t - static_cast<double>(ticks[bracket.start])) / span, 0.0, 1.0);
    return bracket;
}

std::size_t Drainable(const std::span<const std::uint64_t> ticks, const double t) {
    std::size_t drop = 0;
    while (ticks.size() - drop >= 3 && static_cast<double>(ticks[drop + 1]) <= t) ++drop;
    return drop;
}

float LerpRadiansShortest(const float a, const float b, const double fraction) {
    const float difference = GYO::Engine::Math::WrapRadians(b - a);
    return GYO::Engine::Math::WrapRadians(a + static_cast<float>(difference * fraction));
}

RemotePose BlendPose(const RemotePose& a, const RemotePose& b, const double fraction) {
    if (a.life != b.life) return a;
    const float f = static_cast<float>(fraction);
    RemotePose out = a;
    out.feet = a.feet + (b.feet - a.feet) * f;
    out.yaw = LerpRadiansShortest(a.yaw, b.yaw, fraction);
    out.pitch = a.pitch + (b.pitch - a.pitch) * f;
    return out;
}

} // namespace object_fps_pvp::present
