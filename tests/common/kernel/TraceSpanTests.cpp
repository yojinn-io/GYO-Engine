// The kernel's spans (trace declaration docs/architecture/target/v2/trace-declaration.zh-Hant.md, D6): one span
// per Run of a schedule (RunPlan) and per System that runs, the World's name on each; the kernel's Fail marks
// the span on top of the thread's stack, origin where the error is produced and not where a nested run's error
// passes through (D4 (i)); spans an exception unwinds say so. The kernel only calls the Base facade; this test
// installs a recording receiver of its own (no trace Module). Each CHECK carries a tag "[ks-trace-...]".

#include "WorldTestSupport.hpp"

// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Trace.hpp"

namespace {

using namespace GYO::Test::Kernel;
namespace Base = ::GYO::Engine::Base;

std::vector<std::string>* events = nullptr;
std::uint64_t nextId = 1;

Base::TraceToken Begin(const std::string_view category, const std::string_view name, const std::string_view world) noexcept {
    events->push_back("begin " + std::string(category) + " " + std::string(name) + "@" + std::string(world));
    return Base::TraceToken{.id = nextId++, .generation = 1, .depth = 1};
}
void End(const Base::TraceToken token, const bool byException) noexcept {
    events->push_back("end " + std::to_string(token.id) + (byException ? " exception" : ""));
}
void Error(const Base::TraceToken token, const std::string_view text, const bool origin) noexcept {
    events->push_back("error " + std::to_string(token.id) + (origin ? " origin " : " through ") + std::string(text));
}
void MarkCurrent(const std::string_view, const bool origin) noexcept {
    events->push_back(origin ? "current origin" : "current through");
}
std::size_t Path(std::span<char>) noexcept {
    return 0;
}
constexpr Base::TraceReceiver kReceiver{.begin = &Begin, .end = &End, .error = &Error, .markCurrent = &MarkCurrent, .path = &Path};

struct Recording {
    std::vector<std::string> log;
    const Base::TraceReceiver* previous;
    Recording() : previous(Base::SetTraceReceiver(&kReceiver)) {
        events = &log;
        nextId = 1;
    }
    ~Recording() {
        Base::SetTraceReceiver(previous);
        events = nullptr;
    }
    [[nodiscard]] std::string Joined() const {
        std::string out;
        for (const std::string& e : log) out += (out.empty() ? "" : " | ") + e;
        return out;
    }
};

SystemResult Quiet(Query<Read<Tag>>) {
    return Ok();
}
SystemResult Fails(Query<Read<Tag>>) {
    return GYO::Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "boom"));
}
SystemResult Throws(Query<Read<Tag>>) {
    throw std::runtime_error("thrown inside");
}
SystemResult RunsStep(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Step"); !ran) return GYO::Engine::Base::Err(std::move(ran).error());
    return Ok();
}
bool Never(Res<Flag>) {
    return false;
}

SystemResult DespawnSix(Despawn despawn) {
    despawn.Entity(6);
    return Ok();
}
// A hook (run while a sync point applies its records) that tries to run a schedule: refused (X-13).
SystemResult HookRunsSchedule(ExclusiveWorld<> world) {
    static_cast<void>(world.RunSchedule("Tick"));
    return Ok();
}

struct Bell { std::uint32_t n; };
SystemResult HearBell(On<Bell>) {
    return Ok();
}
SystemResult BellDespawnsSix(On<Bell>, Despawn despawn) {
    despawn.Entity(6);
    return Ok();
}
SystemResult RingsNow(ExclusiveWorld<> world) {
    return world.Trigger(Bell{2});
}
SystemResult BellFails(On<Bell>) {
    return GYO::Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "the bell cracked"));
}

ScheduleOptions StepSchedule() {
    ScheduleOptions s;
    s.name = "Step";
    s.stages = {"S"};
    return s;
}

} // namespace

TEST_CASE("kernel trace: a Run is one span, each System that runs one span inside it, with the World's name") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&Quiet>(B("First", "A"));
        r.AddSystem<&Quiet>(B("Second", "B", {}, {"First"}));
    }, false, "Match");
    Recording recording;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(recording.Joined() == "begin kernel Tick@Match | begin kernel First@Match | end 2 | begin kernel Second@Match | end 3 | end 1",
                  "[ks-trace-spans] " << recording.Joined());
}

TEST_CASE("kernel trace: a System's Err is marked on its own span, as the origin, before the span ends") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&Fails>(B("Breaks", "A"));
    }, false, "Match");
    Recording recording;
    REQUIRE_FALSE(rig->Run());
    CHECK_MESSAGE(recording.Joined() == "begin kernel Tick@Match | begin kernel Breaks@Match | current origin | end 2 | end 1",
                  "[ks-trace-err] " << recording.Joined());
}

TEST_CASE("kernel trace: a nested run's error is the origin inside and passes through the outer System") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Schedule(StepSchedule());
        r.AddSystem<&RunsStep>(B("Driver", "A"));
        r.AddSystem<&Fails>(Sys("Inner", {At("Step", "S")}));
    }, false, "Match");
    Recording recording;
    REQUIRE_FALSE(rig->Run());
    CHECK_MESSAGE(recording.Joined() ==
                      "begin kernel Tick@Match | begin kernel Driver@Match | begin kernel Step@Match | begin kernel Inner@Match | "
                      "current origin | end 4 | end 3 | current through | end 2 | end 1",
                  "[ks-trace-nested] " << recording.Joined());
}

