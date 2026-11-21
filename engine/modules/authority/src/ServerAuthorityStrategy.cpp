#include "engine/authority/ServerAuthorityStrategy.hpp"

#include "engine/authority/Consistency.hpp"
#include "engine/base/Sha256.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace GYO::Engine::Authority {

namespace {

std::string NameOf(const Kernel::FrozenRegistry& registry, const Kernel::AccessKey& key) {
    switch (key.kind) {
    case Kernel::AccessKind::Component: return registry.Components()[key.id].name;
    case Kernel::AccessKind::Resource: return registry.Resources()[key.id].name;
    case Kernel::AccessKind::Message: return registry.Messages()[key.id].name;
    }
    return {};
}

bool HasSchedule(const Kernel::FrozenRegistry& registry, const std::string& name) {
    const auto schedules = registry.Schedules();
    return std::any_of(schedules.begin(), schedules.end(),
                       [&name](const Kernel::ScheduleInfo& info) { return info.options.name == name; });
}

bool HasResource(const Kernel::FrozenRegistry& registry, const std::string& name) {
    const auto resources = registry.Resources();
    return std::any_of(resources.begin(), resources.end(), [&name](const Kernel::ResourceInfo& info) { return info.name == name; });
}

// A System of that name that runs (a tombstone left by RemoveSystem does not).
const Kernel::SystemInfo* LiveSystem(const Kernel::FrozenRegistry& registry, const std::string& name) {
    const Kernel::SystemInfo* system = registry.SystemNamed(name);
    return system != nullptr && !system->removed ? system : nullptr;
}

bool HasHookOrObserver(const Kernel::FrozenRegistry& registry, const std::string& name) {
    const auto hooks = registry.Hooks();
    const auto observers = registry.Observers();
    return std::any_of(hooks.begin(), hooks.end(), [&name](const Kernel::HookInfo& h) { return h.name == name; }) ||
           std::any_of(observers.begin(), observers.end(), [&name](const Kernel::ObserverInfo& o) { return o.name == name; });
}

bool MountedIn(const Kernel::SystemInfo& system, const std::set<std::string>& schedules) {
    return std::any_of(system.mounts.begin(), system.mounts.end(),
                       [&schedules](const Kernel::Mount& mount) { return schedules.count(mount.schedule) != 0; });
}

// The keys of the rollback Module's resolved registrations: what may roll back, and what is Derived.
struct RollbackKeys {
    std::set<Kernel::AccessKey> rolls;
    std::set<Kernel::AccessKey> derived;
};
RollbackKeys RollbackKeysOf(const Kernel::FrozenRegistry& registry, const Rollback::RollbackRegistry& rollback) {
    RollbackKeys out;
    for (const Rollback::DataRecord& record : rollback.data) {
        if (const auto id = record.ops->idIn(registry)) {
            out.rolls.insert({record.kind == Rollback::DataKind::Component ? Kernel::AccessKind::Component
                                                                           : Kernel::AccessKind::Resource,
                              *id});
        }
    }
    for (const Rollback::DerivedRecord& record : rollback.derived) {
        if (const auto id = record.idIn(registry)) out.derived.insert({Kernel::AccessKind::Component, *id});
    }
    return out;
}

} // namespace

void ServerAuthorityStrategy::OnRegister(RegisterContext& context) {
    if (context.Data() != nullptr) {
        context.RegisterInKernel();
        // The rollback registration is made whether or not the World has a rollback strategy (authority
        // declaration 4.3: it takes no effect without one, as on a Match).
        context.RegisterInRollback();
        return;
    }
    if (context.Ingress() != nullptr) {
        // The run condition goes on before AddSystem (a mount's condition cannot be added later).
        context.Rollback().SkipDuringResimulation(context.IngressOptions());
        context.AddIngress();
    }
    // Marks, Runs and OneEndOnly register nothing: they are read at Check and in the fingerprint.
}

