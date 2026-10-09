#include <doctest/doctest.h>

#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/ClientSimulationRole.hpp"
#include "RetroFPS/Pvp/FireGate.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace fps::pvp;
using Engine::Time::Duration;
using Engine::Time::TimePoint;

Duration After(double seconds) {
    return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(seconds));
}
double SecondsAt(TimePoint at) { return std::chrono::duration<double>(at.time_since_epoch()).count(); }

Arena RoleArena(const char* id = "synthetic_role_arena") {
    Arena arena;
    arena.id = id;
    arena.width = arena.depth = 1000;
    arena.spawns = {{{2, 0, 2}, 0}, {{20, 0, 2}, 0}};
    return arena;
}

// The main thread: the interval from one publish to the next (by publish time
// in seconds and publish index) and the intent it publishes then.
struct MainThread final {
    std::function<double(double, unsigned)> interval;
    std::function<ClientIntent(double)> intent;
    // Whose view the intent shows, from the host's (untransformed) authority;
    // by default the session, player and life the host reports.
    std::function<ClientIntentOwner(double, const PlayerState&)> owner;
};

std::function<double(double, unsigned)> Every(double seconds) {
    return [seconds](double, unsigned) { return seconds; };
}
// Steady 60 FPS with one long frame starting at the first publish from `at`.
std::function<double(double, unsigned)> LongFrame(double at, double seconds) {
    return [at, seconds, taken = false](double now, unsigned) mutable {
        if (!taken && now >= at) { taken = true; return seconds; }
        return 1.0 / 60.0;
    };
}

ClientIntent Moving(double) { return {1, 0, 0.3F, 0.1F, true, 0, {}}; }

struct Options final {
    double duration{4};
    double oneWaySeconds{0.01};
    // How late the simulation wakes after each deadline.
    double lateSeconds{};
    // When a snapshot produced at `produced` (seconds) for `tick` reaches the client.
    std::function<double(double produced, std::uint64_t tick)> deliverAt;
    // Changes a drain before the simulation runs (session, player, epoch, life, arena).
    std::function<void(double now, ClientSimulationDrain&)> transform;
    std::vector<Arena> arenas{RoleArena()};
};

struct StepRecord final {
    double at{};
    std::optional<PlayerInput> window;
    LocalMovementObservation observation;
    ClientPresentation presentation;
    bool stale{};
    std::size_t delivered{};
};

struct Generated final {
    double at{};
    MovementCommand command;
    std::uint64_t epoch{};
    std::uint64_t life{};
};

struct RoleRun final {
    std::vector<StepRecord> steps;
    // Every command by (epoch, sequence), with the time it was first generated.
    std::map<std::pair<std::uint64_t, std::uint64_t>, Generated> generated;
    std::vector<double> publishes;
    // Resolutions after one second: substituted (Held) ones, and when each happened.
    unsigned resolved{}, held{};
    std::vector<double> heldAt;
};

