// Scenario test segment 2 (time-events declaration
// docs/architecture/target/v2/time-events-declaration.zh-Hant.md, section 7,
// S2-1 to S2-9; judgement 2): "walk and score" with the time Module. Walk runs
// in the fixed step, one unit a step; a point is a Message (ScoreGained) the
// fixed step writes and a frame System reads; a coin taken runs a Remove<Coin>
// observer; the save keeps the time Resources and a new World goes on from it.
// The product's own description: WorldTimeModule at 64 Hz, manual "per-frame
// duration" sampling (no check depends on the time base).
//
// A failure is classified by contract first (orchestration skeleton
// declaration 8.4): each message names the step and the contract it relies on,
// so a reader can tell "the scenario is written wrong" (1), "the mechanism
// does not do what the contract promises" (2) and "no contract promised it" (3).
// S2-9 (segment 1 without the domain declarations) is segment 1 itself.

#include "WalkerGame.hpp"

#include "engine/world_time/FixedStep.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::WorldTime;
using W::Nanoseconds;
using Walker::Player;
using Walker::Position;
using Walker::Score;
using Walker::Velocity;

constexpr Nanoseconds kStep = 15'625'000;  // 64 Hz
constexpr Nanoseconds kMs = 1'000'000;
constexpr K::EntityId kPlayer = 1;
constexpr K::EntityId kCoin = 2;
constexpr int kCoinX = 20;

// ---- The game's types (segment 2 adds these to segment 1's Position, Velocity, Player, Score)

// A point, written in the fixed step; tick is the step that earned it (the
// writer reads Res<Tick>: a message that needs the tick carries it, cursor
// declaration 3.2).
struct ScoreGained {
    std::uint64_t tick = 0;
};
struct Coin {
    int x = 0;
};

// What the game's Systems saw, for the checks (the test's record, not game state).
struct Seen2 {
    // Per frame, as CountGained saw it: Tick, and the ScoreGained records it read.
    std::vector<std::uint64_t> frameTick;
    std::vector<std::size_t> gainedPerFrame;
    // Every ScoreGained payload read, in the order read.
    std::vector<std::uint64_t> gainedTicks;
    // Every TimeDiscarded record read by the frame's watcher.
    std::vector<W::TimeDiscarded> discarded;
    // The order of PickUp, the Remove<Coin> observer, the OnRemove hook and the frame's last System.
    std::vector<std::string> log;
    int coinObserved = 0;
};
Seen2& Seen() {
    static Seen2 seen;
    return seen;
}

// ---- Systems

// FixedUpdate/Update: one point a step.
K::SystemResult Gain(K::Res<W::Tick> tick, K::MessageWriter<ScoreGained> gained) {
    gained.Send(ScoreGained{tick->value});
    return {};
}

// Main/Update: the frame reads the points the fixed steps wrote since it last ran.
K::SystemResult CountGained(K::MessageReader<ScoreGained> gained, K::ResMut<Score> score, K::Res<W::Tick> tick) {
    std::size_t n = 0;
    for (const ScoreGained* g : gained.Read()) {
        Seen().gainedTicks.push_back(g->tick);
        ++n;
    }
    score->v += static_cast<int>(n);
    Seen().frameTick.push_back(tick->value);
    Seen().gainedPerFrame.push_back(n);
    return {};
}

// Main/Update: the frame's watcher of discarded time.
K::SystemResult WatchDiscarded(K::MessageReader<W::TimeDiscarded> discarded) {
    for (const W::TimeDiscarded* d : discarded.Read()) Seen().discarded.push_back(*d);
    return {};
}

// Main/Update: the player takes a coin it reached; the despawn is deferred to the sync point (DI-KS 9).
K::SystemResult PickUp(K::Query<K::Read<Position>, K::Read<Player>> players, K::Query<K::Read<Coin>> coins, K::Despawn despawn) {
    for (K::EntityId p : players.Entities()) {
        const int x = players.Get<Position>(p)->x;
        for (K::EntityId c : coins.Entities()) {
            if (coins.Get<Coin>(c)->x <= x) {
                Seen().log.push_back("pickup@" + std::to_string(c));
                despawn.Entity(c);
            }
        }
    }
    return {};
}

// Main/Last: marks the end of the frame in the log.
K::SystemResult FrameEnd() {
    if (!Seen().log.empty()) Seen().log.push_back("frame-end");
    return {};
}

