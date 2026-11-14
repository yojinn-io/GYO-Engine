// Fingerprints and text exports of a frozen Registry (spec C.2 point 5, H-4,
// G-5; migration plan section 3.5).
//
// Encoding (format version 1), the bytes hashed by SHA-256:
//   - a string is its byte length as u32 little-endian, then its bytes;
//   - a u32 is 4 bytes little-endian; a flag or enum is one byte (the enum's
//     underlying value);
//   - a list is its length as u32, then its elements;
//   - every fingerprint starts with its format string and version
//     ("gyo.kernel.registry", "gyo.kernel.registry-c", "gyo.kernel.plan"; 1).
// Registry fingerprint: plugin names in list order; components, resources,
// events in id order (name, size, align, zone, writePolicy, role, rollback;
// resources add external and affinity; events have name, domain, zone);
// effects; named sets (name, members, digest); the allotter; schedules (every
// ScheduleOptions field); sets; Systems in registrationSeq order (name, zone,
// ingress, affinity, effects, rollbackSafe, mounts with every field, access
// set); hooks (name, component, trigger, zone, access); then every Plan's
// fingerprint in schedule order. Registration sites (file paths) are never
// hashed: they differ per checkout.
// C fingerprint (format 2): only zone C data, zone C Systems (with mounts and
// access) and zone C hooks, each section counted; and for every schedule that
// runs a zone C System, its rollbackEnabled, stages and eventTrim, and its Plan
// reduced to the zone C System steps plus every sync point and the trim
// (spec H.3; review B6).
// Plan fingerprint: schedule name, domain, then each step's kind and name.
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

constexpr std::uint32_t kFingerprintVersion = 1;
// The C fingerprint's format: 2 since the review (B6) added zone C hooks, the
// schedules that hold zone C steps, and the sync points around those steps.
constexpr std::uint32_t kCFingerprintVersion = 2;

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
    out.Flag(mount.notInRollback);
}

void SystemBytes(Bytes& out, const SystemRecord& system) {
    out.Str(system.options.name);
    out.Enum(system.options.zone);
    out.Flag(system.options.ingress);
    out.Str(system.options.affinity.role);
    out.Ids(system.effects);
    out.Flag(system.options.rollbackSafe);
    out.U32(system.options.mounts.size());
    for (const Mount& mount : system.options.mounts) MountBytes(out, mount);
    Access(out, system.access);
}

void ColumnBytes(Bytes& out, const ColumnInfo& info) {
    out.Str(info.name);
    out.U32(info.size);
    out.U32(info.align);
    out.Enum(info.zone);
    out.Enum(info.writePolicy);
    out.Enum(info.role);
    out.Enum(info.rollback);
}

void ResourceBytes(Bytes& out, const ResourceInfo& info) {
    out.Str(info.name);
    out.U32(info.size);
    out.U32(info.align);
    out.Enum(info.zone);
    out.Enum(info.writePolicy);
    out.Enum(info.role);
    out.Enum(info.rollback);
    out.Flag(info.external);
    out.Str(info.affinity.role);
}

void ScheduleBytes(Bytes& out, const ScheduleOptions& s) {
    out.Str(s.name);
    out.Enum(s.domain);
    out.Flag(s.startup);
    out.Flag(s.clock);
    out.Flag(s.extract);
    out.Flag(s.rollbackEnabled);
    out.Strings(s.stages);
    out.Str(s.ingressStage);
    out.U32(s.syncPoints.size());
    for (const SyncPointOptions& sync : s.syncPoints) {
        out.Str(sync.name);
        out.Str(sync.stage);
    }
    out.Str(s.eventTrim);
    out.Enum(s.executor);
    out.Str(s.parallelGuardTest);
    out.Str(s.runner);
}

void PlanBytes(Bytes& out, const Plan& plan) {
    out.Str(plan.schedule);
    out.Enum(plan.domain);
    out.U32(plan.steps.size());
    for (const Step& step : plan.steps) {
        out.Enum(step.kind);
        out.Str(step.name);
    }
}

std::string Percent(std::size_t part, std::size_t whole) {
    if (whole == 0) return "0%";
    // Rounded to the nearest whole percent, ties up; integers only.
    const std::size_t rounded = (part * 200 + whole) / (whole * 2);
    return std::to_string(rounded) + "%";
}

