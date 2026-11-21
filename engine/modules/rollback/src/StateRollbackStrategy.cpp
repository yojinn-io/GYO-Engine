#include "engine/rollback/StateRollbackStrategy.hpp"

#include "engine/rollback/History.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/world_time/FixedStep.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <optional>
#include <utility>

namespace GYO::Engine::Rollback {

namespace {

using Ok = ::Engine::Base::Result<void, Kernel::KernelError>;

Kernel::KernelError Failed(std::string message) {
    return Kernel::KernelError(Kernel::KernelErrorCode::SystemFailed, std::move(message));
}

} // namespace

void StateRollbackStrategy::Check(CheckContext&) {
    // The startup checks of the official strategy (Derived order, messages across the step schedules): later in
    // step 4 of the batch.
}

Ok StateRollbackStrategy::Record(RecordContext& context) {
    Exclusive& world = context.World();
    const std::uint64_t tick = context.Tick();
    const std::uint64_t depth = settings_.depth;
    auto done = world.Modify<RollbackHistory>([&](RollbackHistory& history) {
        const Kernel::ChangeStamp since = history.lastStamp;
        for (auto& column : history.columns) column->Record(world, since, tick);
        for (auto& input : history.inputs) input->Record(world, tick);
        while (!history.alive.empty() && history.alive.back().first >= tick) history.alive.pop_back();
        history.alive.emplace_back(tick, world.Entities());
        history.lastStamp = world.Stamp();
        history.lastTick = tick;
        if (tick > depth) {
            const std::uint64_t oldest = tick - depth;
            for (auto& column : history.columns) column->Prune(oldest);
            while (history.alive.size() >= 2 && history.alive[1].first <= oldest) history.alive.pop_front();
        }
    });
    if (!done) return ::Engine::Base::Err(std::move(done).error());
    if (!*done) return ::Engine::Base::Err(Failed("StateRollbackStrategy::Record: the World has no RollbackHistory"));
    return {};
}

Ok StateRollbackStrategy::Decide(DecideContext& context) {
    Exclusive& world = context.World();
    const std::uint64_t tick = context.Tick();
    const auto confirmed = world.Read<RollbackInbox>([](const RollbackInbox& inbox) { return inbox.Confirmed(); });
    if (!confirmed) return ::Engine::Base::Err(std::move(confirmed).error());
    if (!*confirmed || !**confirmed) return {};
    const std::uint64_t point = ***confirmed;
    if (point > tick) return {};  // the authority is ahead: wait until this World has run that step
    std::optional<bool> rollBack;
    auto decided = world.Modify<RollbackHistory>([&](RollbackHistory& history) {
        if (history.decided && point <= *history.decided) return;
        history.decided = point;
        if (settings_.mode == RollbackMode::Disabled) return;
        if (tick - point > settings_.depth || history.AliveAt(point) == nullptr) {
            context.Abandon(point, settings_.depth);
            return;
        }
        if (settings_.mode == RollbackMode::Always) {
            rollBack = true;
            return;
        }
        bool differs = false;
        static_cast<void>(world.Read<RollbackInbox>([&](const RollbackInbox& inbox) {
            for (const auto& column : history.columns) {
                if (const InboxColumnBase* values = inbox.ValuesOf(column->Key())) {
                    differs = differs || column->Differs(point, *values);
                }
            }
        }));
        rollBack = differs;
    });
    if (!decided) return ::Engine::Base::Err(std::move(decided).error());
    if (rollBack && *rollBack) {
        context.RollBackTo(point);
        // Deciding to go back raises "resimulating" (rollback declaration 5.3; lightyear raises it at the decision):
        // the hooks the restore runs, and the observers they trigger, see it raised.
        auto raised = world.Modify<WorldTime::Resimulation>([](WorldTime::Resimulation& r) { r.active = 1; });
        if (!raised) return ::Engine::Base::Err(std::move(raised).error());
    }
    return {};
}

Ok StateRollbackStrategy::Restore(RestoreContext& context) {
    Exclusive& world = context.World();
    const std::uint64_t target = context.Target();
    auto built = world.Modify<RollbackHistory>([&](RollbackHistory& history) -> std::optional<Kernel::RestoreSet> {
        const std::vector<Kernel::EntityId>* alive = history.AliveAt(target);
        if (alive == nullptr) return std::nullopt;
        Kernel::RestoreSet set;
        set.Alive(*alive);
        static_cast<void>(world.Read<RollbackInbox>([&](const RollbackInbox& inbox) {
            for (const auto& column : history.columns) {
                const InboxColumnBase* values = inbox.ValuesOf(column->Key());
                column->AddTo(set, target, *alive, values);
                // The history at target agrees with the authority afterwards.
                if (values != nullptr) column->Adopt(target, *values);
            }
        }));
        return set;
    });
    if (!built) return ::Engine::Base::Err(std::move(built).error());
    if (!*built || !**built) return ::Engine::Base::Err(Failed("StateRollbackStrategy::Restore: step " + std::to_string(target) + " is not in the history"));
    return world.Restore(std::move(***built));
}

Ok StateRollbackStrategy::Resimulate(ResimulateContext& context) {
    Exclusive& world = context.World();
    const std::uint64_t target = context.Target();
    const std::uint64_t steps = context.Tick() - target;
    WorldTime::RewindFixedSteps(world, steps);
    for (std::uint64_t step = target + 1; step <= context.Tick(); ++step) {
        // The inputs of that step, written back (their entries are skipped while resimulating).
        auto written = world.Read<RollbackHistory>([&](const RollbackHistory& history) {
            for (const auto& input : history.inputs) input->WriteBack(world, step);
        });
        if (!written) return ::Engine::Base::Err(std::move(written).error());
        if (auto ran = WorldTime::RunOneFixedStep(world, context.StepSchedule()); !ran) return ran;
    }
    auto lowered = world.Modify<WorldTime::Resimulation>([](WorldTime::Resimulation& r) { r.active = 0; });
    if (!lowered) return ::Engine::Base::Err(std::move(lowered).error());
    auto pruned = world.Modify<RollbackInbox>([target](RollbackInbox& inbox) { inbox.Prune(target); });
    if (!pruned) return ::Engine::Base::Err(std::move(pruned).error());
    return {};
}

Ok StateRollbackStrategy::Correct(CorrectContext&) { return {}; }

} // namespace GYO::Engine::Rollback