// A deterministic run on one virtual clock: the real host, a network of fixed
// one-way delays, a main thread that only publishes intents, and the
// simulation loop stepping at its own deadlines exactly as the role does.
RoleRun RunRole(const MainThread& main, const Options& options = {}) {
    const auto& arena = options.arenas.front();
    TimePoint clock{};
    MatchRuntimeHost host(arena, [&clock] { return clock; });
    REQUIRE(host.QueueJoin(1, 1));
    ClientSimulationLoop loop(options.arenas);
    struct InputPacket { TimePoint due; PlayerInput input; };
    struct SnapshotPacket { TimePoint due; WorldSnapshot snapshot; };
    std::vector<InputPacket> inputs;
    std::vector<SnapshotPacket> snapshots;
    std::vector<ReceivedSnapshot> inbox;
    std::set<std::pair<std::uint64_t, std::uint64_t>> accepted;
    std::optional<PlayerState> authority;
    ClientIntent intent;
    std::uint64_t published{};
    RoleRun run;
    const TimePoint end = TimePoint{} + After(options.duration);
    std::uint64_t tickIndex = 1;
    const auto tickAt = [](std::uint64_t index) { return TimePoint{} + After(double(index) / AuthorityTickRate); };
    TimePoint nextTick = tickAt(1), nextFrame{}, nextSimulation{};
    unsigned frame{};
    for (;;) {
        TimePoint now = (std::min)({nextTick, nextFrame, nextSimulation});
        for (const auto& packet : inputs) now = (std::min)(now, packet.due);
        for (const auto& packet : snapshots) now = (std::min)(now, packet.due);
        if (now > end) break;
        clock = now;
        const double seconds = SecondsAt(now);
        for (auto packet = inputs.begin(); packet != inputs.end();) {
            if (packet->due > now) { ++packet; continue; }
            const auto input = std::move(packet->input);
            packet = inputs.erase(packet);
            if (!host.SubmitInput(input) || !authority) continue;
            for (const auto& command : input.commands)
                if (command.sequence > authority->lastResolvedCommand)
                    accepted.emplace(input.movementEpoch, command.sequence);
        }
        if (now == nextTick) {
            nextTick = tickAt(++tickIndex);
            REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
            auto snapshot = host.TakeSnapshot();
            REQUIRE(snapshot);
            REQUIRE(snapshot->players.size() == 1);
            const auto& state = snapshot->players.front();
            if (authority && state.movementEpoch == authority->movementEpoch &&
                state.lastResolvedCommand > authority->lastResolvedCommand && seconds >= 1) {
                for (auto sequence = authority->lastResolvedCommand + 1; sequence <= state.lastResolvedCommand; ++sequence) {
                    ++run.resolved;
                    if (!accepted.contains({state.movementEpoch, sequence})) { ++run.held; run.heldAt.push_back(seconds); }
                }
            }
            authority = state;
            const double due = options.deliverAt ? options.deliverAt(seconds, snapshot->tick) : seconds;
            snapshots.push_back({TimePoint{} + After(due + options.oneWaySeconds), std::move(*snapshot)});
        }
        for (auto packet = snapshots.begin(); packet != snapshots.end();) {
            if (packet->due > now) { ++packet; continue; }
            inbox.push_back({std::move(packet->snapshot), now});
            packet = snapshots.erase(packet);
        }
        if (now == nextFrame) {
            intent = main.intent(seconds);
            intent.sampledAt = now;
            intent.owner = !authority ? ClientIntentOwner{} :
                main.owner ? main.owner(seconds, *authority) : ClientIntentOwner{1, 1, authority->lifeGeneration};
            intent.sequence = ++published;
            run.publishes.push_back(seconds);
            nextFrame = now + After(main.interval(seconds, frame++));
        }
        if (now == nextSimulation) {
            ClientSimulationDrain drain{inbox, 1, 1, arena.id, std::nullopt, 0};
            inbox.clear();
            if (options.transform) options.transform(seconds, drain);
            const auto delivered = drain.snapshots.size();
            auto step = loop.Run(now, intent, std::move(drain));
            const auto& pending = loop.Simulation().PendingInput();
            for (const auto& command : pending.commands)
                run.generated.try_emplace({pending.movementEpoch, command.sequence},
                    Generated{seconds, command, pending.movementEpoch, pending.lifeGeneration});
            if (step.window) inputs.push_back({now + After(options.oneWaySeconds), *step.window});
            run.steps.push_back({seconds, step.window, loop.Simulation().Observation(), step.presentation,
                step.intentStale, delivered});
            REQUIRE(step.nextDeadline > now);
            nextSimulation = step.nextDeadline + After(options.lateSeconds);
        }
    }
    return run;
}

// The commands of one epoch in sequence order.
std::vector<Generated> Commands(const RoleRun& run) {
    std::vector<Generated> result;
    for (const auto& [key, generated] : run.generated) result.push_back(generated);
    return result;
}

void SameStep(const StepRecord& actual, const StepRecord& expected) {
    CHECK(actual.at == expected.at);
    REQUIRE(actual.window.has_value() == expected.window.has_value());
    if (expected.window) CHECK(actual.window->commands == expected.window->commands);
    CHECK(actual.observation.latestCommand == expected.observation.latestCommand);
    CHECK(actual.observation.lastResolvedCommand == expected.observation.lastResolvedCommand);
    CHECK(actual.observation.predictedPosition.x == expected.observation.predictedPosition.x);
    CHECK(actual.observation.predictedPosition.z == expected.observation.predictedPosition.z);
    CHECK(actual.observation.phaseSamples == expected.observation.phaseSamples);
    CHECK(actual.observation.phaseCorrections == expected.observation.phaseCorrections);
}
} // namespace

