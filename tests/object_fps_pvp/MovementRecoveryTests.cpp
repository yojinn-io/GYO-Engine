#include <doctest/doctest.h>

#include "RetroFPS/Pvp/ClientSimulation.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"

#include <algorithm>
#include <chrono>
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
    // before any stall.
    std::vector<Time> actualLatencies;
    unsigned cleanFallbacks{};
    // Every resolution from 1 s to the end: substituted (Held) commands and the
    // generation-to-execution time of Actual ones, also split into the first
    // ten seconds from 21 s (after the first window) and the last ten of a long run.
    unsigned resolved{}, held{};
    std::vector<Time> latencies, earlyLatencies, lateLatencies;
    // Phase tracking as the run ended, and when it first decided.
    std::uint32_t corrections{}, lateCorrections{};
    std::optional<Time> firstDecision;
    // Input packets the worker sent, and the most in any one second.
    unsigned inputPackets{}, maximumInputPacketsPerSecond{};
};

// Clock model. By default the worker is phase-locked: it wakes exactly at its
// deadline. A product worker mirrors ClientConnection.cpp: each sleep wakes
// late by a seeded overshoot and sends draw from the 60/s token bucket of two
// tokens (a window with a never-sent command goes at the next poll; an
// unchanged one at the deadline, sentAt + period, with a token in reserve).
// Frames may drift against the steady clock (a display clock in ppm) and
// jitter uniformly around that grid; the Client's own clock may run fast or
// slow against the Host's (clientPpm), which scales both its frame-time
// measurements and its worker period. A vsync-paced display shows each frame
// for whole refreshes (one refresh is Units / fps): a cycled pattern of
// refresh counts, plus one more refresh missed at random per mille, or one
// more refresh on every Nth frame interval the client counts from a chosen
// first one (interval 1 is the elapsed time its second active frame reports).
// The first frames after the client starts may instead be startup hitches.
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
    int clientPpm{};
};

// Only public product command windows and snapshots cross this virtual wire.
// The product MatchRuntimeHost runs on the virtual clock, so its movement
// slack samples are the ones the Client tracks. Worker receipt/ACK pruning
// remains live during render stalls. Prediction is reconciled once per render
// with the latest received snapshot, just as in the application. A client
// cannot bootstrap from a synthetic pre-tick snapshot. Without hostSamples
// the snapshots lose their slack samples: the untracked, seeded phase.
// Path::V6 drives LocalPlayerPrediction with the newest snapshot of each frame
// only; Path::Product runs the application's ClientSimulation, which also takes
// the phase sample of every older snapshot drained since the previous frame.
enum class Path { V6, Product };

