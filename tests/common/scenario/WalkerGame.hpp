#pragma once

// "Walk and score": the synthetic game of the scenario tests
// (docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
// section 8; v8-arch-gyo2-scenario-tests). Written against the new contracts
// only; no old game's code, no screen, no input, not a product (it is the
// product's own registration of a Product built in the test).
//
// Each column has one counted writer until batch 4 (DI-KS 6): Position is
// written by Walk, Velocity by Accelerate, Score by CountScore, KeyBindings by
// OptionsMenu; Boot, PlacePlayer and LoadSave use ExclusiveWorld (not counted)
// and BestScore goes through the untyped Commands (not counted).

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <array>
#include <type_traits>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Walker {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;

struct Position {
    int x = 0;
    int y = 0;
};
struct Velocity {
    int x = 0;
    int y = 0;
};
struct Player {
    int tag = 0;
};
// The cursor LoadSave spawns on the spot after putting the player back.
struct Cursor {
    int tag = 0;
};
struct Score {
    int v = 0;
};
struct BestScore {
    int v = 0;
};
struct KeyBindings {
    int jump = 0;
};
// The author's save format (the engine knows nothing of it): the player's
// state, the scores and the allotter's next number. The velocity is not saved
// (the core of a save is the player's state; new-world-default). Fixed size:
// the World summary hashes a Resource's bytes.
struct SaveData {
    bool hasPlayer = true;
    // Zeroed bytes where padding would be: the kernel compares and hashes a
    // Resource's raw bytes, and padding is not copied the same on every compiler.
    std::array<std::uint8_t, 7> reserved{};
    K::EntityId player = 0;
    int x = 0;
    int y = 0;
    int score = 0;
    int best = 0;
    K::EntityId next = 0;
};
static_assert(std::has_unique_object_representations_v<SaveData>, "no padding: the kernel hashes the raw bytes");

inline constexpr K::EntityId kLast = 1000000;

// What the game's Systems saw, for the checks (the test's own record, not game state).
struct Notes {
    int bootRuns = 0;
    // The order Boot and PlacePlayer ran in ("boot", "place").
    std::vector<std::string> order;
    // BestScore as Peek saw it each frame, after CountScore and before the sync point.
    std::vector<bool> bestSeenBeforeSync;
    // LoadSave: the player's x read back right after SpawnWithId; the cursor's id.
    std::optional<int> playerSeenOnSpot;
    std::optional<K::EntityId> cursor;
    // LoadSave: why SpawnWithId did not put the player back (DI-KS 18: a dropped spawn is Ok with its reason).
    std::optional<K::SpawnRejectReason> playerRejected;
    // LoadSave: the code of a spawn LoadSave handled itself.
    std::optional<K::KernelErrorCode> handledSpawnError;
};
inline Notes& Seen() {
    static Notes notes;
    return notes;
}

// Each World's prepare puts one with a new serial; the destruction of its last
// live copy (moves hand the life over) records the serial, so a check can tell
// which World is gone.
struct WorldLife {
    int serial = 0;
    bool live = true;
    WorldLife() : serial(++Issued()) {}
    WorldLife(WorldLife&& other) noexcept : serial(other.serial), live(other.live) { other.live = false; }
    WorldLife& operator=(WorldLife&& other) noexcept {
        serial = other.serial;
        live = other.live;
        other.live = false;
        return *this;
    }
    WorldLife(const WorldLife&) = delete;
    WorldLife& operator=(const WorldLife&) = delete;
    ~WorldLife() {
        if (live) Destroyed().push_back(serial);
    }
    static int& Issued() {
        static int issued = 0;
        return issued;
    }
    static std::vector<int>& Destroyed() {
        static std::vector<int> destroyed;
        return destroyed;
    }
};

inline K::SystemResult Fail(K::KernelError error) { return ::GYO::Engine::Base::Err(std::move(error)); }

// ---- Systems

// Game initialization: the score starts at 0, the key bindings at 1.
inline K::SystemResult Boot(K::ExclusiveWorld<> world) {
    ++Seen().bootRuns;
    Seen().order.push_back("boot");
    if (auto set = world.SetResource(Score{0}); !set) return Fail(std::move(set).error());
    if (auto set = world.SetResource(KeyBindings{1}); !set) return Fail(std::move(set).error());
    return {};
}

inline K::SystemResult PlacePlayer(K::ExclusiveWorld<> world) {
    Seen().order.push_back("place");
    auto spawned = world.Spawn(Player{}, Position{0, 0}, Velocity{1, 0});
    if (!spawned) return Fail(std::move(spawned).error());
    return {};
}