K::SystemResult CoinTaken(K::On<K::Remove<Coin>> on, K::Query<K::Read<Coin>> coins) {
    ++Seen().coinObserved;
    const K::EntityId target = on.Target() ? *on.Target() : 0;
    Seen().log.push_back("obs-Coin@" + std::to_string(target) + " cell:" + std::to_string(coins.Contains(target) ? 1 : 0));
    return {};
}

K::SystemResult CoinRemovedHook(K::HookCtx ctx) {
    Seen().log.push_back("hook-Coin@" + std::to_string(ctx.Entity()));
    return {};
}

// ---- Registration (the product's own build)

K::SystemOptions At(std::string name, std::string_view schedule, std::string_view stage, std::vector<std::string> after = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::string(schedule);
    m.stage = std::string(stage);
    m.after = std::move(after);
    o.mounts.push_back(std::move(m));
    return o;
}

void Register(O::WorldSetup& s) {
    using D = O::DefaultSchedules;
    K::Registrar& r = s.Registrar();
    r.Component<Position>({.name = "Position"});
    r.Component<Velocity>({.name = "Velocity"});
    r.Component<Player>({.name = "Player"});
    r.Component<Coin>({.name = "Coin"});
    r.Resource<Score>({.name = "Score"});
    r.Message<ScoreGained>({.name = "ScoreGained"});
    // Segment 1's Walk, now one unit a fixed step.
    r.AddSystem<&Walker::Walk>(At("Walk", D::fixed, D::update));
    r.AddSystem<&Gain>(At("Gain", D::fixed, D::update, {"Walk"}));
    r.AddSystem<&CountGained>(At("CountGained", D::frame, D::update));
    r.AddSystem<&WatchDiscarded>(At("WatchDiscarded", D::frame, D::update));
    r.AddSystem<&PickUp>(At("PickUp", D::frame, D::update));
    r.AddSystem<&FrameEnd>(At("FrameEnd", D::frame, D::last));
    r.Observer<K::Remove<Coin>, &CoinTaken>({.name = "CoinTaken"});
    r.Hook<Coin, &CoinRemovedHook>(K::HookTrigger::OnRemove, {.name = "CoinRemovedHook"});
}

// The description's prepare: manual sampling, the player at (0, 0), a coin at x 20, the score 0.
::GYO::Engine::Base::Result<void, O::OrchestrationError> Defaults(K::World& w) {
    w.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    w.SetResource(Score{0});
    w.Insert(kPlayer, Player{});
    w.Insert(kPlayer, Position{0, 0});
    w.Insert(kPlayer, Velocity{1, 0});
    w.Insert(kCoin, Coin{kCoinX});
    return {};
}

O::ProductDescription Game() {
    O::ProductDescription d;
    d.name = "walker-timed";
    O::WorldDescription main;
    main.name = "Main";
    main.build = &Register;
    main.options.Set(W::WorldTimeOptions{});  // 64 Hz, 5 steps a frame, pausable
    main.prepare.push_back(&Defaults);
    d.worlds.push_back(std::move(main));
    return d;
}

O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    O::RegisterRow row;
    row.name = "world_time";
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    units.rows.push_back(row);
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    return units;
}

// ---- The author's save (outside the engine; declaration 3.5)

// Tick, VirtualTime and the fixed-step state (FixedTime's elapsed time and the
// accumulator's remainder; not the step length, not RealTime, not
// Resimulation), the player and the score. The bytes are the save: no padding.
struct TimedSave {
    W::Tick tick;
    W::VirtualTime virt;
    Nanoseconds fixedElapsed = 0;
    Nanoseconds fixedOverstep = 0;
    K::EntityId player = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t score = 0;
    std::uint8_t coinTaken = 0;
    std::uint8_t reserved[3] = {};
};
static_assert(sizeof(TimedSave) == sizeof(W::Tick) + sizeof(W::VirtualTime) + 2 * sizeof(Nanoseconds) + sizeof(K::EntityId) +
                                       3 * sizeof(std::int32_t) + 4,
              "no padding: the save is the struct's bytes");
static_assert(std::is_trivially_copyable_v<TimedSave>);
using SaveBytes = std::array<std::byte, sizeof(TimedSave)>;

