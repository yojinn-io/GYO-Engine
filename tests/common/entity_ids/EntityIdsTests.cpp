// The default entity allotter (world-gen declaration docs/architecture/target/v2/world-gen-declaration.zh-Hant.md,
// sections 5.10 and 11; Design Invariants EI-1, EI-2 in docs/architecture/target/v2/entity-ids.zh-Hant.md).
// "allotter:" are RangeAllotter's own properties (moved from the kernel's J-8 and D-9 with the type, section 10);
// "module:" are cases C13 to C15 (a Product with EntityIdsModule and units written by hand).
#include <doctest/doctest.h>

#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <vector>

#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/entity_ids/RangeAllotter.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace E = GYO::Engine::EntityIds;
using E::EntityId;
using E::RangeAllotter;

namespace {

template <class A>
concept HasReset = requires(A allotter) { allotter.Reset(); };

struct Card {
    std::int32_t value = 0;
};
inline void HashValue(K::Hasher& h, const Card& v) { HashValue(h, v.value); }

std::vector<EntityId>& Spawned() {
    static std::vector<EntityId> spawned;
    return spawned;
}

// Spawns one card per Run with an id from the World's allotter (the author declares no allotter).
K::SystemResult DealCard(K::Spawn<K::Bundle<Card>> spawn) {
    auto id = spawn.New(Card{7});
    if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
    Spawned().push_back(*id);
    return {};
}

K::SystemOptions InUpdate(std::string name) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::string(O::DefaultSchedules::frame);
    m.stage = std::string(O::DefaultSchedules::update);
    o.mounts.push_back(std::move(m));
    return o;
}

O::RegisteredUnits IdUnits() {
    O::RegisteredUnits units;
    O::RegisterRow row;
    row.name = "entity_ids";
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    units.rows.push_back(row);
    units.modules.push_back({"entity_ids", std::make_unique<E::EntityIdsModule>()});
    return units;
}

O::WorldDescription CardWorld(std::string name) {
    O::WorldDescription w;
    w.name = std::move(name);
    w.build = [](O::WorldSetup& s) {
        s.Registrar().Component<Card>({.name = "Card"});
        s.Registrar().AddSystem<&DealCard>(InUpdate("DealCard"));
    };
    return w;
}

std::unique_ptr<O::Product> Make(O::ProductDescription d) {
    auto built = O::Product::Build(std::move(d), IdUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

// RangeAllotter::Of built in memory that held `fill` before (what a stack slot may hold), then put into a World.
K::Fingerprint StoredAllotterSummary(unsigned char fill) {
    alignas(RangeAllotter) unsigned char raw[sizeof(RangeAllotter)];
    std::memset(raw, fill, sizeof raw);
    const RangeAllotter* made = new (raw) RangeAllotter(RangeAllotter::Of(900, 1000000));
    O::ProductDescription d;
    d.name = "summary";
    d.worlds.push_back(CardWorld("Main"));
    auto p = Make(std::move(d));
    K::World& world = p->World("Main").GetWorld();
    // Taken out first, so the value is copied in whole (as into a World that has none), padding bytes included.
    world.RemoveResource<RangeAllotter>();
    world.SetResource(*made);
    REQUIRE(world.Resource<RangeAllotter>()->next == 900);
    auto summary = world.Summary();
    REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().message));
    return *summary;
}

}  // namespace

TEST_CASE("allotter: monotonic, exhaustion reported, never wraps; no Reset") {
    RangeAllotter allotter = RangeAllotter::Of(100, 102);
    CHECK(allotter.Allot() == std::optional<EntityId>(100));
    CHECK(allotter.Allot() == std::optional<EntityId>(101));
    CHECK(allotter.Allot() == std::optional<EntityId>(102));
    CHECK(allotter.Allot() == std::nullopt);
    CHECK(allotter.Allot() == std::nullopt);
    CHECK(allotter.Owns(100));
    CHECK(allotter.Owns(102));
    CHECK_FALSE(allotter.Owns(103));
    CHECK_FALSE(allotter.Owns(99));
    CHECK(RangeAllotter::Of(5, 4).Allot() == std::nullopt);
    RangeAllotter top = RangeAllotter::Of(~EntityId{0} - 1, ~EntityId{0});
    CHECK(top.Allot() == std::optional<EntityId>(~EntityId{0} - 1));
    CHECK(top.Allot() == std::optional<EntityId>(~EntityId{0}));
    CHECK(top.Allot() == std::nullopt);
    // No way to reset it (DI-KS 4, W-9: a new generation is a new World).
    CHECK_MESSAGE(!HasReset<RangeAllotter>, "RangeAllotter must not have a Reset");
}

