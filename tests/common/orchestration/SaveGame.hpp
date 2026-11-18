#pragma once

// A synthetic single-player game with a save (declaration section 11, judgement
// 2; restore-entry-declaration revisions 10 and 11), registered by the
// product's own build. Not a product: only what the orchestration tests need.

#include "OrchestrationTestSupport.hpp"

#include "engine/base/Log.hpp"

#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace OrchTest::Save {

struct Player {
    int hp = 0;
};
struct Pos {
    int x = 0;
};
struct ChaseTarget {
    K::EntityId target = 0;
};
// Only SpawnBadId spawns it (one counted writer per component until batch 4, DI-KS 6).
struct Junk {
    int v = 0;
};
struct Gold {
    int v = 0;
};
struct KeyBindings {
    int jump = 0;
};
struct Score {
    int v = 0;
};
struct Volume {
    int level = 0;
};
// A Resource whose last live copy counts its destruction (moves hand the life over).
struct Sentinel {
    bool live = true;
    Sentinel() = default;
    Sentinel(Sentinel&& other) noexcept : live(other.live) { other.live = false; }
    Sentinel& operator=(Sentinel&& other) noexcept {
        live = other.live;
        other.live = false;
        return *this;
    }
    Sentinel(const Sentinel&) = delete;
    Sentinel& operator=(const Sentinel&) = delete;
    ~Sentinel() {
        if (live) ++Destroyed();
    }
    static int& Destroyed() {
        static int count = 0;
        return count;
    }
};
// The author's save format (the engine knows nothing of it). Fixed size: the
// World summary hashes a Resource's bytes, so no heap pointer may be in it.
struct SaveData {
    struct Ent {
        K::EntityId id = 0;
        int hp = 0;
        int x = 0;
        K::EntityId chase = 0;
    };
    std::array<Ent, 3> ents{};
    int gold = 0;
    K::EntityId next = 0;
    // The entity part cannot be read: Load fails.
    bool unreadable = false;
};

inline SaveData TheSave() {
    SaveData save;
    save.ents = {SaveData::Ent{12, 5, 1, 0}, SaveData::Ent{57, 7, 2, 12}, SaveData::Ent{58, 9, 3, 0}};
    save.gold = 120;
    save.next = 900;
    return save;
}

inline constexpr K::EntityId kLast = 1000000;

inline int& BootRuns() {
    static int runs = 0;
    return runs;
}

inline K::SystemResult Boot(K::ExclusiveWorld<> world) {
    ++BootRuns();
    if (auto set = world.SetResource(KeyBindings{1}); !set) return Engine::Base::Err(std::move(set).error());
    if (auto set = world.SetResource(Score{5}); !set) return Engine::Base::Err(std::move(set).error());
    return {};
}

// Puts the save back (an exclusive System: its writes are not counted as
// writers, so Gold has one counted writer, GoldUp; DI-KS 6 until batch 4).
inline K::SystemResult LoadSave(K::ExclusiveWorld<> world) {
    auto read = world.Read<SaveData>([](const SaveData& s) { return s; });
    if (!read) return Engine::Base::Err(std::move(read).error());
    if (!*read) return {};
    const SaveData& data = **read;
    if (data.unreadable) return Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "save entities unreadable"));
    for (const SaveData::Ent& e : data.ents) {
        auto spawned = world.SpawnWithId(e.id, Player{e.hp}, Pos{e.x}, ChaseTarget{e.chase});
        if (!spawned) return Engine::Base::Err(std::move(spawned).error());
        if (*spawned) Log().push_back("dropped " + std::to_string(e.id));
    }
    auto gold = world.Modify<Gold>([&](Gold& g) { g.v = data.gold; });
    if (!gold) return Engine::Base::Err(std::move(gold).error());
    return {};
}

inline K::SystemResult GoldUp(K::ResMut<Gold> gold) {
    gold->v += 10;
    return {};
}
inline K::SystemResult SpawnRock(K::Spawn<K::Bundle<Pos>> spawn) {
    static_cast<void>(spawn.New(Pos{-1}));
    return {};
}
// Spawns an id inside the allotter range: dropped as IdInAllotterRange (a record).
inline K::SystemResult SpawnBadId(K::SpawnWithId<K::Bundle<Junk>> spawn) {
    spawn.New(1, Junk{-2});
    return {};
}

struct Options {
    // Allotter range of the description's prepare.
    K::EntityId first = 1;
    // A save the description's prepare puts in (startup loading).
    bool startupSave = false;
};

