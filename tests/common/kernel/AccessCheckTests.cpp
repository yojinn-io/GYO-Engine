// Access verification (spec A.6, two layers, and the handle epoch): spec J-4
// and J-5. Moved from spikes/kernel/tests/AccessCheckTests.cpp (judgement 2 of
// v8-arch-kernel-spike) and changed as the world-exec declaration (section 2)
// says: a violation is an Err returned through the Executor, not an assertion;
// the switch is the run-time KernelSettings value, so one executable runs two
// Worlds, one with verification on and one with it off (LIM-6).
//
// The three bypasses of the -spike README's judgement (2), as written there (spec J-5):
//   (a) keep a reference from Read<T> and write through it in another System;
//   (b) write a read-only column through a raw pointer;
//   (c) read the World through an id the System did not declare (a World* in a global).
// "Limit:" cases fix shapes verification does not see (spike-report L1 to L8, LIM-3).

#include "WorldTestSupport.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

const Pos* g_keptPos = nullptr;
const Res<Out>* g_keptHandle = nullptr;

// doctest's own check (doctest::detail::isDebuggerActive): IsDebuggerPresent() on Windows (ci-readiness section 3.2), the
// platform's equivalent elsewhere. No platform header here (engine/os boundary guard).
std::string DebuggerPresent() {
    return doctest::detail::isDebuggerActive() ? "yes" : "no";
}

void Populate(World& world) {
    world.Insert(1, Pos{1});
    world.Insert(2, Pos{2});
    world.Insert(1, Vel{10});
    world.Insert(2, Vel{20});
    world.Insert(1, Tag{0});
    world.SetResource(Out{0});
    world.SetResource(Flag{0});
}

std::unique_ptr<Rig> RigWith(void (*add)(Registrar&), bool verification, const char* name) {
    auto rig = MakeRig([add](Registrar& r) {
        Data(r);
        add(r);
    }, verification, name);
    Populate(rig->world());
    return rig;
}

// The two-World pattern of the moved test: the same plugin list, verification on
// in one World and off in the other, in one executable.
struct Pair {
    std::unique_ptr<Rig> on;
    std::unique_ptr<Rig> off;
};

Pair Both(void (*add)(Registrar&)) {
    return Pair{RigWith(add, true, "verification-on"), RigWith(add, false, "verification-off")};
}

// ---- Control: declared access never trips verification

SystemResult HealAll(Query<Write<Tag>> tags, ResMut<Out> out) {
    for (EntityId id : tags.Entities()) tags.GetMut<Tag>(id)->t += 1;
    out->value += 1;
    return Ok();
}
SystemResult ReadThroughDeclaredWrite(Query<Write<Pos>>) {
    // A declared write also allows reading the same storage through the World API.
    static_cast<void>(KeptWorld()->Get<Pos>(1));
    return Ok();
}

// ---- (a)

SystemResult KeepPos(Query<Read<Pos>> pos) {
    g_keptPos = pos.Get<Pos>(1);
    return Ok();
}
SystemResult TamperKeptPos(Query<Read<Vel>>) {
    const_cast<Pos*>(g_keptPos)->x = 99;
    return Ok();
}

// ---- (b)

SystemResult RawPointerWrite(Query<Read<Pos>> pos) {
    const Pos* p = pos.Get<Pos>(2);
    *reinterpret_cast<std::int32_t*>(const_cast<Pos*>(p)) = 77;
    return Ok();
}

// ---- (c)

SystemResult UndeclaredRead(Query<Read<Vel>>) {
    static_cast<void>(KeptWorld()->Get<Pos>(1));
    return Ok();
}
SystemResult ReadDeclaredButWriteThroughWorld(Query<Read<Pos>>) {
    if (Pos* p = KeptWorld()->GetMut<Pos>(1)) p->x = p->x;  // same value: only layer 1 can see it
    return Ok();
}

// ---- J-4

