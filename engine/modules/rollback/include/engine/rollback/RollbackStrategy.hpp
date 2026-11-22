#pragma once

// The rollback strategy interface (rollback declaration 5.3, the contract; the
// user's v8-arch-gyo2-strategy-interface-impl): an abstract base class with
// virtual functions. The official strategy and an author's go through the same
// socket, with no privilege; an author may override some hooks of the official
// one or write all of them.
//
// Each hook takes one context object of its own: what the hook reads and does
// goes through it, so a hook can be given more later without its signature
// changing (authority-rollback declaration 10.1).
//
// A strategy object is chosen per World (RollbackOptions in the World
// description's options) and holds no per-World state: the description is
// reused by ReplaceWorld; a World's state lives in that World's Resources
// (authority-rollback declaration 4.2).

#include "engine/base/Result.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/rollback/Registrations.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <string>
#include <string_view>

namespace GYO::Engine::Rollback {

// What a strategy's startup check reads, and where it reports (after every
// unit's Finish, before the World is built; O-5: with the rollback Module's
// string codes, RB-E...).
class CheckContext {
public:
    CheckContext(const Kernel::FrozenRegistry& registry, const RollbackRegistry& registrations,
                 Orchestration::WorldCheck& check)
        : registry_(&registry), registrations_(&registrations), check_(&check) {}

    [[nodiscard]] const Kernel::FrozenRegistry& Registry() const noexcept { return *registry_; }
    [[nodiscard]] const RollbackRegistry& Registrations() const noexcept { return *registrations_; }
    [[nodiscard]] const std::string& WorldName() const noexcept { return check_->WorldName(); }
    void Report(std::string code, std::string message) { check_->Report(std::move(code), std::move(message)); }

private:
    const Kernel::FrozenRegistry* registry_;
    const RollbackRegistry* registrations_;
    Orchestration::WorldCheck* check_;
};

// The run-time hooks' contexts. Each hook runs inside an exclusive System of the
// rollback Module (ExclusiveWorld<> without Writes<...>: the authority Module's
// checks see no decision data written by it; rollback declaration 5.5 point 9);
// a hook reads and writes the World through World(), its own state in its own
// World Resources (a Read or Modify closure returns nothing that points into the
// World, so a context holds no pointer into it).

// After each step of the step schedule: Tick() is the step just run.
class RecordContext {
public:
    RecordContext(Exclusive& world, std::uint64_t tick) : world_(&world), tick_(tick) {}
    [[nodiscard]] Exclusive& World() noexcept { return *world_; }
    [[nodiscard]] std::uint64_t Tick() const noexcept { return tick_; }

private:
    Exclusive* world_;
    std::uint64_t tick_;
};

// Before the step schedule each frame: Tick() is the last step run. RollBackTo
// decides to go back to that step (at most Tick()); Abandon tells the World a
// requested rollback is given up (beyond the depth: rollback declaration 5.5
// point 4), which the rollback Module sends as RollbackAbandoned.
class DecideContext {
public:
    DecideContext(Exclusive& world, std::uint64_t tick) : world_(&world), tick_(tick) {}
    [[nodiscard]] Exclusive& World() noexcept { return *world_; }
    [[nodiscard]] std::uint64_t Tick() const noexcept { return tick_; }
    void RollBackTo(std::uint64_t step) {
        GYO_ASSERT(step <= tick_ && "a rollback goes back, to a step already run");
        target_ = step;
    }
    [[nodiscard]] std::optional<std::uint64_t> Target() const noexcept { return target_; }
    void Abandon(std::uint64_t requested, std::uint64_t depth) { abandoned_.push_back({requested, depth}); }
    [[nodiscard]] const std::vector<std::pair<std::uint64_t, std::uint64_t>>& Abandoned() const noexcept { return abandoned_; }

private:
    Exclusive* world_;
    std::uint64_t tick_;
    std::optional<std::uint64_t> target_;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> abandoned_;
};

// After deciding: put the World back to step Target(); Tick() is still the last step run.
class RestoreContext {
public:
    RestoreContext(Exclusive& world, std::uint64_t target, std::uint64_t tick) : world_(&world), target_(target), tick_(tick) {}
    [[nodiscard]] Exclusive& World() noexcept { return *world_; }
    [[nodiscard]] std::uint64_t Target() const noexcept { return target_; }
    [[nodiscard]] std::uint64_t Tick() const noexcept { return tick_; }

private:
    Exclusive* world_;
    std::uint64_t target_;
    std::uint64_t tick_;
};

// After restoring: run StepSchedule() again from Target() up to Tick().
class ResimulateContext {
public:
    ResimulateContext(Exclusive& world, std::uint64_t target, std::uint64_t tick, std::string_view stepSchedule)
        : world_(&world), target_(target), tick_(tick), stepSchedule_(stepSchedule) {}
    [[nodiscard]] Exclusive& World() noexcept { return *world_; }
    [[nodiscard]] std::uint64_t Target() const noexcept { return target_; }
    [[nodiscard]] std::uint64_t Tick() const noexcept { return tick_; }
    [[nodiscard]] std::string_view StepSchedule() const noexcept { return stepSchedule_; }

private:
    Exclusive* world_;
    std::uint64_t target_;
    std::uint64_t tick_;
    std::string_view stepSchedule_;
};

// Each frame, on the view side.
class CorrectContext {
public:
    explicit CorrectContext(Exclusive& world) : world_(&world) {}
    [[nodiscard]] Exclusive& World() noexcept { return *world_; }

private:
    Exclusive* world_;
};

class RollbackStrategy {
public:
    virtual ~RollbackStrategy() = default;