SaveBytes MakeSave(const K::World& w) {
    TimedSave s;
    s.tick = *w.Resource<W::Tick>();
    s.virt = *w.Resource<W::VirtualTime>();
    const W::FixedTime& fixed = *w.Resource<W::FixedTime>();
    s.fixedElapsed = fixed.elapsed;
    s.fixedOverstep = fixed.accumulator.Overstep();
    s.player = kPlayer;
    s.x = w.Get<Position>(kPlayer)->x;
    s.y = w.Get<Position>(kPlayer)->y;
    s.score = w.Resource<Score>()->v;
    s.coinTaken = w.Get<Coin>(kCoin) == nullptr ? 1 : 0;
    SaveBytes bytes;
    std::memcpy(bytes.data(), &s, sizeof s);
    return bytes;
}

TimedSave ReadSave(const SaveBytes& bytes) {
    TimedSave s;
    std::memcpy(&s, bytes.data(), sizeof s);
    return s;
}

// The extra prepare of a load: after the description's Defaults, puts the save back.
O::PrepareFn LoadInto(SaveBytes bytes) {
    return [bytes](K::World& w) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        const TimedSave s = ReadSave(bytes);
        w.SetResource(s.tick);
        w.SetResource(s.virt);
        W::RestoreFixedTime(w, s.fixedElapsed, s.fixedOverstep);
        w.Insert(s.player, Position{s.x, s.y});
        w.SetResource(Score{s.score});
        if (s.coinTaken != 0) w.Despawn(kCoin);
        return {};
    };
}

// ---- Helpers

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

template <class T>
const T& Get(O::Product& p) {
    const T* value = WorldOf(p).Resource<T>();
    REQUIRE_MESSAGE(value != nullptr, "[sc-s2-resource-present] (WT-1) a time Resource is absent");
    return *value;
}

int XOf(O::Product& p) {
    const Position* pos = WorldOf(p).Get<Position>(kPlayer);
    REQUIRE(pos != nullptr);
    return pos->x;
}

// One frame whose real time advances by delta (manual sampling, declaration 3.4).
void Frame(O::Product& p, Nanoseconds delta, const char* step) {
    WorldOf(p).SetResource(W::TimeSampling{delta, W::SamplingMode::ManualDelta});
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), step << ": " << (ran ? std::string() : ran.error().message + " " + ran.error().detail));
}
void Frames(O::Product& p, int n, Nanoseconds delta, const char* step) {
    for (int i = 0; i < n; ++i) Frame(p, delta, step);
}

// S2-4 over every frame so far: the records each frame read equal the steps that
// frame ran (the Tick it saw minus the Tick of the frame before), and the ticks
// in the records are each step once, in order.
void CheckReadsEqualSteps(std::uint64_t tickBefore, const char* step) {
    const Seen2& s = Seen();
    REQUIRE(s.frameTick.size() == s.gainedPerFrame.size());
    std::uint64_t before = tickBefore;
    std::string perFrame;
    bool equal = true;
    for (std::size_t i = 0; i < s.frameTick.size(); ++i) {
        const std::uint64_t steps = s.frameTick[i] - before;
        perFrame += std::to_string(s.gainedPerFrame[i]) + "/" + std::to_string(steps) + " ";
        equal = equal && s.gainedPerFrame[i] == steps;
        before = s.frameTick[i];
    }
    CHECK_MESSAGE(equal, "[sc-s2-4-reads-equal-steps] (cursor DI-KS 10: every record once to every reader that ran) " << step
                                                                                                                   << " read/steps per frame: " << perFrame);
    bool inOrder = s.gainedTicks.size() == before - tickBefore;
    for (std::size_t i = 0; inOrder && i < s.gainedTicks.size(); ++i) inOrder = s.gainedTicks[i] == tickBefore + 1 + i;
    CHECK_MESSAGE(inOrder, "[sc-s2-4-each-step-once] (cursor DI-KS 10) " << step << ": " << s.gainedTicks.size() << " records for "
                                                                        << (before - tickBefore) << " steps");
}

std::string Joined(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) out += (out.empty() ? "" : " | ") + line;
    return out;
}

// The bytes a message type's buffer holds in the World: its records' allocation
// (vector capacity x record size; the record is the payload plus an 8-byte seq)
// and the buffer object. The allocator's own overhead is not counted.
template <class M>
std::size_t MessageBufferBytes(K::World& w) {
    const K::MessageTypeId id = K::Detail::MessageIdOf(w, K::KeyOf<M>());
    const auto& buffer = static_cast<const K::Detail::MessageBuffer<M>&>(K::Detail::MessagesAt(w, id));
    return buffer.records.capacity() * sizeof(typename K::Detail::MessageBuffer<M>::Record) + sizeof(buffer);
}

} // namespace

