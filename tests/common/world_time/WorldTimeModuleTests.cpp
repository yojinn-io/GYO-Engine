// The time Module (time-events declaration docs/architecture/target/v2/time-events-declaration.zh-Hant.md,
// section 6.4 "time Module"; WT-1 to WT-6). A Product with WorldTimeModule and the orchestration layer's default
// skeleton; the product's own Systems log what they read. Manual sampling: no test depends on the time base's value.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
    Frame(*p, 40 * kMs);   // 44.375 ms: 2 steps, remainder 13.125 ms
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
