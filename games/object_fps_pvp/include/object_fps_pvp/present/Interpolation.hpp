#pragma once

// The remote players' interpolation (PV17; port declaration, segment 6): what the Client shows of the other players
// runs on a timeline of its own, a little behind the Match, and blends between two states the Match confirmed.
// Presentation only: it never changes decision data. The concept is lightyear's (its InterpolationTimeline,
// crates/replication/interpolation/src/timeline.rs, and its interpolation of a confirmed history,
// crates/replication/interpolation/src/interpolate.rs); the numbers are lightyear's defaults.

#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace object_fps_pvp::present {

struct TimelineSettings final {
    // Following the objective (lightyear's SyncConfig defaults, crates/core/sync/src/timeline/sync.rs:210-220; the
    // net Module's ClockSyncSettings has the same ones): jump past snapTicks, change speed after `consecutive` errors
    // past deadbandTicks on one side.
    double snapTicks = 10.0;
    double deadbandTicks = 1.0;
    std::uint8_t consecutive = 3;
    double speedup = 1.05;
    // The delay behind the Match (lightyear's InterpolationConfig defaults, timeline.rs:50-75, and the jitter margin,
    // sync.rs:205-207): max(send interval x ratio, minimum) + jitter x multiple + margin.
    double sendIntervalTicks = 1.0;  // this game's Match sends every step
    double sendIntervalRatio = 1.2;
    double minimumDelaySeconds = 0.005;
    double jitterMultiple = 4.0;
    double jitterMarginTicks = 1.0;
};

// The delay in ticks for a measured jitter (ticks) and a step of stepSeconds.
[[nodiscard]] double InterpolationDelay(const TimelineSettings& settings, double jitterTicks, double stepSeconds);

// The timeline: a Match tick with its fraction, moved every frame by the frame's time at its speed and kept near its
// objective (the Match's tick now, estimated, minus the delay).
struct InterpolationTimeline final {
    bool synced = false;
    double tick = 0.0;
    double speed = 1.0;
    std::uint8_t consecutive = 0;
    bool previousBehind = true;
    std::uint64_t jumps = 0;
};

// Moves the timeline by deltaTicks at its speed, then toward the objective: the first time, or past snapTicks, it
// jumps there; past deadbandTicks `consecutive` times on one side it speeds up (behind) or slows down (ahead);
// within it, its speed is 1.
void Advance(InterpolationTimeline& timeline, double deltaTicks, double objective, const TimelineSettings& settings);

// Where t falls among the ticks of a history (ascending, not empty): the last entry at or before t, the next one
// and the fraction toward it. Before the first entry, or at or after the last, there is no next one (the entry is
// held: no extrapolation, lightyear's registry.rs:553-559).
struct Bracket final {
    std::size_t start = 0;
    std::optional<std::size_t> end;
    double fraction = 0.0;
};
[[nodiscard]] Bracket BracketAt(std::span<const std::uint64_t> ticks, double t);

// How many of the oldest entries can go (lightyear's smart drain, interpolate.rs:273-287): only while three or more
// remain and the second oldest is already passed, so a pair to blend stays when the next state is late.
[[nodiscard]] std::size_t Drainable(std::span<const std::uint64_t> ticks, double t);

// a to b by the fraction, the short way around (radians).
[[nodiscard]] float LerpRadiansShortest(float a, float b, double fraction);

// What is blended of a player: where it stands and where it looks, and which life it is in (a new life between two
// states is a jump, not a walk: it is not blended, a holds until b's tick; master's segments, concept only).
struct RemotePose final {
    GYO::Engine::Math::Vec3 feet{};
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::uint64_t life = 0;  // the life's first tick
};
// a toward b: the feet and the pitch straight, the yaw the short way.
[[nodiscard]] RemotePose BlendPose(const RemotePose& a, const RemotePose& b, double fraction);

} // namespace object_fps_pvp::present
