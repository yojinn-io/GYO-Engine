// The time Module (time-events declaration docs/architecture/target/v2/time-events-declaration.zh-Hant.md,
// section 6.4 "time Module"; WT-1 to WT-6). A Product with WorldTimeModule and the orchestration layer's default
// skeleton; the product's own Systems log what they read. Manual sampling: no test depends on the time base's value.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_time/FixedStep.hpp"
#include "engine/world_time/Sampling.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::WorldTime;
using W::Nanoseconds;

namespace {

constexpr Nanoseconds kStep = 15'625'000;  // 64 Hz
constexpr Nanoseconds kMs = 1'000'000;

std::vector<std::string>& Log() {
    static std::vector<std::string> log;
    return log;
}
std::string Joined() {
    std::string out;
    for (const std::string& line : Log()) out += (out.empty() ? "" : " | ") + line;
    return out;
}

struct Gate { std::int32_t on = 1; };       // the author's condition on RunFixedSteps
struct FailAt { std::uint64_t tick = 0; };  // a fixed step that fails at this tick (0: never)
struct Rewind { std::uint64_t k = 0; };     // a rollback-like rerun of the last k steps

std::string Times(const W::ActiveTime& active) {
    return std::to_string(active.delta) + "/" + std::to_string(active.elapsed);
}

// FixedUpdate/First: what a fixed step reads.
K::SystemResult LogStep(K::Res<W::Tick> tick, K::Res<W::ActiveTime> active, K::Res<W::FixedTime> fixed) {
    Log().push_back("step " + std::to_string(tick->value) + " " + Times(*active) + " fixed " + std::to_string(fixed->elapsed));
    return {};
}
// FixedUpdate/Update: fails at FailAt.
K::SystemResult FailAtTick(K::Res<W::Tick> tick, K::Res<FailAt> at) {
    if (at->tick != 0 && tick->value == at->tick) {
        return ::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "the fixed step failed"));
    }
    return {};
}
// Main/Update: what a frame System reads after the fixed steps, and the TimeDiscarded messages.
K::SystemResult LogFrame(K::Res<W::Tick> tick, K::Res<W::ActiveTime> active, K::MessageReader<W::TimeDiscarded> discarded) {
    std::string line = "frame " + std::to_string(tick->value) + " " + Times(*active);
    for (const W::TimeDiscarded* d : discarded.Read()) {
        line += " discarded " + std::to_string(d->amount) + " cause " + std::to_string(static_cast<int>(d->cause)) + " real " +
                std::to_string(d->realDelta);
    }
    Log().push_back(line);
    return {};
}
bool GateOn(K::Res<Gate> gate) { return gate->on != 0; }
// Main/PostUpdate: reruns the last k steps (RewindFixedSteps, then RunOneFixedStep k times).
K::SystemResult Rerun(K::ExclusiveWorld<> world) {
    const std::uint64_t k = W::Detail::Present(world.Read<Rewind>([](const Rewind& r) { return r.k; }));
    if (k == 0) return {};
    W::Detail::Called(world.Modify<Rewind>([](Rewind& r) { r.k = 0; }));
    W::RewindFixedSteps(world, k);
    Log().push_back("rewind");
    for (std::uint64_t i = 0; i < k; ++i) {
        if (auto ran = W::RunOneFixedStep(world, "FixedUpdate"); !ran) return ran;
    }
    return {};
}

K::SystemOptions At(std::string name, std::string schedule, std::string stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    o.zone = K::Zone::B;
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

// The frozen Registry of the last World built (a test unit's Finish keeps it; the Product owns the Registry).
const K::FrozenRegistry*& Frozen() {
    static const K::FrozenRegistry* frozen = nullptr;
    return frozen;
}
class ProbeModule final : public O::Module {
public:
    void Build(O::WorldSetup&) override {}
    void Finish(const K::FrozenRegistry& registry, O::WorldFinish&) override { Frozen() = &registry; }
};

O::RegisteredUnits TimeUnits() {
    O::RegisteredUnits units;
    for (const char* name : {"world_time", "probe"}) {
        O::RegisterRow row;
        row.name = name;
        row.kind = O::UnitKind::Module;
        row.libraryOnly = false;
        row.defaultOn = true;
        units.rows.push_back(row);
    }
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    units.modules.push_back({"probe", std::make_unique<ProbeModule>()});
    return units;
}

void LoggingSystems(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Resource<Gate>({.name = "Gate", .zone = K::Zone::B});
    r.Resource<FailAt>({.name = "FailAt", .zone = K::Zone::B});
    r.Resource<Rewind>({.name = "Rewind", .zone = K::Zone::B});
    r.AddSystem<&LogStep>(At("LogStep", "FixedUpdate", "First"));
    r.AddSystem<&FailAtTick>(At("FailAtTick", "FixedUpdate", "Update"));
    r.AddSystem<&LogFrame>(At("LogFrame", "Main", "Update"));
    r.AddSystem<&Rerun>(At("Rerun", "Main", "PostUpdate"));
}

// A Product of one World "Main" with the time Module; manual "per-frame duration" sampling of delta.
O::ProductDescription Game(Nanoseconds delta, W::WorldTimeOptions options = {}) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.options.Set(std::move(options));
    w.build = LoggingSystems;
    w.prepare.push_back([delta](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{delta, W::SamplingMode::ManualDelta, {}});
        world.SetResource(Gate{});
        world.SetResource(FailAt{});
        world.SetResource(Rewind{});
        return {};
    });
    d.worlds.push_back(std::move(w));
    return d;
}