TEST_CASE("kernel trace: spans a System's exception unwinds say so; the exception goes on") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&Throws>(B("Thrower", "A"));
    }, false, "Match");
    Recording recording;
    CHECK_THROWS_AS(static_cast<void>(rig->Run()), std::runtime_error);
    CHECK_MESSAGE(recording.Joined() == "begin kernel Tick@Match | begin kernel Thrower@Match | end 2 exception | end 1 exception",
                  "[ks-trace-exception] " << recording.Joined());
}

TEST_CASE("kernel trace: a System its run condition skips opens no span") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Mount m = At("Tick", "A");
        m.conditions.push_back(Condition<&Never>("Never"));
        r.AddSystem<&Quiet>(Sys("Skipped", {m}));
    }, false, "Match");
    Recording recording;
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(recording.Joined() == "begin kernel Tick@Match | end 1", "[ks-trace-skipped] " << recording.Joined());
}

TEST_CASE("kernel trace: with no receiver a Run, an Err and a nested run behave as before") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&Fails>(B("Breaks", "A"));
    }, false, "Match");
    REQUIRE(Base::SetTraceReceiver(nullptr) == nullptr);
    const auto result = rig->Run();
    CHECK(HasCode(result, KernelErrorCode::SystemFailed));
}

TEST_CASE("kernel trace: a Trigger from the Host is one span of its own") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Event<Bell>({.name = "Bell"});
        r.Observer<Bell, &HearBell>({.name = "hear-bell"});
    }, false, "Match");
    Recording recording;
    REQUIRE(rig->Trigger(Bell{1}));
    CHECK_MESSAGE(recording.Joined().rfind("begin kernel Trigger@Match", 0) == 0, "[ks-trace-trigger] " << recording.Joined());
}

TEST_CASE("kernel trace: a refusal inside a sync point is the origin once; the sync point's Fail only passes it through") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&DespawnSix>(B("Despawner", "A"));
        r.Hook<Tag, &HookRunsSchedule>(HookTrigger::OnRemove, {.name = "HookRuns"});
    }, false, "Match");
    rig->world().Insert(6, Tag{6});
    Recording recording;
    REQUIRE_FALSE(rig->Run());
    CHECK_MESSAGE(recording.Joined() == "begin kernel Tick@Match | begin kernel Despawner@Match | end 2 | current origin | current through | end 1",
                  "[ks-trace-sync-once] " << recording.Joined());
}

TEST_CASE("kernel trace: a change outside the World API caught by the Ready window is the origin on the schedule's span") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&Quiet>(B("First", "A"));
    }, true, "Match");
    rig->world().Insert(1, Tag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const Tag* kept = rig->world().Get<Tag>(1);
    const_cast<Tag*>(kept)->t = 42;  // not through the World API
    Recording recording;
    REQUIRE_FALSE(rig->Run());
    CHECK_MESSAGE(recording.Joined() == "begin kernel Tick@Match | current origin | end 1", "[ks-trace-ready] " << recording.Joined());
}

TEST_CASE("kernel trace: an observer's Err in a Trigger from the Host is the origin once") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Event<Bell>({.name = "Bell"});
        r.Observer<Bell, &BellFails>({.name = "bell-fails"});
    }, false, "Match");
    Recording recording;
    REQUIRE_FALSE(rig->Trigger(Bell{1}));
    std::size_t origins = 0;
    for (const std::string& e : recording.log) origins += e == "current origin" ? 1U : 0U;
    CHECK_MESSAGE(origins == 1, "[ks-trace-trigger-err] " << recording.Joined());
}

namespace {
std::size_t Origins(const Recording& recording) {
    std::size_t origins = 0;
    for (const std::string& e : recording.log) origins += e == "current origin" ? 1U : 0U;
    return origins;
}
} // namespace

TEST_CASE("kernel trace: a hook refused in a Host Trigger's rounds is the origin once") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Event<Bell>({.name = "Bell"});
        r.Observer<Bell, &BellDespawnsSix>({.name = "bell-despawns"});
        r.Hook<Tag, &HookRunsSchedule>(HookTrigger::OnRemove, {.name = "HookRuns"});
    }, false, "Match");
    rig->world().Insert(6, Tag{6});
    Recording recording;
    REQUIRE_FALSE(rig->Trigger(Bell{1}));
    CHECK_MESSAGE(Origins(recording) == 1, "[ks-trace-host-trigger-once] " << recording.Joined());
}

TEST_CASE("kernel trace: a hook refused in a System's Trigger is the origin once") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Event<Bell>({.name = "Bell"});
        r.Observer<Bell, &BellDespawnsSix>({.name = "bell-despawns"});
        r.Hook<Tag, &HookRunsSchedule>(HookTrigger::OnRemove, {.name = "HookRuns"});
        r.AddSystem<&RingsNow>(B("Ringer", "A"));
    }, false, "Match");
    rig->world().Insert(6, Tag{6});
    Recording recording;
    REQUIRE_FALSE(rig->Run());
    CHECK_MESSAGE(Origins(recording) == 1, "[ks-trace-system-trigger-once] " << recording.Joined());
}
