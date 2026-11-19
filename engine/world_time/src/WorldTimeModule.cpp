#include "engine/world_time/WorldTimeModule.hpp"

#include "engine/base/Assert.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/World.hpp"
#include "engine/world_time/Sampling.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace GYO::Engine::WorldTime {

namespace {

using SystemResult = ::Engine::Base::Result<void, Kernel::KernelError>;

const WorldTimeOptions& OptionsOf(const WorldTimeOptions* given) {
    static const WorldTimeOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// AdvanceTime (declaration 3.3): one sample, then real time, the untaken time of the last frame, virtual time,
// ActiveTime (the frame's value) and the fixed step's time of this frame. The one System that writes
// VirtualTime::discarded (WT-2). A World without a fixed step (TimeConfig::fixedStep 0): no accumulator, so
// AdvanceVirtual caps the frame at maxFrameDelta, and nothing is handed to a fixed step (no NotDriven).
SystemResult Advance(RealTime& real, VirtualTime& virt, FixedTime& fixed, ActiveTime& active, const TimeSampling& sampling,
                     const TimeControl& control, const TimeConfig& config, Kernel::MessageWriter<TimeDiscarded>& discarded) {
    SampleRealTime(real, sampling);
    if (fixed.pending > 0) {
        // The fixed step did not take the last frame's time (RunFixedSteps did not run): discarded, not carried.
        virt.discarded += fixed.pending;
        discarded.Send(TimeDiscarded{fixed.pending, real.delta, DiscardCause::NotDriven, {}});
        fixed.pending = 0;
    }
    const bool hasFixedStep = config.fixedStep != 0;
    const VirtualAdvance advanced =
        AdvanceVirtual(virt, real.delta, control, hasFixedStep ? &fixed.accumulator : nullptr, config.limits);
    active = ActiveTime{virt.delta, virt.elapsed};
    if (hasFixedStep) {
        fixed.pending = advanced.delta;
    }
    if (advanced.discarded > 0) discarded.Send(TimeDiscarded{advanced.discarded, real.delta, advanced.cause, {}});
    return {};
}

SystemResult AdvanceTime(Kernel::ResMut<RealTime> real, Kernel::ResMut<VirtualTime> virt, Kernel::ResMut<FixedTime> fixed,
                         Kernel::ResMut<ActiveTime> active, Kernel::Res<TimeSampling> sampling, Kernel::Res<TimeControl> control,
                         Kernel::Res<TimeConfig> config, Kernel::MessageWriter<TimeDiscarded> discarded) {
    return Advance(*real, *virt, *fixed, *active, *sampling, *control, *config, discarded);
}

// The signature without TimeControl (pausable = false): a type not registered is a startup error (RE4).
SystemResult AdvanceTimeUnpausable(Kernel::ResMut<RealTime> real, Kernel::ResMut<VirtualTime> virt,
                                   Kernel::ResMut<FixedTime> fixed, Kernel::ResMut<ActiveTime> active,
                                   Kernel::Res<TimeSampling> sampling, Kernel::Res<TimeConfig> config,
                                   Kernel::MessageWriter<TimeDiscarded> discarded) {
    return Advance(*real, *virt, *fixed, *active, *sampling, TimeControl{}, *config, discarded);
}

// RunFixedSteps (declaration 3.3): takes the frame's time, adds it to the accumulator (AdvanceVirtual already kept
// the steps within the cap) and runs that many fixed steps. Its own stage (-same-input-same-result).
SystemResult RunFixedSteps(Kernel::ExclusiveWorld<Kernel::Writes<Tick, FixedTime, ActiveTime>> world) {
    const TimeConfig config = Detail::Present(world.Read<TimeConfig>([](const TimeConfig& c) { return c; }));
    const std::uint32_t steps = Detail::Present(world.Modify<FixedTime>([&config](FixedTime& time) {
        const Nanoseconds pending = time.pending;
        time.pending = 0;
        return time.accumulator.Add(pending, config.limits.maxStepsPerFrame);
    }));
    for (std::uint32_t step = 0; step < steps; ++step) {
        if (auto ran = RunOneFixedStep(world, config.fixedSchedule); !ran) return ran;
    }
    return {};
}

Kernel::SystemOptions Mounted(std::string name, const std::string& schedule, const std::string& stage) {
    Kernel::SystemOptions options;
    options.name = std::move(name);
    options.zone = Kernel::Zone::B;
    Kernel::Mount mount;
    mount.schedule = schedule;
    mount.stage = stage;
    options.mounts.push_back(std::move(mount));
    return options;
}

template <class T>
void PutIfAbsent(Kernel::World& world, T value) {
    if (world.Resource<T>() == nullptr) world.SetResource(std::move(value));
}

} // namespace

void WorldTimeModule::Build(Orchestration::WorldSetup& setup) {
    const WorldTimeOptions& options = OptionsOf(setup.Options<WorldTimeOptions>());
    // The fixed-step schedule's name fits TimeConfig (a Programmer Error otherwise).
    GYO_ASSERT(options.fixedSchedule.size() < sizeof(TimeConfig::fixedSchedule));
    Kernel::Registrar& r = setup.Registrar();
    // Every time type is zone B (declaration 4.5): a zone C System reads no Tick until batch 4 (G4).
    r.Resource<RealTime>({.name = "RealTime", .zone = Kernel::Zone::B});
    r.Resource<VirtualTime>({.name = "VirtualTime", .zone = Kernel::Zone::B});
    r.Resource<FixedTime>({.name = "FixedTime", .zone = Kernel::Zone::B});
    r.Resource<ActiveTime>({.name = "ActiveTime", .zone = Kernel::Zone::B});
    r.Resource<Tick>({.name = "Tick", .zone = Kernel::Zone::B});
    r.Resource<TimeSampling>({.name = "TimeSampling", .zone = Kernel::Zone::B});
    r.Resource<Resimulation>({.name = "Resimulation", .zone = Kernel::Zone::B});
    r.Resource<TimeConfig>({.name = "TimeConfig", .zone = Kernel::Zone::B});
    if (options.pausable) r.Resource<TimeControl>({.name = "TimeControl", .zone = Kernel::Zone::B});
    r.Message<TimeDiscarded>({.name = "TimeDiscarded", .zone = Kernel::Zone::B});

    if (options.pausable) {
        r.AddSystem<&AdvanceTime>(Mounted("AdvanceTime", options.frameSchedule, options.advanceStage));
    } else {
        r.AddSystem<&AdvanceTimeUnpausable>(Mounted("AdvanceTime", options.frameSchedule, options.advanceStage));
    }
    if (options.fixedStep) {
        Kernel::SystemOptions fixedSteps = Mounted("RunFixedSteps", options.frameSchedule, options.fixedStepStage);
        fixedSteps.mounts.front().conditions = options.fixedStepConditions;
        r.AddSystem<&RunFixedSteps>(std::move(fixedSteps));
    }

    // The initial values, before the first Run, without overwriting a value carried over (O-7).
    TimeConfig config;
    config.limits = options.limits;
    config.fixedStep = options.fixedStep ? 1 : 0;
    std::memcpy(config.fixedSchedule, options.fixedSchedule.data(), options.fixedSchedule.size());
    const bool pausable = options.pausable;
    const StepLength step = options.step;
    setup.Prepare([config, pausable, step](Kernel::World& world) -> ::Engine::Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent(world, RealTime{});
        PutIfAbsent(world, VirtualTime{});
        // Carried over (O-7): its elapsed time and remainder stay, the step length is this World's setting
        // (the whole steps a longer old step left in the remainder go to VirtualTime::discarded, as a restore's).
        if (FixedTime* carried = world.ResourceMut<FixedTime>()) {
            const Nanoseconds excess = Detail::PutBackFixedTime(*carried, step, carried->elapsed, carried->accumulator.Overstep());
            if (excess > 0) world.ResourceMut<VirtualTime>()->discarded += excess;
        } else {
            world.SetResource(FixedTime{TickAccumulator{step}, 0, 0});
        }
        PutIfAbsent(world, ActiveTime{});
        PutIfAbsent(world, Tick{});
        PutIfAbsent(world, TimeSampling{});
        PutIfAbsent(world, Resimulation{});
        // The settings of this World, always (the step length too, through FixedTime above: a restore or a
        // carry-over keeps FixedTime's elapsed time and remainder only, RestoreFixedTime).
        world.SetResource(config);
        if (pausable) PutIfAbsent(world, TimeControl{});
        return {};
    });
}

void WorldTimeModule::Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) {
    const WorldTimeOptions& options = OptionsOf(context.Options<WorldTimeOptions>());
    // A World without a fixed step runs no fixed-step schedule and needs none.
    if (!options.fixedStep) return;
    const auto schedules = registry.Schedules();
    const bool found = std::any_of(schedules.begin(), schedules.end(), [&options](const Kernel::ScheduleInfo& info) {
        return info.options.name == options.fixedSchedule;
    });
    if (!found) {
        context.Report("WT-E1", "the fixed-step schedule '" + options.fixedSchedule + "' that RunFixedSteps runs is not a schedule of World '" +
                                    context.WorldName() + "'");
    }
}

std::vector<std::string> WorldTimeModule::TickWriters() {
    return {"RunFixedSteps"};
}

} // namespace GYO::Engine::WorldTime
