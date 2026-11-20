#pragma once

// Test types and a rig for the world-exec batch's tests (World, Serial
// Executor, access verification). Fixed-width members only and no padding, so
// World summaries compare across the L1 platforms (spec J-32 golden).
//
// Systems are captureless (DI-KS 1), so what a test System observes goes into
// the test-side logs below (test code, not kernel code: spec J.1).

#include "KernelTestSupport.hpp"

#include "AssertTestSupport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace WorldTest {

using namespace KernelTest;

struct Pos { std::int32_t x; };    // zone C, Exclusive, rollback Yes
struct Vel { std::int32_t v; };    // zone C, Exclusive, rollback Yes
struct Tag { std::int32_t t; };    // zone B, Shared
struct Mode { std::int32_t m; };   // zone B, Exclusive
struct Boundary { std::int32_t b; };  // zone C, role Input (a boundary column)
struct CShared { std::int32_t s; };   // zone C, Shared (the zone half of F-10 and S-13)
struct Out { std::int64_t value; };   // resource, zone B, Shared
struct Flag { std::int32_t on; };     // resource, zone B, Exclusive
struct CRes { std::int64_t value; };  // resource, zone C, rollback Yes
struct Ping { std::uint32_t n; };     // message, zone B
struct Note { std::uint32_t code; };  // diagnostics record
// The Runs of Tick a test counted, written from outside between two Runs (World
// Ready, SetResource; Rig::Run does it): the "on the N-th Run" gate and log of a
// test System. It replaces the kernel's domain tick (the tick handle, World::Tick),
// which left the kernel with the clock (v8-arch-gyo2-clock-out-of-kernel;
// time-events declaration 6.2). Zone C, role Input (a boundary resource): zone B
// and zone C Systems both read it. No System writes it, so it adds no Plan step
// and takes no stamp.
struct TickCount { std::uint64_t value; };

// The log of every test System, hook and condition that records something.
inline std::vector<std::string>& Log() {
    static std::vector<std::string> log;
    return log;
}

inline ScheduleOptions TickSchedule() {
    ScheduleOptions s;
    s.name = "Tick";
    s.stages = {"A", "B", "C"};
    return s;
}

// Registers the data above and the Tick schedule; allotter: optional.
inline void Data(Registrar& r, bool allotter = false) {
    r.Component<Pos>({.name = "Pos", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<Vel>({.name = "Vel", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<Tag>({.name = "Tag", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Mode>({.name = "Mode", .zone = Zone::B});
    r.Component<Boundary>({.name = "Boundary", .zone = Zone::C, .role = Role::Input});
    r.Component<CShared>({.name = "CShared", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
    r.Resource<Out>({.name = "Out", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Resource<Flag>({.name = "Flag", .zone = Zone::B});
    r.Resource<CRes>({.name = "CRes", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Message<Ping>({.name = "Ping", .zone = Zone::B});
    if (allotter) {
        r.Resource<RangeAllotter>({.name = "Allotter", .zone = Zone::B});
        r.Allotter<RangeAllotter>();
    }
    r.Resource<TickCount>({.name = "TickCount", .zone = Zone::C, .role = Role::Input});
    r.Schedule(TickSchedule());
}

inline SystemOptions B(std::string name, std::string stage, std::vector<std::string> before = {},
                       std::vector<std::string> after = {}) {
    return Sys(std::move(name), Zone::B, {At("Tick", std::move(stage), std::move(before), std::move(after))});
}

inline SystemOptions C(std::string name, std::string stage, std::vector<std::string> before = {},
                       std::vector<std::string> after = {}) {
    return Sys(std::move(name), Zone::C, {At("Tick", std::move(stage), std::move(before), std::move(after))});
}

// A finished Registry, a WorldExecutor over it and the errors its outermost Runs
// and Triggers returned. The kernel has no error handler (error levels D1): an Err
// is returned once by the outermost call, so recorded holds one entry per Err
// returned through Run and Trigger here.
struct Rig {
    Built built;
    std::unique_ptr<WorldExecutor> executor;
    std::vector<KernelError> recorded;

    // The Runs of Tick so far (TickCount).
    std::uint64_t ticks = 0;

    World& world() { return executor->GetWorld(); }
    // A Run of Tick first writes TickCount (when the World has it and is Ready).
    Engine::Base::Result<void, KernelError> Run(std::string_view schedule = "Tick") {
        if (schedule == "Tick") {
            ++ticks;
            if (world().State() == WorldState::Ready && built.registry->Frozen().ResourceIdOf<TickCount>()) {
                world().SetResource(TickCount{ticks});
            }
        }
        auto ran = executor->Run(schedule);
        if (!ran) recorded.push_back(ran.error());
        return ran;
    }
    // A Trigger from outside the World, recorded like Run.
    template <class E>
    Engine::Base::Result<void, KernelError> Trigger(E event, std::optional<EntityId> target = std::nullopt) {
        auto triggered = executor->Trigger(std::move(event), target);
        if (!triggered) recorded.push_back(triggered.error());
        return triggered;
    }
};

// verification: the KernelSettings switch written in the Ready window before the first Run.
inline std::unique_ptr<Rig> MakeRig(std::function<void(Registrar&)> build, bool verification = true,
                                    std::string worldName = "test-world") {
    auto rig = std::make_unique<Rig>();
    rig->built = Finish({{"test", std::move(build)}});
    if (!rig->built.result) {
        FAIL("Finish failed: " << Describe(rig->built));
        return rig;
    }
    rig->executor = std::make_unique<WorldExecutor>(*rig->built.registry, std::move(worldName));
    rig->world().SetResource(KernelSettings{verification, false});
    return rig;
}

inline std::string Text(const Engine::Base::Result<void, KernelError>& result) {
    return result ? std::string("ok") : result.error().Text();
}

inline bool HasCode(const Engine::Base::Result<void, KernelError>& result, KernelErrorCode code) {
    return !result && result.error().Has(code);
}

// A test-side World pointer for the bypass cases (spec J-5 (c): a World* kept in a global).
inline World*& KeptWorld() {
    static World* world = nullptr;
    return world;
}

} // namespace WorldTest

#define KERNEL_REQUIRE_RUN_OK(expr)                                  \
    do {                                                             \
        const auto kernelRunResult = (expr);                         \
        REQUIRE_MESSAGE(static_cast<bool>(kernelRunResult), WorldTest::Text(kernelRunResult)); \
    } while (false)
