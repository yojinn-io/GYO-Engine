#pragma once

// Test types and helpers of the kernel tests (spec J.1: test types live only in
// tests; the kernel headers name no game type). Fixed-width members only, so a
// type's size is the same on every L1 platform and the fingerprint goldens
// compare across platforms.

#include "engine/kernel/Kernel.hpp"

#include "TestRangeAllotter.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GYO::Test::Kernel {

using namespace GYO::Kernel;
using GYO::Test::TestRangeAllotter;

struct TestPos { std::int32_t x, y, z; };
struct TestVel { std::int32_t x, y, z; };
struct TestMarker { std::uint8_t on; };
struct TestHealth { std::int32_t points; };
struct TestMode { std::uint32_t mode; };
struct TestDerived { std::int32_t value; };
struct TestEvent { std::uint32_t id; };
struct TestOtherEvent { std::uint32_t id; };
struct TestFrameEvent { std::uint32_t id; };
struct TestRes { std::int64_t value; };
struct TestOtherRes { std::int64_t value; };
// Satisfies EntityAllotter with one 8-byte member, so its registered shape (and
// the Registry fingerprint goldens of registry-schedule) stays as it was.
struct TestAllotter {
    std::uint64_t next;
    std::optional<EntityId> Allot() { return next++; }
    bool Owns(EntityId) const { return false; }
};
struct TestRecord { std::uint32_t code; };
struct ReplicatedSet {};
struct DigestTag {};

inline SystemResult Ok() { return {}; }

// The kernel's two registration steps over functions, in list order: one
// Register per (owner name, build), then BuildPlans. Registration units and
// their lifecycle are the orchestration layer's (v8-arch-gyo2-orchestration-skeleton).
inline GYO::Engine::Base::Result<void, StartupReport> RegisterAll(
    Registry& registry, std::vector<std::pair<std::string, std::function<void(Registrar&)>>> builds) {
    std::uint32_t seq = 0;
    for (auto& [name, build] : builds) {
        auto registered = registry.Register(RegistrationOwner{name, seq++}, [&](Registrar& r) {
            if (build) build(r);
        });
        GYO_ASSERT(static_cast<bool>(registered));
    }
    return registry.BuildPlans();
}

// Schedules most tests use: a frame schedule (Update) and a fixed-step schedule (FixedUpdate).
inline ScheduleOptions UpdateSchedule() {
    ScheduleOptions s;
    s.name = "Update";
    s.stages = {"First", "Second"};
    return s;
}

inline ScheduleOptions FixedSchedule() {
    ScheduleOptions s;
    s.name = "FixedUpdate";
    s.stages = {"Ingress", "Simulate", "Present"};
    return s;
}

inline Mount At(std::string schedule, std::string stage, std::vector<std::string> before = {},
                std::vector<std::string> after = {}) {
    Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    m.before = std::move(before);
    m.after = std::move(after);
    return m;
}

inline SystemOptions Sys(std::string name, std::vector<Mount> mounts) {
    SystemOptions o;
    o.name = std::move(name);
    o.mounts = std::move(mounts);
    return o;
}

// Registers the common data types.
inline void RegisterCommonData(Registrar& r) {
    r.Component<TestPos>({.name = "TestPos"});
    r.Component<TestVel>({.name = "TestVel"});
    r.Component<TestMarker>({.name = "TestMarker"});
    r.Component<TestHealth>({.name = "TestHealth"});
    r.Component<TestMode>({.name = "TestMode"});
    r.Message<TestEvent>({.name = "TestEvent"});
    r.Message<TestFrameEvent>({.name = "TestFrameEvent"});
    r.Resource<TestRes>({.name = "TestRes"});
    r.Resource<TestOtherRes>({.name = "TestOtherRes"});
}

// Registers functions in list order and builds the Plans (RegisterAll).
struct Built {
    std::unique_ptr<Registry> registry;
    GYO::Engine::Base::Result<void, StartupReport> result;
};

inline Built Finish(std::vector<std::pair<std::string, std::function<void(Registrar&)>>> builds) {
    Built built{std::make_unique<Registry>(), {}};
    built.result = RegisterAll(*built.registry, std::move(builds));
    return built;
}

// One plugin with the common data, the two schedules and the given extra registrations.
inline Built FinishOne(std::function<void(Registrar&)> extra) {
    return Finish({{"test", [extra = std::move(extra)](Registrar& r) {
                        RegisterCommonData(r);
                        r.Schedule(UpdateSchedule());
                        r.Schedule(FixedSchedule());
                        extra(r);
                    }}});
}

inline std::vector<std::string> Codes(const Built& built) {
    std::vector<std::string> codes;
    if (built.result) return codes;
    for (const StartupError& error : built.result.error().errors) codes.emplace_back(ToString(error.code));
    return codes;
}

inline std::string Describe(const Built& built) {
    return built.result ? std::string("ok") : built.result.error().detail;
}

inline bool HasRule(const Built& built, StartupCode code, std::string_view rule) {
    if (built.result) return false;
    const auto& errors = built.result.error().errors;
    return std::any_of(errors.begin(), errors.end(),
                       [&](const StartupError& e) { return e.Is(code) && e.rule == rule; });
}

// Safe on a successful Finish (0 / false), so a mutant that removes a check is a failed CHECK naming the code,
// not a crash in error() (AGENTS 14.2: the expected failure must be identifiable).
inline std::size_t ErrorCount(const Built& built, StartupCode code) {
    return built.result ? 0 : built.result.error().Count(code);
}

inline bool HasError(const Built& built, StartupCode code) {
    return ErrorCount(built, code) > 0;
}

inline std::size_t ErrorCount(const GYO::Engine::Base::Result<void, StartupReport>& result, StartupCode code) {
    return result ? 0 : result.error().Count(code);
}

// The ties of a finished Registry, each as "schedule: first > second", in the order
// FrozenRegistry::TieBreaks lists them (empty after a failed Finish).
inline std::vector<std::string> Ties(const Built& built) {
    std::vector<std::string> out;
    if (!built.result) return out;
    for (const TieBreak& tie : built.registry->Frozen().TieBreaks()) {
        out.push_back(tie.schedule + ": " + tie.first + " > " + tie.second);
    }
    return out;
}

inline std::vector<std::string> SystemSteps(const Plan& plan) {
    std::vector<std::string> names;
    for (const Step& step : plan.steps) {
        if (step.kind == StepKind::System) names.push_back(step.name);
    }
    return names;
}

// The World summary of a World every value type of which has a hash function:
// fails the test case with the error when Summary returns one.
inline Fingerprint SummaryOf(const World& world) {
    auto summary = world.Summary();
    REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().Text()));
    return *summary;
}

inline std::size_t PositionOf(const Plan& plan, std::string_view name) {
    for (std::size_t i = 0; i < plan.steps.size(); ++i) {
        if (plan.steps[i].name == name) return i;
    }
    return plan.steps.size();
}

} // namespace GYO::Test::Kernel

#define KERNEL_REQUIRE_OK(built) REQUIRE_MESSAGE(static_cast<bool>((built).result), ::GYO::Test::Kernel::Describe(built))