// The product's own registration of the game.
inline void Register(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Component<Tag>({.name = "Tag"});
    r.Component<Player>({.name = "Player"});
    r.Component<Pos>({.name = "Pos"});
    r.Component<ChaseTarget>({.name = "ChaseTarget"});
    r.Component<Junk>({.name = "Junk"});
    r.Resource<Gold>({.name = "Gold"});
    r.Resource<KeyBindings>({.name = "KeyBindings"});
    r.Resource<Score>({.name = "Score"});
    r.Resource<SaveData>({.name = "SaveData"});
    r.Resource<Sentinel>({.name = "Sentinel"});
    r.Resource<K::RangeAllotter>({.name = "Allotter"});
    r.Allotter<K::RangeAllotter>();
    r.Schedule(Once("Boot"));
    r.Schedule(Once("Load"));
    r.Schedule(Once("Junk"));
    r.Schedule(FrameClock("Play"));
    r.AddSystem<&Boot>(In("Boot", "Boot"));
    r.AddSystem<&LoadSave>(In("LoadSave", "Load"));
    r.AddSystem<&SpawnBadId>(In("SpawnBadId", "Junk"));
    r.AddSystem<&GoldUp>(In("GoldUp", "Play"));
    r.AddSystem<&SpawnRock>(In("SpawnRock", "Play"));
    s.CarryOver<KeyBindings>("KeyBindings");
}

// Defaults that never overwrite a carried value (section 3.12).
inline Engine::Base::Result<void, O::OrchestrationError> Defaults(K::World& w, K::EntityId first) {
    if (w.Resource<KeyBindings>() == nullptr) w.SetResource(KeyBindings{0});
    w.SetResource(Gold{0});
    w.SetResource(Score{0});
    w.SetResource(Sentinel{});
    w.SetResource(K::RangeAllotter::Of(first, kLast));
    return {};
}

// The extra prepare of an in-game load: the save and the allotter's lower bound (restore 5.1).
inline O::PrepareFn LoadInto(SaveData save, bool moveLowerBound = true) {
    return [save, moveLowerBound](K::World& w) -> Engine::Base::Result<void, O::OrchestrationError> {
        w.SetResource(save);
        if (moveLowerBound) w.SetResource(K::RangeAllotter::Of(save.next, kLast));
        return {};
    };
}

inline O::ProductDescription Game(Options options = {}) {
    O::ProductDescription d;
    d.name = "rpg";
    d.gameInit = {"Boot"};
    O::WorldDescription main = World("Main", &Register, {"Play"});
    main.prepare.push_back([options](K::World& w) -> Engine::Base::Result<void, O::OrchestrationError> {
        if (auto done = Defaults(w, options.first); !done) return done;
        if (options.startupSave) w.SetResource(TheSave());
        return {};
    });
    d.worlds.push_back(std::move(main));
    return d;
}

// Values read for checks; -1 when the Resource is absent (never a null dereference in an assertion).
inline int JumpOf(O::Product& p, std::string_view world = "Main") {
    const KeyBindings* k = p.World(world).GetWorld().Resource<KeyBindings>();
    return k == nullptr ? -1 : k->jump;
}
inline int VolumeOf(O::Product& p, std::string_view world = "Main") {
    const Volume* v = p.World(world).GetWorld().Resource<Volume>();
    return v == nullptr ? -1 : v->level;
}
inline int ScoreOf(O::Product& p, std::string_view world = "Main") {
    const Score* v = p.World(world).GetWorld().Resource<Score>();
    return v == nullptr ? -1 : v->v;
}

// Counts orchestration warnings (Engine::Base::Log) while alive.
class WarningCounter {
public:
    WarningCounter() : previous_(Engine::Base::SetLogHandler(&Count)) {
        Counter().store(0);
        Last().clear();
    }
    ~WarningCounter() { Engine::Base::SetLogHandler(previous_); }
    WarningCounter(const WarningCounter&) = delete;
    WarningCounter& operator=(const WarningCounter&) = delete;
    [[nodiscard]] int Warnings() const { return Counter().load(); }
    // The text of the last orchestration warning (single-threaded tests only).
    [[nodiscard]] static std::string& Last() {
        static std::string last;
        return last;
    }

private:
    static std::atomic<int>& Counter() {
        static std::atomic<int> counter{0};
        return counter;
    }
    static void Count(const Engine::Base::LogMessage& message) noexcept {
        if (message.level == Engine::Base::LogLevel::Warning && message.category == "orchestration") {
            ++Counter();
            try {
                Last() = std::string(message.text);
            } catch (...) {
            }
        }
    }
    Engine::Base::LogHandler previous_;
};

} // namespace OrchTest::Save
