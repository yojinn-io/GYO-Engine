#include "engine/authority/Consistency.hpp"

#include "engine/authority/AuthorityStrategy.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace GYO::Engine::Authority {

namespace {

void SortUnique(std::vector<Kernel::AccessKey>& keys) {
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
}

std::optional<std::uint32_t> ResourceNamed(const Kernel::FrozenRegistry& registry, const std::string& name) {
    const auto resources = registry.Resources();
    for (std::size_t id = 0; id < resources.size(); ++id) {
        if (resources[id].name == name) return static_cast<std::uint32_t>(id);
    }
    return std::nullopt;
}

std::optional<Kernel::AccessKey> KeyOfMark(const Kernel::FrozenRegistry& registry, const MarkRecord& mark) {
    const Kernel::AccessKind kind = mark.kind == DataKind::Component ? Kernel::AccessKind::Component : Kernel::AccessKind::Resource;
    if (mark.ops != nullptr) {
        if (const auto id = mark.ops->idIn(registry)) return Kernel::AccessKey{kind, *id};
        return std::nullopt;
    }
    if (const auto id = ResourceNamed(registry, mark.name)) return Kernel::AccessKey{Kernel::AccessKind::Resource, *id};
    return std::nullopt;
}

Kernel::AccessKind KindOf(DataKind kind) {
    switch (kind) {
    case DataKind::Component: return Kernel::AccessKind::Component;
    case DataKind::Resource: return Kernel::AccessKind::Resource;
    case DataKind::Message: return Kernel::AccessKind::Message;
    }
    return Kernel::AccessKind::Component;
}

bool OneEnd(const AuthorityRegistry& registrations, const std::string& name) {
    return std::any_of(registrations.oneEnd.begin(), registrations.oneEnd.end(),
                       [&name](const OneEndRecord& record) { return record.system == name; });
}

bool WritesDecision(const Kernel::FrozenRegistry& registry, const DecisionKeys& decision, const Kernel::AccessSet& access) {
    for (const Kernel::AccessKey& key : WrittenKeys(registry, access)) {
        if (decision.Has(key)) return true;
    }
    return false;
}

} // namespace

bool DecisionKeys::Has(const Kernel::AccessKey& key) const {
    return std::binary_search(keys.begin(), keys.end(), key);
}

DecisionKeys DecisionKeysOf(const Kernel::FrozenRegistry& registry, const std::vector<DataRecord>& data,
                            const std::vector<MarkRecord>& marks) {
    DecisionKeys out;
    for (const DataRecord& record : data) {
        if (const auto id = record.ops->idIn(registry)) out.keys.push_back({KindOf(record.kind), *id});
    }
    for (const MarkRecord& mark : marks) {
        if (const auto key = KeyOfMark(registry, mark)) out.keys.push_back(*key);
    }
    SortUnique(out.keys);
    return out;
}

DecisionKeys DecisionKeysOf(const Kernel::FrozenRegistry& registry, const std::deque<DataRecord>& data,
                            const std::deque<MarkRecord>& marks) {
    return DecisionKeysOf(registry, std::vector<DataRecord>(data.begin(), data.end()), std::vector<MarkRecord>(marks.begin(), marks.end()));
}

std::vector<Kernel::AccessKey> WrittenKeys(const Kernel::FrozenRegistry& registry, const Kernel::AccessSet& access) {
    std::vector<Kernel::AccessKey> keys = access.writes;
    for (const Kernel::ComponentId id : access.deferredWrites) keys.push_back({Kernel::AccessKind::Component, id});
    keys.insert(keys.end(), access.annotatedWrites.begin(), access.annotatedWrites.end());
    // Registrar::InitialValue: a sync point fills a component for whoever added a cell of its request, through
    // chains of declarations (the funnel; authority-rollback declaration 4.4). A write in place or a remove of a
    // request adds no cell and fills nothing.
    std::vector<Kernel::ComponentId> pending(access.deferredInserts.begin(), access.deferredInserts.end());
    std::set<Kernel::ComponentId> seen(pending.begin(), pending.end());
    while (!pending.empty()) {
        const Kernel::ComponentId request = pending.back();
        pending.pop_back();
        for (const Kernel::InitialValueInfo& rule : registry.InitialValues()) {
            if (rule.request != request || !seen.insert(rule.target).second) continue;
            keys.push_back({Kernel::AccessKind::Component, rule.target});
            pending.push_back(rule.target);
        }
    }
    SortUnique(keys);
    return keys;
}