std::unique_ptr<O::Product> Make(O::ProductDescription d) {
    auto built = O::Product::Build(std::move(d), TimeUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

template <class T>
const T& Get(O::Product& p) {
    const T* value = WorldOf(p).Resource<T>();
    REQUIRE(value != nullptr);
    return *value;
}

void Frame(O::Product& p, Nanoseconds delta) {
    WorldOf(p).SetResource(W::TimeSampling{delta, W::SamplingMode::ManualDelta, {}});
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

} // namespace

// ---- WT-1: registration

TEST_CASE("module: every time Resource is registered and put in; Resimulation is there and false; Tick starts at 0") {
    auto p = Make(Game(0));
    const K::FrozenRegistry& registry = *Frozen();
    CHECK_MESSAGE(registry.ResourceIdOf<W::RealTime>().has_value(), "[wt-registered] RealTime");
    CHECK_MESSAGE(registry.ResourceIdOf<W::VirtualTime>().has_value(), "[wt-registered] VirtualTime");
    CHECK_MESSAGE(registry.ResourceIdOf<W::FixedTime>().has_value(), "[wt-registered] FixedTime");
    CHECK_MESSAGE(registry.ResourceIdOf<W::ActiveTime>().has_value(), "[wt-registered] ActiveTime");
    CHECK_MESSAGE(registry.ResourceIdOf<W::Tick>().has_value(), "[wt-registered] Tick");
    CHECK_MESSAGE(registry.ResourceIdOf<W::TimeSampling>().has_value(), "[wt-registered] TimeSampling");
    CHECK_MESSAGE(registry.ResourceIdOf<W::TimeControl>().has_value(), "[wt-registered] TimeControl");
    CHECK_MESSAGE(registry.ResourceIdOf<W::Resimulation>().has_value(), "[wt-registered] Resimulation");
    CHECK_MESSAGE(registry.MessageIdOf<W::TimeDiscarded>().has_value(), "[wt-registered] TimeDiscarded");
    K::World& world = WorldOf(*p);
    CHECK_MESSAGE((world.Resource<W::Resimulation>() != nullptr && world.Resource<W::Resimulation>()->active == 0),
                  "[wt-resimulation-false]");
    CHECK_MESSAGE((world.Resource<W::Tick>() != nullptr && world.Resource<W::Tick>()->value == 0), "[wt-tick-zero]");
    CHECK_MESSAGE((world.Resource<W::FixedTime>() != nullptr && world.Resource<W::FixedTime>()->accumulator.Step() == kStep),
                  "[wt-step-default] 64 Hz");
    // Every time Resource is zone B (declaration 4.5): a zone C System reads no Tick until batch 4 (G4).
    for (const K::ResourceInfo& info : registry.Resources()) {
        if (info.owner != "world_time") continue;
        CHECK_MESSAGE(info.zone == K::Zone::B, "[wt-time-zone-b] " << info.name);
    }
    CHECK_MESSAGE(registry.Resources()[*registry.ResourceIdOf<W::Tick>()].zone == K::Zone::B, "[wt-time-zone-b] Tick");
}

// Authority-rollback declaration 3.3: every type the time Module puts into a World has a hash function, so a
// World with only the Module (no test Resource) has a summary, before and after a frame that discards time.
TEST_CASE("module: every time type has a hash function: a World with only the time Module has a summary") {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{kStep * 9, W::SamplingMode::ManualDelta, {}});
        return {};
    });
    d.worlds.push_back(std::move(w));
    auto p = Make(std::move(d));
    const auto before = WorldOf(*p).Summary();
    CHECK_MESSAGE(static_cast<bool>(before), "[wt-hash-summary] " << (before ? std::string() : before.error().Text()));
    auto ran = p->RunFrame();
    REQUIRE(static_cast<bool>(ran));
    const auto after = WorldOf(*p).Summary();
    CHECK_MESSAGE(static_cast<bool>(after), "[wt-hash-summary] " << (after ? std::string() : after.error().Text()));
    CHECK_MESSAGE((before && after && *before != *after), "[wt-hash-summary] a frame left the summary the same");
}

