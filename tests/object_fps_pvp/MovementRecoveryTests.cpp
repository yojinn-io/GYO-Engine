#include <doctest/doctest.h>

#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace fps::pvp;
using Time = std::int64_t;
// Exact integer periods for 30/60/144 FPS and the real worker's 2 ms poll.
constexpr Time Units = 720000;
constexpr Time Tick = Units / AuthorityTickRate;
constexpr Time Poll = Units / 500;

Arena RecoveryArena() {
    Arena arena;
    arena.id = "synthetic_recovery_arena";
    arena.width = arena.depth = 1000;
    arena.spawns = {{{2, 0, 2}, 0}, {{20, 0, 2}, 0}};
    return arena;
}
struct RecoveryResult {
    std::size_t maximumPending{};
    std::uint32_t maximumFuture{};
    unsigned unexpectedRejections{};
    unsigned staleEpochRejections{};
    unsigned resets{};
    unsigned fallbacksAfterBound{};
    unsigned resetsAfterBound{};
    unsigned excessiveQueueAfterBound{};
    Time stableStart{-1};
    // Generation-to-execution time of Actual commands during the clean second
    // before any stall, and the start wait the virtual Host reported.
    std::vector<Time> actualLatencies;
    unsigned cleanFallbacks{};
    std::optional<std::uint32_t> firstStartWaitMicros;
    // Every resolution from 1 s to the end: substituted (Held) commands and the
    // generation-to-execution time of Actual ones. Start phase as the run ended.
    unsigned resolved{}, held{};
    std::vector<Time> latencies;
    std::optional<double> startPhaseShiftSeconds;
    std::optional<StartPhaseSkip> startPhaseSkip;
    // Frames with a start-phase decision (the wait taken), with the shift
    // applied, with it applied although the documented frame-rate measure (the
    // mean of the latest 32 intervals, each at most two ticks) was above the
    // cut, and the first frame withdrawn for the frame rate or applied.
    unsigned decidedFrames{}, shiftedFrames{}, shiftedBelowCut{};
    std::optional<Time> firstWithdrawn, firstShifted;
};

// Clock model. By default the worker is phase-locked: it wakes exactly at its
// deadline, so its send grid never drifts against the frames. A product worker
// mirrors ClientConnection.cpp: each sleep wakes late by a seeded overshoot,
// the deadline is re-based on that actual send (sentAt + period) and only a
// full ACK or an epoch change clears it. Frames may drift against the steady
// clock (a display clock in ppm) and jitter uniformly around that grid. A
// vsync-paced display shows each frame for whole refreshes (one refresh is
// Units / fps): a cycled pattern of refresh counts, plus one more refresh
// missed at random per mille, or one more refresh on every Nth frame interval
// the client counts from a chosen first one (interval 1 is the elapsed time
// its second active frame reports). The first frames after the client starts
// may instead be startup hitches of a fixed length.
struct ClockOptions {
    Time overshootMin{}, overshootMax{};
    int frameDriftPpm{};
    Time frameJitter{};
    std::uint32_t seed{1};
    Time runUntil{};
    std::vector<int> refreshPattern;
    unsigned missedRefreshPerMille{};
    std::size_t droppedRefreshEvery{}, firstDroppedRefresh{1};
    unsigned startupHitches{};
    Time startupHitch{};
};