Collected CollectedOf(const Kernel::FrozenRegistry& registry, const AuthorityRegistry& registrations) {
    const DecisionKeys decision = DecisionKeysOf(registry, registrations.data, registrations.marks);
    Collected out;
    for (const Kernel::SystemInfo& system : registry.Systems()) {
        if (system.removed || OneEnd(registrations, system.name)) continue;
        if (WritesDecision(registry, decision, system.access)) out.systems.push_back(system.name);
    }
    for (const Kernel::HookInfo& hook : registry.Hooks()) {
        if (!OneEnd(registrations, hook.name) && WritesDecision(registry, decision, hook.access)) out.hooks.push_back(hook.name);
    }
    for (const Kernel::ObserverInfo& observer : registry.Observers()) {
        if (!OneEnd(registrations, observer.name) && WritesDecision(registry, decision, observer.access)) {
            out.observers.push_back(observer.name);
        }
    }
    return out;
}

::Engine::Base::Result<Kernel::Fingerprint, AuthorityFailure> ConsistencyFingerprintOf(const Kernel::World& world) {
    if (!world.Types().ResourceIdOf<AuthorityRegistry>()) {
        return ::Engine::Base::Err(AuthorityFailure{AuthorityFailureCode::NoAuthority, "the World has no authority Module"});
    }
    const AuthorityRegistry* registrations = world.Resource<AuthorityRegistry>();
    if (registrations == nullptr || !registrations->strategy) {
        return ::Engine::Base::Err(AuthorityFailure{AuthorityFailureCode::NoAuthority, "the World chose no authority strategy"});
    }
    FingerprintContext context(world.Types(), *registrations);
    return registrations->strategy->ConsistencyFingerprint(context);
}

::Engine::Base::Result<std::uint64_t, AuthorityFailure> Digest(const Kernel::World& world) {
    if (!world.Types().ResourceIdOf<AuthorityRegistry>()) {
        return ::Engine::Base::Err(AuthorityFailure{AuthorityFailureCode::NoAuthority, "the World has no authority Module"});
    }
    const AuthorityRegistry* registrations = world.Resource<AuthorityRegistry>();
    if (registrations == nullptr) {
        return ::Engine::Base::Err(AuthorityFailure{AuthorityFailureCode::NoAuthority, "the World has no AuthorityRegistry"});
    }
    std::uint64_t sum = 0;
    for (const DataRecord& record : registrations->data) {
        if (record.options.digestExclude || record.ops->digest == nullptr) continue;
        sum += record.ops->digest(world, record.seq);
    }
    for (const MarkRecord& mark : registrations->marks) {
        if (mark.ops != nullptr && mark.ops->digest != nullptr) sum += mark.ops->digest(world, mark.seq);
    }
    return sum;
}

::Engine::Base::Result<std::string, AuthorityFailure> DatasetReport(const Kernel::World& world) {
    if (!world.Types().ResourceIdOf<AuthorityRegistry>() || world.Resource<AuthorityRegistry>() == nullptr) {
        return ::Engine::Base::Err(AuthorityFailure{AuthorityFailureCode::NoAuthority, "the World has no authority Module"});
    }
    const Kernel::FrozenRegistry& registry = world.Types();
    const AuthorityRegistry& registrations = *world.Resource<AuthorityRegistry>();
    std::string out = "gyo.authority.dataset 1\n";
    out += "strategy " + (registrations.strategy ? std::string(registrations.strategy->Name()) + " " +
                                                       std::to_string(registrations.strategy->Version())
                                                 : std::string("none")) + "\n";
    std::vector<std::pair<std::uint32_t, std::string>> types;
    const char* kinds[] = {"component", "resource", "message"};
    for (const DataRecord& record : registrations.data) {
        std::string line = std::string(kinds[static_cast<int>(record.kind)]) + " " + record.name;
        if (record.kind == DataKind::Message) line += " capacity=" + std::to_string(record.capacity);
        if (record.options.input) line += " input";
        if (record.options.digestExclude) line += " digest-exclude";
        types.emplace_back(record.seq, line);
    }
    for (const MarkRecord& mark : registrations.marks) {
        std::string name = mark.name;
        if (mark.ops != nullptr) {
            if (const auto id = mark.ops->idIn(registry)) {
                name = mark.kind == DataKind::Component ? registry.Components()[*id].name : registry.Resources()[*id].name;
            }
        }
        types.emplace_back(mark.seq, std::string("marked ") + kinds[static_cast<int>(mark.kind)] + " " + name +
                                         (mark.ops == nullptr ? " (by name)" : ""));
    }
    std::sort(types.begin(), types.end());
    for (const auto& [seq, line] : types) out += "type " + std::to_string(seq) + " " + line + "\n";
    const Collected collected = CollectedOf(registry, registrations);
    for (const std::string& name : collected.systems) out += "system " + name + "\n";
    for (const std::string& name : collected.hooks) out += "hook " + name + "\n";
    for (const std::string& name : collected.observers) out += "observer " + name + "\n";
    for (const RunsRecord& runs : registrations.runs) {
        out += "runs " + runs.system;
        for (const std::string& schedule : runs.schedules) out += " " + schedule;
        out += "\n";
    }
    for (const OneEndRecord& one : registrations.oneEnd) out += "one-end-only " + one.system + " (" + one.reason + ")\n";
    return out;
}

