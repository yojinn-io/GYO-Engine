// The rollback primitives of ExclusiveWorld (v8-arch-gyo2-authority-rollback
// step 2; declaration 2.1 and the Kernel cases of 9.3): ReadColumn gives every
// cell with its stamps (primitive 1), Stamp the handle's stamp, Entities the
// alive set and ResourceStamp a resource's stamp (primitives 3 and 4, the read),
// and Restore puts a recorded state back as one immediate change (primitives
// 2 to 4): cells that did not change are not written, the ones that did take
// one stamp, an entity of the allotter's range comes back alive with its id,
// and an OnAdd hook sees the entity's other cells already put back. Each CHECK
// carries a tag in brackets ("[ks-restore-...]"): the step's mutants expect it.

#include "WorldTestSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace WorldTest;

std::string Join(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) out += (out.empty() ? "" : " ") + line;
    return out;
}

bool SamePos(const Pos& a, const Pos& b) { return a.x == b.x; }
bool SameTag(const Tag& a, const Tag& b) { return a.t == b.t; }
bool SameOut(const Out& a, const Out& b) { return a.value == b.value; }

struct Recorded {
    std::vector<EntityId> alive;
    std::vector<std::pair<EntityId, Pos>> pos;
    std::vector<std::tuple<EntityId, ChangeStamp, ChangeStamp>> posStamps;
    std::vector<std::pair<EntityId, Tag>> tag;
    std::optional<Out> out;
    std::optional<ChangeStamp> outStamp;
    ChangeStamp stamp = 0;
    ChangeStamp restoreStamp = 0;
};
Recorded& Record() {
    static Recorded recorded;
    return recorded;
}
int& Phase() {
    static int phase = 0;
    return phase;
}

// One exclusive System, three phases chosen by the test between Runs: record, change, restore.
SystemResult Act(ExclusiveWorld<> world) {
    Recorded& r = Record();
    switch (Phase()) {
    case 1: {
        r = Recorded{};
        r.alive = world.Entities();
        world.ReadColumn<Pos>([&](EntityId e, ChangeStamp added, ChangeStamp changed, const Pos& p) {
            r.pos.push_back({e, p});
            r.posStamps.emplace_back(e, added, changed);
        });
        world.ReadColumn<Tag>([&](EntityId e, ChangeStamp, ChangeStamp, const Tag& t) { r.tag.push_back({e, t}); });
        auto out = world.Read<Out>([](const Out& o) { return o; });
        if (!out) return Engine::Base::Err(std::move(out).error());
        r.out = *out;
        r.outStamp = world.ResourceStamp<Out>();
        r.stamp = world.Stamp();
        return Ok();
    }
    case 2: {
        auto despawned = world.Despawn(101);
        if (!despawned) return Engine::Base::Err(std::move(despawned).error());
        auto moved = world.Modify<Pos>(2, [](Pos& p) { p.x = 99; });
        if (!moved) return Engine::Base::Err(std::move(moved).error());
        auto removed = world.Remove<Tag>(3);
        if (!removed) return Engine::Base::Err(std::move(removed).error());
        auto set = world.SetResource(Out{7});
        if (!set) return Engine::Base::Err(std::move(set).error());
        return Ok();
    }
    case 3: {
        RestoreSet set;
        set.Alive(r.alive);
        set.Column<Pos>(r.pos, &SamePos);
        set.Column<Tag>(r.tag, &SameTag);
        set.Resource<Out>(r.out, &SameOut);
        r.restoreStamp = world.Stamp();
        auto restored = world.Restore(std::move(set));
        if (!restored) return Engine::Base::Err(std::move(restored).error());
        return Ok();
    }
    case 4: {
        // Writes Pos of 2 in a later step than the Host put it in: its changed stamp is above its added stamp.
        auto moved = world.Modify<Pos>(2, [](Pos& p) { p.x = 20; });
        if (!moved) return Engine::Base::Err(std::move(moved).error());
        return Ok();
    }
    case 6: {
        auto despawned = world.Despawn(2);
        if (!despawned) return Engine::Base::Err(std::move(despawned).error());
        return Ok();
    }
    case 5: {
        // Only the Pos column: no Alive (the alive set stays), no Tag, no Out.
        RestoreSet set;
        set.Column<Pos>(r.pos, &SamePos);
        auto restored = world.Restore(std::move(set));
        if (!restored) return Engine::Base::Err(std::move(restored).error());
        return Ok();
    }
    default: return Ok();
    }
}

// An OnAdd of Pos reads the entity's Tag: put back in the same restore, it is there already (whatever the
// order of the columns).
SystemResult PosAddedReadsTag(HookCtx ctx, Query<Read<Tag>> tags) {
    const Tag* tag = tags.Get<Tag>(ctx.Entity());
    Log().push_back("pos-added:" + std::to_string(ctx.Entity()) + " tag=" + (tag ? std::to_string(tag->t) : std::string("none")));
    return Ok();
}