SystemResult UseKeptHandle(Query<Read<Tag>>) {
    if (g_keptHandle != nullptr) static_cast<void>(g_keptHandle->Present());
    return Ok();
}
SystemResult KeepPosAcrossRuns(Query<Read<Pos>> pos, CurrentTick tick) {
    if (tick.Tick() == 1) {
        g_keptPos = pos.Get<Pos>(1);
    } else if (g_keptPos != nullptr) {
        const_cast<Pos*>(g_keptPos)->x = 55;  // a reference kept to the next run, written
    }
    return Ok();
}
SystemResult ReadKeptAcrossRuns(Query<Read<Vel>>, CurrentTick tick) {
    if (tick.Tick() == 1) return Ok();
    Log().push_back("kept read " + std::to_string(g_keptPos != nullptr ? g_keptPos->x : -1));
    return Ok();
}
SystemResult WriteBackSameValue(Query<Read<Vel>>) {
    // Limit L1: written and written back before returning.
    Pos* p = const_cast<Pos*>(g_keptPos);
    const std::int32_t old = p->x;
    p->x = 1234;
    p->x = old;
    return Ok();
}

} // namespace

TEST_CASE("J-5: control: declared reads and writes never trip verification") {
    MESSAGE("debugger attached (doctest::detail::isDebuggerActive, IsDebuggerPresent on Windows): " << DebuggerPresent());
    auto rig = RigWith([](Registrar& r) {
        r.AddSystem<&HealAll>(B("HealAll", "A"));
        r.AddSystem<&ReadThroughDeclaredWrite>(C("ReadThroughDeclaredWrite", "B"));
    }, true, "control");
    KeptWorld() = &rig->world();
    for (int i = 0; i < 2; ++i) KERNEL_REQUIRE_RUN_OK(rig->Run());
    // The "no violation" report carries the switch (target/system-contract 2b condition 2).
    const VerificationReport report = rig->app->Verification();
    CHECK(report.accessVerification);
    CHECK_FALSE(report.breakOnKernelError);
    CHECK(report.world == "control");
    CHECK(report.excludedColumns.empty());
    MESSAGE("no violation; verification on: " << report.accessVerification << ", world " << report.world);
    // Host writes through the World API between runs are not reported.
    CHECK(rig->world().Get<Tag>(1)->t == 2);
    rig->world().GetMut<Tag>(1)->t = 50;
    rig->world().Insert(3, Tag{3});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().Get<Tag>(1)->t == 51);
    KeptWorld() = nullptr;
}

TEST_CASE("J-5: (a) a reference kept from Read<T> and written in another System: caught at that System's boundary") {
    MESSAGE("debugger attached (doctest::detail::isDebuggerActive, IsDebuggerPresent on Windows): " << DebuggerPresent());
    Pair pair = Both([](Registrar& r) {
        r.AddSystem<&KeepPos>(B("KeepPos", "A"));
        r.AddSystem<&TamperKeptPos>(B("TamperKeptPos", "B"));
    });
    const auto on = pair.on->Run();
    CHECK(HasCode(on, KernelErrorCode::BoundaryChange));
    REQUIRE_FALSE(static_cast<bool>(on));
    CHECK(on.error().Text().find("TamperKeptPos") != std::string::npos);
    CHECK(on.error().world == "verification-on");
    CHECK(pair.on->recorded.size() == 1);
    CHECK(pair.on->world().State() == WorldState::Interrupted);

    KERNEL_REQUIRE_RUN_OK(pair.off->Run());
    CHECK(pair.off->world().Get<Pos>(1)->x == 99);
    CHECK_FALSE(pair.off->app->Verification().accessVerification);
    g_keptPos = nullptr;
}

TEST_CASE("J-5: (b) a raw pointer write to a read-only column: caught at the System's boundary") {
    Pair pair = Both([](Registrar& r) { r.AddSystem<&RawPointerWrite>(B("RawPointerWrite", "A")); });
    const auto on = pair.on->Run();
    CHECK(HasCode(on, KernelErrorCode::BoundaryChange));
    CHECK(pair.on->recorded.size() == 1);
    KERNEL_REQUIRE_RUN_OK(pair.off->Run());
    CHECK(pair.off->world().Get<Pos>(2)->x == 77);
}