std::string JsonString(const std::string& text) {
    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buffer[8] = "\\u00";
            const char* hex = "0123456789abcdef";
            buffer[4] = hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
            buffer[5] = hex[static_cast<unsigned char>(c) & 0xF];
            buffer[6] = '\0';
            out += buffer;
        } else {
            out += c;
        }
    }
    return out + "\"";
}

std::string Normalize(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

// True when path starts with root. A Windows drive path ("C:/...") compares
// without case: compilers and CMake may spell the same drive and folders in
// different case there.
bool StartsWith(const std::string& path, const std::string& root) {
    if (path.size() < root.size()) return false;
    const bool drive = root.size() > 1 && root[1] == ':';
    for (std::size_t i = 0; i < root.size(); ++i) {
        char a = path[i];
        char b = root[i];
        if (drive) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) return false;
    }
    return true;
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
    Header(out, "gyo.kernel.registry");
    out.U32(data.plugins.size());
    for (const PluginRecord& plugin : data.plugins) out.Str(plugin.name);
    out.U32(data.components.size());
    for (const ColumnInfo& info : data.components) ColumnBytes(out, info);
    out.U32(data.resources.size());
    for (const ResourceInfo& info : data.resources) ResourceBytes(out, info);
    out.U32(data.events.size());
    for (const EventInfo& info : data.events) {
        out.Str(info.name);
        out.Enum(info.domain);
        out.Enum(info.zone);
    }
    out.U32(data.effects.size());
    for (const EffectRecord& effect : data.effects) out.Str(effect.name);
    out.U32(data.namedSets.size());
    for (const NamedSetRecord& set : data.namedSets) {
        out.Str(set.name);
        out.Ids(set.members);
        out.Flag(set.digest);
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
    out.U32(data.systems.size());
    for (const SystemRecord& system : data.systems) SystemBytes(out, system);
    out.U32(data.hooks.size());
    for (const HookRecord& hook : data.hooks) {
        out.Str(hook.name);
        out.U32(hook.componentId.value_or(0));
        out.Enum(hook.trigger);
        out.Enum(hook.zone);
        Access(out, hook.access);
    }
    out.U32(data.plans.size());
    for (const Plan& plan : data.plans) out.Str(plan.fingerprint.Text());
    return out.Finish();
}

Fingerprint CFingerprintOf(const RegistryData& data) {
    Bytes out;
    Header(out, "gyo.kernel.registry-c", kCFingerprintVersion);
    // Each section is counted first, so moving an item between sections changes the bytes.
    const auto count = [](const auto& items, auto&& pick) {
        std::uint32_t n = 0;
        for (const auto& item : items) n += pick(item) ? 1U : 0U;
        return n;
    };
    out.U32(count(data.components, [](const ColumnInfo& c) { return c.zone == Zone::C; }));
    for (std::size_t id = 0; id < data.components.size(); ++id) {
        if (data.components[id].zone != Zone::C) continue;
        out.U32(id);
        ColumnBytes(out, data.components[id]);
    }
    out.U32(count(data.resources, [](const ResourceInfo& r) { return r.zone == Zone::C; }));
    for (std::size_t id = 0; id < data.resources.size(); ++id) {
        if (data.resources[id].zone != Zone::C) continue;
        out.U32(id);
        ResourceBytes(out, data.resources[id]);
    }
    out.U32(count(data.events, [](const EventInfo& e) { return e.zone == Zone::C; }));
    for (std::size_t id = 0; id < data.events.size(); ++id) {
        if (data.events[id].zone != Zone::C) continue;
        out.U32(id);
        out.Str(data.events[id].name);
        out.Enum(data.events[id].domain);
    }
    out.U32(count(data.systems, [](const SystemRecord& s) { return s.options.zone == Zone::C; }));
    for (const SystemRecord& system : data.systems) {
        if (system.options.zone == Zone::C) SystemBytes(out, system);
    }
    out.U32(count(data.hooks, [](const HookRecord& h) { return h.zone == Zone::C; }));
    for (const HookRecord& hook : data.hooks) {
        if (hook.zone != Zone::C) continue;
        out.Str(hook.name);
        out.U32(hook.componentId.value_or(0));
        out.Enum(hook.trigger);
        Access(out, hook.access);
    }
    // Every schedule that runs a zone C System: its properties that change (c)
    // results (rollback, stage sequence, event trim) and its Plan reduced to the
    // zone C steps with the sync points and trim between them, which decide in
    // which window a deferred write lands.
    for (const Plan& plan : data.plans) {
        bool holdsC = false;
        for (const Step& step : plan.steps) {
            holdsC = holdsC || (step.kind == StepKind::System && data.systems[step.system].options.zone == Zone::C);
        }
        if (!holdsC) continue;
        out.Str(plan.schedule);
        if (const ScheduleRecord* schedule = data.FindSchedule(plan.schedule)) {
            out.Flag(schedule->options.rollbackEnabled);
            out.Strings(schedule->options.stages);
            out.Str(schedule->options.eventTrim);
        }
        std::vector<const Step*> reduced;
        for (const Step& step : plan.steps) {
            if (step.kind != StepKind::System || data.systems[step.system].options.zone == Zone::C) reduced.push_back(&step);
        }
        out.U32(reduced.size());
        for (const Step* step : reduced) {
            out.Enum(step->kind);
            out.Str(step->name);
        }
    }
    return out.Finish(kCFingerprintVersion);
}

} // namespace Detail