// Only public product command windows and snapshots cross this virtual wire.
// Worker receipt/ACK pruning remains live during render stalls. Prediction is
// reconciled once per render with the latest received snapshot, just as in the
// application. A client cannot bootstrap from a synthetic pre-tick snapshot.
// Like the product Host, receipt of an epoch's sequence 1 is timed to the tick
// that executes it and that wait rides on the epoch's later snapshots.
RecoveryResult RunRecovery(int fps, int rttMs, int stallMs, bool impaired,
                           Time workerPhase, Time authorityPhase, double firstElapsed,
                           Time bootstrapGap = 0, bool hostStartWait = true,
                           const ClockOptions& clock = {}) {
    const auto arena = RecoveryArena();
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    LocalPlayerPrediction client(arena);
    struct InputPacket { Time due; PlayerInput input; };
    struct SnapshotPacket { Time due; WorldSnapshot snapshot; };
    std::vector<InputPacket> inputs;
    std::vector<SnapshotPacket> snapshots;
    std::map<std::pair<std::uint64_t, std::uint64_t>, MovementCommand> accepted;
    PlayerInput published;
    WorldSnapshot received;
    RecoveryResult result;
    std::uint32_t randomState = 0x425682;
    const auto random = [&] {
        randomState = randomState * 1664525U + 1013904223U;
        return randomState;
    };
    const Time resume = 2 * Units + Time(stallMs) * Units / 1000;
    const Time reference = (std::max)(resume, impaired ? 4 * Units : Time(0));
    const Time bound = reference + 3 * Units / 2;
    const auto delay = [&](Time now) {
        const auto jitter = impaired && now < 4 * Units ? int(random() % 21) - 10 : 0;
        return (std::max)(Time(0), Time(rttMs) * Units / 2000 + jitter * Units / 1000);
    };
    std::uint32_t clockState = clock.seed;
    const auto uniform = [&](Time low, Time high) {
        if (high <= low) return low;
        clockState = clockState * 1664525U + 1013904223U;
        return low + Time((clockState >> 8) % std::uint32_t(high - low + 1));
    };
    const Time framePeriod = Units / fps + Units / fps * clock.frameDriftPpm / 1000000;
    const Time runEnd = clock.runUntil ? clock.runUntil : reference + 3 * Units;
    Time frameGrid{};
    std::size_t frameIndex{}, activeFrames{};
    std::deque<double> recentFrames;
    Time nextFrame{}, nextWorker = workerPhase, nextTick = authorityPhase;
    Time previousFrame{}, nextSend{};
    bool haveSent{};
    bool firstAdvance = true;
    unsigned inputNumber{}, resetSnapshotsToDrop{};
    std::uint64_t previousEpoch = 1;
    std::deque<std::uint32_t> queueSamples;
    std::uint32_t queueSum{};
    unsigned actualRun{};
    Time actualRunStart{}, previousResolution{-Tick};
    struct EpochStart { std::uint64_t epoch{}; Time receivedAt{}; std::optional<std::uint32_t> waitMicros; };
    std::optional<EpochStart> epochStart;
    std::map<std::pair<std::uint64_t, std::uint64_t>, Time> generatedAt;
    for (;;) {
        Time now = (std::min)({nextFrame, nextWorker, nextTick});
        for (const auto& packet : inputs) now = (std::min)(now, packet.due);
        if (now > runEnd) break;
        for (auto packet = inputs.begin(); packet != inputs.end();) {
            if (packet->due > now) { ++packet; continue; }
            auto input = std::move(packet->input);
            packet = inputs.erase(packet);
            const auto state = match.Snapshot().players.front();
            const bool startsEpoch = state.lastResolvedCommand == 0 && input.movementEpoch == state.movementEpoch &&
                std::any_of(input.commands.begin(), input.commands.end(),
                    [](const auto& command) { return command.sequence == 1; });
            if (match.SubmitInput(input)) {
                if (startsEpoch && (!epochStart || epochStart->epoch != input.movementEpoch))
                    epochStart = EpochStart{input.movementEpoch, now, std::nullopt};
                for (const auto& command : input.commands)
                    if (command.sequence > state.lastResolvedCommand)
                        accepted.try_emplace(std::pair{input.movementEpoch, command.sequence}, command);
            } else if (input.movementEpoch < state.movementEpoch) {
                ++result.staleEpochRejections;
            } else {
                ++result.unexpectedRejections;
            }
        }
        if (now == nextWorker) {
            std::stable_sort(snapshots.begin(), snapshots.end(), [](const auto& a, const auto& b) {
                return a.due < b.due;
            });
            while (!snapshots.empty() && snapshots.front().due <= now) {
                auto snapshot = std::move(snapshots.front().snapshot);
                snapshots.erase(snapshots.begin());
                if (snapshot.tick <= received.tick) continue;
                received = std::move(snapshot);
                const auto& state = received.players.front();
                if (published.movementEpoch != state.movementEpoch) {
                    published.commands.clear();
                    haveSent = false;
                    nextSend = 0;
                }
                const bool hadCommands = !published.commands.empty();
                std::erase_if(published.commands, [&](const auto& command) {
                    return command.sequence <= state.lastResolvedCommand;
                });
                if (hadCommands && published.commands.empty()) {
                    haveSent = false;
                    nextSend = 0;
                }
            }
            if ((!haveSent || now >= nextSend) && !published.commands.empty()) {
                ++inputNumber;
                const bool deliver = !impaired || (inputNumber > 2 &&
                    (now >= 4 * Units || random() % 20 != 0));
                if (deliver) {
                    inputs.push_back({now + delay(now), published});
                    if (impaired && now < 4 * Units && inputNumber % 17 == 0)
                        inputs.push_back({now + delay(now) + 45 * Units / 1000, published});
                }
                haveSent = true;
                nextSend = now + Tick;
            }
            nextWorker = now + (haveSent && nextSend > now ?
                (std::min)(Poll, nextSend - now) : Poll) + uniform(clock.overshootMin, clock.overshootMax);
        }
        if (now == nextFrame) {
            Time period = framePeriod;
            if (!clock.refreshPattern.empty())
                period *= clock.refreshPattern[frameIndex++ % clock.refreshPattern.size()];
            if (clock.missedRefreshPerMille && uniform(0, 999) < clock.missedRefreshPerMille) period += framePeriod;
            // An active client reports the interval this frame starts as its counted interval activeFrames + 1.
            if (clock.droppedRefreshEvery && !received.players.empty() && activeFrames + 1 >= clock.firstDroppedRefresh &&
                (activeFrames + 1 - clock.firstDroppedRefresh) % clock.droppedRefreshEvery == 0) period += framePeriod;
            if (activeFrames > 0 && activeFrames <= clock.startupHitches) period = clock.startupHitch;
            frameGrid += period;
            nextFrame = frameGrid + uniform(-clock.frameJitter, clock.frameJitter);
            if (now < 2 * Units || now >= resume) {
                if (!received.players.empty())
                    client.Reconcile(received.players.front(), received.tick);
                const double elapsed = firstAdvance && !received.players.empty() ? firstElapsed :
                    double(now - previousFrame) / Units;
                if (firstAdvance && !received.players.empty() && bootstrapGap)
                    nextFrame = frameGrid = now + bootstrapGap;
                if (client.Advance(elapsed, 1, 0, 0, 0)) published = client.PendingInput();
                for (const auto& command : client.PendingInput().commands)
                    generatedAt.try_emplace({client.PendingInput().movementEpoch, command.sequence}, now);
                if (!received.players.empty()) {
                    firstAdvance = false;
                    ++activeFrames;
                    if (elapsed > 0) {
                        recentFrames.push_back((std::min)(elapsed, 2 * MovementTickSeconds));
                        if (recentFrames.size() > 32) recentFrames.pop_front();
                    }
                    const auto& observation = client.Observation();
                    if (observation.epochStartWaitSeconds) ++result.decidedFrames;
                    if (observation.startPhaseShiftSeconds) {
                        double sum{};
                        for (const double seconds : recentFrames) sum += seconds;
                        ++result.shiftedFrames;
                        if (sum / double(recentFrames.size()) > MovementStartPhaseMaximumFrameSeconds + 1.0e-12)
                            ++result.shiftedBelowCut;
                        if (!result.firstShifted) result.firstShifted = now;
                    } else if (observation.epochStartWaitSeconds && !result.firstWithdrawn &&
                               observation.startPhaseSkip == StartPhaseSkip::FrameRateBelowTick) {
                        result.firstWithdrawn = now;
                    }
                }
                previousFrame = now;
                result.maximumPending = (std::max)(result.maximumPending, client.PendingInput().commands.size());
                if (!received.players.empty()) {
                    const auto ack = received.players.front().lastResolvedCommand;
                    std::erase_if(published.commands, [ack](const auto& command) {
                        return command.sequence <= ack;
                    });
                }
            }
        }
        if (now == nextTick) {
            nextTick += Tick;
            const auto before = match.Snapshot().players.front();
            match.Tick({match.TickCount() + 1, MovementTickSeconds});
            auto snapshot = match.Snapshot();
            auto& state = snapshot.players.front();
            if (epochStart && epochStart->epoch == state.movementEpoch) {
                if (!epochStart->waitMicros && state.lastResolvedCommand >= 1) {
                    epochStart->waitMicros = static_cast<std::uint32_t>((now - epochStart->receivedAt) * 1000000 / Units);
                    if (!result.firstStartWaitMicros) result.firstStartWaitMicros = epochStart->waitMicros;
                }
                if (hostStartWait) state.epochStartWaitMicros = epochStart->waitMicros;
            }
            result.maximumFuture = (std::max)(result.maximumFuture, state.contiguousPendingCommands);
            if (state.movementEpoch != previousEpoch) {
                ++result.resets;
                if (now >= bound) ++result.resetsAfterBound;
                previousEpoch = state.movementEpoch;
                resetSnapshotsToDrop = impaired ? 2 : 0;
                queueSamples.clear();
                queueSum = 0;
                actualRun = 0;
            }
            if (resetSnapshotsToDrop) --resetSnapshotsToDrop;
            else if (!impaired || now >= 4 * Units || random() % 20 != 0)
                snapshots.push_back({now + delay(now), snapshot});
            if (state.movementEpoch == before.movementEpoch &&
                state.lastResolvedCommand > before.lastResolvedCommand) {
                const bool actual = accepted.contains({state.movementEpoch, state.lastResolvedCommand});
                const auto generated = generatedAt.find({state.movementEpoch, state.lastResolvedCommand});
                if (now >= Units && now < 2 * Units) {
                    if (!actual) ++result.cleanFallbacks;
                    else if (generated != generatedAt.end()) result.actualLatencies.push_back(now - generated->second);
                }
                if (now >= Units) {
                    ++result.resolved;
                    if (!actual) ++result.held;
                    else if (generated != generatedAt.end()) result.latencies.push_back(now - generated->second);
                }
                queueSamples.push_back(state.contiguousPendingCommands);
                queueSum += state.contiguousPendingCommands;
                if (queueSamples.size() > MovementBacklogSampleTicks) {
                    queueSum -= queueSamples.front();
                    queueSamples.pop_front();
                }
                if (now >= reference) {
                    if (!actual || now - previousResolution != Tick) actualRun = 0;
                    if (actual) {
                        if (actualRun == 0) actualRunStart = now;
                        if (++actualRun >= 15 && result.stableStart < 0)
                            result.stableStart = actualRunStart - reference;
                    }
                    previousResolution = now;
                    if (now >= bound) {
                        if (!actual) ++result.fallbacksAfterBound;
                        if (queueSum >= MovementBacklogCommandSum) ++result.excessiveQueueAfterBound;
                    }
                }
            }
            const auto resolved = [&](const auto& entry) {
                return entry.first.first < state.movementEpoch ||
                    (entry.first.first == state.movementEpoch && entry.first.second <= state.lastResolvedCommand);
            };
            std::erase_if(accepted, resolved);
            std::erase_if(generatedAt, resolved);
        }
    }
    result.startPhaseShiftSeconds = client.Observation().startPhaseShiftSeconds;
    result.startPhaseSkip = client.Observation().startPhaseSkip;
    return result;
}
} // namespace