inline K::SystemResult Walk(K::Query<K::Read<Velocity>, K::Write<Position>> walkers) {
    for (K::EntityId id : walkers.Entities()) {
        const Velocity* v = walkers.Get<Velocity>(id);
        auto p = walkers.GetMut<Position>(id);
        p->x += v->x;
        p->y += v->y;
    }
    return {};
}

// One point a frame; the first point also records the best score through Commands
// (a deferred change, applied at the sync point: DI-KS 9).
inline K::SystemResult CountScore(K::ResMut<Score> score, K::Res<BestScore> best, K::Commands commands) {
    score->v += 1;
    if (!best.Present()) commands.SetResource(BestScore{score.Read().v});
    return {};
}

inline K::SystemResult Peek(K::Res<BestScore> best) {
    Seen().bestSeenBeforeSync.push_back(best.Present());
    return {};
}

// The options menu: on the third point the player rebinds jump to 7.
inline K::SystemResult OptionsMenu(K::ResMut<KeyBindings> keys, K::Res<Score> score) {
    if (score->v == 3 && keys.Read().jump == 1) keys->jump = 7;
    return {};
}

// Inserted later by the console (S1-4, S1-9): before Walk.
inline K::SystemResult Accelerate(K::Query<K::Write<Velocity>> movers) {
    for (K::EntityId id : movers.Entities()) movers.GetMut<Velocity>(id)->x = 2;
    return {};
}

// Puts the save back on the spot (DI-KS 18; restore declaration 5.1): the player
// with its saved id, the scores, then a cursor with an id from the allotter.
inline K::SystemResult LoadSave(K::ExclusiveWorld<> world) {
    auto read = world.Read<SaveData>([](const SaveData& s) { return s; });
    if (!read) return Fail(std::move(read).error());
    if (!*read) return {};
    const SaveData save = **read;
    if (save.hasPlayer) {
        auto put = world.SpawnWithId(save.player, Player{}, Position{save.x, save.y}, Velocity{1, 0});
        if (!put) return Fail(std::move(put).error());
        if (*put) {
            // Dropped (its id is in the allotter's range, or alive): the game records why and goes on.
            Seen().playerRejected = **put;
            return {};
        }
        auto x = world.Read<Position>(save.player, [](const Position& p) { return p.x; });
        if (!x) return Fail(std::move(x).error());
        Seen().playerSeenOnSpot = *x;
    }
    auto scored = world.Modify<Score>([&](Score& s) { s.v = save.score; });
    if (!scored) return Fail(std::move(scored).error());
    if (auto set = world.SetResource(BestScore{save.best}); !set) return Fail(std::move(set).error());
    auto cursor = world.Spawn(Cursor{});
    if (!cursor) {
        // The allotter gave an id that is alive (S1-7b): the game handles it and goes on.
        if (cursor.error().code != K::KernelErrorCode::AllottedIdAlive) return Fail(std::move(cursor).error());
        Seen().handledSpawnError = cursor.error().code;
        return {};
    }
    Seen().cursor = *cursor;
    return {};
}

// ---- Registration (the product's own build, section 3.3)

// A one-stage schedule. The domain declarations batch 2 added for D-E12 left with
// the kernel's clock in batch 3 (S2-9, v8-arch-gyo2-clock-out-of-kernel).
inline K::ScheduleOptions Schedule(std::string name) {
    K::ScheduleOptions s;
    s.name = std::move(name);
    s.stages = {"S"};
    return s;
}
inline K::SystemOptions In(std::string name, std::string schedule, std::vector<std::string> before = {},
                           std::vector<std::string> after = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = "S";
    m.before = std::move(before);
    m.after = std::move(after);
    o.mounts.push_back(std::move(m));
    return o;
}

inline void Register(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Component<Position>({.name = "Position"});
    r.Component<Velocity>({.name = "Velocity"});
    r.Component<Player>({.name = "Player"});
    r.Component<Cursor>({.name = "Cursor"});
    r.Resource<Score>({.name = "Score"});
    r.Resource<BestScore>({.name = "BestScore"});
    r.Resource<KeyBindings>({.name = "KeyBindings"});
    r.Resource<SaveData>({.name = "SaveData"});
    r.Resource<WorldLife>({.name = "WorldLife"});
    r.Resource<K::RangeAllotter>({.name = "Allotter"});
    r.Allotter<K::RangeAllotter>();
    r.Schedule(Schedule("Boot"));
    r.Schedule(Schedule("PlacePlayer"));
    r.Schedule(Schedule("Load"));
    r.Schedule(Schedule("Play"));
    r.AddSystem<&Boot>(In("Boot", "Boot"));
    r.AddSystem<&PlacePlayer>(In("PlacePlayer", "PlacePlayer"));
    r.AddSystem<&LoadSave>(In("LoadSave", "Load"));
    r.AddSystem<&Walk>(In("Walk", "Play"));
    r.AddSystem<&CountScore>(In("CountScore", "Play", {}, {"Walk"}));
    r.AddSystem<&Peek>(In("Peek", "Play", {}, {"CountScore"}));
    r.AddSystem<&OptionsMenu>(In("OptionsMenu", "Play", {}, {"CountScore"}));
    // The key bindings outlive a World replacement (section 3.12; carry-over-mechanism).
    s.CarryOver<KeyBindings>("KeyBindings");
}