// An OnAdd of Tag reads the entity's Pos: put back in the same restore, it is there already.
SystemResult TagAddedReadsPos(HookCtx ctx, Query<Read<Pos>> positions) {
    const Pos* pos = positions.Get<Pos>(ctx.Entity());
    Log().push_back("tag-added:" + std::to_string(ctx.Entity()) + " pos=" + (pos ? std::to_string(pos->x) : std::string("none")));
    return Ok();
}

Tag TagFromPos(const Pos& pos) { return Tag{pos.x + 1}; }

std::unique_ptr<Rig> RestoreRig(bool posHook = false, bool initialTag = false) {
    auto rig = MakeRig([posHook, initialTag](Registrar& r) {
        Data(r, true);
        if (initialTag) r.InitialValue<Tag, &TagFromPos>();
        r.AddSystem<&Act>(B("Act", "A"));
        if (posHook) {
            r.Hook<Pos, &PosAddedReadsTag>(HookTrigger::OnAdd, {.name = "PosAddedReadsTag"});
        } else {
            r.Hook<Tag, &TagAddedReadsPos>(HookTrigger::OnAdd, {.name = "TagAddedReadsPos"});
        }
    });
    World& world = rig->world();
    world.SetResource(RangeAllotter::Of(100, 199));
    world.SetResource(Out{1});
    // 101 is in the allotter's range (the Host may put it in); 1 to 3 are not.
    world.Insert(101, Pos{5});
    world.Insert(101, Tag{50});
    world.Insert(1, Pos{10});
    world.Insert(2, Pos{20});
    world.Insert(3, Pos{30});
    world.Insert(3, Tag{31});
    return rig;
}

} // namespace

TEST_CASE("restore: ReadColumn gives every cell with its stamps; Stamp, Entities and ResourceStamp read the rest") {
    auto rig = RestoreRig();
    // The first Run's step may take the stamp the Host's writes of the Ready window took: write in the second.
    Phase() = 0;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 4;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const Recorded& r = Record();
    const auto written = rig->world().Stamps<Pos>(2);
    REQUIRE((written.has_value() && written->second > written->first));
    CHECK_MESSAGE((r.alive == std::vector<EntityId>{1, 2, 3, 101}), "[ks-restore-entities] " << r.alive.size());
    REQUIRE_MESSAGE(r.posStamps.size() == 4, "[ks-restore-read-column] " << r.posStamps.size());
    for (const auto& [entity, added, changed] : r.posStamps) {
        const auto stamps = rig->world().Stamps<Pos>(entity);
        REQUIRE(stamps.has_value());
        CHECK_MESSAGE((added == stamps->first && changed == stamps->second), "[ks-restore-read-column] stamps of " << entity);
    }
    CHECK_MESSAGE(std::get<0>(r.posStamps.front()) == 1, "[ks-restore-read-column] ascending EntityId");
    CHECK_MESSAGE((r.out.has_value() && r.out->value == 1), "[ks-restore-read-column] Out");
    CHECK_MESSAGE(r.outStamp.has_value(), "[ks-restore-resource-stamp] present");
    CHECK_MESSAGE(r.stamp > 0, "[ks-restore-stamp] " << r.stamp);
}

TEST_CASE("restore: Restore puts the recorded state back as one immediate change") {
    auto rig = RestoreRig();
    World& world = rig->world();
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const auto untouchedBefore = world.Stamps<Pos>(1);
    Phase() = 2;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE((!world.Alive(101) && world.Get<Pos>(2)->x == 99 && world.Get<Tag>(3) == nullptr && world.Resource<Out>()->value == 7));
    Log().clear();
    Phase() = 3;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const Recorded& r = Record();
    // Primitive 3: back alive with its id, though it is in the allotter's range; no RejectedSpawn.
    CHECK_MESSAGE(world.Alive(101), "[ks-restore-revive] 101 is not alive");
    CHECK_MESSAGE(world.RejectedSpawns() == 0, "[ks-restore-revive] " << world.RejectedSpawns() << " rejected spawns");
    CHECK_MESSAGE((world.Get<Pos>(101) != nullptr && world.Get<Pos>(101)->x == 5), "[ks-restore-revive] Pos of 101");
    CHECK_MESSAGE((world.Get<Tag>(101) != nullptr && world.Get<Tag>(101)->t == 50), "[ks-restore-revive] Tag of 101");
    // Primitive 2: what changed is back, with the one stamp of the restore.
    CHECK_MESSAGE(world.Get<Pos>(2)->x == 20, "[ks-restore-value] Pos of 2: " << world.Get<Pos>(2)->x);
    CHECK_MESSAGE(world.Stamps<Pos>(2)->second == r.restoreStamp, "[ks-restore-one-stamp] Pos of 2");
    CHECK_MESSAGE(world.Stamps<Pos>(101)->second == r.restoreStamp, "[ks-restore-one-stamp] Pos of 101");
    CHECK_MESSAGE((world.Get<Tag>(3) != nullptr && world.Get<Tag>(3)->t == 31), "[ks-restore-value] Tag of 3");
    CHECK_MESSAGE(world.Resource<Out>()->value == 1, "[ks-restore-value] Out: " << world.Resource<Out>()->value);
    // What did not change is not written: its stamps are the ones from before.
    CHECK_MESSAGE(world.Stamps<Pos>(1) == untouchedBefore, "[ks-restore-unchanged-kept] Pos of 1 was written");
    // OnAdd once every cell is back: the hook of Tag sees the entity's Pos.
    CHECK_MESSAGE(Join(Log()) == "tag-added:3 pos=30 tag-added:101 pos=5", "[ks-restore-hooks-after] " << Join(Log()));
}