void ServerAuthorityStrategy::Check(CheckContext& context) {
    const Kernel::FrozenRegistry& registry = context.Registry();
    const AuthorityRegistrations& table = context.Registrations();
    const Rollback::RollbackRegistry& rollback = context.RollbackRegistrations();

    // AU-E3: the decision part does not change after startup, so the consistency fingerprint is made once (L5).
    if (context.World().DynamicInsert()) {
        context.Report("AU-E3", "ServerAuthorityStrategy: World '" + context.World().WorldName() +
                                    "' allows dynamic insertion (WorldDescription::dynamicInsert must be false)");
    }

    // AU-E4: every name and marked type resolves in the read-only registry (DI-A 10).
    for (const MarkRecord& mark : table.Marks()) {
        const bool found = mark.ops != nullptr ? mark.ops->idIn(registry).has_value() : HasResource(registry, mark.name);
        if (!found) {
            context.Report("AU-E4", mark.ops != nullptr ? std::string("authority.Mark: the marked ") +
                                                              (mark.kind == DataKind::Component ? "component" : "resource") +
                                                              " type is not registered in this World"
                                                        : "authority.MarkResource: no resource named '" + mark.name + "' in this World");
        }
    }
    for (const IngressRecord& entry : table.Ingress()) {
        if (LiveSystem(registry, entry.system) == nullptr) {
            context.Report("AU-E4", "authority.Ingress: System '" + entry.system + "' was not added (OnRegister must add it)");
        }
    }
    for (const RunsRecord& runs : table.Runs()) {
        if (LiveSystem(registry, runs.system) == nullptr) {
            context.Report("AU-E4", "authority.Runs: no System named '" + runs.system + "' in this World");
        }
        for (const std::string& schedule : runs.schedules) {
            if (!HasSchedule(registry, schedule)) {
                context.Report("AU-E4", "authority.Runs '" + runs.system + "': no schedule named '" + schedule + "' in this World");
            }
        }
    }
    for (const OneEndRecord& one : table.OneEnd()) {
        // A System, a hook or an observer (authority 4.3 names Systems; hooks and observers that write decision data
        // at one end only are taken the same way; implementation).
        if (LiveSystem(registry, one.system) == nullptr && !HasHookOrObserver(registry, one.system)) {
            context.Report("AU-E4", "authority.OneEndOnly: no System, hook or observer named '" + one.system + "' in this World");
        }
    }

    // AU-E5: the tick has one source, the time Module's drivers (authority declaration 5.4).
    std::optional<Kernel::AccessKey> tick;
    if (const auto id = registry.ResourceIdOf<WorldTime::Tick>()) tick = Kernel::AccessKey{Kernel::AccessKind::Resource, *id};
    if (tick) {
        const std::vector<std::string> allowed = WorldTime::WorldTimeModule::TickWriters();
        const auto writesTick = [&](const Kernel::AccessSet& access) {
            const std::vector<Kernel::AccessKey> written = WrittenKeys(registry, access);
            return std::binary_search(written.begin(), written.end(), *tick);
        };
        for (const Kernel::SystemInfo& system : registry.Systems()) {
            if (system.removed || !writesTick(system.access)) continue;
            if (std::find(allowed.begin(), allowed.end(), system.name) != allowed.end()) continue;
            context.Report("AU-E5", "System '" + system.name + "' writes Tick; only the time Module's drivers may (" +
                                        system.source + ")");
        }
        for (const Kernel::HookInfo& hook : registry.Hooks()) {
            if (writesTick(hook.access)) {
                context.Report("AU-E5", "hook '" + hook.name + "' writes Tick; only the time Module's drivers may (" + hook.source + ")");
            }
        }
        for (const Kernel::ObserverInfo& observer : registry.Observers()) {
            if (writesTick(observer.access)) {
                context.Report("AU-E5", "observer '" + observer.name + "' writes Tick; only the time Module's drivers may (" +
                                            observer.source + ")");
            }
        }
    }

    // Decision data × rollback (authority declaration 5.11): read through the rollback Module's public registrations.
    std::set<std::string> resimulated;
    for (const Rollback::StepScheduleRecord& record : rollback.stepSchedules) resimulated.insert(record.name);
    for (const Rollback::NestedScheduleRecord& record : rollback.nestedSchedules) resimulated.insert(record.name);
    const DecisionKeys decision = DecisionKeysOf(registry, table.Data(), table.Marks());
    const RollbackKeys rolls = RollbackKeysOf(registry, rollback);
    // A decision input whose history the rollback strategy keeps (rollback.Input<T>): written back before each step
    // run again, so a step that writes it (consumes it) does not need it to roll back (it may not: Z8).
    std::set<Kernel::AccessKey> inputsKept;
    for (const Rollback::InputRecord& record : rollback.inputs) {
        if (const auto id = record.idIn(registry)) inputsKept.insert({Kernel::AccessKind::Resource, *id});
    }
    // The decision data a writer's check looks at: not a message (messages are not in a rollback snapshot), not
    // the tick (a decision datum that does not roll back: a resimulation moves it back itself), not an input with
    // a kept history.
    const auto judged = [&](const Kernel::AccessKey& key) {
        return key.kind != Kernel::AccessKind::Message && decision.Has(key) && !(tick && key == *tick) &&
               inputsKept.count(key) == 0;
    };
    // AU-E6 (Z6): Systems mounted in a resimulated schedule that run again while resimulating (one skipped while
    // resimulating, an entry, does not write during a resimulation).
    for (const Kernel::SystemInfo& system : registry.Systems()) {
        if (system.removed || !MountedIn(system, resimulated) || rollback.Skipped(system.name)) continue;
        for (const Kernel::AccessKey& key : WrittenKeys(registry, system.access)) {
            if (!judged(key) || rolls.rolls.count(key) != 0 || rolls.derived.count(key) != 0) continue;
            context.Report("AU-E6", "System '" + system.name + "' runs in a resimulated schedule and writes decision data '" +
                                        NameOf(registry, key) + "', which neither rolls back nor is Derived (Z6)");
        }
    }
    // Hooks and observers, whoever triggers them: in a World with a step schedule, what they write rolls back
    // (not Derived).
    if (!rollback.stepSchedules.empty()) {
        const auto checkWriter = [&](const std::string& what, const Kernel::AccessSet& access) {
            for (const Kernel::AccessKey& key : WrittenKeys(registry, access)) {
                if (!judged(key) || rolls.rolls.count(key) != 0) continue;
                context.Report("AU-E6", what + " writes decision data '" + NameOf(registry, key) +
                                            "', which does not roll back, in a World with a step schedule (Z6)");
            }
        };
        for (const Kernel::HookInfo& hook : registry.Hooks()) checkWriter("hook '" + hook.name + "'", hook.access);
        for (const Kernel::ObserverInfo& observer : registry.Observers()) checkWriter("observer '" + observer.name + "'", observer.access);
    }
    // AU-E7 (Z8): a decision input is not rollback state (rollback.Input<T> is not a violation).
    for (const DataRecord& record : table.Data()) {
        if (!record.options.input) continue;
        const auto id = record.ops->idIn(registry);
        if (id && rolls.rolls.count({record.kind == DataKind::Component ? Kernel::AccessKind::Component : Kernel::AccessKind::Resource, *id}) != 0) {
            context.Report("AU-E7", "decision input '" + record.name + "' is registered in the rollback Module as rollback state (Z8)");
        }
    }
    // AU-E8 (D-E15): an entry in a resimulated schedule is skipped while resimulating.
    for (const IngressRecord& entry : table.Ingress()) {
        const Kernel::SystemInfo* system = LiveSystem(registry, entry.system);
        if (system == nullptr || !MountedIn(*system, resimulated) || rollback.Skipped(entry.system)) continue;
        context.Report("AU-E8", "entry System '" + entry.system + "' runs in a resimulated schedule and is not skipped "
                                                                  "while resimulating (D-E15)");
    }
    // AU-E9: a Runs System in a resimulated schedule runs only schedules registered as nested (authority 5.11).
    std::set<std::string> nested;
    for (const Rollback::NestedScheduleRecord& record : rollback.nestedSchedules) nested.insert(record.name);
    for (const RunsRecord& runs : table.Runs()) {
        const Kernel::SystemInfo* system = LiveSystem(registry, runs.system);
        if (system == nullptr || !MountedIn(*system, resimulated)) continue;
        for (const std::string& schedule : runs.schedules) {
            if (nested.count(schedule) != 0) continue;
            context.Report("AU-E9", "System '" + runs.system + "' runs in a resimulated schedule and runs schedule '" + schedule +
                                        "', which is not registered as nested (rollback.NestedSchedule)");
        }
    }
}