RecoveryResult RunRecovery(int fps, int rttMs, int stallMs, bool impaired,
                           Time workerPhase, Time authorityPhase, double firstElapsed,
                           Time bootstrapGap = 0, bool hostSamples = true,
                           const ClockOptions& clock = {}, Path path = Path::V6) {
    // The product path samples elapsed time itself; its first sample is zero.
    REQUIRE((path == Path::V6 || firstElapsed == 0.0));
    const auto arena = RecoveryArena();
    Time now{};
    MatchRuntimeHost host(arena, [&] {
        return std::chrono::steady_clock::time_point(std::chrono::nanoseconds(now * 12500 / 9));
    });
    REQUIRE(host.QueueJoin(1, 1));
    LocalPlayerPrediction client(arena);
    ClientSimulation simulation(arena);
    // Snapshots the worker drained since the previous frame (product path) and the
    // Client's own clock, advanced by each frame's measured interval.
    std::vector<WorldSnapshot> drained;
    std::chrono::steady_clock::time_point clientClock{};
    const auto observation = [&]() -> const LocalMovementObservation& {
        return path == Path::Product ? simulation.Observation() : client.Observation();
    };
    const auto pending = [&] { return path == Path::Product ? simulation.PendingInput() : client.PendingInput(); };
    struct InputPacket { Time due; PlayerInput input; };
    struct SnapshotPacket { Time due; WorldSnapshot snapshot; };
    std::vector<InputPacket> inputs;
    std::vector<SnapshotPacket> snapshots;
    std::map<std::pair<std::uint64_t, std::uint64_t>, MovementCommand> accepted;
    PlayerInput published;
    WorldSnapshot received;
    std::optional<PlayerState> authority;
    RecoveryResult result;
    std::uint32_t randomState = 0x425682;
    const auto random = [&] {
        randomState = randomState * 1664525U + 1013904223U;
        return randomState;
    };
    const Time resume = 2 * Units + Time(stallMs) * Units / 1000;
    const Time reference = (std::max)(resume, impaired ? 4 * Units : Time(0));
    const Time bound = reference + 3 * Units / 2;
    const auto delay = [&](Time at) {
        const auto jitter = impaired && at < 4 * Units ? int(random() % 21) - 10 : 0;
        return (std::max)(Time(0), Time(rttMs) * Units / 2000 + jitter * Units / 1000);
    };
    std::uint32_t clockState = clock.seed;
    const auto uniform = [&](Time low, Time high) {
        if (high <= low) return low;
        clockState = clockState * 1664525U + 1013904223U;
        return low + Time((clockState >> 8) % std::uint32_t(high - low + 1));
    };
    const double clientRate = 1.0 + clock.clientPpm * 1.0e-6;
    const Time framePeriod = Units / fps + Units / fps * clock.frameDriftPpm / 1000000;
    const Time workerPeriod = Time(double(Tick) / clientRate + 0.5);
    const Time runEnd = clock.runUntil ? clock.runUntil : reference + 3 * Units;
    Time frameGrid{};
    std::size_t frameIndex{}, activeFrames{};
    Time nextFrame{}, nextWorker = workerPhase, nextTick = authorityPhase;
    Time previousFrame{}, nextSend{};
    bool haveSent{};
    double tokens = InputSendBurst;
    Time tokensAt{};
    std::uint64_t sentEpoch{}, sentLife{}, sentThrough{};
    std::map<Time, unsigned> packetsPerSecond;
    bool firstAdvance = true;
    unsigned inputNumber{}, resetSnapshotsToDrop{};
    std::uint64_t previousEpoch = 1;
    std::deque<std::uint32_t> queueSamples;
    std::uint32_t queueSum{};
    unsigned actualRun{};
    Time actualRunStart{}, previousResolution{-Tick};
    std::map<std::pair<std::uint64_t, std::uint64_t>, Time> generatedAt;
    for (;;) {
        now = (std::min)({nextFrame, nextWorker, nextTick});
        for (const auto& packet : inputs) now = (std::min)(now, packet.due);
        if (now > runEnd) break;
        for (auto packet = inputs.begin(); packet != inputs.end();) {
            if (packet->due > now) { ++packet; continue; }
            auto input = std::move(packet->input);
            packet = inputs.erase(packet);
            REQUIRE(authority);
            if (host.SubmitInput(input)) {
                for (const auto& command : input.commands)
                    if (input.movementEpoch == authority->movementEpoch && command.sequence > authority->lastResolvedCommand)
                        accepted.try_emplace(std::pair{input.movementEpoch, command.sequence}, command);
            } else if (input.movementEpoch < authority->movementEpoch) {
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
                if (path == Path::Product) drained.push_back(snapshot);
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
            if (!published.commands.empty()) {
                tokens = (std::min)(InputSendBurst, tokens + double(now - tokensAt) / double(workerPeriod));
                tokensAt = now;
                const bool fresh = !haveSent || published.movementEpoch != sentEpoch ||
                    published.lifeGeneration != sentLife || published.commands.back().sequence > sentThrough;
                if ((fresh && tokens >= 1) || (now >= nextSend && tokens >= InputSendBurst)) {
                    tokens -= 1;
                    ++inputNumber;
                    ++result.inputPackets;
                    ++packetsPerSecond[now / Units];
                    const bool deliver = !impaired || (inputNumber > 2 &&
                        (now >= 4 * Units || random() % 20 != 0));
                    if (deliver) {
                        inputs.push_back({now + delay(now), published});
                        if (impaired && now < 4 * Units && inputNumber % 17 == 0)
                            inputs.push_back({now + delay(now) + 45 * Units / 1000, published});
                    }
                    haveSent = true;
                    nextSend = now + workerPeriod;
                    sentEpoch = published.movementEpoch;
                    sentLife = published.lifeGeneration;
                    sentThrough = published.commands.back().sequence;
                }
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
                // The Client measures frame time on its own clock.
                const double elapsed = firstAdvance && !received.players.empty() ? firstElapsed :
                    double(now - previousFrame) / Units * clientRate;
                if (path == Path::V6) {
                    if (!received.players.empty())
                        client.Reconcile(received.players.front(), received.tick);
                    if (client.Advance(elapsed, 1, 0, 0, 0)) published = client.PendingInput();
                } else {
                    for (const auto& snapshot : drained) simulation.ObserveSample(snapshot, 1);
                    drained.clear();
                    if (!received.players.empty())
                        simulation.Observe(received.players.front(), received.tick, std::nullopt);
                    clientClock += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(elapsed));
                    // Only an active client runs the frame, as in the application.
                    if (!received.players.empty())
                        if (auto window = simulation.Frame(clientClock, {1, 0, 0, 0})) published = *window;
                }
                if (firstAdvance && !received.players.empty() && bootstrapGap)
                    nextFrame = frameGrid = now + bootstrapGap;
                for (const auto& command : pending().commands)
                    generatedAt.try_emplace({pending().movementEpoch, command.sequence}, now);
                if (!received.players.empty()) {
                    firstAdvance = false;
                    ++activeFrames;
                    if (observation().phaseErrorSeconds && !result.firstDecision) result.firstDecision = now;
                }
                previousFrame = now;
                result.maximumPending = (std::max)(result.maximumPending, pending().commands.size());
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
            const auto before = authority;
            REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
            auto snapshot = host.TakeSnapshot();
            REQUIRE(snapshot);
            REQUIRE(snapshot->players.size() == 1);
            auto& state = snapshot->players.front();
            authority = state;
            if (!hostSamples) {
                state.movementSlackSequence.reset();
                state.movementSlackMicros.reset();
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
                snapshots.push_back({now + delay(now), *snapshot});
            if (before && state.movementEpoch == before->movementEpoch &&
                state.lastResolvedCommand > before->lastResolvedCommand) {
                const bool actual = accepted.contains({state.movementEpoch, state.lastResolvedCommand});
                const auto generated = generatedAt.find({state.movementEpoch, state.lastResolvedCommand});
                if (now >= Units && now < 2 * Units) {
                    if (!actual) ++result.cleanFallbacks;
                    else if (generated != generatedAt.end()) result.actualLatencies.push_back(now - generated->second);
                }
                if (now >= Units) {
                    ++result.resolved;
                    if (!actual) ++result.held;
                    else if (generated != generatedAt.end()) {
                        result.latencies.push_back(now - generated->second);
                        if (now >= 21 * Units && now < 31 * Units) result.earlyLatencies.push_back(now - generated->second);
                        if (now >= runEnd - 10 * Units) result.lateLatencies.push_back(now - generated->second);
                    }
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
    result.corrections = observation().phaseCorrections;
    result.lateCorrections = observation().phaseLateCorrections;
    for (const auto& [second, count] : packetsPerSecond)
        result.maximumInputPacketsPerSecond = (std::max)(result.maximumInputPacketsPerSecond, count);
    return result;
}
} // namespace

TEST_CASE("PvP exhausted lead recovers 108 ms 250 ms and six second render stalls across LAN phases") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
        for (const int fps : {30, 60, 144})
            for (const int rtt : {0, 20, 40})
                for (const int stall : {108, 250, 6000})
                    for (const bool impaired : {false, true})
                        for (const Time worker : {Time(0), Time(4000), Time(8000)})
                            for (const Time authority : {Time(0), Time(6000), Time(11999)})
                              for (const double firstElapsed : {0.0, 0.002}) {
                                INFO("fps ", fps, " rtt ", rtt, " stall ", stall, " impaired ", impaired,
                                    " worker ", worker, " authority ", authority, " first elapsed ", firstElapsed);
                                if (path == Path::Product && firstElapsed != 0.0) continue;
                                const auto result = RunRecovery(fps, rtt, stall, impaired, worker, authority, firstElapsed,
                                    0, true, {}, path);
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
}

TEST_CASE("PvP a delayed first rendered frame does not establish a persistent bootstrap backlog") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
        for (const int fps : {30, 60, 144})
            for (const int rtt : {0, 20, 40})
                for (const int gapMs : {49, 51, 64, 108})
                    for (const Time worker : {Time(0), Time(4000), Time(8000)})
                        for (const Time authority : {Time(0), Time(6000), Time(11999)})
                          for (const double firstElapsed : {0.0, 0.015}) {
                            INFO("fps ", fps, " rtt ", rtt, " bootstrap gap ", gapMs,
                                " worker ", worker, " authority ", authority, " first elapsed ", firstElapsed);
                            if (path == Path::Product && firstElapsed != 0.0) continue;
                            const auto result = RunRecovery(fps, rtt, 0, false, worker, authority,
                                firstElapsed, Time(gapMs) * Units / 1000, true, {}, path);
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
}

namespace {
Time Median(std::vector<Time> values) {
    REQUIRE(!values.empty());
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}
// Product-like clocks: the worker wakes 0.175-0.525 ms late (0.35 ms per send
// measured on a Mac), the display clock drifts by ppm against the steady
// clock, and frames jitter around it.
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

TEST_CASE("PvP phase tracking removes the start lottery at every frame rate without losing Actual commands") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
        for (const int fps : {30, 60, 144})
            for (const int rtt : {0, 20, 40})
                for (const Time worker : {Time(0), Time(4000), Time(8000)}) {
                    Time trackedHigh{}, untrackedHigh{};
                    for (Time authority = 0; authority < Tick; authority += Tick / 12) {
                        INFO("fps ", fps, " rtt ", rtt, " worker ", worker, " authority ", authority);
                        const auto tracked = RunRecovery(fps, rtt, 0, false, worker, authority, 0.0, 0, true, {}, path);
                        const auto untracked = RunRecovery(fps, rtt, 0, false, worker, authority, 0.0, 0, false);
                        REQUIRE(tracked.firstDecision);
                        CHECK(tracked.cleanFallbacks == 0);
                        CHECK(tracked.fallbacksAfterBound == 0);
                        CHECK(tracked.held == 0);
                        CHECK(tracked.resets == 0);
                        const auto trackedMedian = Median(tracked.actualLatencies);
                        const auto untrackedMedian = Median(untracked.actualLatencies);
                        // A start tighter than the target is loosened to it, which a frame
                        // boundary can turn into one more frame.
                        CHECK(trackedMedian <= untrackedMedian + Units / fps);
                        trackedHigh = (std::max)(trackedHigh, trackedMedian);
                        untrackedHigh = (std::max)(untrackedHigh, untrackedMedian);
                    }
                    INFO("fps ", fps, " rtt ", rtt, " worker ", worker);
                    // The worst start phase gains at least a third of a tick at every frame rate.
                    CHECK(trackedHigh + Tick / 3 <= untrackedHigh);
                }
    }
}

// With A1 (a single shift) at 30-50 FPS the shift landed on whole frames and
// the older command of each frame pair waited for the drifting worker
// deadline; 10-14 % of commands were substituted at RTT 20/40, so A1 stayed
// unaligned below about 54.5 FPS. Tracking measures each command against the
// age it really had, and the worker sends new commands at once, so these
// rates align with no command lost and no reset, under drifting product clocks.
TEST_CASE("PvP at 30-50 FPS and on vsync drops phase tracking aligns without Held commands under product clocks") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
        struct Model { const char* name; int fps; std::vector<int> refreshPattern; unsigned missedPerMille; };
        const Model models[] = {{"steady 30 FPS", 30, {}, 0}, {"steady 40 FPS", 40, {}, 0},
            {"vsync 50 FPS (1,1,1,1,2)", 60, {1, 1, 1, 1, 2}, 0}, {"vsync 48 FPS (1,1,1,2)", 60, {1, 1, 1, 2}, 0},
            {"vsync 45 FPS (1,1,2)", 60, {1, 1, 2}, 0},
            {"vsync 50 FPS random misses", 60, {}, 200}, {"vsync 46 FPS random misses", 60, {}, 300}};
        for (const auto& model : models)
            for (const int rtt : {0, 20, 40})
                for (const Time worker : {Time(0), Time(4000), Time(8000)})
                    for (Time authority = 0; authority < Tick; authority += Tick / 3) {
                        INFO(std::string(model.name), " rtt ", rtt, " worker ", worker, " authority ", authority);
                        const bool vsync = model.fps == 60;
                        auto clock = ProductClock(1000, vsync ? Units / 2000 : Units * 15 / 10000,
                            std::uint32_t(worker * 31 + authority * 7 + model.fps * 131 + rtt + model.missedPerMille + 1),
                            11 * Units);
                        clock.refreshPattern = model.refreshPattern;
                        clock.missedRefreshPerMille = model.missedPerMille;
                        const auto tracked = RunRecovery(model.fps, rtt, 0, false, worker, authority, 0.0, 0, true, clock, path);
                        const auto untracked = RunRecovery(model.fps, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                        REQUIRE(tracked.firstDecision);
                        REQUIRE(tracked.resolved > 0);
                        // The untracked baseline may lose ticks to resets; tracking never does.
                        CHECK(tracked.resolved >= untracked.resolved);
                        CHECK(tracked.resets == 0);
                        // Two steps per frame: the older one may keep only the target slack,
                        // so frame jitter can substitute an occasional command (at most 0.5 %).
                        CHECK(tracked.held * 1000 <= untracked.held * 1000 + tracked.resolved * 5);
                        if (vsync && !model.missedPerMille) CHECK(tracked.held == 0);
                        CHECK(Median(tracked.latencies) <= Median(untracked.latencies) + Units / 60);
                        CHECK(tracked.maximumInputPacketsPerSecond <= InputSendRate + 1);
                    }
    }
}

// Startup hitches do not delay tracking beyond its frame and sample evidence:
// the first decision comes within about ten frames, without Held commands.
TEST_CASE("PvP phase tracking decides soon after startup hitches and a 58 FPS GUI cadence") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
        for (const int frameDriftPpm : {0, 30800, 36000})
            for (const int rtt : {0, 20, 40})
                for (const Time worker : {Time(0), Time(4000), Time(8000)})
                    for (Time authority = 0; authority < Tick; authority += Tick / 6) {
                        const Time frame = Tick + Tick * frameDriftPpm / 1000000;
                        INFO("frame ", frame, " units, rtt ", rtt, " worker ", worker, " authority ", authority);
                        auto clock = ProductClock(frameDriftPpm, Units * 3 / 10000,
                            std::uint32_t(worker * 31 + authority * 7 + rtt + frameDriftPpm + 9), 4 * Units);
                        clock.startupHitches = 4;
                        clock.startupHitch = 2 * Tick;
                        const auto tracked = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, true, clock, path);
                        const auto untracked = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                        REQUIRE(tracked.firstDecision);
                        CHECK(*tracked.firstDecision <= 4 * 2 * Tick + 12 * frame + Time(rtt) * Units / 1000 + 4 * Tick);
                        CHECK(tracked.resets <= untracked.resets);
                        CHECK(tracked.held == 0);
                    }
    }
}