TEST_CASE("module: by default AdvanceTime is mounted in Main's First and RunFixedSteps in Main's FixedMainLoop") {
    auto p = Make(Game(0));
    const K::FrozenRegistry& registry = *Frozen();
    const K::SystemInfo* advance = registry.SystemNamed("AdvanceTime");
    const K::SystemInfo* driver = registry.SystemNamed("RunFixedSteps");
    REQUIRE((advance != nullptr && driver != nullptr && advance->mounts.size() == 1 && driver->mounts.size() == 1));
    CHECK_MESSAGE((advance->mounts[0].schedule == "Main" && advance->mounts[0].stage == "First"),
                  "[wt-default-mount-advance] " << advance->mounts[0].schedule << "/" << advance->mounts[0].stage);
    CHECK_MESSAGE((driver->mounts[0].schedule == "Main" && driver->mounts[0].stage == "FixedMainLoop"),
                  "[wt-default-mount-fixed] " << driver->mounts[0].schedule << "/" << driver->mounts[0].stage);
}

TEST_CASE("module: pausable = false registers no TimeControl, and AdvanceTime still runs") {
    W::WorldTimeOptions options;
    options.pausable = false;
    auto p = Make(Game(20 * kMs, options));
    CHECK_MESSAGE(!Frozen()->ResourceIdOf<W::TimeControl>().has_value(),
                  "[wt-unpausable] TimeControl registered");
    Frame(*p, 20 * kMs);
    CHECK_MESSAGE(Get<W::VirtualTime>(*p).elapsed == 20 * kMs, "[wt-unpausable-advances] " << Get<W::VirtualTime>(*p).elapsed);
    CHECK(Get<W::Tick>(*p).value == 1);
}

TEST_CASE("module: the step length comes from the settings when the World is built") {
    W::WorldTimeOptions options;
    options.step = W::StepLength::FromHz(32.0);
    auto p = Make(Game(0, options));
    Frame(*p, 70 * kMs);
    CHECK_MESSAGE(Get<W::FixedTime>(*p).accumulator.Step() == 31'250'000, "[wt-step-setting]");
    CHECK(Get<W::Tick>(*p).value == 2);
}

TEST_CASE("module: TickWriters names RunFixedSteps, which annotates Writes<Tick>") {
    CHECK_MESSAGE((W::WorldTimeModule::TickWriters() == std::vector<std::string>{"RunFixedSteps"}), "[wt-tick-writers]");
    auto p = Make(Game(0));
    const K::FrozenRegistry& registry = *Frozen();
    const K::SystemInfo* driver = registry.SystemNamed("RunFixedSteps");
    REQUIRE(driver != nullptr);
    const K::ResourceId tick = *registry.ResourceIdOf<W::Tick>();
    const auto& listed = driver->access.annotatedWrites;
    CHECK_MESSAGE(std::find(listed.begin(), listed.end(), tick) != listed.end(), "[wt-driver-writes-tick]");
    CHECK(driver->access.exclusive);
}

TEST_CASE("module: a fixed-step schedule that does not exist is reported by Check (WT-E1)") {
    W::WorldTimeOptions options;
    options.fixedSchedule = "NoSuchSchedule";
    auto built = O::Product::Build(Game(0, options), TimeUnits());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[wt-check-fixed-schedule] built");
    bool reported = false;
    for (const O::UnitReportedError& e : built.error().unitErrors) {
        reported = reported || (e.code == "WT-E1" && e.message.find("'NoSuchSchedule'") != std::string::npos);
    }
    CHECK_MESSAGE(reported, "[wt-check-fixed-schedule] " << built.error().detail);
}

TEST_CASE("module: a mount target that does not exist is the kernel's D-E1") {
    W::WorldTimeOptions options;
    options.advanceStage = "NoSuchStage";
    auto built = O::Product::Build(Game(0, options), TimeUnits());
    REQUIRE_FALSE(static_cast<bool>(built));
    const K::StartupReport* report = built.error().KernelReportOf("Main");
    REQUIRE(report != nullptr);
    CHECK_MESSAGE(report->Has(K::StartupCode::DE1), "[wt-mount-missing] " << built.error().detail);
}

TEST_CASE("module: an author who turns the skeleton off names its own schedules: the Systems mount there, RunFixedSteps runs that one") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.skeleton = O::ScheduleSkeleton::Off;
    w.frame = {"Frame"};
    W::WorldTimeOptions options;
    options.frameSchedule = "Frame";
    options.advanceStage = "Begin";
    options.fixedStepStage = "Steps";
    options.fixedSchedule = "Physics";
    w.options.Set(std::move(options));
    w.build = [](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        K::ScheduleOptions frame;
        frame.name = "Frame";
        frame.stages = {"Begin", "Steps", "Late"};
        r.Schedule(frame);
        K::ScheduleOptions physics;
        physics.name = "Physics";
        physics.stages = {"S"};
        r.Schedule(physics);
        r.AddSystem<&LogStep>(At("LogStep", "Physics", "S"));
    };
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{40 * kMs, W::SamplingMode::ManualDelta, {}});
        return {};
    });
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), TimeUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), "[wt-own-names-built] " << (built ? std::string() : built.error().detail));
    auto ran = (*built)->RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), "[wt-own-names-ran] " << (ran ? std::string() : ran.error().detail));
    // 40 ms: two 15.625 ms steps of Physics.
    CHECK_MESSAGE(Joined() == "step 1 15625000/15625000 fixed 15625000 | step 2 15625000/31250000 fixed 31250000",
                  "[wt-own-names-steps] " << Joined());
}