namespace {

// The bytes of the consistency fingerprint. Strings carry their length (Hasher's std::string).
class Bytes {
public:
    void Str(const std::string& s) { HashValue(h_, s); }
    void U64(std::uint64_t v) { h_.U64(v); }
    void Flag(bool b) { h_.U64(b ? 1 : 0); }
    [[nodiscard]] Kernel::Fingerprint Finish(std::uint32_t version) const {
        return Kernel::Fingerprint{version, ::Engine::Base::Sha256(h_.Data())};
    }

private:
    Kernel::Hasher h_;
};

constexpr std::uint32_t kConsistencyVersion = 1;

// An access set by names (never kernel ids: a Client registers more ordinary data, L10), each list sorted.
// leaveOut: a read left out (the rollback's condition's read of Resimulation, at one end only).
void AccessByName(Bytes& out, const Kernel::FrozenRegistry& registry, const Kernel::AccessSet& access,
                  std::optional<Kernel::AccessKey> leaveOut = std::nullopt) {
    const auto names = [&registry](const std::vector<Kernel::AccessKey>& keys) {
        std::vector<std::string> out;
        for (const Kernel::AccessKey& key : keys) out.push_back(std::to_string(static_cast<int>(key.kind)) + NameOf(registry, key));
        std::sort(out.begin(), out.end());
        return out;
    };
    const auto components = [&registry](const std::vector<Kernel::ComponentId>& ids) {
        std::vector<std::string> out;
        for (const Kernel::ComponentId id : ids) out.push_back(registry.Components()[id].name);
        std::sort(out.begin(), out.end());
        return out;
    };
    std::vector<Kernel::AccessKey> reads = access.reads;
    if (leaveOut) reads.erase(std::remove(reads.begin(), reads.end(), *leaveOut), reads.end());
    for (const auto& list : {names(reads), names(access.writes), components(access.deferredWrites),
                             names(access.annotatedWrites), components(access.membershipReads)}) {
        out.U64(list.size());
        for (const std::string& name : list) out.Str(name);
    }
    out.Flag(access.exclusive);
    out.Flag(access.spawn);
    out.Flag(access.despawn);
}

} // namespace

