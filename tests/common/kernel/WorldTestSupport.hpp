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

namespace GYO::Test::Kernel {

struct Pos { std::int32_t x; };
struct Vel { std::int32_t v; };
struct Tag { std::int32_t t; };
struct Mode { std::int32_t m; };
struct Boundary { std::int32_t b; };  // a column written from outside the Systems
struct CShared { std::int32_t s; };
struct Out { std::int64_t value; };   // resource
struct Flag { std::int32_t on; };     // resource
struct CRes { std::int64_t value; };  // resource
struct Ping { std::uint32_t n; };     // message
struct Note { std::uint32_t code; };  // diagnostics record
// The Runs of Tick a test counted, written from outside between two Runs (World
// Ready, SetResource; Rig::Run does it): the "on the N-th Run" gate and log of a
// test System. It replaces the kernel's domain tick (the tick handle, World::Tick),
// which left the kernel with the clock (v8-arch-gyo2-clock-out-of-kernel;
// time-events declaration 6.2). Systems read it; no System writes it, so it adds
// no Plan step and takes no stamp.
struct TickCount { std::uint64_t value; };

// The hash functions of the types above (World::Summary; Hash.hpp).
inline void HashValue(Hasher& h, const Pos& v) { HashValue(h, v.x); }
inline void HashValue(Hasher& h, const Vel& v) { HashValue(h, v.v); }
inline void HashValue(Hasher& h, const Tag& v) { HashValue(h, v.t); }
inline void HashValue(Hasher& h, const Mode& v) { HashValue(h, v.m); }
inline void HashValue(Hasher& h, const Boundary& v) { HashValue(h, v.b); }
inline void HashValue(Hasher& h, const CShared& v) { HashValue(h, v.s); }
inline void HashValue(Hasher& h, const Out& v) { HashValue(h, v.value); }
inline void HashValue(Hasher& h, const Flag& v) { HashValue(h, v.on); }
inline void HashValue(Hasher& h, const CRes& v) { HashValue(h, v.value); }
inline void HashValue(Hasher& h, const Ping& v) { HashValue(h, v.n); }
inline void HashValue(Hasher& h, const TickCount& v) { HashValue(h, v.value); }

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
    r.Component<Pos>({.name = "Pos"});
    r.Component<Vel>({.name = "Vel"});
    r.Component<Tag>({.name = "Tag"});
    r.Component<Mode>({.name = "Mode"});
    r.Component<Boundary>({.name = "Boundary"});
    r.Component<CShared>({.name = "CShared"});
    r.Resource<Out>({.name = "Out"});
    r.Resource<Flag>({.name = "Flag"});
    r.Resource<CRes>({.name = "CRes"});
    r.Message<Ping>({.name = "Ping"});
    if (allotter) {
        r.Resource<TestRangeAllotter>({.name = "Allotter"});
        r.Allotter<TestRangeAllotter>();
    }
    r.Resource<TickCount>({.name = "TickCount"});
    r.Schedule(TickSchedule());
}

inline SystemOptions B(std::string name, std::string stage, std::vector<std::string> before = {},
                       std::vector<std::string> after = {}) {
    return Sys(std::move(name), {At("Tick", std::move(stage), std::move(before), std::move(after))});
}

inline SystemOptions C(std::string name, std::string stage, std::vector<std::string> before = {},
                       std::vector<std::string> after = {}) {
    return Sys(std::move(name), {At("Tick", std::move(stage), std::move(before), std::move(after))});
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
    GYO::Engine::Base::Result<void, KernelError> Run(std::string_view schedule = "Tick") {
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
    GYO::Engine::Base::Result<void, KernelError> Trigger(E event, std::optional<EntityId> target = std::nullopt) {
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

inline std::string Text(const GYO::Engine::Base::Result<void, KernelError>& result) {
    return result ? std::string("ok") : result.error().Text();
}

inline bool HasCode(const GYO::Engine::Base::Result<void, KernelError>& result, KernelErrorCode code) {
    return !result && result.error().Has(code);
}

// A test-side World pointer for the bypass cases (spec J-5 (c): a World* kept in a global).
inline World*& KeptWorld() {
    static World* world = nullptr;
    return world;
}

} // namespace GYO::Test::Kernel

#define KERNEL_REQUIRE_RUN_OK(expr)                                  \
    do {                                                             \
        const auto kernelRunResult = (expr);                         \
        REQUIRE_MESSAGE(static_cast<bool>(kernelRunResult), ::GYO::Test::Kernel::Text(kernelRunResult)); \
    } while (false)