TEST_CASE("PvP at 59.94 60 and 144 FPS phase tracking still aligns under drifting product clocks") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
        struct Rate { int fps; int frameDriftPpm; };
        for (const auto rate : {Rate{60, 1000}, Rate{60, 0}, Rate{144, 0}})
            for (const int rtt : {0, 20, 40})
                for (const Time worker : {Time(0), Time(4000), Time(8000)}) {
                    Time trackedHigh{}, untrackedHigh{};
                    for (Time authority = 0; authority < Tick; authority += Tick / 12) {
                        INFO("fps ", rate.fps, " drift ppm ", rate.frameDriftPpm, " rtt ", rtt,
                             " worker ", worker, " authority ", authority);
                        // Ordinary frame jitter of +-1 ms.
                        const auto clock = ProductClock(rate.frameDriftPpm, Units / 1000,
                            std::uint32_t(worker * 31 + authority * 7 + rate.fps * 131 + rtt + rate.frameDriftPpm));
                        const auto tracked = RunRecovery(rate.fps, rtt, 0, false, worker, authority, 0.0, 0, true, clock, path);
                        const auto untracked = RunRecovery(rate.fps, rtt, 0, false, worker, authority, 0.0, 0, false, clock);
                        REQUIRE(tracked.firstDecision);
                        CHECK(tracked.held == 0);
                        CHECK(tracked.resets == 0);
                        const auto trackedMedian = Median(tracked.latencies);
                        const auto untrackedMedian = Median(untracked.latencies);
                        CHECK(trackedMedian <= untrackedMedian + Units / rate.fps + Units / 1000);
                        trackedHigh = (std::max)(trackedHigh, trackedMedian);
                        untrackedHigh = (std::max)(untrackedHigh, untrackedMedian);
                    }
                    INFO("fps ", rate.fps, " drift ppm ", rate.frameDriftPpm, " rtt ", rtt, " worker ", worker);
                    CHECK(trackedHigh + Tick / 3 <= untrackedHigh);
                }
    }
}