Kernel::Fingerprint ServerAuthorityStrategy::ConsistencyFingerprint(FingerprintContext& context) {
    const Kernel::FrozenRegistry& registry = context.Registry();
    const AuthorityRegistry& registrations = context.Registrations();
    Bytes out;
    out.Str("gyo.authority.consistency");
    out.Str(std::string(Name()));
    out.U64(Version());
    // The decision types in authority registration order (data and marks together), by name.
    std::vector<std::pair<std::uint32_t, std::string>> types;
    for (const DataRecord& record : registrations.data) {
        types.emplace_back(record.seq, std::to_string(static_cast<int>(record.kind)) + record.name + "#" + std::to_string(record.capacity) +
                                           (record.options.input ? " input" : "") + (record.options.digestExclude ? " digest-exclude" : ""));
    }
    for (const MarkRecord& mark : registrations.marks) {
        std::string name = mark.name;
        if (mark.ops != nullptr) {
            if (const auto id = mark.ops->idIn(registry)) {
                name = mark.kind == DataKind::Component ? registry.Components()[*id].name : registry.Resources()[*id].name;
            }
        }
        types.emplace_back(mark.seq, "mark" + std::to_string(static_cast<int>(mark.kind)) + name + (mark.ops == nullptr ? " by-name" : ""));
    }
    std::sort(types.begin(), types.end());
    out.U64(types.size());
    for (const auto& [seq, text] : types) out.Str(text);

    // The Systems seen to write decision data, less OneEndOnly: their access, mounts and run conditions (not
    // before / after: already in the order, and they may name a System of one end only).
    const Collected collected = CollectedOf(registry, registrations);
    const Kernel::Detail::ConditionFn notResimulating = Kernel::Condition<&Rollback::NotResimulating>("").Evaluator();
    out.U64(collected.systems.size());
    for (const std::string& name : collected.systems) {
        const Kernel::SystemInfo& system = *registry.SystemNamed(name);
        out.Str(system.name);
        // The rollback's "skipped while resimulating" joins its read of Resimulation to the System's reads: left
        // out with the condition (a System that reads Resimulation itself and also has the condition is not told
        // apart; a limit).
        const bool skipped = std::any_of(system.mounts.begin(), system.mounts.end(), [&](const Kernel::Mount& mount) {
            return std::any_of(mount.conditions.begin(), mount.conditions.end(),
                               [&](const Kernel::ConditionRef& c) { return c.Evaluator() == notResimulating; });
        });
        std::optional<Kernel::AccessKey> resimulationRead;
        if (const auto id = registry.ResourceIdOf<WorldTime::Resimulation>(); id && skipped) {
            resimulationRead = Kernel::AccessKey{Kernel::AccessKind::Resource, *id};
        }
        AccessByName(out, registry, system.access, resimulationRead);
        out.U64(system.mounts.size());
        for (const Kernel::Mount& mount : system.mounts) {
            out.Str(mount.schedule);
            out.Str(mount.stage);
            // Its run conditions, less the rollback Module's "skipped while resimulating" (a rollback
            // registration, at one end only; authority declaration 5.5).
            std::vector<std::string> conditions;
            for (const Kernel::ConditionRef& condition : mount.conditions) {
                if (condition.Evaluator() != notResimulating) conditions.push_back(condition.Name());
            }
            out.U64(conditions.size());
            for (const std::string& condition : conditions) out.Str(condition);
            // The sets it runs in and their run conditions (a set's condition decides whether it runs too).
            std::vector<std::string> sets = mount.sets;
            std::sort(sets.begin(), sets.end());
            out.U64(sets.size());
            for (const std::string& set : sets) {
                out.Str(set);
                const auto infos = registry.Sets();
                const auto info = std::find_if(infos.begin(), infos.end(),
                                               [&set, &mount](const Kernel::SetInfo& s) {
                                                   return s.options.name == set && s.options.schedule == mount.schedule;
                                               });
                out.U64(info != infos.end() ? info->conditions.size() : 0);
                if (info != infos.end()) {
                    for (const std::string& condition : info->conditions) out.Str(condition);
                }
            }
        }
    }
    // Each Plan holding a collected System: the collected Systems in Plan order, with whether a sync point lies
    // between two of them (not its name: a Client adds a stage for its screen), and the hook round limit.
    const std::set<std::string> collectedSet(collected.systems.begin(), collected.systems.end());
    for (const Kernel::Plan& plan : registry.Plans()) {
        std::vector<std::pair<std::string, bool>> steps;
        bool syncSince = false;
        for (const Kernel::Step& step : plan.steps) {
            if (step.kind == Kernel::StepKind::SyncPoint) {
                syncSince = true;
                continue;
            }
            if (collectedSet.count(step.name) == 0) continue;
            steps.emplace_back(step.name, !steps.empty() && syncSince);
            syncSince = false;
        }
        if (steps.empty()) continue;
        out.Str(plan.schedule);
        out.Flag(plan.hookRoundLimit.has_value());
        if (plan.hookRoundLimit) out.U64(*plan.hookRoundLimit);
        out.U64(steps.size());
        for (const auto& [name, sync] : steps) {
            out.Str(name);
            out.Flag(sync);
        }
    }
    // Hooks that write decision data, in registration order (they run in it); observers by name only (their
    // order is not promised to the author).
    out.U64(collected.hooks.size());
    for (const Kernel::HookInfo& hook : registry.Hooks()) {
        if (std::find(collected.hooks.begin(), collected.hooks.end(), hook.name) == collected.hooks.end()) continue;
        out.Str(hook.name);
        out.Str(registry.Components()[hook.component].name);
        out.U64(static_cast<std::uint64_t>(hook.trigger));
        AccessByName(out, registry, hook.access);
    }
    std::vector<std::string> observers = collected.observers;
    std::sort(observers.begin(), observers.end());
    out.U64(observers.size());
    for (const std::string& name : observers) {
        for (const Kernel::ObserverInfo& observer : registry.Observers()) {
            if (observer.name != name) continue;
            out.Str(observer.name);
            out.U64(static_cast<std::uint64_t>(observer.trigger));
            // What it watches: the event type, or the component of an Add / Remove.
            out.Str(observer.trigger == Kernel::ObserverTrigger::Event ? registry.Events()[observer.id].name
                                                                      : registry.Components()[observer.id].name);
            AccessByName(out, registry, observer.access);
        }
    }
    // Registrar::InitialValue for a decision component: the kernel writes it at a sync point.
    const DecisionKeys decision = DecisionKeysOf(registry, registrations.data, registrations.marks);
    std::vector<std::string> initial;
    for (const Kernel::InitialValueInfo& rule : registry.InitialValues()) {
        if (decision.Has({Kernel::AccessKind::Component, rule.target})) {
            initial.push_back(registry.Components()[rule.target].name + "<-" + registry.Components()[rule.request].name);
        }
    }
    out.U64(initial.size());
    for (const std::string& rule : initial) out.Str(rule);
    // The schedules a Runs System runs nested.
    out.U64(registrations.runs.size());
    for (const RunsRecord& runs : registrations.runs) {
        out.Str(runs.system);
        out.U64(runs.schedules.size());
        for (const std::string& schedule : runs.schedules) out.Str(schedule);
    }
    return out.Finish(kConsistencyVersion);
}

} // namespace GYO::Engine::Authority