TEST_CASE("module: maxStepsPerFrame comes from the settings: a frame runs that many steps and discards the rest") {
    W::WorldTimeOptions options;
    options.limits.maxStepsPerFrame = 2;
    auto p = Make(Game(0, options));
    Frame(*p, 1000 * kMs);  // 64 steps: 2 run, 62 cut
    CHECK_MESSAGE((Get<W::Tick>(*p).value == 2 && Get<W::VirtualTime>(*p).discarded == 62 * kStep),
                  "[wt-step-limit-setting] " << Get<W::Tick>(*p).value << " " << Get<W::VirtualTime>(*p).discarded);
}

TEST_CASE("module: a 64-byte fixed-step schedule name does not fit TimeConfig: a Programmer Error") {
    W::WorldTimeOptions options;
    options.fixedSchedule = std::string(64, 'f');
    GYO_CHECK_ASSERTS(O::Product::Build(Game(0, options), TimeUnits()));
}

// ---- A World without a fixed step (WorldTimeOptions::fixedStep = false; declaration 3.2, v8-arch-gyo2-t1-three-items)

TEST_CASE("module: fixedStep = false mounts no RunFixedSteps, needs no fixed-step schedule, still registers FixedTime and Tick") {
    auto on = Make(Game(0));
    CHECK_MESSAGE(Frozen()->SystemNamed("RunFixedSteps") != nullptr, "[wt-fixed-step-default] RunFixedSteps not mounted by default");
    W::WorldTimeOptions options;
    options.fixedStep = false;
    options.fixedSchedule = "NoSuchSchedule";  // a World without a fixed step needs none
    auto built = O::Product::Build(Game(0, options), TimeUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), "[wt-no-fixed-no-check] " << (built ? std::string() : built.error().detail));
    const K::FrozenRegistry& registry = *Frozen();
    CHECK_MESSAGE(registry.SystemNamed("RunFixedSteps") == nullptr, "[wt-no-fixed-no-driver] RunFixedSteps mounted");
    CHECK_MESSAGE(registry.SystemNamed("AdvanceTime") != nullptr, "[wt-no-fixed-advance] AdvanceTime not mounted");
    CHECK_MESSAGE((registry.ResourceIdOf<W::FixedTime>().has_value() && registry.ResourceIdOf<W::Tick>().has_value()),
                  "[wt-no-fixed-registered] FixedTime and Tick");
}

TEST_CASE("module: fixedStep = false: a frame advances at most 250 ms, the excess is discarded with one TimeDiscarded (FrameLimit)") {
    Log().clear();
    W::WorldTimeOptions options;
    options.fixedStep = false;
    auto p = Make(Game(0, options));
    Frame(*p, 400 * kMs);
    const W::VirtualTime& virt = Get<W::VirtualTime>(*p);
    CHECK_MESSAGE((virt.delta == 250 * kMs && virt.elapsed == 250 * kMs), "[wt-no-fixed-frame-cap] " << virt.delta << " " << virt.elapsed);
    CHECK_MESSAGE(virt.discarded == 150 * kMs, "[wt-no-fixed-discarded] " << virt.discarded);
    CHECK_MESSAGE(Joined() == "frame 0 250000000/250000000 discarded 150000000 cause 2 real 400000000", "[wt-no-fixed-message] " << Joined());
    CHECK_MESSAGE(Get<W::Tick>(*p).value == 0, "[wt-no-fixed-tick] " << Get<W::Tick>(*p).value);
    CHECK_MESSAGE((Get<W::ActiveTime>(*p).delta == 250 * kMs && Get<W::ActiveTime>(*p).elapsed == 250 * kMs), "[wt-no-fixed-active]");
    const W::FixedTime& fixed = Get<W::FixedTime>(*p);
    CHECK_MESSAGE(fixed.pending == 0, "[wt-no-fixed-pending] " << fixed.pending);
    CHECK_MESSAGE((fixed.elapsed == 0 && fixed.accumulator.Overstep() == 0), "[wt-no-fixed-untouched]");
    // The next frame: within the cap, nothing discarded, no NotDriven.
    Frame(*p, 20 * kMs);
    CHECK_MESSAGE(Log().back() == "frame 0 20000000/270000000", "[wt-no-fixed-no-not-driven] " << Joined());
    CHECK_MESSAGE(Get<W::VirtualTime>(*p).discarded == 150 * kMs, "[wt-no-fixed-no-not-driven] discarded " << Get<W::VirtualTime>(*p).discarded);
}

TEST_CASE("module: fixedStep = false: the cap is TimeLimits::maxFrameDelta from the settings") {
    W::WorldTimeOptions options;
    options.fixedStep = false;
    options.limits.maxFrameDelta = 100 * kMs;
    auto p = Make(Game(0, options));
    Frame(*p, 400 * kMs);
    CHECK_MESSAGE((Get<W::VirtualTime>(*p).delta == 100 * kMs && Get<W::VirtualTime>(*p).discarded == 300 * kMs),
                  "[wt-no-fixed-limit-setting] " << Get<W::VirtualTime>(*p).delta << " " << Get<W::VirtualTime>(*p).discarded);
}

