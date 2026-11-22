// Res::Changed and ResMut::Changed (target/kernel DI-KS 28;
// v8-arch-gyo2-kernel-res-changed): a resource is changed for a System mount
// when it is present and was written after the mount's last run, the rule of
// the component filter Changed<T> (spec B-12). A write counts whether or not it
// changed the value; putting a value in is a write. Each CHECK carries a tag in
// brackets ("[rc-...]"): the batch's mutants expect it.

#include "WorldTestSupport.hpp"

#include <string>
#include <vector>

namespace {

using namespace GYO::Test::Kernel;

std::string Join(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) out += (out.empty() ? "" : " ") + line;
    return out;
}

std::string Bit(bool b) { return b ? "1" : "0"; }

// What the writers below do, chosen by the test between Runs (Flag would be a
// resource of its own; a static keeps the readers' resource the only one written).
int& Act() {
    static int act = 0;
    return act;
}

// Out, Flag and CRes; "Tick" with stages A, B, C and a sync point after A;
// "Other" with stage B; "Inner" with stage A (run nested).
void Setup(Registrar& r) {
    r.Resource<Out>({.name = "Out"});
    r.Resource<Flag>({.name = "Flag"});
    r.Resource<CRes>({.name = "CRes"});
    ScheduleOptions tick;
    tick.name = "Tick";
    tick.stages = {"A", "B", "C"};
    tick.syncPoints = {SyncPointOptions{"AfterA", "A"}};
    r.Schedule(tick);
    ScheduleOptions other;
    other.name = "Other";
    other.stages = {"B"};
    r.Schedule(other);
    ScheduleOptions inner;
    inner.name = "Inner";
    inner.stages = {"A"};
    r.Schedule(inner);
}

SystemOptions In(std::string name, std::string schedule, std::string stage) {
    return Sys(std::move(name), {At(std::move(schedule), std::move(stage))});
}

SystemResult SeeOut(Res<Out> out) {
    Log().push_back("see:" + Bit(out.Changed()));
    return Ok();
}

// Act 1: writes Out through Get() with the value it already has; 2: only Read().
SystemResult WriteOut(ResMut<Out> out) {
    if (!out.Present()) return Ok();
    if (Act() == 1) out.Get().value = out.Read().value;
    if (Act() == 2) static_cast<void>(out.Read());
    return Ok();
}

// Changed before and after its own write.
SystemResult WriteAndSee(ResMut<Out> out) {
    const bool before = out.Changed();
    out.Get().value += 1;
    Log().push_back("own:" + Bit(before) + Bit(out.Changed()));
    return Ok();
}

// Act 1: deferred SetResource; 2: deferred RemoveResource.
SystemResult CommandOut(Commands commands) {
    if (Act() == 1) commands.SetResource(Out{5});
    if (Act() == 2) commands.RemoveResource<Out>();
    return Ok();
}

// Act 1: Modify with no change of value; 2: SetResource now.
SystemResult ExclusiveOut(ExclusiveWorld<> world) {
    if (Act() == 1) {
        auto modified = world.Modify<Out>([](Out&) {});
        if (!modified) return GYO::Engine::Base::Err(std::move(modified).error());
    }
    if (Act() == 2) {
        auto set = world.SetResource(Out{9});
        if (!set) return GYO::Engine::Base::Err(std::move(set).error());
    }
    return Ok();
}

// Act 1: Restore Out to 7; reads equal values as equal.
bool SameOut(const Out& a, const Out& b) { return a.value == b.value; }
SystemResult RestoreOut(ExclusiveWorld<> world) {
    if (Act() != 1) return Ok();
    RestoreSet set;
    set.Resource<Out>(Out{7}, SameOut);
    return world.Restore(std::move(set));
}

// Act 1: writes Out, then runs Inner; otherwise runs Inner only.
SystemResult DriveInner(ExclusiveWorld<> world) {
    if (Act() == 1) {
        auto modified = world.Modify<Out>([](Out& o) { o.value += 1; });
        if (!modified) return GYO::Engine::Base::Err(std::move(modified).error());
    }
    return world.RunSchedule("Inner");
}

SystemResult SeeSettings(Res<KernelSettings> settings) {
    Log().push_back("settings:" + Bit(settings.Changed()));
    return Ok();
}