    // Its name, for reports and for the authority Module's fingerprint.
    [[nodiscard]] virtual std::string_view Name() const = 0;
    // Startup: reads the read-only registry and the rollback registrations; reports what cannot hold.
    virtual void Check(CheckContext& context) = 0;
    // After each step of a step schedule: keeps what the step left.
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Record(RecordContext& context) = 0;
    // Before the step schedule each frame: reads what came from outside and decides whether, and to which step,
    // to go back; deciding to, it raises "resimulating".
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Decide(DecideContext& context) = 0;
    // After deciding: puts the World back to that step (keeping first what a correction needs).
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Restore(RestoreContext& context) = 0;
    // After restoring: runs the step schedule again up to now, then lowers "resimulating".
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Resimulate(ResimulateContext& context) = 0;
    // Each frame, on the view side: what the frame adds to hide the difference a resimulation made.
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Correct(CorrectContext& context) = 0;
    // The steps of a recorded input kept when its length is InputHistoryLength::StrategyDepth (a resimulation
    // writes back at most this many). The official strategy: its depth. An author's strategy that keeps its own
    // inputs may leave it at 0 (the official history then keeps none).
    [[nodiscard]] virtual std::uint32_t DefaultInputLength() const noexcept { return 0; }

protected:
    RollbackStrategy() = default;
};

// The rollback Module's setting in one World (WorldDescription::options). No
// strategy: the World does not roll back, and that is no error (a Match); the
// registrations are still made and readable (rollback declaration 5.5 point 10).
struct RollbackOptions {
    std::shared_ptr<RollbackStrategy> strategy;
    // Where the rollback Module's Systems of each frame are mounted (the default skeleton's names): decide, restore
    // and resimulate in decideStage, before the fixed step (authority-rollback declaration 5.2: lightyear's
    // PreUpdate); the correction in correctStage, chosen by the author (rollback declaration 5.5 point 7).
    // Empty: the time Module's frame schedule (WorldTimeOptions::frameSchedule; the default skeleton's without it).
    std::string frameSchedule;
    std::string decideStage = std::string(Orchestration::DefaultSchedules::preUpdate);
    std::string correctStage = std::string(Orchestration::DefaultSchedules::postUpdate);
};

// The peer-to-peer rollback strategy (a fighting game; named by the user,
// placeholder only, rollback declaration 5.6): its startup check reports that
// it is not implemented (RB-E6), so a World never runs with it.
class PeerRollbackStrategy final : public RollbackStrategy {
public:
    [[nodiscard]] std::string_view Name() const override { return "PeerRollbackStrategy"; }
    void Check(CheckContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Record(RecordContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Decide(DecideContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Restore(RestoreContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Resimulate(ResimulateContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Correct(CorrectContext& context) override;
};

} // namespace GYO::Engine::Rollback