// ---- WT-2: sampling

TEST_CASE("sampling: a manual duration applies every frame, the first included, until changed") {
    auto p = Make(Game(10 * kMs));
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE(Get<W::RealTime>(*p).delta == 10 * kMs, "[wt-manual-delta] first frame");
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Get<W::RealTime>(*p).delta == 10 * kMs && Get<W::RealTime>(*p).elapsed == 20 * kMs), "[wt-manual-delta] kept");
    Frame(*p, 3 * kMs);
    CHECK(Get<W::RealTime>(*p).elapsed == 23 * kMs);
}

TEST_CASE("sampling: a manual next instant: the first sample after the World is built gives delta 0") {
    auto p = Make(Game(0));
    WorldOf(*p).SetResource(W::TimeSampling{500 * kMs, W::SamplingMode::ManualInstant, {}});
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Get<W::RealTime>(*p).delta == 0 && Get<W::RealTime>(*p).now == 500 * kMs), "[wt-first-sample-zero]");
    WorldOf(*p).SetResource(W::TimeSampling{520 * kMs, W::SamplingMode::ManualInstant, {}});
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Get<W::RealTime>(*p).delta == 20 * kMs && Get<W::VirtualTime>(*p).elapsed == 20 * kMs), "[wt-manual-instant]");
}

TEST_CASE("sampling: the time base: the first delta is 0 and time goes forward") {
    auto p = Make(Game(0));
    WorldOf(*p).SetResource(W::TimeSampling{});
    REQUIRE(static_cast<bool>(p->RunFrame()));
    const W::RealTime first = Get<W::RealTime>(*p);
    CHECK_MESSAGE((first.delta == 0 && first.sampled == 1), "[wt-time-base-first]");
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Get<W::RealTime>(*p).now >= first.now && Get<W::RealTime>(*p).delta >= 0), "[wt-time-base-forward]");
}

TEST_CASE("sampling: SampleRealTime reads the given time base in the time-base mode only") {
    W::RealTime real;
    W::SampleRealTime(real, W::TimeSampling{}, []() noexcept -> Nanoseconds { return 7; });
    W::SampleRealTime(real, W::TimeSampling{}, []() noexcept -> Nanoseconds { return 19; });
    CHECK_MESSAGE((real.now == 19 && real.delta == 12), "[wt-sample-reader] " << real.now << " " << real.delta);
    W::SampleRealTime(real, W::TimeSampling{5, W::SamplingMode::ManualDelta, {}}, []() noexcept -> Nanoseconds { return 1000; });
    CHECK_MESSAGE((real.now == 24 && real.delta == 5), "[wt-sample-manual-no-reader] " << real.now << " " << real.delta);
}

TEST_CASE("sampling: a paused World advances real time, not virtual time, and runs no fixed step") {
    auto p = Make(Game(20 * kMs));
    WorldOf(*p).SetResource(W::TimeControl{1.0, 1, {}});
    REQUIRE(static_cast<bool>(p->RunFrame()));
    CHECK_MESSAGE((Get<W::RealTime>(*p).elapsed == 20 * kMs && Get<W::VirtualTime>(*p).elapsed == 0 && Get<W::Tick>(*p).value == 0),
                  "[wt-paused]");
    CHECK(Get<W::VirtualTime>(*p).paused == 1);
    CHECK_MESSAGE((Get<W::ActiveTime>(*p).delta == 0 && Get<W::ActiveTime>(*p).elapsed == 0), "[wt-paused-active] the frame's virtual time");
}

// ---- WT-3: the fixed step and Tick

TEST_CASE("fixed: k steps a frame, Tick + k, the n-th step reads Tick n; ActiveTime is the fixed one inside, the frame's after") {
    Log().clear();
    auto p = Make(Game(20 * kMs));
    Frame(*p, 20 * kMs);   // 1 step, remainder 4.375 ms
    // In the frame that sampled it: Tick + 1, FixedTime::elapsed + one step, pending taken (each value printed:
    // the mutants of AdvanceTime and RunFixedSteps each give their own line).
    CHECK_MESSAGE((Get<W::Tick>(*p).value == 1 && Get<W::FixedTime>(*p).pending == 0 && Get<W::FixedTime>(*p).elapsed == kStep),
                  "[wt-fixed-steps-frame1] tick " << Get<W::Tick>(*p).value << " pending " << Get<W::FixedTime>(*p).pending
                                                  << " fixed " << Get<W::FixedTime>(*p).elapsed);
    Frame(*p, 40 * kMs);   // 44.375 ms: 2 steps, remainder 13.125 ms
    CHECK_MESSAGE(Get<W::Tick>(*p).value == 3, "[wt-fixed-steps-k] " << Get<W::Tick>(*p).value);
    // After the steps, the frame's value again (the next AdvanceTime would write it anyway: checked here, before it).
    CHECK_MESSAGE((Get<W::ActiveTime>(*p).delta == 40 * kMs && Get<W::ActiveTime>(*p).elapsed == 60 * kMs), "[wt-active-restored]");
    Frame(*p, 0);          // no step
    CHECK_MESSAGE(Joined() == "step 1 15625000/15625000 fixed 15625000 | frame 1 20000000/20000000 | "
                              "step 2 15625000/31250000 fixed 31250000 | step 3 15625000/46875000 fixed 46875000 | "
                              "frame 3 40000000/60000000 | frame 3 0/60000000",
                  "[wt-fixed-steps] " << Joined());
    CHECK_MESSAGE(Get<W::FixedTime>(*p).accumulator.Overstep() == 13'125'000, "[wt-remainder]");
    CHECK_MESSAGE(Get<W::FixedTime>(*p).pending == 0, "[wt-pending-taken]");
    CHECK_MESSAGE((Get<W::ActiveTime>(*p).delta == 0 && Get<W::ActiveTime>(*p).elapsed == 60 * kMs), "[wt-active-frame]");
}