TEST_CASE("restore: a cell the World has and the recorded state does not is removed; an entity not recorded is despawned") {
    auto rig = RestoreRig();
    World& world = rig->world();
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // After the record: 4 gets a Pos and 2 a Tag, from the Host.
    world.Insert(4, Pos{40});
    world.Insert(2, Tag{21});
    Phase() = 3;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(!world.Alive(4), "[ks-restore-despawn] 4 is alive");
    CHECK_MESSAGE(world.Get<Tag>(2) == nullptr, "[ks-restore-remove] Tag of 2 is still there");
    CHECK_MESSAGE((world.Alive(2) && world.Get<Pos>(2)->x == 20), "[ks-restore-remove] 2 lost more than its Tag");
}

TEST_CASE("restore: a resource: equal is not written, a recorded absence removes it, ResourceStamp follows") {
    auto rig = RestoreRig();
    World& world = rig->world();
    Phase() = 0;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::optional<ChangeStamp> first = Record().outStamp;
    REQUIRE(first.has_value());
    Phase() = 3;  // nothing changed: Out is equal
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Record().outStamp == first, "[ks-restore-resource-equal-kept] Out was written");
    Phase() = 2;  // SetResource(Out{7})
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE((Record().outStamp.has_value() && *Record().outStamp > *first), "[ks-restore-resource-stamp-moves]");
    Record().out.reset();
    Phase() = 3;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(world.Resource<Out>() == nullptr, "[ks-restore-resource-absent] Out is still there");
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(!Record().outStamp.has_value(), "[ks-restore-resource-stamp-absent]");
}

TEST_CASE("restore: without Alive the alive set stays; Alive and the cells may come in any order and twice") {
    {
        auto rig = RestoreRig();
        World& world = rig->world();
        Phase() = 1;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        world.Insert(4, Tag{40});
        Phase() = 5;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE((world.Alive(1) && world.Alive(4)), "[ks-restore-no-alive-keeps]");
    }
    auto rig = RestoreRig();
    World& world = rig->world();
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 2;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Recorded& r = Record();
    std::reverse(r.alive.begin(), r.alive.end());
    r.alive.push_back(r.alive.front());
    std::reverse(r.pos.begin(), r.pos.end());
    std::reverse(r.tag.begin(), r.tag.end());
    Phase() = 3;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE((world.Alive(101) && world.Alive(1) && world.Alive(2) && world.Alive(3)), "[ks-restore-any-order] alive");
    CHECK_MESSAGE((world.Get<Pos>(2) != nullptr && world.Get<Pos>(2)->x == 20), "[ks-restore-any-order] Pos of 2");
    CHECK_MESSAGE((world.Get<Tag>(3) != nullptr && world.Get<Tag>(3)->t == 31), "[ks-restore-any-order] Tag of 3");
}

TEST_CASE("restore: an OnAdd of the first column sees the entity's later column put back too") {
    auto rig = RestoreRig(true);
    Phase() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 2;  // despawns 101
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Log().clear();
    Phase() = 3;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Join(Log()) == "pos-added:101 tag=50", "[ks-restore-hooks-after-any-column] " << Join(Log()));
}

TEST_CASE("restore: Restore puts back what it is given: it derives no InitialValue") {
    auto rig = RestoreRig(false, true);
    World& world = rig->world();
    REQUIRE(world.Get<Tag>(2) == nullptr);  // the Host's insert of Pos of 2 filled nothing
    Phase() = 1;  // 2 is recorded with Pos and no Tag
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    Phase() = 6;  // despawns 2
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(!world.Alive(2));
    Log().clear();
    Phase() = 3;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE((world.Alive(2) && world.Get<Pos>(2) != nullptr));
    CHECK_MESSAGE(world.Get<Tag>(2) == nullptr, "[ks-restore-no-initial-value] the restore filled Tag of 2 from its Pos");
}