TEST_CASE("PvP the simulation role generates the same commands at the same steps for 30 60 and 144 Hz intents") {
    const auto at60 = RunRole({Every(1.0 / 60.0), Moving});
    for (const double hz : {30.0, 144.0}) {
        INFO("intents at ", hz, " Hz");
        const auto other = RunRole({Every(1.0 / hz), Moving});
        REQUIRE(other.steps.size() == at60.steps.size());
        for (std::size_t n = 0; n < at60.steps.size(); ++n) {
            CAPTURE(at60.steps[n].at);
            SameStep(other.steps[n], at60.steps[n]);
        }
    }
    // One command per fixed step: none missing between consecutive steps, and
    // 60 Hz over the steady part of the run.
    const auto commands = Commands(at60);
    REQUIRE(commands.size() > 200);
    std::size_t steady{};
    for (std::size_t n = 1; n < commands.size(); ++n) {
        if (commands[n].at < 1) continue;
        ++steady;
        CHECK(commands[n].command.sequence == commands[n - 1].command.sequence + 1);
        CHECK(commands[n].at - commands[n - 1].at < 1.5 * MovementTickSeconds);
    }
    CHECK(steady >= 178);
    CHECK(steady <= 182);
    CHECK(at60.held == 0);
    CHECK(at60.steps.back().observation.phaseTracking == PhaseTrackingState::Tracking);
}

TEST_CASE("PvP a 250 ms main-thread stall keeps the simulation stepping and turns the stale intent neutral") {
    constexpr double StallAt = 1.5;
    MainThread main{LongFrame(StallAt, 0.25), [](double now) {
        // One jump pressed in the frame that starts the stall.
        return ClientIntent{1, 0.5F, 0.4F, -0.2F, true, now >= StallAt ? 1U : 0U, {}};
    }};
    const auto run = RunRole(main);
    const auto stall = *std::find_if(run.publishes.begin(), run.publishes.end(), [](double at) { return at >= StallAt; });
    const auto resumed = *std::find_if(run.publishes.begin(), run.publishes.end(), [stall](double at) { return at > stall; });
    REQUIRE(resumed - stall >= 0.25);
    const auto commands = Commands(run);
    std::size_t during{}, jumps{}, stale{};
    for (std::size_t n = 0; n < commands.size(); ++n) {
        const auto& [at, command, epoch, life] = commands[n];
        CAPTURE(at);
        if (command.jumpRequested) ++jumps;
        if (at <= stall || at >= resumed) continue;
        ++during;
        // (b) The stall never stops the simulation: no step is missing.
        CHECK(command.sequence == commands[n - 1].command.sequence + 1);
        CHECK(at - commands[n - 1].at < 1.5 * MovementTickSeconds);
        // (c) Until D30's limit the last intent stands; after it movement is
        // neutral and the aim stays.
        if (at <= stall + ClientIntentMinimumStaleSeconds) {
            CHECK(command.moveForward == 1.0F);
            CHECK(command.moveRight == 0.5F);
        } else {
            ++stale;
            CHECK(command.moveForward == 0.0F);
            CHECK(command.moveRight == 0.0F);
            CHECK(command.yaw == 0.4F);
            CHECK(command.pitch == -0.2F);
            CHECK_FALSE(command.jumpRequested);
        }
    }
    CHECK(during >= 14);
    CHECK(stale >= 5);
    // The press reaches exactly one command, never repeated by the stale intent.
    CHECK(jumps == 1);
    CHECK(run.held == 0);
    // The authority never resolved past the local commands.
    CHECK(run.steps.back().observation.stallReseeds == 0);
    CHECK(std::any_of(run.steps.begin(), run.steps.end(), [](const StepRecord& step) { return step.stale; }));
    // Movement resumes with the next intent.
    CHECK(std::any_of(commands.begin(), commands.end(), [resumed](const Generated& generated) {
        return generated.at > resumed && generated.command.moveForward == 1.0F;
    }));
}

TEST_CASE("PvP a long main-thread stall does not stretch the stale limit of the next one") {
    // 60 FPS, a one-second stall at 1.5 s, three frames, then a 250 ms stall.
    const auto interval = [first = std::optional<double>{}, frames = 0](double now, unsigned) mutable {
        if (!first && now >= 1.5) { first = now; return 1.0; }
        if (first && ++frames == 4) return 0.25;
        return 1.0 / 60.0;
    };
    const auto run = RunRole({interval, Moving});
    REQUIRE(run.publishes.size() > 10);
    // The publish that starts the second stall.
    std::optional<double> second;
    for (std::size_t n = 1; n + 1 < run.publishes.size(); ++n)
        if (run.publishes[n] > 2.5 && run.publishes[n + 1] - run.publishes[n] >= 0.25) { second = run.publishes[n]; break; }
    REQUIRE(second);
    std::size_t neutral{};
    for (const auto& generated : Commands(run)) {
        if (generated.at <= *second + ClientIntentMinimumStaleSeconds || generated.at >= *second + 0.25) continue;
        CAPTURE(generated.at);
        CHECK(generated.command.moveForward == 0.0F);
        ++neutral;
    }
    CHECK(neutral >= 5);
}

