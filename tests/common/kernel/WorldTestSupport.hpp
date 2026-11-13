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
struct Ping { std::uint32_t n; };     // event, zone B, Fixed domain
struct Note { std::uint32_t code; };  // diagnostics record

// The log of every test System, hook and condition that records something.
inline std::vector<std::string>& Log() {
    static std::vector<std::string> log;
    return log;
}

inline ScheduleOptions TickSchedule() {
    ScheduleOptions s;
    s.name = "Tick";
    s.domain = Domain::Fixed;
    s.clock = true;
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
    r.Event<Ping>({.name = "Ping", .zone = Zone::B, .domain = Domain::Fixed});
    if (allotter) {
        r.Resource<RangeAllotter>({.name = "Allotter", .zone = Zone::B});
        r.Allotter<RangeAllotter>();
    }
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

// A finished Registry, an App over it and the errors its handler recorded.
struct Rig {
    Built built;
    std::unique_ptr<App> app;
    std::vector<KernelError> recorded;

    World& world() { return app->GetWorld(); }
    Engine::Base::Result<void, KernelError> Run(std::string_view schedule = "Tick") { return app->Run(schedule); }
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
    rig->app = std::make_unique<App>(*rig->built.registry, std::move(worldName));
    Rig* raw = rig.get();
    rig->app->SetErrorHandler([raw](const KernelError& error) { raw->recorded.push_back(error); });
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