bool OutChanged(Res<Out> out) { return out.Changed(); }
bool FlagOn(Res<Flag> flag) { return flag.Present() && flag->on != 0; }
SystemResult Ran() {
    Log().push_back("ran");
    return Ok();
}

} // namespace

TEST_CASE("DI-KS 28: a mount's first run sees a resource put in; a run with no write does not") {
    auto rig = MakeRig([](Registrar& r) {
        Setup(r);
        r.AddSystem<&SeeOut>(In("SeeOut", "Tick", "B"));
    });
    Log().clear();
    rig->world().SetResource(Out{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    rig->world().SetResource(Out{1});  // the Host writes between two Runs, the same value
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Join(Log()) == "see:1 see:0 see:1", "[rc-first-run] " << Join(Log()));
}

TEST_CASE("DI-KS 28: an absent resource is not changed, also right after it was taken out") {
    auto rig = MakeRig([](Registrar& r) {
        Setup(r);
        r.AddSystem<&CommandOut>(In("CommandOut", "Tick", "A"));
        r.AddSystem<&SeeOut>(In("SeeOut", "Tick", "B"));
    });
    Log().clear();
    Act() = 0;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // never put in
    rig->world().SetResource(Out{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // put in
    Act() = 2;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // taken out at the sync point after A: the stamp moves, the value is gone
    Act() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // put back at the sync point after A
    CHECK_MESSAGE(Join(Log()) == "see:0 see:1 see:0 see:1", "[rc-absent] " << Join(Log()));
}

TEST_CASE("DI-KS 28: a write counts whatever the value; Read() does not; seen by the next reader once") {
    auto rig = MakeRig([](Registrar& r) {
        Setup(r);
        r.AddSystem<&WriteOut>(In("WriteOut", "Tick", "A"));
        r.AddSystem<&SeeOut>(In("SeeOut", "Tick", "B"));
    });
    rig->world().SetResource(Out{3});
    Act() = 0;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // the Host's put-in
    Log().clear();
    Act() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // Get() with the same value
    Act() = 2;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // Read() only
    Act() = 0;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Join(Log()) == "see:1 see:0 see:0", "[rc-write-not-value] " << Join(Log()));
}

TEST_CASE("DI-KS 28: a write after the reader in the same Run is seen at its next run") {
    auto rig = MakeRig([](Registrar& r) {
        Setup(r);
        r.AddSystem<&SeeOut>(In("SeeOut", "Tick", "A"));
        r.AddSystem<&WriteOut>(In("WriteOut", "Tick", "C"));
    });
    rig->world().SetResource(Out{3});
    Log().clear();
    Act() = 1;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // sees the put-in; the write comes after it
    Act() = 0;
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // sees the last Run's write
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Join(Log()) == "see:1 see:1 see:0", "[rc-write-after] " << Join(Log()));
}

TEST_CASE("DI-KS 28: ResMut sees its own write in the call, not at its next run") {
    auto rig = MakeRig([](Registrar& r) {
        Setup(r);
        r.AddSystem<&WriteAndSee>(In("WriteAndSee", "Tick", "B"));
    });
    rig->world().SetResource(Out{0});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Join(Log()) == "own:11 own:01", "[rc-own-write] " << Join(Log()));
}

TEST_CASE("DI-KS 28: deferred and immediate writes, and a restore, are seen by a later mount") {
    SUBCASE("Commands::SetResource at the sync point") {
        auto rig = MakeRig([](Registrar& r) {
            Setup(r);
            r.AddSystem<&CommandOut>(In("CommandOut", "Tick", "A"));
            r.AddSystem<&SeeOut>(In("SeeOut", "Tick", "B"));
        });
        rig->world().SetResource(Out{0});
        Act() = 0;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Log().clear();
        Act() = 1;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Act() = 0;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()) == "see:1 see:0", "[rc-commands] " << Join(Log()));
    }
    SUBCASE("ExclusiveWorld::Modify (no change of value) and SetResource") {
        auto rig = MakeRig([](Registrar& r) {
            Setup(r);
            r.AddSystem<&ExclusiveOut>(In("ExclusiveOut", "Tick", "A"));
            r.AddSystem<&SeeOut>(In("SeeOut", "Tick", "B"));
        });
        rig->world().SetResource(Out{0});
        Act() = 0;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Log().clear();
        Act() = 1;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Act() = 0;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Act() = 2;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()) == "see:1 see:0 see:1", "[rc-exclusive] " << Join(Log()));
    }
    SUBCASE("Restore writes a different value, not an equal one") {
        auto rig = MakeRig([](Registrar& r) {
            Setup(r);
            r.AddSystem<&RestoreOut>(In("RestoreOut", "Tick", "A"));
            r.AddSystem<&SeeOut>(In("SeeOut", "Tick", "B"));
        });
        rig->world().SetResource(Out{1});
        Act() = 0;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Log().clear();
        Act() = 1;
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 1 -> 7: written
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 7 -> 7: not written
        CHECK_MESSAGE(Join(Log()) == "see:1 see:0", "[rc-restore] " << Join(Log()));
    }
}