TEST_CASE("fixed: a frame that falls behind runs the cap of steps, adds the cut to discarded and sends one TimeDiscarded") {
    Log().clear();
    auto p = Make(Game(0));
    Frame(*p, 1000 * kMs);  // 64 steps: 5 run, 59 cut
    CHECK_MESSAGE(Get<W::Tick>(*p).value == 5, "[wt-step-limit] tick");
    CHECK_MESSAGE(Get<W::VirtualTime>(*p).discarded == 59 * kStep, "[wt-step-limit] discarded " << Get<W::VirtualTime>(*p).discarded);
    CHECK_MESSAGE(Log().back() == "frame 5 78125000/78125000 discarded 921875000 cause 1 real 1000000000",
                  "[wt-step-limit-message] " << Joined());
}

TEST_CASE("fixed: a frame whose RunFixedSteps a run condition skips: the next AdvanceTime discards the untaken time (NotDriven)") {
    Log().clear();
    W::WorldTimeOptions options;
    options.fixedStepConditions = {K::Condition<&GateOn>("GateOn")};
    auto p = Make(Game(0, options));
    Frame(*p, 20 * kMs);  // 1 step, remainder 4.375 ms
    WorldOf(*p).SetResource(Gate{0});
    Frame(*p, 20 * kMs);  // RunFixedSteps skipped: 20 ms not taken
    CHECK_MESSAGE(Get<W::FixedTime>(*p).pending == 20 * kMs, "[wt-not-driven] pending left");
    WorldOf(*p).SetResource(Gate{1});
    Frame(*p, 20 * kMs);  // the untaken 20 ms discarded; 4.375 + 20 ms: 1 step
    CHECK_MESSAGE(Get<W::VirtualTime>(*p).discarded == 20 * kMs, "[wt-not-driven] discarded " << Get<W::VirtualTime>(*p).discarded);
    CHECK_MESSAGE(Get<W::Tick>(*p).value == 2, "[wt-not-driven] tick " << Get<W::Tick>(*p).value);
    CHECK_MESSAGE(Log().back() == "frame 2 20000000/60000000 discarded 20000000 cause 3 real 20000000", "[wt-not-driven-message] " << Joined());
    // The untaken time is recorded, not taken back from virtual time (WT-2).
    CHECK_MESSAGE(Get<W::VirtualTime>(*p).elapsed == 60 * kMs, "[wt-not-driven-elapsed-kept] " << Get<W::VirtualTime>(*p).elapsed);
}

TEST_CASE("fixed: RewindFixedSteps then RunOneFixedStep k times reads the same Tick and fixed time as the first time") {
    Log().clear();
    auto p = Make(Game(0));
    Frame(*p, 20 * kMs);
    Frame(*p, 40 * kMs);  // steps 2 and 3
    WorldOf(*p).SetResource(Rewind{2});
    Frame(*p, 0);
    CHECK_MESSAGE(Joined() == "step 1 15625000/15625000 fixed 15625000 | frame 1 20000000/20000000 | "
                              "step 2 15625000/31250000 fixed 31250000 | step 3 15625000/46875000 fixed 46875000 | "
                              "frame 3 40000000/60000000 | frame 3 0/60000000 | rewind | "
                              "step 2 15625000/31250000 fixed 31250000 | step 3 15625000/46875000 fixed 46875000",
                  "[wt-rewind] " << Joined());
    CHECK_MESSAGE((Get<W::Tick>(*p).value == 3 && Get<W::FixedTime>(*p).elapsed == 3 * kStep), "[wt-rewind-back]");
    CHECK_MESSAGE(Get<W::FixedTime>(*p).accumulator.Overstep() == 13'125'000, "[wt-rewind-accumulator-kept]");
    CHECK_MESSAGE((Get<W::ActiveTime>(*p).delta == 0 && Get<W::ActiveTime>(*p).elapsed == 60 * kMs), "[wt-rewind-active-restored]");
}