TEST_CASE("PvP the simulation keeps the intent at 20 and 25 FPS and across single long frames up to 120 ms") {
    struct Case { const char* name; std::function<double(double, unsigned)> interval; };
    const Case cases[] = {{"20 FPS", Every(1.0 / 20.0)}, {"25 FPS", Every(1.0 / 25.0)},
        {"60 FPS with a 60 ms frame", LongFrame(1.5, 0.06)}, {"60 FPS with an 80 ms frame", LongFrame(1.5, 0.08)},
        {"60 FPS with a 100 ms frame", LongFrame(1.5, 0.1)}, {"60 FPS with a 120 ms frame", LongFrame(1.5, 0.12)}};
    for (const auto& item : cases) {
        INFO(std::string(item.name));
        const auto run = RunRole({item.interval, Moving});
        const auto commands = Commands(run);
        std::size_t checked{};
        for (const auto& generated : commands) {
            if (generated.at < 1) continue;
            CAPTURE(generated.at);
            CHECK(generated.command.moveForward == 1.0F);
            ++checked;
        }
        CHECK(checked > 150);
        CHECK(std::none_of(run.steps.begin(), run.steps.end(),
            [](const StepRecord& step) { return step.at >= 1 && step.stale; }));
        CHECK(run.held == 0);
    }
}