TEST_CASE("DI-KS 28: per mount: a skipped mount keeps its last run; two mounts and a nested run keep their own") {
    SUBCASE("a mount skipped by a run condition sees a write made before the skipped run") {
        auto rig = MakeRig([](Registrar& r) {
            Setup(r);
            SystemOptions options = In("SeeOut", "Tick", "B");
            options.mounts[0].conditions.push_back(Condition<&FlagOn>("FlagOn"));
            r.AddSystem<&SeeOut>(std::move(options));
        });
        rig->world().SetResource(Flag{1});
        rig->world().SetResource(Out{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Log().clear();
        rig->world().SetResource(Out{1});
        rig->world().SetResource(Flag{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // skipped
        rig->world().SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()) == "see:1", "[rc-skipped] " << Join(Log()));
    }
    SUBCASE("one System mounted twice") {
        auto rig = MakeRig([](Registrar& r) {
            Setup(r);
            r.AddSystem<&SeeOut>(Sys("SeeOut", {At("Tick", "B"), At("Other", "B")}));
        });
        rig->world().SetResource(Out{0});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
        KERNEL_REQUIRE_RUN_OK(rig->Run("Other"));  // its own first run
        CHECK_MESSAGE(Join(Log()) == "see:1 see:0 see:1", "[rc-two-mounts] " << Join(Log()));
    }
    SUBCASE("a nested schedule's mount") {
        auto rig = MakeRig([](Registrar& r) {
            Setup(r);
            r.AddSystem<&DriveInner>(In("DriveInner", "Tick", "A"));
            r.AddSystem<&SeeOut>(In("SeeOut", "Inner", "A"));
        });
        rig->world().SetResource(Out{0});
        Act() = 0;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Act() = 1;
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()) == "see:0 see:1", "[rc-nested] " << Join(Log()));
    }
}

TEST_CASE("DI-KS 28: in a run condition, Changed compares with the mount's last run") {
    // The System runs when Out changed and Flag is on. A write while Flag is off
    // is kept until the System runs (Bevy's condition has a last run of its own
    // and would have missed it).
    auto rig = MakeRig([](Registrar& r) {
        Setup(r);
        SystemOptions options = In("Ran", "Tick", "B");
        options.mounts[0].conditions.push_back(Condition<&OutChanged>("OutChanged"));
        options.mounts[0].conditions.push_back(Condition<&FlagOn>("FlagOn"));
        r.AddSystem<&Ran>(std::move(options));
    });
    rig->world().SetResource(Flag{1});
    rig->world().SetResource(Out{0});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // runs
    Log().push_back("|");
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // no write: does not run
    Log().push_back("|");
    rig->world().SetResource(Out{1});
    rig->world().SetResource(Flag{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // Out changed, Flag off: skipped
    Log().push_back("|");
    rig->world().SetResource(Flag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // the write is still a change for the mount: runs
    CHECK_MESSAGE(Join(Log()) == "ran | | | ran", "[rc-condition] " << Join(Log()));
}

TEST_CASE("DI-KS 28: KernelSettings, put in when the World is made, is changed at a mount's first run") {
    // No Rig: MakeRig writes KernelSettings itself.
    Built built = Finish({{"test", [](Registrar& r) {
                               Setup(r);
                               r.AddSystem<&SeeSettings>(In("SeeSettings", "Tick", "B"));
                           }}});
    REQUIRE_MESSAGE(static_cast<bool>(built.result), Describe(built));
    WorldExecutor executor(*built.registry, "settings-world");
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    CHECK_MESSAGE(Join(Log()) == "settings:1 settings:0", "[rc-settings] " << Join(Log()));
}