TEST_CASE("allotter: equal allotters give one World summary, whatever their padding holds") {
    const K::Fingerprint zeros = StoredAllotterSummary(0x00);
    const K::Fingerprint ones = StoredAllotterSummary(0xFF);
    CHECK_MESSAGE(zeros == ones, "[summary-padding-equal] equal allotters, different summaries: " << zeros.Text() << " vs " << ones.Text());
}

// C13: the author declares no allotter and spawns.
TEST_CASE("module: with entity_ids an author spawns without declaring an allotter; the first id is 1") {
    Spawned().clear();
    O::ProductDescription d;
    d.name = "cards";
    d.worlds.push_back(CardWorld("Main"));
    auto p = Make(std::move(d));
    REQUIRE(static_cast<bool>(p->RunFrame()));
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Spawned() == std::vector<EntityId>{1, 2}), "[ei-first-id] the default range starts at 1");
    const RangeAllotter* allotter = p->World("Main").GetWorld().Resource<RangeAllotter>();
    REQUIRE(allotter != nullptr);
    CHECK(allotter->last == 0xFFFF'FFFFULL);
}

// C14: each World its own range; options per World.
TEST_CASE("module: each World allots from its own range; EntityIdsOptions sets one World's range") {
    Spawned().clear();
    O::ProductDescription d;
    d.name = "two";
    O::WorldDescription ranged = CardWorld("Ranged");
    ranged.options.Set(E::EntityIdsOptions{100, 199});
    d.worlds.push_back(std::move(ranged));
    d.worlds.push_back(CardWorld("Plain"));
    auto p = Make(std::move(d));
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Spawned() == std::vector<EntityId>{100, 1}), "[ei-per-world] Ranged gave 100, Plain gave 1");
}

// C15: a value carried over is not replaced by the default.
TEST_CASE("module: a carried-over allotter is not replaced by the default range") {
    Spawned().clear();
    O::ProductDescription d;
    d.name = "carry";
    O::WorldDescription w = CardWorld("Main");
    w.build = [](O::WorldSetup& s) {
        s.Registrar().Component<Card>({.name = "Card"});
        s.Registrar().AddSystem<&DealCard>(InUpdate("DealCard"));
        s.CarryOver<RangeAllotter>("RangeAllotter");
    };
    d.worlds.push_back(std::move(w));
    auto p = Make(std::move(d));
    REQUIRE(static_cast<bool>(p->RunFrame()));
    REQUIRE(static_cast<bool>(p->RunFrame()));
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", O::ReplaceRequest{})));
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Spawned() == std::vector<EntityId>{1, 2, 3}),
                  "[ei-carry-kept] the carried allotter goes on at 3; the default would restart at 1");
}

namespace {

// Spawns one card with an id inside the default range (dropped at the sync point: IdInAllotterRange).
K::SystemResult PutCardAt5(K::SpawnWithId<K::Bundle<Card>> put) {
    put.New(5, Card{9});
    return {};
}

}  // namespace

// The kernel's machinery with the shipped allotter (the §14.4 check of the world-gen merge approval, 1.1): the
// kernel and orchestration tests run it with a test allotter, so this is the one case with the real type.
TEST_CASE("module: with the default allotter, an id in its range is dropped and a used-up range is reported") {
    Spawned().clear();
    {
        O::ProductDescription d;
        d.name = "in-range";
        O::WorldDescription w = CardWorld("Main");
        w.build = [](O::WorldSetup& s) {
            s.Registrar().Component<Card>({.name = "Card"});
            s.Registrar().AddSystem<&PutCardAt5>(InUpdate("PutCardAt5"));
        };
        d.worlds.push_back(std::move(w));
        auto p = Make(std::move(d));
        REQUIRE(static_cast<bool>(p->RunFrame()));
        K::World& world = p->World("Main").GetWorld();
        CHECK_MESSAGE(!world.Alive(5), "[ei-real-in-range-dropped] an id in the default range is not spawned");
        const std::vector<K::RejectedSpawn> rejected = world.Diagnostics<K::RejectedSpawn>();
        REQUIRE(rejected.size() == 1);
        CHECK(rejected[0].reason == K::SpawnRejectReason::IdInAllotterRange);
    }
    {
        O::ProductDescription d;
        d.name = "used-up";
        O::WorldDescription w = CardWorld("Main");
        w.options.Set(E::EntityIdsOptions{100, 101});
        d.worlds.push_back(std::move(w));
        auto p = Make(std::move(d));
        REQUIRE(static_cast<bool>(p->RunFrame()));
        REQUIRE(static_cast<bool>(p->RunFrame()));
        auto third = p->RunFrame();
        REQUIRE(!third);
        REQUIRE(p->Failure() != nullptr);
        CHECK_MESSAGE(p->Failure()->kernelCode == std::optional<K::KernelErrorCode>(K::KernelErrorCode::AllotterExhausted),
                      "[ei-real-exhausted] a used-up default range is reported as AllotterExhausted");
        CHECK((Spawned() == std::vector<EntityId>{100, 101}));
    }
}