TEST_CASE("PvP exhausted lead recovers 108 ms 250 ms and six second render stalls across LAN phases") {
    for (const int fps : {30, 60, 144})
        for (const int rtt : {0, 20, 40})
            for (const int stall : {108, 250, 6000})
                for (const bool impaired : {false, true})
                    for (const Time worker : {Time(0), Time(4000), Time(8000)})
                        for (const Time authority : {Time(0), Time(6000), Time(11999)})
                          for (const double firstElapsed : {0.0, 0.002}) {
                            INFO("fps ", fps, " rtt ", rtt, " stall ", stall, " impaired ", impaired,
                                " worker ", worker, " authority ", authority, " first elapsed ", firstElapsed);
                            const auto result = RunRecovery(fps, rtt, stall, impaired, worker, authority, firstElapsed);
                            CHECK(result.maximumPending <= MaxPendingCommands);
                            CHECK(result.maximumFuture <= MaxFutureCommands);
                            CHECK(result.unexpectedRejections == 0);
                            CHECK(result.resets <= 3);
                            REQUIRE(result.stableStart >= 0);
                            CHECK(result.stableStart <= 3 * Units / 2);
                            CHECK(result.fallbacksAfterBound == 0);
                            CHECK(result.resetsAfterBound == 0);
                            CHECK(result.excessiveQueueAfterBound == 0);
                        }
}