TEST_CASE("PvP the simulation role restarts on a new session or player and reseeds on epoch and life changes") {
    constexpr double ChangeAt = 2;
    const auto before = [&](const RoleRun& run) {
        return std::find_if(run.steps.rbegin(), run.steps.rend(), [&](const StepRecord& step) { return step.at < ChangeAt; })->observation;
    };
    SUBCASE("connection generation") {
        Options options;
        options.transform = [&](double now, ClientSimulationDrain& drain) { if (now >= ChangeAt) drain.generation = 2; };
        const auto run = RunRole({Every(1.0 / 60.0), Moving}, options);
        REQUIRE(before(run).phaseTracking == PhaseTrackingState::Tracking);
        const auto changed = std::find_if(run.steps.begin(), run.steps.end(),
            [&](const StepRecord& step) { return step.at >= ChangeAt; });
        REQUIRE(changed != run.steps.end());
        // The prediction started over: phase tracking acquires again from a fresh seed.
        CHECK(changed->observation.phaseTracking != PhaseTrackingState::Tracking);
        CHECK(changed->observation.phaseCorrections == 0);
        CHECK(changed->observation.pendingCommands <= InitialCommandLead + 1);
        CHECK(run.steps.back().observation.active);
        // Each presentation names its session, so the main thread can tell an earlier one.
        for (const auto& step : run.steps) CHECK(step.presentation.generation == (step.at >= ChangeAt ? 2U : 1U));
    }
    SUBCASE("player id") {
        // The same pawn under a new id: only the id says it is another player.
        Options options;
        options.transform = [&](double now, ClientSimulationDrain& drain) {
            if (now < ChangeAt) return;
            drain.playerId = 2;
            for (auto& received : drain.snapshots)
                for (auto& player : received.snapshot.players) player.playerId = 2;
        };
        const auto run = RunRole({Every(1.0 / 60.0), Moving}, options);
        REQUIRE(before(run).phaseTracking == PhaseTrackingState::Tracking);
        const auto changed = std::find_if(run.steps.begin(), run.steps.end(),
            [&](const StepRecord& step) { return step.at >= ChangeAt && step.delivered > 0; });
        REQUIRE(changed != run.steps.end());
        CHECK(changed->observation.phaseTracking != PhaseTrackingState::Tracking);
        CHECK(changed->observation.pendingCommands <= InitialCommandLead + 1);
        REQUIRE(changed->window.has_value());
        CHECK(changed->window->playerId == 2);
    }
    SUBCASE("a player missing from the newest snapshot") {
        Options options;
        options.transform = [&](double now, ClientSimulationDrain& drain) {
            if (now < ChangeAt) return;
            for (auto& received : drain.snapshots) received.snapshot.players.clear();
        };
        const auto run = RunRole({Every(1.0 / 60.0), Moving}, options);
        REQUIRE(before(run).active);
        for (const auto& step : run.steps) {
            if (step.at < ChangeAt || step.delivered == 0) continue;
            CHECK_FALSE(step.observation.active);
            CHECK_FALSE(step.window.has_value());
        }
    }
    SUBCASE("movement epoch and life generation") {
        for (const bool life : {false, true}) {
            INFO(std::string(life ? "life generation" : "movement epoch"));
            Options options;
            // From ChangeAt on, the authority the client sees is one epoch (and life) ahead.
            options.transform = [&](double now, ClientSimulationDrain& drain) {
                if (now < ChangeAt) return;
                for (auto& received : drain.snapshots)
                    for (auto& player : received.snapshot.players) {
                        ++player.movementEpoch;
                        if (life) ++player.lifeGeneration;
                    }
            };
            const auto run = RunRole({Every(1.0 / 60.0), Moving}, options);
            const auto old = before(run);
            REQUIRE(old.active);
            const auto changed = std::find_if(run.steps.begin(), run.steps.end(),
                [&](const StepRecord& step) { return step.at >= ChangeAt && step.delivered > 0; });
            REQUIRE(changed != run.steps.end());
            // The first step that sees it reseeds into it and sends only its commands.
            CHECK(changed->observation.movementEpoch == old.movementEpoch + 1);
            CHECK(changed->observation.lifeGeneration == old.lifeGeneration + (life ? 1 : 0));
            CHECK(changed->observation.pendingCommands <= InitialCommandLead + 1);
            REQUIRE(changed->window.has_value());
            CHECK(changed->window->movementEpoch == old.movementEpoch + 1);
            CHECK(changed->window->lifeGeneration == old.lifeGeneration + (life ? 1 : 0));
        }
    }
    SUBCASE("arena selected by the joined Match") {
        Options options;
        options.arenas = {RoleArena("first_arena"), RoleArena("second_arena")};
        options.transform = [&](double now, ClientSimulationDrain& drain) {
            drain.arenaId = now >= ChangeAt ? "second_arena" : "first_arena";
        };
        const auto run = RunRole({Every(1.0 / 60.0), Moving}, options);
        for (const auto& step : run.steps) CHECK(step.presentation.arenaIndex == (step.at >= ChangeAt ? 1U : 0U));
        CHECK(run.steps.back().observation.active);
    }
}

TEST_CASE("PvP the first commands of a new life or session aim where the authority does until the main thread sees it") {
    constexpr double ChangeAt = 2;
    constexpr double SeenAt = ChangeAt + 0.05;
    for (const bool session : {false, true}) {
        INFO(std::string(session ? "new session" : "new life"));
        Options options;
        options.transform = [&](double now, ClientSimulationDrain& drain) {
            if (now < ChangeAt) return;
            if (session) drain.generation = 2;
            for (auto& received : drain.snapshots)
                for (auto& player : received.snapshot.players) {
                    if (!session) { ++player.movementEpoch; ++player.lifeGeneration; }
                    // The spawn orientation of the new life or session.
                    player.yaw = 1.0F;
                    player.pitch = -0.1F;
                }
        };
        // The main thread keeps its old view until SeenAt, then resets it to the spawn.
        MainThread main{Every(1.0 / 60.0), [&](double now) {
            return now < SeenAt ? ClientIntent{1, 0, 0.3F, 0.1F, true, 0, {}} : ClientIntent{1, 0, 1.0F, -0.1F, true, 0, {}};
        }, [&](double now, const PlayerState& authority) {
            if (now < SeenAt) return ClientIntentOwner{1, 1, authority.lifeGeneration};
            return session ? ClientIntentOwner{2, 1, authority.lifeGeneration} : ClientIntentOwner{1, 1, authority.lifeGeneration + 1};
        }};
        const auto run = RunRole(main, options);
        std::size_t before{}, after{};
        for (const auto& step : run.steps) {
            if (step.at < ChangeAt || !step.window) continue;
            CAPTURE(step.at);
            for (const auto& command : step.window->commands) {
                CHECK(command.yaw == 1.0F);
                CHECK(command.pitch == -0.1F);
                if (step.at < SeenAt) {
                    // The old view's aim and movement never reach the new life or session.
                    CHECK(command.moveForward == 0.0F);
                    ++before;
                } else if (step.at > SeenAt + 2 * MovementTickSeconds && command.moveForward == 1.0F) ++after;
            }
        }
        CHECK(before > 0);
        CHECK(after > 0);
    }
}