namespace {

std::string Normalize(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

// True when path starts with root. A Windows drive path ("C:/...") compares without case: compilers and CMake
// may spell the same drive and folders in different case there (as the kernel's former export did).
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

// "file:line" (the registry's source text) to the file.
std::string FileOf(const std::string& source) {
    const std::size_t colon = source.rfind(':');
    return colon == std::string::npos ? source : source.substr(0, colon);
}

std::string JsonString(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            const char* hex = "0123456789abcdef";
            out += "\\u00";
            out += hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
            out += hex[static_cast<unsigned char>(c) & 0xF];
        } else {
            out += c;
        }
    }
    return out + "\"";
}

} // namespace

::Engine::Base::Result<std::string, AuthorityFailure> ExportAuthoritySources(const Kernel::World& world,
                                                                             const AuthoritySourcesOptions& options) {
    if (!world.Types().ResourceIdOf<AuthorityRegistry>() || world.Resource<AuthorityRegistry>() == nullptr) {
        return ::Engine::Base::Err(AuthorityFailure{AuthorityFailureCode::NoAuthority, "the World has no authority Module"});
    }
    const Kernel::FrozenRegistry& registry = world.Types();
    const AuthorityRegistry& registrations = *world.Resource<AuthorityRegistry>();
    const std::string root = Normalize(options.repositoryRoot) + "/";
    const std::string owner = Normalize(options.ownerRoot);
    std::vector<std::string> files;
    std::optional<AuthorityFailure> failed;
    // what: "a System that writes decision data", ...
    const auto add = [&](const std::string& source, const std::string& what, const std::string& name) {
        if (failed) return;
        const std::string path = Normalize(FileOf(source));
        if (!StartsWith(path, root)) {
            failed.emplace(AuthorityFailureCode::SourceOutsideRoot, what + " is registered outside the repository root",
                           name + ": " + path);
            return;
        }
        const std::string relative = path.substr(root.size());
        if (relative.rfind(owner + "/", 0) != 0) {
            failed.emplace(AuthorityFailureCode::SourceOutsideRoot, what + " is registered outside the owner root " + owner,
                           name + ": " + relative);
            return;
        }
        files.push_back(relative);
    };
    const Collected collected = CollectedOf(registry, registrations);
    for (const Kernel::SystemInfo& system : registry.Systems()) {
        if (std::find(collected.systems.begin(), collected.systems.end(), system.name) != collected.systems.end()) {
            add(system.source, "a System that writes decision data", system.name);
        }
    }
    for (const Kernel::HookInfo& hook : registry.Hooks()) {
        if (std::find(collected.hooks.begin(), collected.hooks.end(), hook.name) != collected.hooks.end()) {
            add(hook.source, "a hook that writes decision data", hook.name);
        }
    }
    for (const Kernel::ObserverInfo& observer : registry.Observers()) {
        if (std::find(collected.observers.begin(), collected.observers.end(), observer.name) != collected.observers.end()) {
            add(observer.source, "an observer that writes decision data", observer.name);
        }
    }
    // The function of Registrar::InitialValue computes a decision component's value at a sync point.
    const DecisionKeys decision = DecisionKeysOf(registry, registrations.data, registrations.marks);
    for (const Kernel::InitialValueInfo& rule : registry.InitialValues()) {
        if (decision.Has({Kernel::AccessKind::Component, rule.target})) {
            add(rule.source, "the initial value of a decision component", registry.Components()[rule.target].name);
        }
    }
    if (failed) return ::Engine::Base::Err(std::move(*failed));
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    std::string out = "{\n";
    out += "  \"format\": \"gyo.authority-sources\",\n";
    out += "  \"version\": 1,\n";
    out += "  \"generated_by\": \"gyo-authority-report\",\n";
    out += "  \"repo_commit\": " + JsonString(options.repoCommit) + ",\n";
    out += "  \"owner_root\": " + JsonString(owner) + ",\n";
    out += "  \"include_roots\": [";
    for (std::size_t i = 0; i < options.includeRoots.size(); ++i) {
        out += (i == 0 ? "" : ", ") + JsonString(Normalize(options.includeRoots[i]));
    }
    out += "],\n  \"files\": [";
    for (std::size_t i = 0; i < files.size(); ++i) out += std::string(i == 0 ? "\n    " : ",\n    ") + JsonString(files[i]);
    out += files.empty() ? "]\n}\n" : "\n  ]\n}\n";
    return out;
}

} // namespace GYO::Engine::Authority
