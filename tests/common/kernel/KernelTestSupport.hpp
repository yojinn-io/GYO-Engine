#pragma once

// Test types and helpers of the kernel tests (spec J.1: test types live only in
// tests; the kernel headers name no game type). Fixed-width members only, so a
// type's size is the same on every L1 platform and the fingerprint goldens
// compare across platforms.

#include "engine/kernel/Kernel.hpp"

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

namespace KernelTest {

using namespace GYO::Kernel;

struct TestPos { std::int32_t x, y, z; };
struct TestVel { std::int32_t x, y, z; };
struct TestMarker { std::uint8_t on; };
struct TestHealth { std::int32_t points; };
struct TestMode { std::uint32_t mode; };
struct TestInput { std::uint32_t bits; };
struct TestDerived { std::int32_t value; };
struct TestEvent { std::uint32_t id; };
struct TestOtherEvent { std::uint32_t id; };
struct TestFrameEvent { std::uint32_t id; };
struct TestRes { std::int64_t value; };
struct TestOtherRes { std::int64_t value; };
struct TestLink { std::uint32_t handle; };
struct TestFrameTime { std::int64_t ns; };
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

// A plugin whose Build is a function (the kernel tests' only plugin shape).
class TestPlugin final : public Plugin {
public:
    TestPlugin(std::string name, std::function<void(Registrar&)> build, std::vector<std::string> required = {},
               std::function<void(const FrozenRegistry&, FinishContext&)> finish = {})
        : name_(std::move(name)), build_(std::move(build)), requires_(std::move(required)), finish_(std::move(finish)) {}

    [[nodiscard]] std::string_view Name() const override { return name_; }
    [[nodiscard]] std::vector<std::string_view> Requires() const override {
        return std::vector<std::string_view>(requires_.begin(), requires_.end());
    }
    void Build(Registrar& registrar) override {
        ++builds;
        if (build_) build_(registrar);
    }
    void Finish(const FrozenRegistry& registry, FinishContext& context) override {
        ++finishes;
        if (finish_) finish_(registry, context);
    }

    int builds = 0;
    int finishes = 0;

private:
    std::string name_;
    std::function<void(Registrar&)> build_;
    std::vector<std::string> requires_;
    std::function<void(const FrozenRegistry&, FinishContext&)> finish_;
};

// Schedules most tests use: a Frame clock schedule and a Fixed clock schedule
// with an ingress stage and rollback.
inline ScheduleOptions UpdateSchedule() {
    ScheduleOptions s;
    s.name = "Update";
    s.domain = Domain::Frame;
    s.clock = true;
    s.stages = {"First", "Second"};
    return s;
}

inline ScheduleOptions FixedSchedule(bool rollback = false) {
    ScheduleOptions s;
    s.name = "FixedUpdate";
    s.domain = Domain::Fixed;
    s.clock = true;
    s.rollbackEnabled = rollback;
    s.stages = {"Ingress", "Simulate", "Present"};
    s.ingressStage = "Ingress";
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

inline SystemOptions Sys(std::string name, Zone zone, std::vector<Mount> mounts) {
    SystemOptions o;
    o.name = std::move(name);
    o.zone = zone;
    o.mounts = std::move(mounts);
    return o;
}

// Registers the common data types; zone C unless stated.
inline void RegisterCommonData(Registrar& r) {
    r.Component<TestPos>({.name = "TestPos", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<TestVel>({.name = "TestVel", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<TestMarker>({.name = "TestMarker", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<TestHealth>({.name = "TestHealth", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<TestMode>({.name = "TestMode", .zone = Zone::B});
    r.Event<TestEvent>({.name = "TestEvent", .zone = Zone::C, .domain = Domain::Fixed});
    r.Event<TestFrameEvent>({.name = "TestFrameEvent", .zone = Zone::B, .domain = Domain::Frame});
    r.Resource<TestRes>({.name = "TestRes", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Resource<TestOtherRes>({.name = "TestOtherRes", .zone = Zone::B});
}

// Runs Finish over plugins built from functions, in list order.
struct Built {
    std::vector<std::unique_ptr<TestPlugin>> plugins;
    std::unique_ptr<Registry> registry;
    Engine::Base::Result<void, StartupReport> result;
};

inline Built Finish(std::vector<std::pair<std::string, std::function<void(Registrar&)>>> builds) {
    Built built{{}, nullptr, {}};
    std::vector<Plugin*> list;
    for (auto& [name, build] : builds) {
        built.plugins.push_back(std::make_unique<TestPlugin>(name, std::move(build)));
        list.push_back(built.plugins.back().get());
    }
    built.registry = std::make_unique<Registry>(list);
    built.result = built.registry->Finish();
    return built;
}

// One plugin with the common data, the two schedules and the given extra registrations.
inline Built FinishOne(std::function<void(Registrar&)> extra, bool rollback = false) {
    return Finish({{"test", [extra = std::move(extra), rollback](Registrar& r) {
                        RegisterCommonData(r);
                        r.Schedule(UpdateSchedule());
                        r.Schedule(FixedSchedule(rollback));
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

inline std::size_t ErrorCount(const Engine::Base::Result<void, StartupReport>& result, StartupCode code) {
    return result ? 0 : result.error().Count(code);
}

inline std::vector<std::string> SystemSteps(const Plan& plan) {
    std::vector<std::string> names;
    for (const Step& step : plan.steps) {
        if (step.kind == StepKind::System) names.push_back(step.name);
    }
    return names;
}

inline std::size_t PositionOf(const Plan& plan, std::string_view name) {
    for (std::size_t i = 0; i < plan.steps.size(); ++i) {
        if (plan.steps[i].name == name) return i;
    }
    return plan.steps.size();
}

} // namespace KernelTest

#define KERNEL_REQUIRE_OK(built) REQUIRE_MESSAGE(static_cast<bool>((built).result), KernelTest::Describe(built))