TEST_CASE("PvP an intent without controls moves nothing, keeps its aim and drops the jumps it carries") {
    MainThread main{Every(1.0 / 60.0), [](double now) {
        // Controls drop for half a second with a new aim and a jump press that
        // is still counted when controls return.
        if (now >= 1.5 && now < 2.0) return ClientIntent{1, 0.5F, 0.6F, 0.2F, false, 1, {}};
        return ClientIntent{1, 0, 0.3F, 0.1F, true, now >= 2.0 ? 1U : 0U, {}};
    }};
    const auto run = RunRole(main);
    std::size_t without{}, with{};
    for (const auto& generated : Commands(run)) {
        CAPTURE(generated.at);
        CHECK_FALSE(generated.command.jumpRequested);
        if (generated.at > 1.5 + MovementTickSeconds && generated.at < 2.0) {
            CHECK(generated.command.moveForward == 0.0F);
            CHECK(generated.command.moveRight == 0.0F);
            CHECK(generated.command.yaw == 0.6F);
            CHECK(generated.command.pitch == 0.2F);
            ++without;
        } else if (generated.at > 2.0 + MovementTickSeconds) {
            CHECK(generated.command.moveForward == 1.0F);
            ++with;
        }
    }
    CHECK(without >= 25);
    CHECK(with >= 100);
}

TEST_CASE("PvP a slow publisher's intent stands for three of its intervals") {
    // 12.5 FPS: three intervals are 240 ms, longer than the 150 ms minimum.
    const auto interval = [first = false, second = false](double now, unsigned) mutable {
        if (!first && now >= 1.5) { first = true; return 0.2; }
        if (!second && now >= 2.5) { second = true; return 0.3; }
        return 0.08;
    };
    const auto run = RunRole({interval, Moving});
    std::optional<double> shortGap, longGap;
    for (std::size_t n = 0; n + 1 < run.publishes.size(); ++n) {
        const double gap = run.publishes[n + 1] - run.publishes[n];
        if (gap > 0.19 && gap < 0.21) shortGap = run.publishes[n];
        if (gap > 0.29 && gap < 0.31) longGap = run.publishes[n];
    }
    REQUIRE(shortGap);
    REQUIRE(longGap);
    std::size_t kept{}, neutral{};
    for (const auto& generated : Commands(run)) {
        CAPTURE(generated.at);
        if (generated.at > *shortGap && generated.at < *shortGap + 0.2) {
            CHECK(generated.command.moveForward == 1.0F);
            ++kept;
        }
        if (generated.at > *longGap + 3 * 0.08 && generated.at < *longGap + 0.3) {
            CHECK(generated.command.moveForward == 0.0F);
            ++neutral;
        }
    }
    CHECK(kept >= 10);
    CHECK(neutral >= 2);
}

TEST_CASE("PvP a later placement advances interpolation by elapsed steps and decays the correction") {
    const auto arena = RoleArena();
    const LocalPresentationState state{{0, 0, 0}, {1, 0, 0}, {0.3F, 0, 0}, 0.1, 0.5F};
    const auto own = InterpolateLocalPresentation(arena, state, 0.0);
    CHECK(own.interpolationAlpha == 0.5F);
    CHECK(own.renderPosition.x == doctest::Approx(0.8));
    CHECK(own.correctionOffset.x == doctest::Approx(0.3));
    const double later = 0.004;
    const auto placed = InterpolateLocalPresentation(arena, state, later);
    const double alpha = 0.5 + later / MovementTickSeconds;
    CHECK(placed.interpolationAlpha == doctest::Approx(alpha));
    // The correction decays as the prediction decays it: linearly over its remaining time.
    CHECK(placed.correctionOffset.x == doctest::Approx(0.3 * (0.1 - later) / 0.1));
    CHECK(placed.renderPosition.x == doctest::Approx(alpha + 0.3 * (0.1 - later) / 0.1));
    // Past the next step the interpolation holds the newest state.
    CHECK(InterpolateLocalPresentation(arena, state, MovementTickSeconds).interpolationAlpha == 1.0F);
}

