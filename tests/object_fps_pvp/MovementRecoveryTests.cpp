#include <doctest/doctest.h>

#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
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
};

// Only public product command windows and snapshots cross this virtual wire.
// Worker receipt/ACK pruning remains live during render stalls. Prediction is
// reconciled once per render with the latest received snapshot, just as in the
// application. A client cannot bootstrap from a synthetic pre-tick snapshot.
// Like the product Host, receipt of an epoch's sequence 1 is timed to the tick
// that executes it and that wait rides on the epoch's later snapshots.
RecoveryResult RunRecovery(int fps, int rttMs, int stallMs, bool impaired,
                           Time workerPhase, Time authorityPhase, double firstElapsed,
                           Time bootstrapGap = 0, bool hostStartWait = true) {
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
        if (now > reference + 3 * Units) break;
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
                (std::min)(Poll, nextSend - now) : Poll);
        }
        if (now == nextFrame) {
            nextFrame += Units / fps;
            if (now < 2 * Units || now >= resume) {
                if (!received.players.empty())
                    client.Reconcile(received.players.front(), received.tick);
                const double elapsed = firstAdvance && !received.players.empty() ? firstElapsed :
                    double(now - previousFrame) / Units;
                if (firstAdvance && !received.players.empty() && bootstrapGap)
                    nextFrame = now + bootstrapGap;
                if (client.Advance(elapsed, 1, 0, 0, 0)) published = client.PendingInput();
                for (const auto& command : client.PendingInput().commands)
                    generatedAt.try_emplace({client.PendingInput().movementEpoch, command.sequence}, now);
                if (!received.players.empty()) firstAdvance = false;
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

TEST_CASE("PvP start phase alignment removes the start lottery without losing Actual commands") {
    const auto median = [](RecoveryResult result) {
        REQUIRE(!result.actualLatencies.empty());
        std::sort(result.actualLatencies.begin(), result.actualLatencies.end());
        return result.actualLatencies[result.actualLatencies.size() / 2];
    };
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
                    const auto alignedMedian = median(aligned), unalignedMedian = median(unaligned);
                    // Never later than the unaligned start; frames only quantize the gain.
                    CHECK(alignedMedian <= unalignedMedian);
                    alignedLow = (std::min)(alignedLow, alignedMedian);
                    alignedHigh = (std::max)(alignedHigh, alignedMedian);
                    unalignedHigh = (std::max)(unalignedHigh, unalignedMedian);
                }
                INFO("fps ", fps, " rtt ", rtt, " worker ", worker);
                // The worst start phase gains at least a third of a tick at every frame rate.
                CHECK(alignedHigh + Tick / 3 <= unalignedHigh);
                // Frames finer than a tick leave only their own quantization.
                if (fps > static_cast<int>(AuthorityTickRate)) CHECK(alignedHigh - alignedLow <= Units / fps);
            }
}