TEST_CASE("PvP a delayed first rendered frame does not establish a persistent bootstrap backlog") {
    for (const int fps : {30, 60, 144})
        for (const int rtt : {0, 20, 40})
            for (const int gapMs : {49, 51, 64, 108})
                for (const Time worker : {Time(0), Time(4000), Time(8000)})
                    for (const Time authority : {Time(0), Time(6000), Time(11999)})
                      for (const double firstElapsed : {0.0, 0.015}) {
                        INFO("fps ", fps, " rtt ", rtt, " bootstrap gap ", gapMs,
                            " worker ", worker, " authority ", authority, " first elapsed ", firstElapsed);
                        const auto result = RunRecovery(fps, rtt, 0, false, worker, authority,
                            firstElapsed, Time(gapMs) * Units / 1000);
                        CHECK(result.maximumPending <= MaxPendingCommands);
                        CHECK(result.maximumFuture <= MaxFutureCommands);
                        CHECK(result.unexpectedRejections == 0);
                        CHECK(result.resets == 0);
                        REQUIRE(result.stableStart >= 0);
                        CHECK(result.stableStart <= 3 * Units / 2);
                        CHECK(result.fallbacksAfterBound == 0);
                        CHECK(result.excessiveQueueAfterBound == 0);
                    }
}