TEST_CASE("PvP the simulation role takes one phase sample per own snapshot of a drained batch") {
    Options options;
    // From two seconds on, snapshots arrive three ticks at a time.
    options.deliverAt = [](double produced, std::uint64_t tick) {
        if (produced < 2) return produced;
        return double((tick + 2) / 3 * 3) / AuthorityTickRate;
    };
    const auto run = RunRole({Every(1.0 / 60.0), Moving}, options);
    std::size_t batches{};
    for (std::size_t n = 1; n < run.steps.size(); ++n) {
        const auto& previous = run.steps[n - 1].observation;
        const auto& step = run.steps[n];
        if (step.at < 2.5 || step.delivered != 3 || step.observation.movementEpoch != previous.movementEpoch) continue;
        CAPTURE(step.at);
        CHECK(step.observation.phaseSamples == previous.phaseSamples + 3);
        ++batches;
    }
    CHECK(batches > 20);
}

TEST_CASE("PvP a presentation placed at a later time advances interpolation and shot timing only") {
    const auto run = RunRole({Every(1.0 / 60.0), Moving});
    const std::vector<Arena> arenas{RoleArena()};
    std::size_t checked{}, gateChecked{};
    for (std::size_t n = 1; n < run.steps.size(); ++n) {
        const auto& step = run.steps[n];
        if (step.at < 1 || !step.observation.active) continue;
        CAPTURE(step.at);
        // At its own step time the placement is the step's observation.
        const auto own = PresentAt(arenas, step.presentation, step.presentation.steppedAt);
        CHECK(own.observation.renderPosition.x == step.observation.renderPosition.x);
        CHECK(own.observation.renderPosition.z == step.observation.renderPosition.z);
        CHECK(own.observation.interpolationAlpha == step.observation.interpolationAlpha);
        const auto later = PresentAt(arenas, step.presentation, step.presentation.steppedAt + After(0.004));
        CHECK(later.observation.latestCommand == step.observation.latestCommand);
        CHECK(later.observation.interpolationAlpha >= own.observation.interpolationAlpha);
        REQUIRE(later.shotTiming.has_value() == step.presentation.shotTiming.has_value());
        if (later.shotTiming)
            CHECK(later.shotTiming->secondsSinceStep == doctest::Approx(step.presentation.shotTiming->secondsSinceStep + 0.004));
        ++checked;
        // Placed at the next step's time, the previous step's shot timing bounds
        // the same authority tick as the next step's own timing, when that step
        // took one fixed step past the previous newest command.
        const auto& previous = run.steps[n - 1];
        const auto newest = [](const FireGateTiming& timing) {
            return timing.authorityTick + (timing.latestCommand - timing.lastResolvedCommand);
        };
        if (!previous.presentation.shotTiming || !step.presentation.shotTiming ||
            previous.presentation.shotTiming->phaseShiftSeconds != 0 || step.presentation.shotTiming->phaseShiftSeconds != 0 ||
            previous.observation.movementEpoch != step.observation.movementEpoch ||
            newest(*step.presentation.shotTiming) != newest(*previous.presentation.shotTiming) + 1) continue;
        const auto carried = PresentAt(arenas, previous.presentation, step.presentation.steppedAt);
        CHECK(EarliestShotResolveTick(*carried.shotTiming) == EarliestShotResolveTick(*step.presentation.shotTiming));
        ++gateChecked;
    }
    CHECK(checked > 150);
    CHECK(gateChecked > 20);
}

TEST_CASE("PvP the simulation role thread runs, publishes and stops while waiting") {
    ClientConnection connection;
    {
        ClientSimulationRole role(connection, {RoleArena()}, "gyo-test-sim");
        role.PublishIntent({1, 0, 0, 0, true, 0, Engine::Time::Now()});
        std::uint64_t runs{};
        // Bounded by the CTest timeout, not by a timing assumption.
        while (runs < 3) {
            runs += role.TakeWakeReport().runs;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK_FALSE(role.Error().has_value());
        // No session: the role is inactive and sends nothing.
        CHECK_FALSE(role.Latest().observation.active);
        CHECK_FALSE(role.PresentAt(Engine::Time::Now()).observation.active);
        CHECK(role.Latest().steppedAt != TimePoint{});
        // Destroyed while waiting for its next deadline.
    }
    CHECK(connection.State().phase == ConnectionPhase::Lobby);
}