inline void InsertAccelerate(K::Registrar& r) { r.AddSystem<&Accelerate>(In("Accelerate", "Play", {"Walk"})); }

// The description's prepare: defaults that never overwrite what is there (a
// carried value), the allotter range [1, last], the life marker.
inline ::GYO::Engine::Base::Result<void, O::OrchestrationError> Defaults(K::World& w) {
    if (w.Resource<KeyBindings>() == nullptr) w.SetResource(KeyBindings{0});
    w.SetResource(K::RangeAllotter::Of(1, kLast));
    w.SetResource(WorldLife{});
    return {};
}

inline O::ProductDescription Game() {
    O::ProductDescription d;
    d.name = "walker";
    d.gameInit = {"Boot"};
    O::WorldDescription main;
    main.name = "Main";
    main.build = &Register;
    main.frame = {"Play"};
    main.prepare.push_back(&Defaults);
    d.worlds.push_back(std::move(main));
    return d;
}

// ---- Save and load (the author's, outside the engine)

// Reads what the save keeps from the World (S1-5).
inline SaveData MakeSave(const K::World& w) {
    SaveData save;
    const std::vector<K::EntityId> players = w.Keys<Player>();
    if (!players.empty()) {
        save.player = players.front();
        const Position* p = w.Get<Position>(save.player);
        save.x = p != nullptr ? p->x : 0;
        save.y = p != nullptr ? p->y : 0;
    }
    const Score* score = w.Resource<Score>();
    save.score = score != nullptr ? score->v : -1;
    const BestScore* best = w.Resource<BestScore>();
    save.best = best != nullptr ? best->v : -1;
    const K::RangeAllotter* allotter = w.Resource<K::RangeAllotter>();
    save.next = allotter != nullptr ? allotter->next : 0;
    return save;
}

// The extra prepare of a load (S1-6): the save, the allotter's lower bound at
// the save's next number, the score's default.
inline O::PrepareFn LoadInto(SaveData save) {
    return [save](K::World& w) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        w.SetResource(save);
        w.SetResource(K::RangeAllotter::Of(save.next, kLast));
        w.SetResource(Score{0});
        return {};
    };
}

// S1-7b: the extra forgets the lower bound and puts the player back itself, so
// the save it leaves has no player. A direct insert through the World API
// neither checks nor moves the allotter's range; moving the lower bound on a
// load is the author's job (DI-KS 18, "a direct insert does not look at the
// allotter"; v8-arch-gyo2-direct-insert-no-allotter-range).
inline O::PrepareFn LoadForgettingTheLowerBound(SaveData save) {
    return [save](K::World& w) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        SaveData rest = save;
        rest.hasPlayer = false;
        w.SetResource(rest);
        w.SetResource(Score{0});
        w.Insert(save.player, Player{});
        w.Insert(save.player, Position{save.x, save.y});
        w.Insert(save.player, Velocity{1, 0});
        return {};
    };
}

// Values for the checks; nothing (not a null dereference) when absent.
inline std::optional<int> ScoreOf(const K::World& w) {
    const Score* s = w.Resource<Score>();
    return s != nullptr ? std::optional<int>(s->v) : std::nullopt;
}
inline std::optional<int> BestOf(const K::World& w) {
    const BestScore* s = w.Resource<BestScore>();
    return s != nullptr ? std::optional<int>(s->v) : std::nullopt;
}
inline std::optional<int> JumpOf(const K::World& w) {
    const KeyBindings* k = w.Resource<KeyBindings>();
    return k != nullptr ? std::optional<int>(k->jump) : std::nullopt;
}
inline std::optional<int> XOf(const K::World& w, K::EntityId id) {
    const Position* p = w.Get<Position>(id);
    return p != nullptr ? std::optional<int>(p->x) : std::nullopt;
}

} // namespace Walker