TEST_CASE("scenario segment 2: walk in the fixed step, score by Message, pause, fall behind, take a coin, save and load the time") {
    Seen() = Seen2{};
    auto built = O::Product::Build(Game(), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), "[sc-s2-1-built] (O-2) " << (built ? std::string() : built.error().detail));
    O::Product& p = **built;
    const K::World& w = WorldOf(p);

    // S2-1: every time Resource is there; Tick is 0 (WT-1; declaration 3.3).
    CHECK_MESSAGE((w.Resource<W::RealTime>() != nullptr && w.Resource<W::VirtualTime>() != nullptr &&
                   w.Resource<W::FixedTime>() != nullptr && w.Resource<W::ActiveTime>() != nullptr &&
                   w.Resource<W::TimeSampling>() != nullptr && w.Resource<W::TimeControl>() != nullptr &&
                   w.Resource<W::Resimulation>() != nullptr && w.Resource<W::TimeConfig>() != nullptr),
                  "[sc-s2-1-time-resources] (WT-1) a time Resource is absent");
    CHECK_MESSAGE(w.Types().MessageIdOf<W::TimeDiscarded>().has_value(), "[sc-s2-1-time-discarded-registered] (WT-1)");
    CHECK_MESSAGE(Get<W::Tick>(p).value == 0, "[sc-s2-1-tick-zero] (WT-1) " << Get<W::Tick>(p).value);
    CHECK_MESSAGE(Get<W::FixedTime>(p).accumulator.Step() == kStep, "[sc-s2-1-64hz] (WT-3) " << Get<W::FixedTime>(p).accumulator.Step());

    // S2-2: ten frames of 15.625 ms: ten steps (a manual duration applies from the first frame, declaration 3.4).
    Frames(p, 10, kStep, "S2-2");
    CHECK_MESSAGE(Get<W::Tick>(p).value == 10, "[sc-s2-2-tick] (WT-3: Tick + 1 before each step) " << Get<W::Tick>(p).value);
    CHECK_MESSAGE((XOf(p) == 10 && WorldOf(p).Get<Position>(kPlayer)->y == 0), "[sc-s2-2-walked] (WT-3) x " << XOf(p));

    // S2-3: one frame of three steps' time: the fixed step runs three times.
    Frame(p, 3 * kStep, "S2-3");
    CHECK_MESSAGE(Seen().gainedPerFrame.back() == 3, "[sc-s2-3-three-steps] (WT-3) " << Seen().gainedPerFrame.back());
    CHECK_MESSAGE((Get<W::Tick>(p).value == 13 && XOf(p) == 13), "[sc-s2-3-walked] (WT-3) tick " << Get<W::Tick>(p).value << " x " << XOf(p));

    // S2-4: a frame of 0: no step, the frame reads 0 records; every frame read what its steps wrote.
    Frame(p, 0, "S2-4");
    CHECK_MESSAGE((Seen().gainedPerFrame.back() == 0 && Get<W::Tick>(p).value == 13), "[sc-s2-4-zero-frame] (WT-3, DI-KS 10) read "
                                                                                          << Seen().gainedPerFrame.back());
    CheckReadsEqualSteps(0, "S2-2 to S2-4");
    CHECK_MESSAGE(Get<Score>(p).v == 13, "[sc-s2-4-score] (DI-KS 10) " << Get<Score>(p).v);

    // S2-5: paused two frames (TimeControl): Tick does not move, the frame's Systems still run.
    WorldOf(p).SetResource(W::TimeControl{1.0, 1});
    const std::size_t framesBefore = Seen().frameTick.size();
    Frames(p, 2, kStep, "S2-5");
    CHECK_MESSAGE(Seen().frameTick.size() == framesBefore + 2, "[sc-s2-5-frame-runs] (time-events 3.2: UI runs while paused) "
                                                                   << (Seen().frameTick.size() - framesBefore));
    CHECK_MESSAGE((Get<W::Tick>(p).value == 13 && XOf(p) == 13), "[sc-s2-5-tick-still] (time-events 3.2) tick " << Get<W::Tick>(p).value);
    CHECK_MESSAGE(Get<W::RealTime>(p).delta == kStep, "[sc-s2-5-real-runs] (time-events 3.2: RealTime not affected) "
                                                          << Get<W::RealTime>(p).delta);
    WorldOf(p).SetResource(W::TimeControl{1.0, 0});

    // S2-6: stuck: one frame of 1 s (remainder 0 before): 5 steps, one TimeDiscarded, discarded + 59 steps.
    // With a fixed step the 250 ms cap does not apply (-t1-three-items 2).
    REQUIRE_MESSAGE(Get<W::FixedTime>(p).accumulator.Overstep() == 0, "[sc-s2-6-remainder-zero] the scenario's premise");
    const Nanoseconds discardedBefore = Get<W::VirtualTime>(p).discarded;
    Frame(p, 1000 * kMs, "S2-6");
    CHECK_MESSAGE((Get<W::Tick>(p).value == 18 && XOf(p) == 18 && Seen().gainedPerFrame.back() == 5),
                  "[sc-s2-6-five-steps] (time-events 3.2: at most 5 steps a frame) tick " << Get<W::Tick>(p).value);
    CHECK_MESSAGE(Get<W::VirtualTime>(p).discarded - discardedBefore == 59 * kStep,
                  "[sc-s2-6-discarded] (time-events 3.2, DI 2) + " << (Get<W::VirtualTime>(p).discarded - discardedBefore));
    CHECK_MESSAGE((Seen().discarded.size() == 1 && Seen().discarded[0].cause == W::DiscardCause::StepLimit &&
                   Seen().discarded[0].amount == 59 * kStep && Seen().discarded[0].realDelta == 1000 * kMs),
                  "[sc-s2-6-one-time-discarded] (time-events 5: TimeDiscarded) " << Seen().discarded.size() << " records");

    // S2-7: the player reaches the coin: PickUp despawns it; at the sync point the Remove<Coin>
    // observer runs once, before the OnRemove hook, while the cell is there (DI-KS 9, 10; declaration 4.2).
    Frame(p, 20 * kMs, "S2-7");  // 1 step, remainder 4.375 ms: x 19
    CHECK_MESSAGE((Seen().log.empty() && WorldOf(p).Get<Coin>(kCoin) != nullptr), "[sc-s2-7-not-yet] " << Joined(Seen().log));
    Frame(p, 20 * kMs, "S2-7");  // 24.375 ms: 1 step, remainder 8.75 ms: x 20, the coin is taken
    Frames(p, 2, kStep, "S2-7");
    CHECK_MESSAGE(Seen().coinObserved == 1, "[sc-s2-7-observed-once] (DI-KS 10) " << Seen().coinObserved);
    CHECK_MESSAGE(Joined(Seen().log) == "pickup@2 | obs-Coin@2 cell:1 | hook-Coin@2 | frame-end | frame-end | frame-end",
                  "[sc-s2-7-order] (DI-KS 9: at the sync point; declaration 4.2: Remove observers, then OnRemove hooks) "
                      << Joined(Seen().log));
    CHECK_MESSAGE(WorldOf(p).Get<Coin>(kCoin) == nullptr, "[sc-s2-7-coin-gone] (DI-KS 9)");
    CheckReadsEqualSteps(0, "S2-2 to S2-7");

    // S2-8: the save (Tick, VirtualTime, the fixed-step state, the player, the score) -> a new World from the save.
    const W::RealTime oldReal = Get<W::RealTime>(p);
    const std::size_t oldBufferBytes = MessageBufferBytes<ScoreGained>(WorldOf(p)) + MessageBufferBytes<W::TimeDiscarded>(WorldOf(p));
    const SaveBytes bytes = MakeSave(WorldOf(p));
    const TimedSave save = ReadSave(bytes);
    REQUIRE_MESSAGE((save.tick.value == 22 && save.x == 22 && save.score == 22 && save.coinTaken == 1 &&
                     save.fixedOverstep == 8'750'000 && save.fixedElapsed == 22 * kStep),
                    "[sc-s2-8-save] tick " << save.tick.value << " x " << save.x << " score " << save.score << " overstep " << save.fixedOverstep);
    auto replaced = p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(bytes), {}, O::ReplaceOrder::BuildNewFirst});
    REQUIRE_MESSAGE(static_cast<bool>(replaced), "[sc-s2-8-replaced] (O-7, O-8) "
                                                     << (replaced ? std::string() : replaced.error().message + " " + replaced.error().detail));
    CHECK_MESSAGE(Get<W::Tick>(p).value == save.tick.value, "[sc-s2-8-tick-put-back] (time-events 3.5) " << Get<W::Tick>(p).value);
    CHECK_MESSAGE((Get<W::FixedTime>(p).elapsed == save.fixedElapsed && Get<W::FixedTime>(p).accumulator.Overstep() == save.fixedOverstep),
                  "[sc-s2-8-fixed-put-back] (time-events 3.5: RestoreFixedTime) " << Get<W::FixedTime>(p).accumulator.Overstep());
    CHECK_MESSAGE((XOf(p) == 22 && Get<Score>(p).v == 22 && WorldOf(p).Get<Coin>(kCoin) == nullptr), "[sc-s2-8-game-put-back] x " << XOf(p));
    // S2-8, adjusted (this card's call, by Bevy's ManualDuration): the declaration says "on the first frame
    // RealTime.delta = 0", but manual "per-frame duration" sampling applies its duration from the first frame on
    // (S2-2 needs that; time-events declaration 3.3, TimeSampling; bevy_time/src/lib.rs:122-128). What the step
    // guards is that RealTime is not put back (declaration 3.5: a time base's instant means nothing across
    // processes). So: the new World's RealTime starts from nothing (not sampled, elapsed 0) and after n frames its
    // elapsed is n frames' duration, not the old World's elapsed plus that.
    CHECK_MESSAGE((Get<W::RealTime>(p).sampled == 0 && Get<W::RealTime>(p).elapsed == 0 && Get<W::RealTime>(p).now == 0),
                  "[sc-s2-8-real-fresh] (time-events 3.5: RealTime not put back) elapsed " << Get<W::RealTime>(p).elapsed);
    Seen().frameTick.clear();
    Seen().gainedPerFrame.clear();
    Seen().gainedTicks.clear();
    Frames(p, 5, kStep, "S2-8");  // 8.75 + 15.625 ms each frame: one step each
    CHECK_MESSAGE((Get<W::Tick>(p).value == save.tick.value + 5 && XOf(p) == save.x + 5), "[sc-s2-8-goes-on] (time-events 3.5) tick "
                                                                                              << Get<W::Tick>(p).value << " x " << XOf(p));
    CHECK_MESSAGE((Get<W::VirtualTime>(p).elapsed == save.virt.elapsed + 5 * kStep && Get<W::VirtualTime>(p).discarded == save.virt.discarded),
                  "[sc-s2-8-virtual-goes-on] (time-events 3.5) elapsed " << Get<W::VirtualTime>(p).elapsed);
    CHECK_MESSAGE((Get<W::FixedTime>(p).elapsed == save.fixedElapsed + 5 * kStep && Get<W::FixedTime>(p).accumulator.Overstep() == save.fixedOverstep),
                  "[sc-s2-8-fixed-goes-on] (time-events 3.5) " << Get<W::FixedTime>(p).elapsed);
    CHECK_MESSAGE((Get<W::RealTime>(p).elapsed == 5 * kStep && Get<W::RealTime>(p).delta == kStep && oldReal.elapsed > 5 * kStep),
                  "[sc-s2-8-real-from-start] (time-events 3.5: RealTime not put back) elapsed " << Get<W::RealTime>(p).elapsed);
    CheckReadsEqualSteps(save.tick.value, "S2-8");
    CHECK_MESSAGE(Get<Score>(p).v == save.score + 5, "[sc-s2-8-score-goes-on] " << Get<Score>(p).v);

    // Budget (judgement 5, measured, not a check): what the message buffers hold at the end of segment 2.
    const std::size_t gainedBytes = MessageBufferBytes<ScoreGained>(WorldOf(p));
    const std::size_t discardedBytes = MessageBufferBytes<W::TimeDiscarded>(WorldOf(p));
    std::printf("[sc-s2-budget-memory] first World before the load: %zu bytes; new World at the end: ScoreGained %zu bytes (record %zu), "
                "TimeDiscarded %zu bytes (record %zu), total %zu bytes\n",
                oldBufferBytes, gainedBytes, sizeof(K::Detail::MessageBuffer<ScoreGained>::Record), discardedBytes,
                sizeof(K::Detail::MessageBuffer<W::TimeDiscarded>::Record), gainedBytes + discardedBytes);
}