namespace {
Time Median(std::vector<Time> values) {
    REQUIRE(!values.empty());
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}
// Product-like clocks: the worker wakes 0.175-0.525 ms late (0.35 ms per send
// measured on a Mac) and re-bases its deadline on that send, the display clock
// drifts by ppm against the steady clock, and frames jitter around it.
ClockOptions ProductClock(int frameDriftPpm, Time frameJitter, std::uint32_t seed, Time runUntil = 0) {
    ClockOptions clock;
    clock.overshootMin = Units * 175 / 1000000;
    clock.overshootMax = Units * 525 / 1000000;
    clock.frameDriftPpm = frameDriftPpm;
    clock.frameJitter = frameJitter;
    clock.seed = seed;
    clock.runUntil = runUntil;
    return clock;
}
} // namespace

TEST_CASE("PvP start phase alignment removes the start lottery at 60 FPS and above without losing Actual commands") {
    for (const int fps : {30, 60, 144})
        for (const int rtt : {0, 20, 40})
            for (const Time worker : {Time(0), Time(4000), Time(8000)}) {
                Time alignedLow = Units, alignedHigh{}, unalignedHigh{};
                for (Time authority = 0; authority < Tick; authority += Tick / 12) {
                    INFO("fps ", fps, " rtt ", rtt, " worker ", worker, " authority ", authority);
                    const auto aligned = RunRecovery(fps, rtt, 0, false, worker, authority, 0.0);
                    const auto unaligned = RunRecovery(fps, rtt, 0, false, worker, authority, 0.0, 0, false);
                    REQUIRE(aligned.firstStartWaitMicros);
                    CHECK(aligned.cleanFallbacks == 0);
                    CHECK(aligned.fallbacksAfterBound == 0);
                    CHECK(aligned.resets == 0);
                    if (fps < static_cast<int>(AuthorityTickRate)) {
                        // 30 FPS is below the cut (about 54.5 FPS): the reported wait is
                        // not applied and this is the unaligned run.
                        CHECK(aligned.startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
                        CHECK_FALSE(aligned.startPhaseShiftSeconds);
                        CHECK(aligned.latencies == unaligned.latencies);
                        continue;
                    }
                    CHECK(aligned.startPhaseShiftSeconds);
                    CHECK_FALSE(aligned.startPhaseSkip);
                    const auto alignedMedian = Median(aligned.actualLatencies);
                    const auto unalignedMedian = Median(unaligned.actualLatencies);
                    // Never later than the unaligned start; frames only quantize the gain.
                    CHECK(alignedMedian <= unalignedMedian);
                    alignedLow = (std::min)(alignedLow, alignedMedian);
                    alignedHigh = (std::max)(alignedHigh, alignedMedian);
                    unalignedHigh = (std::max)(unalignedHigh, unalignedMedian);
                }
                if (fps < static_cast<int>(AuthorityTickRate)) continue;
                INFO("fps ", fps, " rtt ", rtt, " worker ", worker);
                // The worst start phase gains at least a third of a tick at every frame rate.
                CHECK(alignedHigh + Tick / 3 <= unalignedHigh);
                // Frames finer than a tick leave only their own quantization.
                if (fps > static_cast<int>(AuthorityTickRate)) CHECK(alignedHigh - alignedLow <= Units / fps);
            }
}

// With the shift applied at 30 and 40 FPS the shift landed on whole frames and
// the older command of each frame pair kept only the target slack; send-phase
// drift then substituted 10-14 % of commands at RTT 20/40. Below about
// 54.5 FPS (a 32-interval mean above 1.1 tick) the Client keeps the unaligned
// phase, so receiving the wait changes nothing. A vsync-paced display gets
// there by dropping whole refreshes. An estimate that discarded the longest
// intervals missed those drops and kept the shift at 48-50 FPS, where 13-15 of
// 18 starts then reset.
TEST_CASE("PvP below about 54.5 FPS the start phase stays unaligned under drifting product clocks") {
    struct Model { const char* name; int fps; std::vector<int> refreshPattern; unsigned missedPerMille; };
    const Model models[] = {{"steady 30 FPS", 30, {}, 0}, {"steady 40 FPS", 40, {}, 0},
        {"vsync 50 FPS (1,1,1,1,2)", 60, {1, 1, 1, 1, 2}, 0}, {"vsync 48 FPS (1,1,1,2)", 60, {1, 1, 1, 2}, 0},
        {"vsync 45 FPS (1,1,2)", 60, {1, 1, 2}, 0},
        {"vsync 50 FPS random misses", 60, {}, 200}, {"vsync 46 FPS random misses", 60, {}, 300}};
    for (const auto& model : models)
        for (const int rtt : {0, 20, 40})
            for (const Time worker : {Time(0), Time(4000), Time(8000)})
                for (Time authority = 0; authority < Tick; authority += Tick / 6) {
                    INFO(std::string(model.name), " rtt ", rtt, " worker ", worker, " authority ", authority);
                    const bool vsync = model.fps == 60;
                    // A vsync display's frames sit on its 59.94 Hz refresh grid; only their
                    // timestamps jitter. Steady low rates jitter freely around their grid.
                    auto clock = ProductClock(1000, vsync ? Units / 2000 : Units * 15 / 10000,
                        std::uint32_t(worker * 31 + authority * 7 + model.fps * 131 + rtt + model.missedPerMille + 1),
                        11 * Units);
                    clock.refreshPattern = model.refreshPattern;
                    clock.missedRefreshPerMille = model.missedPerMille;
                    const auto aligned = RunRecovery(model.fps, rtt, 0, false, worker, authority, 0.0, 0, true, clock);
                    const auto unaligned = RunRecovery(model.fps, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                    REQUIRE(aligned.firstStartWaitMicros);
                    REQUIRE(aligned.resolved > 0);
                    CHECK(aligned.resolved == unaligned.resolved);
                    // Never applied while the frame rate measure is below the cut.
                    CHECK(aligned.shiftedBelowCut == 0);
                    REQUIRE(aligned.firstWithdrawn);
                    if (model.missedPerMille) {
                        // Random misses leave short runs of whole 60 Hz frames: the
                        // shift may return briefly, never as a reset or a lost command.
                        CHECK(aligned.resets <= unaligned.resets);
                        CHECK(aligned.held * 1000 <= unaligned.held * 1000 + aligned.resolved * 5);
                        continue;
                    }
                    // A steady or patterned rate below the cut never applies the shift.
                    CHECK(aligned.startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
                    CHECK_FALSE(aligned.startPhaseShiftSeconds);
                    CHECK(aligned.shiftedFrames == 0);
                    CHECK(aligned.held == unaligned.held);
                    CHECK(aligned.latencies == unaligned.latencies);
                    CHECK(aligned.resets == unaligned.resets);
                    // Frame jitter alone substitutes a few commands in a few unaligned
                    // starts; dropped refreshes do so at any phase (Held, not resets).
                    if (!vsync) CHECK(aligned.held * 100 <= aligned.resolved);
                    if (!vsync) CHECK(aligned.resets == 0);
                }
}

// Startup hitches put the first decision below the cut. The shift is armed
// withdrawn and applied once frames recover, so the epoch still gains the
// alignment instead of staying unaligned for its whole life. The hitches alone
// reset a few starts (aligned or not); the alignment adds none.
TEST_CASE("PvP an epoch armed during startup hitches aligns once frames recover") {
    for (const int rtt : {0, 20, 40})
        for (const Time worker : {Time(0), Time(4000), Time(8000)}) {
            Time alignedHigh{}, unalignedHigh{};
            for (Time authority = 0; authority < Tick; authority += Tick / 6) {
                INFO("rtt ", rtt, " worker ", worker, " authority ", authority);
                auto clock = ProductClock(0, Units / 2000, std::uint32_t(worker * 31 + authority * 7 + rtt + 5), 4 * Units);
                clock.startupHitches = 4;
                clock.startupHitch = 2 * Tick;
                const auto aligned = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, true, clock);
                const auto unaligned = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                REQUIRE(aligned.firstWithdrawn);
                REQUIRE(aligned.firstShifted);
                CHECK(*aligned.firstShifted > *aligned.firstWithdrawn);
                // The four hitches leave the 32-frame window about 35 frames after
                // the client starts; the restore bound then holds for 32 frames.
                CHECK(*aligned.firstShifted <= *aligned.firstWithdrawn + 70 * Tick);
                CHECK(aligned.shiftedBelowCut == 0);
                CHECK(aligned.startPhaseShiftSeconds);
                CHECK_FALSE(aligned.startPhaseSkip);
                CHECK(aligned.resets <= unaligned.resets);
                CHECK(aligned.held == 0);
                const auto alignedMedian = Median(aligned.latencies);
                const auto unalignedMedian = Median(unaligned.latencies);
                const Time loosened = *aligned.firstStartWaitMicros <
                    MovementStartPhaseTargetSeconds * 1.0e6 ? Tick : 0;
                CHECK(alignedMedian <= unalignedMedian + loosened + Units / 1000);
                alignedHigh = (std::max)(alignedHigh, alignedMedian);
                unalignedHigh = (std::max)(unalignedHigh, unalignedMedian);
            }
            INFO("rtt ", rtt, " worker ", worker);
            CHECK(alignedHigh + Tick / 3 <= unalignedHigh);
        }
}

// The product GUI loop on a Mac runs at about 58 FPS (median 1.033 tick) after
// its startup hitches, which put the decision below the cut. The shift returns
// only if the restore bound (1.06 tick, about 56.6 FPS) stays above that
// cadence: 17.18 ms frames (1.031 tick) with frame jitter, and 17.27 ms (1.036
// tick). A restore bound of 1.03 tick would never restore here.
TEST_CASE("PvP after startup hitches a 58 FPS GUI cadence restores the start phase within 64 frames") {
    for (const int frameDriftPpm : {30800, 36000})
        for (const int rtt : {0, 20, 40})
            for (const Time worker : {Time(0), Time(4000), Time(8000)})
                for (Time authority = 0; authority < Tick; authority += Tick / 6) {
                    const Time frame = Tick + Tick * frameDriftPpm / 1000000;
                    INFO("frame ", frame, " units, rtt ", rtt, " worker ", worker, " authority ", authority);
                    auto clock = ProductClock(frameDriftPpm, Units * 3 / 10000,
                        std::uint32_t(worker * 31 + authority * 7 + rtt + frameDriftPpm + 9), 4 * Units);
                    clock.startupHitches = 4;
                    clock.startupHitch = 2 * Tick;
                    const auto aligned = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, true, clock);
                    const auto unaligned = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                    REQUIRE(aligned.firstWithdrawn);
                    REQUIRE(aligned.firstShifted);
                    CHECK(*aligned.firstShifted > *aligned.firstWithdrawn);
                    // The decision comes about eight frames after the client starts; the
                    // four hitches leave the 32-interval window about 28 frames later,
                    // and the mean then holds the restore bound for the 32-frame dwell.
                    CHECK(*aligned.firstShifted <= *aligned.firstWithdrawn + 64 * frame);
                    CHECK(aligned.shiftedBelowCut == 0);
                    CHECK(aligned.startPhaseShiftSeconds);
                    CHECK_FALSE(aligned.startPhaseSkip);
                    CHECK(aligned.resets <= unaligned.resets);
                    CHECK(aligned.held == 0);
                }
}

TEST_CASE("PvP at 59.94 60 and 144 FPS the start phase still aligns under drifting product clocks") {
    struct Rate { int fps; int frameDriftPpm; };
    for (const auto rate : {Rate{60, 1000}, Rate{60, 0}, Rate{144, 0}})
        for (const int rtt : {0, 20, 40})
            for (const Time worker : {Time(0), Time(4000), Time(8000)}) {
                Time alignedHigh{}, unalignedHigh{};
                for (Time authority = 0; authority < Tick; authority += Tick / 12) {
                    INFO("fps ", rate.fps, " drift ppm ", rate.frameDriftPpm, " rtt ", rtt,
                         " worker ", worker, " authority ", authority);
                    // Ordinary frame jitter of +-1 ms must not read as a lower frame rate.
                    const auto clock = ProductClock(rate.frameDriftPpm, Units / 1000,
                        std::uint32_t(worker * 31 + authority * 7 + rate.fps * 131 + rtt + rate.frameDriftPpm));
                    const auto aligned = RunRecovery(rate.fps, rtt, 0, false, worker, authority, 0.0, 0, true, clock);
                    const auto unaligned = RunRecovery(rate.fps, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                    REQUIRE(aligned.startPhaseShiftSeconds);
                    CHECK_FALSE(aligned.startPhaseSkip);
                    CHECK(aligned.held == 0);
                    CHECK(aligned.resets == 0);
                    const auto alignedMedian = Median(aligned.latencies);
                    const auto unalignedMedian = Median(unaligned.latencies);
                    // A start already tighter than the target is loosened to it, which a
                    // frame boundary can turn into one more frame; jitter moves a median by 1 ms.
                    const Time loosened = *aligned.firstStartWaitMicros <
                        MovementStartPhaseTargetSeconds * 1.0e6 ? Units / rate.fps : 0;
                    CHECK(alignedMedian <= unalignedMedian + loosened + Units / 1000);
                    alignedHigh = (std::max)(alignedHigh, alignedMedian);
                    unalignedHigh = (std::max)(unalignedHigh, unalignedMedian);
                }
                INFO("fps ", rate.fps, " drift ppm ", rate.frameDriftPpm, " rtt ", rtt, " worker ", worker);
                CHECK(alignedHigh + Tick / 3 <= unalignedHigh);
            }
}

// A 60 Hz display that drops one refresh in N frames (N = 12-30, 55.3-58.1 FPS
// at 59.94 Hz) keeps every 32-interval mean at or below about 1.095 tick,
// inside the cut; the approved policy aligns it. This pins that behaviour so
// any change to it is deliberate. Once the frame window holds more than ten
// intervals one dropped refresh no longer reaches the cut, so a shift taken
// with no drop among the first ten intervals stays for the whole run, as the
// unguarded alignment did (at RTT 20/40 that costs Held commands and
// starvation resets the unaligned phase does not have). The decision waits for
// eight intervals, so a drop among the first nine reads as 1.11-1.13 tick and
// withdraws the shift; every later window holding two drops (about 1.064 tick)
// then breaks the restore dwell and that epoch stays unaligned. A drop at the
// tenth interval sits on the cut and is not pinned.
TEST_CASE("PvP a 60 Hz display dropping one refresh in 12-30 frames keeps the start phase state it took") {
    for (std::size_t every = 12; every <= 30; ++every)
        for (const std::size_t first : {std::size_t(1), std::size_t(9), std::size_t(11), every})
            for (const int rtt : {0, 20, 40})
                for (const auto& [worker, authority] : {std::pair{Time(0), Time(0)}, std::pair{Time(8000), Tick / 2}}) {
                    INFO("one refresh dropped in ", every, " frames from interval ", first, " rtt ", rtt,
                         " worker ", worker, " authority ", authority);
                    auto clock = ProductClock(1000, Units / 2000,
                        std::uint32_t(worker * 31 + authority * 7 + Time(every) * 131 + rtt + Time(first) * 17 + 1), 4 * Units);
                    clock.droppedRefreshEvery = every;
                    clock.firstDroppedRefresh = first;
                    const auto aligned = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, true, clock);
                    REQUIRE(aligned.firstStartWaitMicros);
                    REQUIRE(aligned.decidedFrames > 0);
                    CHECK(aligned.shiftedBelowCut == 0);
                    if (first > 10) {
                        // Applied at every decision and never withdrawn. A starvation
                        // reset may leave the last epoch still undecided.
                        CHECK_FALSE(aligned.firstWithdrawn);
                        CHECK(aligned.shiftedFrames == aligned.decidedFrames);
                        CHECK_FALSE(aligned.startPhaseSkip);
                        continue;
                    }
                    // Withdrawn at or right after the decision and never restored: the
                    // run is the unaligned one.
                    const auto unaligned = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                    REQUIRE(aligned.firstWithdrawn);
                    if (aligned.firstShifted) CHECK(*aligned.firstShifted < *aligned.firstWithdrawn);
                    CHECK(aligned.shiftedFrames <= 1);
                    CHECK_FALSE(aligned.startPhaseShiftSeconds);
                    CHECK(aligned.startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
                    CHECK(aligned.resets == unaligned.resets);
                    CHECK(aligned.held == unaligned.held);
                    CHECK(aligned.latencies == unaligned.latencies);
                }
}