namespace {
// The two time Resources RewindFixedSteps writes, behind the ExclusiveWorld calls it makes.
struct RewindWorld {
    W::Tick tick{3};
    W::FixedTime fixed{};
    template <class T, class F>
    ::Engine::Base::Result<bool, K::KernelError> Modify(F&& f) {
        if constexpr (std::is_same_v<T, W::Tick>) {
            f(tick);
        } else {
            f(fixed);
        }
        return true;
    }
};
} // namespace

TEST_CASE("fixed: RewindFixedSteps may not take more steps back than Tick: a Programmer Error") {
    RewindWorld past;
    GYO_CHECK_ASSERTS(W::RewindFixedSteps(past, 4));
    RewindWorld all;
    W::RewindFixedSteps(all, 3);
    CHECK_MESSAGE(all.tick.value == 0, "[wt-rewind-to-zero]");
}

TEST_CASE("fixed: the j-th of k steps returns Err: no later step runs, the Err goes out, the World is interrupted") {
    Log().clear();
    auto p = Make(Game(0));
    WorldOf(*p).SetResource(FailAt{2});
    WorldOf(*p).SetResource(W::TimeSampling{3 * kStep, W::SamplingMode::ManualDelta, {}});
    auto ran = p->RunFrame();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(ran), "[wt-step-err] the frame succeeded");
    CHECK_MESSAGE(ran.error().code == O::OrchestrationErrorCode::ScheduleFailed, "[wt-step-err] code");
    CHECK_MESSAGE(ran.error().detail.find("the fixed step failed") != std::string::npos, "[wt-step-err-passed] " << ran.error().detail);
    CHECK_MESSAGE(Joined() == "step 1 15625000/15625000 fixed 15625000 | step 2 15625000/31250000 fixed 31250000",
                  "[wt-step-err-stops] " << Joined());
    // No later step started (the World's interruption alone would not show it: a third step would still count Tick).
    CHECK_MESSAGE((Get<W::Tick>(*p).value == 2 && Get<W::FixedTime>(*p).elapsed == 2 * kStep),
                  "[wt-step-err-later-not-run] tick " << Get<W::Tick>(*p).value << " fixed " << Get<W::FixedTime>(*p).elapsed);
    CHECK(p->Failure() != nullptr);
    CHECK(WorldOf(*p).State() == K::WorldState::Interrupted);
    // ActiveTime is not put back: the World broke inside the step.
    CHECK_MESSAGE((Get<W::ActiveTime>(*p).delta == kStep && Get<W::ActiveTime>(*p).elapsed == 2 * kStep), "[wt-step-err-not-restored]");
}

// ---- Restore (declaration 3.5)

TEST_CASE("restore: Tick, VirtualTime and the fixed-step state put back into a new World go on from there; the step is this World's setting") {
    Log().clear();
    auto first = Make(Game(0));
    Frame(*first, 20 * kMs);
    Frame(*first, 40 * kMs);
    const W::Tick tick = Get<W::Tick>(*first);
    const W::VirtualTime virt = Get<W::VirtualTime>(*first);
    const Nanoseconds elapsed = Get<W::FixedTime>(*first).elapsed;
    const Nanoseconds overstep = Get<W::FixedTime>(*first).accumulator.Overstep();
    REQUIRE((tick.value == 3 && overstep == 13'125'000));

    W::WorldTimeOptions options;
    options.step = W::StepLength::FromHz(32.0);  // 31.25 ms in the new World
    O::ProductDescription d = Game(0, options);
    d.worlds[0].prepare.push_back([tick, virt, elapsed, overstep](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(tick);
        world.SetResource(virt);
        W::RestoreFixedTime(world, elapsed, overstep);
        return {};
    });
    auto second = Make(std::move(d));
    CHECK_MESSAGE((Get<W::Tick>(*second).value == 3 && Get<W::FixedTime>(*second).elapsed == 3 * kStep), "[wt-restore-put-back]");
    CHECK_MESSAGE(Get<W::FixedTime>(*second).accumulator.Step() == 31'250'000, "[wt-restore-step-setting]");
    Log().clear();
    Frame(*second, 20 * kMs);  // 13.125 + 20 ms: one 31.25 ms step
    CHECK_MESSAGE(Joined() == "step 4 31250000/78125000 fixed 78125000 | frame 4 20000000/80000000", "[wt-restore-goes-on] " << Joined());
    CHECK_MESSAGE(Get<W::RealTime>(*second).elapsed == 20 * kMs, "[wt-restore-real-fresh]");
}

namespace {
// Puts PutBack's values back from an exclusive System (Main/PostUpdate: after this frame's RunFixedSteps).
struct PutBack {
    Nanoseconds elapsed = 0;
    Nanoseconds overstep = 0;
    std::uint8_t armed = 0;
    std::uint8_t reserved[7] = {};
};
K::SystemResult PutBackNow(K::ExclusiveWorld<> world) {
    const PutBack put = W::Detail::Present(world.Read<PutBack>([](const PutBack& p) { return p; }));
    if (put.armed == 0) return {};
    W::Detail::Called(world.Modify<PutBack>([](PutBack& p) { p.armed = 0; }));
    W::RestoreFixedTime(world, put.elapsed, put.overstep);
    return {};
}
} // namespace