std::string FrozenRegistry::ExportPlans() const {
    const Detail::RegistryData& data = *data_;
    std::string out = "gyo.kernel.plans 1\n";
    out += "registry " + Detail::RegistryFingerprintOf(data).Text() + "\n";
    out += "registry_c " + Detail::CFingerprintOf(data).Text() + "\n";
    for (const Plan& plan : data.plans) {
        const Detail::ScheduleRecord* schedule = data.FindSchedule(plan.schedule);
        out += "schedule " + plan.schedule + " domain=" + Detail::DomainName(plan.domain);
        if (schedule != nullptr) {
            out += std::string(" clock=") + (schedule->options.clock ? "1" : "0") +
                   " startup=" + (schedule->options.startup ? "1" : "0") +
                   " rollback=" + (schedule->options.rollbackEnabled ? "1" : "0") + " runner=" + schedule->options.runner;
        }
        out += "\n";
        out += "  fingerprint " + plan.fingerprint.Text() + "\n";
        for (std::size_t i = 0; i < plan.steps.size(); ++i) {
            const Step& step = plan.steps[i];
            out += "  step " + std::to_string(i) + " ";
            switch (step.kind) {
            case StepKind::System:
                out += "system " + step.name + " zone=" + Detail::ZoneName(data.systems[step.system].options.zone);
                break;
            case StepKind::SyncPoint: out += "sync " + step.name; break;
            case StepKind::EventTrim: out += "trim"; break;
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

Engine::Base::Result<std::string, KernelError> FrozenRegistry::ExportZoneCSources(const ZoneCSourcesOptions& options) const {
    const Detail::RegistryData& data = *data_;
    const std::string root = Detail::Normalize(options.repositoryRoot) + "/";
    const std::string owner = Detail::Normalize(options.ownerRoot);
    std::vector<std::string> files;
    for (const Detail::SystemRecord& system : data.systems) {
        if (system.options.zone != Zone::C) continue;
        const std::string path = Detail::Normalize(system.source.file_name());
        if (!Detail::StartsWith(path, root)) {
            return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed,
                                                 "a zone C System is registered outside the repository root",
                                                 system.options.name + ": " + path));
        }
        const std::string relative = path.substr(root.size());
        if (relative.rfind(owner + "/", 0) != 0) {
            return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed,
                                                 "a zone C System is registered outside the owner root " + owner,
                                                 system.options.name + ": " + relative));
        }
        files.push_back(relative);
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    std::string out = "{\n";
    out += "  \"format\": \"gyo.zone-c-sources\",\n";
    out += "  \"version\": 1,\n";
    out += "  \"generated_by\": \"gyo-kernel-report\",\n";
    out += "  \"repo_commit\": " + Detail::JsonString(options.repoCommit) + ",\n";
    out += "  \"owner_root\": " + Detail::JsonString(owner) + ",\n";
    out += "  \"include_roots\": [";
    for (std::size_t i = 0; i < options.includeRoots.size(); ++i) {
        out += (i == 0 ? "" : ", ") + Detail::JsonString(Detail::Normalize(options.includeRoots[i]));
    }
    out += "],\n  \"files\": [";
    for (std::size_t i = 0; i < files.size(); ++i) {
        out += (i == 0 ? "\n    " : ",\n    ") + Detail::JsonString(files[i]);
    }
    out += files.empty() ? "]\n}\n" : "\n  ]\n}\n";
    return out;
}

} // namespace GYO::Kernel