// A 60 Hz display that drops one refresh in N frames (N = 12-30, 55.3-58.1
// FPS). With A1 an aligned start then lost commands to starvation resets at
// RTT 20/40 (the late, deadline-delayed command of a long frame missed its
// tick). Tracking with the token-bucket worker aligns every start without
// a reset or a substituted command, wherever the first drop falls.
TEST_CASE("PvP a 60 Hz display dropping one refresh in 12-30 frames aligns without resets") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
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
                        const auto tracked = RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, true, clock, path);
                        REQUIRE(tracked.firstDecision);
                        CHECK(tracked.resets == 0);
                        CHECK(tracked.held == 0);
                    }
    }
}

// Client and Host clocks differ by ppm on separate machines. A1 measured the
// phase once, so the slack drifted for the whole epoch (about 8 ms per
// 15 minutes per 10 ppm) until a Backlog or Starvation reset. Five minutes at
// +-200 ppm drift 60 ms: the untracked phase resets, tracking keeps
// correcting and its latency stays within a frame, with no command lost.
TEST_CASE("PvP phase tracking holds the latency while the Client clock drifts against the Host") {
    for (const Path path : {Path::V6, Path::Product}) {
        INFO("path ", std::string(path == Path::Product ? "product" : "v6"));
        for (const int ppm : {-200, 200})
            for (const int rtt : {0, 20, 40}) {
                INFO("client clock ppm ", ppm, " rtt ", rtt);
                auto clock = ProductClock(0, Units / 2000, std::uint32_t(rtt * 7 + ppm + 1000), 300 * Units);
                clock.clientPpm = ppm;
                const auto tracked = RunRecovery(60, rtt, 0, false, Time(4000), Time(6000), 0.0, 0, true, clock, path);
                const auto untracked = RunRecovery(60, rtt, 0, false, Time(4000), Time(6000), 0.0, 0, false, clock);
                REQUIRE(tracked.firstDecision);
                CHECK(tracked.corrections >= 10);
                CHECK(tracked.held == 0);
                CHECK(tracked.resets == 0);
                CHECK(untracked.resets > 0);
                const auto early = Median(tracked.earlyLatencies);
                const auto late = Median(tracked.lateLatencies);
                CHECK(late <= early + Units / 60);
                CHECK(late + Units / 60 >= early);
                CHECK(tracked.maximumInputPacketsPerSecond <= InputSendRate + 1);
            }
    }
}
