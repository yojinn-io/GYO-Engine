#pragma once

// The time Module (time-events declaration docs/architecture/target/v2/time-events-declaration.zh-Hant.md,
// section 3; Design Invariants in docs/architecture/target/v2/world-time.zh-Hant.md): the registration unit that
// puts game time into every World it is built into. It declares no schedule: it mounts AdvanceTime in the frame
// schedule's first stage and RunFixedSteps in its fixed-main-loop stage of the orchestration layer's default
// skeleton (ScheduleSkeleton.hpp), and RunFixedSteps runs the fixed-step schedule nested. The kernel and the
// orchestration layer know nothing of it (WT-4).
//
// Its duties (WT-6): (1) register the time Resources in every World it is built into; (2) sample (TimeSampling;
// moves to the runner with the exchange point); (3) advance real and virtual time (pause, scale, caps, the
// discarded record); (4) accumulate and drive the fixed step, Tick, rewind k steps (FixedStep.hpp); (5) switch
// ActiveTime; (6) register Resimulation with its initial value; (7) send TimeDiscarded.

#include "engine/kernel/Registry.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/world_time/FixedStep.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <string>
#include <vector>

namespace GYO::Engine::WorldTime {

// The settings of the time Module in one World (WorldDescription::options; none given: these defaults). Read when
// the World is built: the step length never changes while the World runs (WT-3).
struct WorldTimeOptions final {
    StepLength step{};
    TimeLimits limits{};
    // false: TimeControl is not registered and nothing can pause or scale time (a Match).
    bool pausable = true;
    // Where the two Systems are mounted and what RunFixedSteps runs: the default skeleton's names. An author who
    // turns the skeleton off names its own; a missing mount target is the kernel's D-E1, a missing fixed-step
    // schedule is reported by Check (WT-E1). The fixed-step schedule name is at most 63 bytes (TimeConfig).
    std::string frameSchedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string advanceStage = std::string(Orchestration::DefaultSchedules::first);
    std::string fixedStepStage = std::string(Orchestration::DefaultSchedules::fixedMainLoop);
    std::string fixedSchedule = std::string(Orchestration::DefaultSchedules::fixed);
    // Run conditions of RunFixedSteps chosen by the author (a frame they skip leaves its time untaken: the next
    // AdvanceTime discards it, cause NotDriven).
    std::vector<Kernel::ConditionRef> fixedStepConditions;
};

class WorldTimeModule final : public Orchestration::Module {
public:
    WorldTimeModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
    void Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) override;

    // The Systems that drive Tick (L27; authority strategy, batch 4). A clock-sync System joins with the Client batch.
    [[nodiscard]] static std::vector<std::string> TickWriters();
};

} // namespace GYO::Engine::WorldTime