TEST_CASE("J-5: (c) the World read through an undeclared id: caught when it happens") {
    Pair pair = Both([](Registrar& r) { r.AddSystem<&UndeclaredRead>(B("UndeclaredRead", "A")); });
    KeptWorld() = &pair.on->world();
    const auto on = pair.on->Run();
    KeptWorld() = &pair.off->world();
    const auto off = pair.off->Run();
    KeptWorld() = nullptr;
    CHECK(HasCode(on, KernelErrorCode::AccessViolation));
    REQUIRE_FALSE(static_cast<bool>(on));
    bool w5 = false;
    for (const KernelError* leaf : on.error().Leaves()) w5 = w5 || (leaf->rule == "W-5" && leaf->world == "verification-on");
    CHECK(w5);
    CHECK_MESSAGE(static_cast<bool>(off), Text(off));
}

TEST_CASE("J-5: a declared read taken as a write through the World API: caught when it happens") {
    Pair pair = Both([](Registrar& r) {
        r.AddSystem<&ReadDeclaredButWriteThroughWorld>(B("ReadDeclaredButWriteThroughWorld", "A"));
    });
    KeptWorld() = &pair.on->world();
    const auto on = pair.on->Run();
    KeptWorld() = &pair.off->world();
    const auto off = pair.off->Run();
    KeptWorld() = nullptr;
    CHECK(HasCode(on, KernelErrorCode::AccessViolation));
    CHECK_MESSAGE(static_cast<bool>(off), Text(off));
}

namespace {
SystemResult TamperExcludedPos(Query<Read<Vel>>) {
    const_cast<Pos*>(g_keptPos)->x = 31;
    return Ok();
}
} // namespace

TEST_CASE("J-5: excluded columns are left out of the comparison and listed in the report") {
    auto rig = RigWith([](Registrar& r) {
        r.AddSystem<&KeepPos>(B("KeepPos", "A"));
        r.AddSystem<&TamperExcludedPos>(B("TamperExcludedPos", "B"));
    }, true, "excluded");
    rig->world().ExcludeFromVerification<Pos>();
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // not seen: Pos is excluded (the report says so)
    g_keptPos = nullptr;
    CHECK(rig->app->Verification().excludedColumns == std::vector<std::string>{"Pos"});
    CHECK(rig->world().Get<Pos>(1)->x == 31);
}

TEST_CASE("J-4: a handle kept past its call is a violation; a kept const T& written in a later run is caught; read only is not") {
    SUBCASE("a handle used in another call") {
        // A handle cannot be copied, moved or kept by a System (the compile-failure
        // sample HANDLE_COPY); a pointer to one dies with the call. So the handle
        // here is made by the test through ParamTraits::Fetch for a call that is
        // over (serial 0), and stays alive while a System uses it.
        auto rig = RigWith([](Registrar& r) { r.AddSystem<&UseKeptHandle>(B("UseKeptHandle", "A")); }, true, "handles");
        Detail::SystemContext over;
        over.world = &rig->world();
        over.callSerial = 0;
        over.syncEpoch = rig->world().SyncEpoch();  // the same window: only the call serial differs
        const Res<Out> kept = ParamTraits<Res<Out>>::Fetch(over);
        g_keptHandle = &kept;
        const auto result = rig->Run();
        g_keptHandle = nullptr;
        CHECK(HasCode(result, KernelErrorCode::StaleHandle));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("UseKeptHandle") != std::string::npos);
    }
    SUBCASE("a reference kept to the next run and written: caught at the boundary") {
        auto rig = RigWith([](Registrar& r) { r.AddSystem<&KeepPosAcrossRuns>(B("KeepPosAcrossRuns", "A")); }, true, "kept");
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        const auto second = rig->Run();
        g_keptPos = nullptr;
        CHECK(HasCode(second, KernelErrorCode::BoundaryChange));
    }
    SUBCASE("Limit: a reference kept to the next run and only read is not caught") {
        auto rig = RigWith([](Registrar& r) {
            r.AddSystem<&KeepPos>(B("KeepPos", "A"));
            r.AddSystem<&ReadKeptAcrossRuns>(B("ReadKeptAcrossRuns", "B"));
        }, true, "limit-read");
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        g_keptPos = nullptr;
        CHECK(Log() == std::vector<std::string>{"kept read 1"});
    }
    SUBCASE("Limit L1: written and written back before returning is not caught") {
        auto rig = RigWith([](Registrar& r) {
            r.AddSystem<&KeepPos>(B("KeepPos", "A"));
            r.AddSystem<&WriteBackSameValue>(B("WriteBackSameValue", "B"));
        }, true, "limit-l1");
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        g_keptPos = nullptr;
    }
}
