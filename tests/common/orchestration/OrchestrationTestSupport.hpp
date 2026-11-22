#pragma once

// Shared helpers of the orchestration tests (declaration
// docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
// section 7.3). Synthetic units, synthetic register rows and small schedules:
// no product and no real Module is assumed (AGENTS section 7).

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <doctest/doctest.h>

#include <functional>
#include <memory>
// MSVC: doctest messages stream std::string_view (ToString of the codes); that needs the full ostream.
#include <ostream>
#include <string>
#include <vector>

namespace GYO::Test::Orchestration {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;

// Every lifecycle call and every logging System appends here.
inline std::vector<std::string>& Log() {
    static std::vector<std::string> log;
    return log;
}
inline std::string Joined() {
    std::string out;
    for (const std::string& line : Log()) out += (out.empty() ? "" : " | ") + line;
    return out;
}
inline bool Logged(const std::string& line) {
    for (const std::string& l : Log()) {
        if (l == line) return true;
    }
    return false;
}
inline std::size_t CountLogged(const std::string& line) {
    std::size_t n = 0;
    for (const std::string& l : Log()) n += l == line ? 1 : 0;
    return n;
}

struct Tag {
    int t = 0;
};
struct Counter {
    int n = 0;
};

// A System that logs "<C>" (one System per letter: AddSystem needs one function each).
template <char C>
K::SystemResult Mark(K::Query<K::Read<Tag>>) {
    Log().push_back(std::string(1, C));
    return {};
}
inline K::SystemResult Fail(K::Query<K::Read<Tag>>) {
    return GYO::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "the test System failed"));
}

// A one-stage schedule (the kernel keeps no clock since batch 3: a schedule belongs to no domain).
inline K::ScheduleOptions Once(std::string name) {
    K::ScheduleOptions s;
    s.name = std::move(name);
    s.stages = {"S"};
    return s;
}
inline K::ScheduleOptions FrameClock(std::string name) {
    K::ScheduleOptions s = Once(std::move(name));
    return s;
}
inline K::SystemOptions In(std::string name, std::string schedule, std::vector<std::string> before = {},
                           std::vector<std::string> after = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = "S";
    m.before = std::move(before);
    m.after = std::move(after);
    o.mounts.push_back(std::move(m));
    return o;
}

// The callbacks of a synthetic unit; each one also logs the call.
struct Calls {
    std::function<void(O::ProductSetup&)> configure;
    std::function<void(O::WorldSetup&)> build;
    std::function<void(const K::FrozenRegistry&, O::WorldFinish&)> finish;
    std::function<void(const K::FrozenRegistry&, O::WorldCheck&)> check;
};

template <class Base>
class FnUnit final : public Base {
public:
    FnUnit(std::string name, Calls calls) : name_(std::move(name)), calls_(std::move(calls)) {}
    void Configure(O::ProductSetup& setup) override {
        Log().push_back("configure " + name_);
        if (calls_.configure) calls_.configure(setup);
    }
    void Build(O::WorldSetup& setup) override {
        Log().push_back("build " + name_ + "@" + setup.WorldName());
        if (calls_.build) calls_.build(setup);
    }
    void Finish(const K::FrozenRegistry& registry, O::WorldFinish& context) override {
        Log().push_back("finish " + name_ + "@" + context.WorldName() + " v" + std::to_string(registry.Version()) +
                        (registry.Plans().empty() ? " noplans" : ""));
        if (calls_.finish) calls_.finish(registry, context);
    }
    void Check(const K::FrozenRegistry& registry, O::WorldCheck& context) override {
        Log().push_back("check " + name_ + "@" + context.WorldName());
        if (calls_.check) calls_.check(registry, context);
    }

private:
    std::string name_;
    Calls calls_;
};
using FnModule = FnUnit<O::Module>;
using FnPlugin = FnUnit<O::Plugin>;

inline O::RegisterRow Row(std::string name, O::UnitKind kind, std::vector<std::string> depends = {},
                          bool libraryOnly = false) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = kind;
    row.libraryOnly = libraryOnly;
    row.defaultOn = kind == O::UnitKind::Module && !libraryOnly;
    row.depends = std::move(depends);
    return row;
}

// Builds a RegisteredUnits: every unit added also gets its row unless told otherwise.
class Units {
public:
    Units& Library(std::string name, std::vector<std::string> depends = {}) {
        units_.rows.push_back(Row(std::move(name), O::UnitKind::Module, std::move(depends), true));
        return *this;
    }
    Units& Module(std::string name, Calls calls = {}, std::vector<std::string> depends = {}, bool addRow = true) {
        if (addRow) units_.rows.push_back(Row(name, O::UnitKind::Module, std::move(depends)));
        units_.modules.push_back({name, std::make_unique<FnModule>(name, std::move(calls))});
        return *this;
    }
    Units& Plugin(std::string name, Calls calls = {}, std::vector<std::string> depends = {}, bool addRow = true) {
        if (addRow) units_.rows.push_back(Row(name, O::UnitKind::Plugin, std::move(depends)));
        units_.plugins.push_back({name, std::make_unique<FnPlugin>(name, std::move(calls))});
        return *this;
    }
    Units& RowOnly(O::RegisterRow row) {
        units_.rows.push_back(std::move(row));
        return *this;
    }
    O::RegisteredUnits Take() { return std::move(units_); }

private:
    O::RegisteredUnits units_;
};

// Registers Tag (so Mark Systems have a parameter type) and the given schedules.
inline void Basics(K::Registrar& r, std::vector<K::ScheduleOptions> schedules) {
    r.Component<Tag>({.name = "Tag"});
    for (K::ScheduleOptions& s : schedules) r.Schedule(std::move(s));
}

inline O::WorldDescription World(std::string name, std::function<void(O::WorldSetup&)> build = {},
                                 std::vector<std::string> frame = {}) {
    O::WorldDescription w;
    w.name = std::move(name);
    w.build = std::move(build);
    w.frame = std::move(frame);
    return w;
}

using BuildResult = GYO::Engine::Base::Result<std::unique_ptr<O::Product>, O::ProductStartupReport>;

inline std::string Describe(const BuildResult& built) {
    return built ? std::string("ok") : built.error().detail;
}

} // namespace GYO::Test::Orchestration

#define ORCH_REQUIRE_BUILT(built) REQUIRE_MESSAGE(static_cast<bool>(built), ::GYO::Test::Orchestration::Describe(built))
