// Fingerprints and text exports of a frozen Registry (spec C.2 point 5, H-4;
// migration plan section 3.5). The C fingerprint and the G-5 export moved to the
// authority Module (its consistency fingerprint and ExportAuthoritySources;
// v8-arch-gyo2-authority-rollback step 6).
//
// Encoding (format version 1), the bytes hashed by SHA-256:
//   - a string is its byte length as u32 little-endian, then its bytes;
//   - a u32 is 4 bytes little-endian; a flag or enum is one byte (the enum's
//     underlying value);
//   - a list is its length as u32, then its elements;
//   - every fingerprint starts with its format string and version (the
//     constants below).
// Registry fingerprint (format 9): registration owner names in the order of
// their first Register; components, resources,
// messages in id order (name, size, align;
// resources add threadBound; messages have name and
// capacity; their reader lists are derived from the Systems, hashed below);
// effects; named sets (name, members); the allotter; schedules (every
// ScheduleOptions field but hookRoundLimit, which the Plan fingerprint holds);
// sets; Systems in registrationSeq order, removed ones (tombstones) left out
// (name, effects, mounts with every field, access set with its annotatedWrites);
// hooks (name, component, trigger, access); event types in id order
// (name); observers in Registrar::Observer order (name, trigger, event
// type or component id, access, owner name); initial value
// declarations (Registrar::InitialValue; target and request ComponentId, in the
// (requestId, targetId) order Finish leaves them in); then every Plan's
// fingerprint in schedule order. The function of a declaration is not hashed:
// its address differs per build. Registration sites (file paths) are never
// hashed: they differ per checkout.
// Plan fingerprint (format 8): schedule name, each step's kind and
// name, then the hook round limit (a flag, then the value when declared).
// The hook rounds after the last sync point (spec F-12) are not a step; the
// Plan and the former C formats were raised (1 to 2, 3 to 4) when they were added, so the
// rule shows in the fingerprints (v8-arch-gyo2-hook-flush-not-in-plan-bump-versions).
// Sizes are hashed as the platform reports them; a type whose size differs
// between platforms gives different fingerprints there (the kernel tests use
// fixed-width types).

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Sha256.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GYO::Kernel {

std::string Fingerprint::Text() const {
    return std::to_string(version) + ":" + sha256;
}

namespace Detail {
namespace {

// The Plan fingerprint's format: 2 since the hook rounds after the last sync
// point and the schedule's hook round limit (v8-arch-gyo2-hook-flush); 3 since
// the sync point drops spawn records it cannot apply (v8-arch-gyo2-spawn-reject); 4
// since the kernel runs Startup before the first other schedule and a System may
// run a schedule (v8-arch-gyo2-nested-schedule; neither is a Plan step); 5 since
// a conflicting pair with no order is a tie in zone C too and KernelSettings is
// read when each Run starts (v8-arch-gyo2-kernel-subtract-structure; no Plan
// step changed: the rule of hook-flush-not-in-plan-bump-versions); 6 since the
// kernel runs no Startup before the first other schedule (the orchestration
// layer's game initialization; v8-arch-gyo2-orchestration-skeleton); 7 since
// the message trim is no Plan step (it runs when the outermost Run ends: event
// cursor declaration 3.2, v8-arch-gyo2-time-events step 3); 8 since a Plan has
// no clock domain (the kernel keeps no clock: v8-arch-gyo2-clock-out-of-kernel,
// v8-arch-gyo2-time-events step 4).
constexpr std::uint32_t kFingerprintVersion = 8;
// The Registry fingerprint's format: 2 since initial value declarations are
// hashed (v8-arch-gyo2-funnel-check); 3 since the thread roles left (no schedule
// runner role, no System thread role; a resource's threadBound flag) and ExclusiveWorld's
// Writes<...> is an annotation (annotatedWrites) (v8-arch-gyo2-kernel-subtract-structure);
// 4 since a schedule has no startup flag (v8-arch-gyo2-orchestration-skeleton);
// 5 since a schedule names no trim point and a message type has a capacity
// (v8-arch-gyo2-time-events step 3); 6 since a schedule has no clock domain or
// clock flag and a message type no clock domain (v8-arch-gyo2-clock-out-of-kernel,
// v8-arch-gyo2-time-events step 4); 7 since event types and observers
// (v8-arch-gyo2-time-events step 5); 8 since the write policy, a resource's
// external flag, a schedule's ingress stage and Parallel guard test, and a named
// set's digest mark left with the rules that read them, and annotatedWrites
// holds components too, each a (kind, id) key (v8-arch-gyo2-authority-rollback step 2).
// 9 since zones, roles, rollback options, rollbackEnabled, notInRollback, the
// ingress flag and rollbackSafe left for the authority and rollback Modules
// (v8-arch-gyo2-authority-rollback step 6).
constexpr std::uint32_t kRegistryFingerprintVersion = 9;

class Bytes {
public:
    void U8(std::uint8_t value) { bytes_.push_back(static_cast<std::byte>(value)); }
    void Flag(bool value) { U8(value ? 1U : 0U); }
    template <class E>
    void Enum(E value) {
        U8(static_cast<std::uint8_t>(value));
    }
    void U32(std::uint64_t value) {
        for (int shift = 0; shift < 32; shift += 8) U8(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
    void Str(const std::string& text) {
        U32(text.size());
        for (char c : text) U8(static_cast<std::uint8_t>(c));
    }
    void Strings(const std::vector<std::string>& texts) {
        U32(texts.size());
        for (const std::string& text : texts) Str(text);
    }
    template <class T>
    void Ids(const std::vector<T>& ids) {
        U32(ids.size());
        for (T id : ids) U32(id);
    }
    void Keys(const std::vector<AccessKey>& keys) {
        U32(keys.size());
        for (const AccessKey& key : keys) {
            Enum(key.kind);
            U32(key.id);
        }
    }
    [[nodiscard]] Fingerprint Finish(std::uint32_t version = kFingerprintVersion) const {
        return Fingerprint{version, Engine::Base::Sha256(bytes_)};
    }

private:
    std::vector<std::byte> bytes_;
};

void Header(Bytes& out, const char* format, std::uint32_t version = kFingerprintVersion) {
    out.Str(format);
    out.U32(version);
}

void Access(Bytes& out, const AccessSet& access) {
    out.Keys(access.reads);
    out.Keys(access.writes);
    out.Ids(access.membershipReads);
    out.Ids(access.deferredWrites);
    out.Ids(access.setReads);
    out.Keys(access.extractReads);
    out.Keys(access.annotatedWrites);
    out.Flag(access.changeFilters);
    out.Flag(access.local);
    out.Flag(access.exclusive);
    out.Flag(access.diagnostics);
    out.Flag(access.despawn);
    out.Flag(access.spawn);
    out.Flag(access.untypedCommands);
}

void MountBytes(Bytes& out, const Mount& mount) {
    out.Str(mount.schedule);
    out.Str(mount.stage);
    out.Strings(mount.before);
    out.Strings(mount.after);
    out.Strings(mount.sets);
    out.U32(mount.conditions.size());
    for (const ConditionRef& condition : mount.conditions) out.Str(condition.Name());
}

void SystemBytes(Bytes& out, const SystemRecord& system) {
    out.Str(system.options.name);
    out.Ids(system.effects);
    out.U32(system.options.mounts.size());
    for (const Mount& mount : system.options.mounts) MountBytes(out, mount);
    Access(out, system.access);
}

void ColumnBytes(Bytes& out, const ColumnInfo& info) {
    out.Str(info.name);
    out.U32(info.size);
    out.U32(info.align);
}

void ResourceBytes(Bytes& out, const ResourceInfo& info) {
    out.Str(info.name);
    out.U32(info.size);
    out.U32(info.align);
    out.Flag(info.threadBound);
}

void ScheduleBytes(Bytes& out, const ScheduleOptions& s) {
    out.Str(s.name);
    out.Flag(s.extract);
    out.Strings(s.stages);
    out.U32(s.syncPoints.size());
    for (const SyncPointOptions& sync : s.syncPoints) {
        out.Str(sync.name);
        out.Str(sync.stage);
    }
    out.Enum(s.executor);
}

void PlanBytes(Bytes& out, const Plan& plan) {
    out.Str(plan.schedule);
    out.U32(plan.steps.size());
    for (const Step& step : plan.steps) {
        out.Enum(step.kind);
        out.Str(step.name);
    }
    out.Flag(plan.hookRoundLimit.has_value());
    if (plan.hookRoundLimit) out.U32(*plan.hookRoundLimit);
}

std::string Percent(std::size_t part, std::size_t whole) {
    if (whole == 0) return "0%";
    // Rounded to the nearest whole percent, ties up; integers only.
    const std::size_t rounded = (part * 200 + whole) / (whole * 2);
    return std::to_string(rounded) + "%";
}

} // namespace

Fingerprint PlanFingerprintOf(const Plan& plan) {
    Bytes out;
    Header(out, "gyo.kernel.plan");
    PlanBytes(out, plan);
    return out.Finish();
}

Fingerprint RegistryFingerprintOf(const RegistryData& data) {
    Bytes out;
    Header(out, "gyo.kernel.registry", kRegistryFingerprintVersion);
    out.U32(data.owners.size());
    for (const RegistrationOwner& owner : data.owners) out.Str(owner.name);
    out.U32(data.components.size());
    for (const ColumnInfo& info : data.components) ColumnBytes(out, info);
    out.U32(data.resources.size());
    for (const ResourceInfo& info : data.resources) ResourceBytes(out, info);
    out.U32(data.messages.size());
    for (const MessageInfo& info : data.messages) {
        out.Str(info.name);
        out.U32(info.capacity);
    }
    out.U32(data.effects.size());
    for (const EffectRecord& effect : data.effects) out.Str(effect.name);
    out.U32(data.namedSets.size());
    for (const NamedSetRecord& set : data.namedSets) {
        out.Str(set.name);
        out.Ids(set.members);
    }
    out.Flag(data.allotter.has_value());
    out.U32(data.allotter.value_or(0));
    out.U32(data.schedules.size());
    for (const ScheduleRecord& schedule : data.schedules) ScheduleBytes(out, schedule.options);
    out.U32(data.sets.size());
    for (const SetRecord& set : data.sets) {
        out.Str(set.options.name);
        out.Str(set.options.schedule);
        out.Strings(set.options.before);
        out.Strings(set.options.after);
        out.U32(set.conditions.size());
        for (const ConditionRef& condition : set.conditions) out.Str(condition.Name());
    }
    out.U32(std::count_if(data.systems.begin(), data.systems.end(), [](const SystemRecord& s) { return !s.removed; }));
    for (const SystemRecord& system : data.systems) {
        if (!system.removed) SystemBytes(out, system);
    }
    out.U32(data.hooks.size());
    for (const HookRecord& hook : data.hooks) {
        out.Str(hook.name);
        out.U32(hook.componentId.value_or(0));
        out.Enum(hook.trigger);
        Access(out, hook.access);
    }
    out.U32(data.events.size());
    for (const EventInfo& info : data.events) {
        out.Str(info.name);
    }
    out.U32(data.observers.size());
    for (const ObserverRecord& observer : data.observers) {
        out.Str(observer.name);
        out.Enum(observer.trigger);
        out.U32(observer.observedId.value_or(0));
        Access(out, observer.access);
        out.Str(data.OwnerName(observer.owner));
    }
    out.U32(data.initialValues.size());
    for (const InitialValueRecord& rule : data.initialValues) {
        out.U32(rule.targetId);
        out.U32(rule.requestId);
    }
    out.U32(data.plans.size());
    for (const Plan& plan : data.plans) out.Str(plan.fingerprint.Text());
    return out.Finish(kRegistryFingerprintVersion);
}

} // namespace Detail

std::string FrozenRegistry::ExportPlans() const {
    const Detail::RegistryData& data = *data_;
    std::string out = "gyo.kernel.plans 7\n";
    out += "registry " + Detail::RegistryFingerprintOf(data).Text() + "\n";
    for (const Plan& plan : data.plans) {
        out += "schedule " + plan.schedule + "\n";
        out += "  hook-round-limit " + (plan.hookRoundLimit ? std::to_string(*plan.hookRoundLimit) : std::string("default")) + "\n";
        out += "  fingerprint " + plan.fingerprint.Text() + "\n";
        for (std::size_t i = 0; i < plan.steps.size(); ++i) {
            const Step& step = plan.steps[i];
            out += "  step " + std::to_string(i) + " ";
            switch (step.kind) {
            case StepKind::System:
                out += "system " + step.name;
                break;
            case StepKind::SyncPoint: out += "sync " + step.name; break;
            }
            out += "\n";
        }
        for (const DeclaredEdge& edge : plan.declaredEdges) {
            out += "  edge " + edge.from + " -> " + edge.to + (edge.dataflow ? " dataflow" : " non-dataflow") +
                   (edge.dataflowExtended ? " extended=dataflow" : " extended=non-dataflow") + "\n";
        }
        // The judged ratio (the migration plan's definition), then the extended one beside it.
        const std::size_t non = plan.NonDataflowEdges();
        out += "  non_dataflow_edges " + std::to_string(non) + "/" + std::to_string(plan.declaredEdges.size()) + " " +
               Detail::Percent(non, plan.declaredEdges.size()) + "\n";
        const std::size_t nonExtended = plan.NonDataflowEdgesExtended();
        out += "  non_dataflow_edges_extended " + std::to_string(nonExtended) + "/" +
               std::to_string(plan.declaredEdges.size()) + " " + Detail::Percent(nonExtended, plan.declaredEdges.size()) +
               " (extended definition: deferred writes and membership count; not judged)\n";
        for (const TieBreak& tie : data.ties) {
            if (tie.schedule == plan.schedule) out += "  tie " + tie.first + " -> " + tie.second + " (registration order)\n";
        }
    }
    return out;
}

} // namespace GYO::Kernel
