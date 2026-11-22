// Scenario segment 6 (world-gen declaration docs/architecture/target/v2/world-gen-declaration.zh-Hant.md, section 11):
// a walking game built from the default Modules it needs (entity_ids, world_time). A new game generates its map over
// three frames while time is paused (the loading screen's counter goes up, Tick stays 0); the player walks in the
// fixed step and scores; the test saves; an in-game load replaces the World and puts the player back over two
// generating frames; the walk goes on from the saved place and score. No screen, no input, not a product.
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/entity_ids/RangeAllotter.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::WorldTime;
namespace E = GYO::Engine::EntityIds;

namespace {

constexpr W::Nanoseconds kStep = 15'625'000;  // 64 Hz: one fixed step per frame with ManualDelta of one step
constexpr int kMapTiles = 3;

struct Tile {
    std::int32_t index = 0;
};
struct Hero {
    std::int32_t x = 0;
};
struct Score {
    std::int32_t v = 0;
};
// The loading screen's counter: one per generating frame.
struct LoadingFrames {
    std::int32_t n = 0;
};
// The map generation's cursor and the load's: what is already placed.
struct Progress {
    std::int32_t tiles = 0;
    std::int32_t heroPlaced = 0;
};
// The author's save (the engine knows nothing of it).
struct SaveData {
    std::int32_t present = 0;
    K::EntityId heroId = 0;
    std::int32_t heroX = 0;
    std::int32_t score = 0;
    K::EntityId next = 0;
};
inline void HashValue(K::Hasher& h, const Tile& v) { HashValue(h, v.index); }
inline void HashValue(K::Hasher& h, const Hero& v) { HashValue(h, v.x); }
inline void HashValue(K::Hasher& h, const Score& v) { HashValue(h, v.v); }
inline void HashValue(K::Hasher& h, const LoadingFrames& v) { HashValue(h, v.n); }
inline void HashValue(K::Hasher& h, const Progress& v) {
    HashValue(h, v.tiles);
    HashValue(h, v.heroPlaced);
}
inline void HashValue(K::Hasher& h, const SaveData& v) {
    HashValue(h, v.present);
    HashValue(h, v.heroId);
    HashValue(h, v.heroX);
    HashValue(h, v.score);
    HashValue(h, v.next);
}

int& Boots() {
    static int boots = 0;
    return boots;
}

// Game initialization: once per process.
K::SystemResult Boot() {
    ++Boots();
    return {};
}

// Gen: one map tile per generating frame; the loading screen counts.
K::SystemResult GenerateMap(K::Spawn<K::Bundle<Tile>> spawn, K::ResMut<Progress> progress, K::ResMut<LoadingFrames> loading,
                            K::ResMut<O::WorldGeneration> generation) {
    ++loading->n;
    if (progress->tiles < kMapTiles) {
        auto id = spawn.New(Tile{progress->tiles});
        if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
        ++progress->tiles;
    }
    if (progress->tiles < kMapTiles) generation->Hold();
    return {};
}

// Gen: a new game places the hero once the map is there; a load puts the saved hero back with its id.
K::SystemResult PlaceHero(K::Spawn<K::Bundle<Hero>> spawn, K::SpawnWithId<K::Bundle<Hero>> put, K::Res<SaveData> save,
                          K::ResMut<Progress> progress, K::ResMut<Score> score, K::ResMut<O::WorldGeneration> generation) {
    if (progress->heroPlaced != 0) return {};
    if (progress->tiles < kMapTiles) {
        generation->Hold();
        return {};
    }
    if (save->present != 0) {
        put.New(save->heroId, Hero{save->heroX});
        score->v = save->score;
    } else {
        auto id = spawn.New(Hero{0});
        if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
    }
    progress->heroPlaced = 1;
    return {};
}

// Game: the first frame after generation turns the pause off (world-gen declaration 5.6).
K::SystemResult Unpause(K::ResMut<W::TimeControl> control) {
    control->paused = 0;
    return {};
}

// FixedUpdate: the hero walks one step and scores.
K::SystemResult Walk(K::Query<K::Write<Hero>> heroes, K::ResMut<Score> score) {
    for (K::EntityId id : heroes.Entities()) {
        heroes.GetMut<Hero>(id)->x += 1;
        score->v += 10;
    }
    return {};
}

K::SystemOptions At(std::string name, std::string schedule, std::string stage, std::vector<std::string> sets = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    m.sets = std::move(sets);
    o.mounts.push_back(std::move(m));
    return o;
}

O::RegisteredUnits GameUnits() {
    O::RegisteredUnits units;
    for (const char* name : {"entity_ids", "world_time"}) {
        O::RegisterRow row;
        row.name = name;
        row.kind = O::UnitKind::Module;
        row.libraryOnly = false;
        row.defaultOn = true;
        units.rows.push_back(row);
    }
    units.modules.push_back({"entity_ids", std::make_unique<E::EntityIdsModule>()});
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    return units;
}

void Register(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Component<Tile>({.name = "Tile"});
    r.Component<Hero>({.name = "Hero"});
    r.Resource<Score>({.name = "Score"});
    r.Resource<LoadingFrames>({.name = "LoadingFrames"});
    r.Resource<Progress>({.name = "Progress"});
    r.Resource<SaveData>({.name = "SaveData"});
    r.Schedule(K::ScheduleOptions{.name = "Boot", .stages = {"S"}});
    r.AddSystem<&Boot>(At("Boot", "Boot", "S"));
    const std::string main(O::DefaultSchedules::frame);
    const std::string update(O::DefaultSchedules::update);
    r.Set(K::SetOptions{.name = "Gen", .schedule = main}, {O::WhileGenerating()});
    r.Set(K::SetOptions{.name = "Game", .schedule = main}, {O::AfterGenerating()});
    r.AddSystem<&GenerateMap>(At("GenerateMap", main, update, {"Gen"}));
    K::SystemOptions place = At("PlaceHero", main, update, {"Gen"});
    place.mounts[0].after = {"GenerateMap"};
    r.AddSystem<&PlaceHero>(place);
    r.AddSystem<&Unpause>(At("Unpause", main, update, {"Game"}));
    r.AddSystem<&Walk>(At("Walk", std::string(O::DefaultSchedules::fixed), update));
}

// Every World starts paused, with no save (a load's extra puts one).
GYO::Engine::Base::Result<void, O::OrchestrationError> Defaults(K::World& w) {
    w.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    w.SetResource(W::TimeControl{1.0, 1});
    w.SetResource(Score{});
    w.SetResource(LoadingFrames{});
    w.SetResource(Progress{});
    w.SetResource(SaveData{});
    return {};
}

std::unique_ptr<O::Product> NewGame() {
    O::ProductDescription d;
    d.name = "walker";
    d.gameInit = {"Boot"};
    O::WorldDescription w;
    w.name = "Main";
    w.build = Register;
    w.prepare.push_back(Defaults);
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), GameUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

void Frame(O::Product& p) {
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

std::uint64_t TickOf(O::Product& p) { return WorldOf(p).Resource<W::Tick>()->value; }

struct HeroState {
    K::EntityId id = 0;
    std::int32_t x = 0;
};
HeroState TheHero(O::Product& p) {
    const std::vector<K::EntityId> ids = WorldOf(p).Keys<Hero>();
    REQUIRE(ids.size() == 1);
    return HeroState{ids[0], WorldOf(p).Get<Hero>(ids[0])->x};
}

}  // namespace

TEST_CASE("scenario segment 6: S6-1 to S6-5 generate the map paused, walk, save, load over two frames, walk on") {
    Boots() = 0;
    auto p = NewGame();

    // S6-1: three generating frames; the loading screen counts; time is paused, Tick 0.
    for (int frame = 1; frame <= kMapTiles; ++frame) {
        Frame(*p);
        CHECK_MESSAGE(TickOf(*p) == 0, "[sc-s6-1-paused] frame " << frame << " ticked");
    }
    CHECK_MESSAGE(!p->Generating("Main"), "[sc-s6-1-ended] the map and the hero are placed after three frames");
    CHECK(WorldOf(*p).Resource<LoadingFrames>()->n == kMapTiles);
    CHECK(WorldOf(*p).Keys<Tile>().size() == static_cast<std::size_t>(kMapTiles));
    CHECK(Boots() == 1);

    // S6-2: the first frame after generation unpauses (still no step), then one step per frame.
    Frame(*p);
    CHECK(TickOf(*p) == 0);
    for (int frame = 0; frame < 5; ++frame) Frame(*p);
    CHECK_MESSAGE(TickOf(*p) == 5, "[sc-s6-2-walked] five frames after the unpause, five steps");
    const HeroState walked = TheHero(*p);
    CHECK(walked.x == 5);
    CHECK(WorldOf(*p).Resource<Score>()->v == 50);

    // S6-3: the test saves (the author's format).
    SaveData save;
    save.present = 1;
    save.heroId = walked.id;
    save.heroX = walked.x;
    save.score = WorldOf(*p).Resource<Score>()->v;
    save.next = WorldOf(*p).Resource<E::RangeAllotter>()->next;

    // S6-4: an in-game load: a new World, the save and the allotter's lower bound in extra; generation regenerates
    // the map (three frames) and puts the hero back with its id on the third.
    O::ReplaceRequest request;
    request.extra = [save](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        w.SetResource(save);
        // The Module's prepare function already put the World's range (EntityIdsOptions): keep its last.
        const K::EntityId last = w.Resource<E::RangeAllotter>()->last;
        w.SetResource(E::RangeAllotter::Of(save.next, last));
        return {};
    };
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", std::move(request))));
    REQUIRE(p->Generating("Main"));
    for (int frame = 1; frame <= kMapTiles; ++frame) {
        Frame(*p);
        CHECK(TickOf(*p) == 0);
    }
    CHECK_MESSAGE(!p->Generating("Main"), "[sc-s6-4-ended] the load ended after three frames");
    const HeroState back = TheHero(*p);
    CHECK_MESSAGE(back.id == walked.id, "[sc-s6-4-same-id] the hero is back with its saved id");
    CHECK(back.x == walked.x);
    CHECK(WorldOf(*p).Resource<Score>()->v == 50);
    CHECK_MESSAGE(Boots() == 1, "[sc-s6-4-no-game-init] game initialization did not run again");
    const std::vector<K::EntityId> tiles = WorldOf(*p).Keys<Tile>();
    for (K::EntityId id : tiles) CHECK_MESSAGE(id >= save.next, "[sc-s6-4-above-bound] regenerated tile " << id);

    // S6-5: unpause, then five more steps from the saved place and score.
    Frame(*p);
    for (int frame = 0; frame < 5; ++frame) Frame(*p);
    CHECK_MESSAGE(TheHero(*p).x == walked.x + 5, "[sc-s6-5-walked-on] the walk goes on from the saved place");
    CHECK(WorldOf(*p).Resource<Score>()->v == 100);
}