TEST_CASE("restore: a remainder saved under a longer step keeps what is below this World's step; the whole steps beyond go to discarded") {
    SUBCASE("from a prepare function") {
        // Saved at 32 Hz: two steps run, 20 ms left; this World runs at 64 Hz (15.625 ms).
        O::ProductDescription d = Game(0);
        d.worlds[0].prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
            W::RestoreFixedTime(world, 2 * 31'250'000, 20 * kMs);
            return {};
        });
        auto p = Make(std::move(d));
        const W::FixedTime& fixed = Get<W::FixedTime>(*p);
        CHECK_MESSAGE((fixed.accumulator.Step() == kStep && fixed.elapsed == 62'500'000 && fixed.accumulator.Overstep() == 4'375'000),
                      "[wt-restore-longer-step] " << fixed.accumulator.Step() << " " << fixed.elapsed << " " << fixed.accumulator.Overstep());
        CHECK_MESSAGE(Get<W::VirtualTime>(*p).discarded == kStep, "[wt-restore-longer-discarded] " << Get<W::VirtualTime>(*p).discarded);
    }
    SUBCASE("from an exclusive System") {
        O::ProductDescription d = Game(0);
        auto build = d.worlds[0].build;
        d.worlds[0].build = [build](O::WorldSetup& s) {
            build(s);
            s.Registrar().Resource<PutBack>({.name = "PutBack", .zone = K::Zone::B});
            s.Registrar().AddSystem<&PutBackNow>(At("PutBackNow", "Main", "Last"));
        };
        d.worlds[0].prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
            world.SetResource(PutBack{2 * 31'250'000, 20 * kMs, 1, {}});
            return {};
        });
        auto p = Make(std::move(d));
        Frame(*p, 0);
        const W::FixedTime& fixed = Get<W::FixedTime>(*p);
        CHECK_MESSAGE((fixed.elapsed == 62'500'000 && fixed.accumulator.Overstep() == 4'375'000),
                      "[wt-restore-exclusive] " << fixed.elapsed << " " << fixed.accumulator.Overstep());
        CHECK_MESSAGE(Get<W::VirtualTime>(*p).discarded == kStep, "[wt-restore-exclusive-discarded] " << Get<W::VirtualTime>(*p).discarded);
    }
}

namespace {
bool& FirstBuild() {
    static bool first = true;
    return first;
}
} // namespace

TEST_CASE("carry-over: carried time values are kept; FixedTime takes this World's step; TimeConfig is this World's settings") {
    Log().clear();
    O::ProductDescription d = Game(0);
    auto build = d.worlds[0].build;
    d.worlds[0].build = [build](O::WorldSetup& s) {
        build(s);
        s.CarryOver<W::Tick>("Tick");
        s.CarryOver<W::FixedTime>("FixedTime");
        s.CarryOver<W::TimeConfig>("TimeConfig");
    };
    // The old World only: a 32 Hz FixedTime and its own TimeConfig (one step a frame), as if they were put back.
    FirstBuild() = true;
    d.worlds[0].prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        if (!FirstBuild()) return {};
        world.SetResource(W::FixedTime{W::TickAccumulator{W::StepLength::FromHz(32.0)}, 0, 0});
        W::TimeConfig config = *world.Resource<W::TimeConfig>();
        config.limits.maxStepsPerFrame = 1;
        world.SetResource(config);
        return {};
    });
    auto p = Make(std::move(d));
    Frame(*p, 40 * kMs);  // one 31.25 ms step, 8.75 ms left
    Frame(*p, 20 * kMs);  // 28.75 ms left, no step
    REQUIRE((Get<W::Tick>(*p).value == 1 && Get<W::FixedTime>(*p).accumulator.Overstep() == 28'750'000));
    FirstBuild() = false;
    auto replaced = p->ReplaceWorld("Main", O::ReplaceRequest{});
    REQUIRE_MESSAGE(static_cast<bool>(replaced), (replaced ? std::string() : replaced.error().message + "\n" + replaced.error().detail));
    CHECK_MESSAGE(Get<W::Tick>(*p).value == 1, "[wt-carried-kept] " << Get<W::Tick>(*p).value);
    const W::FixedTime& fixed = Get<W::FixedTime>(*p);
    CHECK_MESSAGE((fixed.accumulator.Step() == kStep && fixed.elapsed == 31'250'000 && fixed.accumulator.Overstep() == 13'125'000),
                  "[wt-carried-step-setting] " << fixed.accumulator.Step() << " " << fixed.elapsed << " " << fixed.accumulator.Overstep());
    CHECK_MESSAGE(Get<W::VirtualTime>(*p).discarded == kStep, "[wt-carried-discarded] " << Get<W::VirtualTime>(*p).discarded);
    CHECK_MESSAGE(Get<W::TimeConfig>(*p).limits.maxStepsPerFrame == 5, "[wt-carried-config-setting] " << Get<W::TimeConfig>(*p).limits.maxStepsPerFrame);
}
