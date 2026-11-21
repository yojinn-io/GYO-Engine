#include "engine/rollback/StateRollbackStrategy.hpp"

#include "engine/rollback/History.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/world_time/FixedStep.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <utility>

namespace GYO::Engine::Rollback {

namespace {

using Ok = ::Engine::Base::Result<void, Kernel::KernelError>;

Kernel::KernelError Failed(std::string message) {
    return Kernel::KernelError(Kernel::KernelErrorCode::SystemFailed, std::move(message));
}

} // namespace

namespace {

// The schedules that run again while resimulating: the step schedules and the schedules nested in them.
std::set<std::string> Resimulated(const RollbackRegistry& registrations) {
    std::set<std::string> names;
    for (const StepScheduleRecord& record : registrations.stepSchedules) names.insert(record.name);
    for (const NestedScheduleRecord& record : registrations.nestedSchedules) names.insert(record.name);
    return names;
}

// R-7 (replaces the kernel's D-E16): in each resimulated schedule, every System that reads a Derived component
// has a System of the same schedule that writes it immediately ordered before it (a path of stage, declared or
// sync point edges of the read-only order graph; a tie by registration is no declared order). Hooks and
// observers that read Derived data are not seen at startup (the author's).
void CheckDerivedOrder(CheckContext& context, const std::set<std::string>& schedules) {
    const Kernel::FrozenRegistry& registry = context.Registry();
    std::vector<Kernel::ComponentId> derived;
    for (const DerivedRecord& record : context.Registrations().derived) {
        if (const auto id = record.idIn(registry)) derived.push_back(*id);
    }
    if (derived.empty()) return;
    for (const std::string& schedule : schedules) {
        std::map<std::string, std::set<std::string>> after;  // System -> the Systems ordered after it
        for (const Kernel::OrderEdge& edge : registry.OrderGraph(schedule)) {
            if (edge.source == Kernel::OrderSource::TieBreak) continue;
            after[edge.first].insert(edge.second);
        }
        const auto reaches = [&after](const std::string& from, const std::string& to) {
            std::vector<std::string> pending{from};
            std::set<std::string> seen{from};
            while (!pending.empty()) {
                const std::string at = pending.back();
                pending.pop_back();
                for (const std::string& next : after[at]) {
                    if (next == to) return true;
                    if (seen.insert(next).second) pending.push_back(next);
                }
            }
            return false;
        };
        for (const Kernel::SystemInfo& reader : registry.Systems()) {
            if (reader.removed) continue;
            const bool mounted = std::any_of(reader.mounts.begin(), reader.mounts.end(),
                                             [&schedule](const Kernel::Mount& m) { return m.schedule == schedule; });
            if (!mounted) continue;
            for (const Kernel::ComponentId id : derived) {
                const Kernel::AccessKey key{Kernel::AccessKind::Component, id};
                if (!std::binary_search(reader.access.reads.begin(), reader.access.reads.end(), key)) continue;
                bool written = false;
                for (const Kernel::SystemInfo& writer : registry.Systems()) {
                    if (writer.removed || writer.name == reader.name) continue;
                    if (!std::binary_search(writer.access.writes.begin(), writer.access.writes.end(), key)) continue;
                    if (reaches(writer.name, reader.name)) {
                        written = true;
                        break;
                    }
                }
                if (!written) {
                    context.Report("RB-E7", "System '" + reader.name + "' reads Derived '" + registry.Components()[id].name +
                                                "' in resimulated schedule '" + schedule +
                                                "' with no immediate writer ordered before it (R-7)");
                }
            }
        }
    }
}

// time-plan-two (5), both directions (rollback declaration 7.1): a message written in a resimulated schedule has
// no reader outside them, and one read in them has no writer outside them. Only the Systems' mounts are looked
// at; what hooks and observers read and write is not seen at startup (the author's).
void CheckMessages(CheckContext& context, const std::set<std::string>& schedules) {
    const Kernel::FrozenRegistry& registry = context.Registry();
    for (const Kernel::MessageInfo& message : registry.Messages()) {
        bool writtenIn = false;
        bool readIn = false;
        for (const Kernel::MessageWriterInfo& writer : message.writers) {
            if (!writer.hook && !writer.observer && schedules.count(writer.schedule) != 0) writtenIn = true;
        }
        for (const Kernel::MessageReaderInfo& reader : message.readers) {
            if (!reader.observer && schedules.count(reader.schedule) != 0) readIn = true;
        }
        if (writtenIn) {
            for (const Kernel::MessageReaderInfo& reader : message.readers) {
                if (reader.observer || schedules.count(reader.schedule) != 0) continue;
                context.Report("RB-E8", "message '" + message.name + "' is written in a resimulated schedule and read by '" +
                                            reader.system + "' in '" + reader.schedule +
                                            "', outside them (it would read the resimulation's records again)");
            }
        }
        if (readIn) {
            for (const Kernel::MessageWriterInfo& writer : message.writers) {
                if (writer.hook || writer.observer || schedules.count(writer.schedule) != 0) continue;
                context.Report("RB-E8", "message '" + message.name + "' is read in a resimulated schedule and written by '" +
                                            writer.system + "' in '" + writer.schedule +
                                            "', outside them (a resimulation would not have it again)");
            }
        }
    }
}

} // namespace

void StateRollbackStrategy::Check(CheckContext& context) {
    const std::set<std::string> schedules = Resimulated(context.Registrations());
    CheckDerivedOrder(context, schedules);
    CheckMessages(context, schedules);
    // A rollback of depth steps writes back the inputs of depth steps: an input history that keeps fewer would
    // resimulate those steps with whatever the input holds, silently (review of step 4, 1-6).
    for (const InputRecord& input : context.Registrations().inputs) {
        if (input.capacity >= settings_.depth) continue;
        context.Report("RB-E9", "rollback.Input: the input history keeps " + std::to_string(input.capacity) +
                                    " steps, fewer than the official strategy's depth " + std::to_string(settings_.depth));
    }
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
    bool fresh = false;
    auto decided = world.Modify<RollbackHistory>([&](RollbackHistory& history) {
        if (history.decided && point <= *history.decided) return;
        history.decided = point;
        fresh = true;
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
        const std::vector<Kernel::EntityId>& alive = *history.AliveAt(point);
        static_cast<void>(world.Read<RollbackInbox>([&](const RollbackInbox& inbox) {
            for (const auto& column : history.columns) {
                if (const InboxColumnBase* values = inbox.ValuesOf(column->Key())) {
                    differs = differs || column->Differs(point, *values, alive);
                }
            }
        }));
        rollBack = differs;
    });
    if (!decided) return ::Engine::Base::Err(std::move(decided).error());
    if (fresh && !(rollBack && *rollBack)) {
        // The confirmed point moved and no rollback follows (it agrees, it was given up, or the mode is Check
        // or Disabled): what is older than it is not needed for a later comparison, except per entity the last
        // value at or before it (review of step 4, 1-3: the inbox was pruned only after a rollback).
        auto pruned = world.Modify<RollbackInbox>([point](RollbackInbox& inbox) { inbox.Prune(point); });
        if (!pruned) return ::Engine::Base::Err(std::move(pruned).error());
    }
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
                if (values != nullptr) column->Adopt(target, *values, *alive);
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
    // The input holds what its entry wrote this frame; it is put back after the steps run again.
    auto kept = world.Modify<RollbackHistory>([&](RollbackHistory& history) {
        for (auto& input : history.inputs) input->Keep(world);
    });
    if (!kept) return ::Engine::Base::Err(std::move(kept).error());
    WorldTime::RewindFixedSteps(world, steps);
    for (std::uint64_t step = target + 1; step <= context.Tick(); ++step) {
        // The inputs of that step, written back (their entries are skipped while resimulating).
        auto written = world.Read<RollbackHistory>([&](const RollbackHistory& history) {
            for (const auto& input : history.inputs) input->WriteBack(world, step);
        });
        if (!written) return ::Engine::Base::Err(std::move(written).error());
        if (auto ran = WorldTime::RunOneFixedStep(world, context.StepSchedule()); !ran) return ran;
    }
    auto putBack = world.Modify<RollbackHistory>([&](RollbackHistory& history) {
        for (auto& input : history.inputs) input->PutBack(world);
    });
    if (!putBack) return ::Engine::Base::Err(std::move(putBack).error());
    auto lowered = world.Modify<WorldTime::Resimulation>([](WorldTime::Resimulation& r) { r.active = 0; });
    if (!lowered) return ::Engine::Base::Err(std::move(lowered).error());
    auto pruned = world.Modify<RollbackInbox>([target](RollbackInbox& inbox) { inbox.Prune(target); });
    if (!pruned) return ::Engine::Base::Err(std::move(pruned).error());
    return {};
}

Ok StateRollbackStrategy::Correct(CorrectContext&) { return {}; }

} // namespace GYO::Engine::Rollback
