// Scheduler (spec D; target/kernel DI-KS 3 and 5; target/scheduler.zh-Hant.md).
//
// Per schedule, at Finish only:
//   1. nodes: each mount of the schedule (a System step), each declared sync
//      point, each stage end (a sync point), and the event trim of a clock
//      schedule;
//   2. order edges: stage order (every node of a stage before its stage end,
//      the stage end before every node of the next stage), before / after of
//      mounts and sets; a before / after against stage order is D-E5, a target
//      that does not exist D-E4, a cycle D-E6;
//   3. conflict edges, kept apart from order edges (DI-KS 5): within a stage,
//      a pair conflicts when one writes what the other reads or writes, either
//      holds ExclusiveWorld, both declare one effect, or both defer a write of
//      one component (spec D.1 conditions 1 to 4);
//   4. a conflicting pair with no order path is ambiguous: either side zone C
//      is D-E8; both zone B get an edge from the lower registrationSeq to the
//      higher (spec D.2 step 10). Pairs are taken in (seq, seq) order and the
//      reachability is updated after each added edge, so the result is fixed
//      and acyclic;
//   5. the Plan: Kahn's algorithm taking, among ready nodes, the System with
//      the lowest registrationSeq; a sync point or the trim only when no
//      System is ready (spec D.2 step 12). The tie key is registrationSeq
//      alone: no name, no address, no hash (spec D-10).
// No hash containers anywhere (DI-KS 3); maps are ordered.

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace GYO::Kernel::Detail {
namespace {

class BitRows {
public:
    explicit BitRows(std::size_t n) : n_(n), words_((n + 63) / 64), bits_(n * words_, 0) {}
    [[nodiscard]] bool Test(std::size_t row, std::size_t column) const {
        return ((bits_[row * words_ + column / 64] >> (column % 64)) & 1U) != 0;
    }
    void Set(std::size_t row, std::size_t column) { bits_[row * words_ + column / 64] |= std::uint64_t{1} << (column % 64); }
    void OrRow(std::size_t into, std::size_t from) {
        for (std::size_t w = 0; w < words_; ++w) bits_[into * words_ + w] |= bits_[from * words_ + w];
    }
    [[nodiscard]] std::size_t Size() const noexcept { return n_; }

private:
    std::size_t n_;
    std::size_t words_;
    std::vector<std::uint64_t> bits_;
};

struct Node {
    StepKind kind;
    std::size_t stage;
    // System: index into data.systems (== registrationSeq) and its mount.
    std::uint32_t system = 0;
    std::size_t mount = 0;
    std::string name;
};

template <class T>
bool Intersects(const std::vector<T>& a, const std::vector<T>& b, T* found) {
    auto i = a.begin();
    auto j = b.begin();
    while (i != a.end() && j != b.end()) {
        if (*i < *j) {
            ++i;
        } else if (*j < *i) {
            ++j;
        } else {
            if (found != nullptr) *found = *i;
            return true;
        }
    }
    return false;
}

std::vector<AccessKey> Written(const AccessSet& access) {
    std::vector<AccessKey> keys = access.writes;
    for (ComponentId id : access.deferredWrites) keys.push_back({AccessKind::Component, id});
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

std::vector<AccessKey> ReadOrMembership(const AccessSet& access) {
    std::vector<AccessKey> keys = access.reads;
    for (ComponentId id : access.membershipReads) keys.push_back({AccessKind::Component, id});
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

std::string KeyName(const RegistryData& data, const AccessKey& key) {
    switch (key.kind) {
    case AccessKind::Component: return "component '" + data.components[key.id].name + "'";
    case AccessKind::Resource: return "resource '" + data.resources[key.id].name + "'";
    case AccessKind::Event: return "event '" + data.events[key.id].name + "'";
    case AccessKind::DomainTick: return std::string("domain tick ") + DomainName(static_cast<Domain>(key.id));
    }
    return "?";
}

// Spec D.1 conditions 1 to 4. Returns what the pair conflicts on, or "".
std::string ConflictBetween(const RegistryData& data, const SystemRecord& a, const SystemRecord& b) {
    if (a.access.exclusive || b.access.exclusive) return "ExclusiveWorld";
    AccessKey key{};
    std::vector<AccessKey> touchedB = b.access.writes;
    touchedB.insert(touchedB.end(), b.access.reads.begin(), b.access.reads.end());
    std::sort(touchedB.begin(), touchedB.end());
    if (Intersects(a.access.writes, touchedB, &key)) return KeyName(data, key);
    std::vector<AccessKey> touchedA = a.access.writes;
    touchedA.insert(touchedA.end(), a.access.reads.begin(), a.access.reads.end());
    std::sort(touchedA.begin(), touchedA.end());
    if (Intersects(b.access.writes, touchedA, &key)) return KeyName(data, key);
    EffectId effect = 0;
    if (Intersects(a.effects, b.effects, &effect)) return "effect '" + data.effects[effect].name + "'";
    ComponentId component = 0;
    if (Intersects(a.access.deferredWrites, b.access.deferredWrites, &component)) {
        return "deferred writes of component '" + data.components[component].name + "'";
    }
    return {};
}

class ScheduleBuilder {
public:
    ScheduleBuilder(RegistryData& data, const ScheduleRecord& record) : data_(data), s_(record.options) {}

    void Build() {
        MakeNodes();
        direct_ = std::make_unique<BitRows>(nodes_.size());
        succ_.assign(nodes_.size(), {});
        StageEdges();
        DeclaredEdges();
        if (failed_) return;
        if (data_.schedulerShuffle != 0) {
            for (auto& list : succ_) {
                if (!list.empty()) std::rotate(list.begin(), list.begin() + data_.schedulerShuffle % list.size(), list.end());
            }
        }
        std::vector<std::size_t> order;
        if (!TopologicalOrder(order)) return;
        Closure(order);
        Conflicts();
        if (failed_) return;
        DerivedOrder();
        if (failed_) return;
        MakePlan();
    }

private:
    void Error(StartupCode code, std::string rule, std::string message, std::vector<std::string> subjects,
               std::vector<std::string> sources = {}) {
        data_.Error(code, std::move(rule), std::move(message), std::move(subjects), std::move(sources));
        failed_ = true;
    }

    const SystemRecord& SystemOf(std::size_t node) const { return data_.systems[nodes_[node].system]; }
    bool IsSystem(std::size_t node) const { return nodes_[node].kind == StepKind::System; }

    void MakeNodes() {
        for (std::size_t stage = 0; stage < s_.stages.size(); ++stage) {
            const std::string& stageName = s_.stages[stage];
            for (std::size_t i = 0; i < data_.systems.size(); ++i) {
                const SystemRecord& system =
                    data_.systems[data_.schedulerShuffle != 0 ? data_.systems.size() - 1 - i : i];
                for (std::size_t m = 0; m < system.options.mounts.size(); ++m) {
                    const Mount& mount = system.options.mounts[m];
                    if (mount.schedule != s_.name || mount.stage != stageName) continue;
                    if (mounted_.count(system.seq) != 0) continue;  // D-E19, reported by Registry.cpp
                    mounted_[system.seq] = nodes_.size();
                    nodes_.push_back(Node{StepKind::System, stage, system.seq, m, system.options.name});
                }
            }
            for (const SyncPointOptions& sync : s_.syncPoints) {
                if (sync.stage != stageName) continue;
                syncNodes_[sync.name] = nodes_.size();
                nodes_.push_back(Node{StepKind::SyncPoint, stage, 0, 0, sync.name});
            }
            stageEnd_.push_back(nodes_.size());
            syncNodes_[stageName] = nodes_.size();
            nodes_.push_back(Node{StepKind::SyncPoint, stage, 0, 0, stageName});
        }
        if (s_.clock && !s_.stages.empty()) {
            const std::string trimAt = s_.eventTrim.empty() ? s_.stages.back() : s_.eventTrim;
            const auto at = syncNodes_.find(trimAt);
            if (at != syncNodes_.end()) {
                trimAfter_ = at->second;
                trim_ = nodes_.size();
                nodes_.push_back(Node{StepKind::EventTrim, nodes_[at->second].stage, 0, 0, "event-trim"});
            }
        }
    }

    void AddEdge(std::size_t from, std::size_t to) {
        if (direct_->Test(from, to)) return;
        direct_->Set(from, to);
        succ_[from].push_back(to);
    }

    void StageEdges() {
        for (std::size_t node = 0; node < nodes_.size(); ++node) {
            const std::size_t stage = nodes_[node].stage;
            if (node == stageEnd_[stage] || node == trim_) continue;
            AddEdge(node, stageEnd_[stage]);
            if (stage > 0) AddEdge(stageEnd_[stage - 1], node);
        }
        for (std::size_t stage = 1; stage < stageEnd_.size(); ++stage) {
            AddEdge(stageEnd_[stage - 1], stageEnd_[stage]);
        }
        if (trim_ != kNone) {
            AddEdge(trimAfter_, trim_);
            const std::size_t stage = nodes_[trim_].stage;
            if (trimAfter_ != stageEnd_[stage]) {
                AddEdge(trim_, stageEnd_[stage]);
            } else if (stage + 1 < stageEnd_.size()) {
                for (std::size_t node = 0; node < nodes_.size(); ++node) {
                    if (nodes_[node].stage == stage + 1 && node != trim_) AddEdge(trim_, node);
                }
            }
        }
    }

    // Nodes a before / after name stands for: a System mounted here, a sync
    // point, or every member of a set of this schedule.
    std::vector<std::size_t> Resolve(const std::string& name) const {
        if (const auto system = systemByName_.find(name); system != systemByName_.end()) return {system->second};
        if (const auto sync = syncNodes_.find(name); sync != syncNodes_.end() && sync->second != trim_) {
            // A stage end is not a before / after target by stage name.
            bool stageName = std::find(s_.stages.begin(), s_.stages.end(), name) != s_.stages.end();
            if (!stageName) return {sync->second};
        }
        if (const auto set = setMembers_.find(name); set != setMembers_.end()) return set->second;
        return {};
    }

    bool Exists(const std::string& name) const {
        if (systemByName_.count(name) != 0 || setMembers_.count(name) != 0) return true;
        const auto sync = syncNodes_.find(name);
        return sync != syncNodes_.end() && std::find(s_.stages.begin(), s_.stages.end(), name) == s_.stages.end();
    }

    void Order(std::size_t from, std::size_t to, const std::string& subject, const std::string& source) {
        if (from == to) {
            Error(StartupCode::DE6, "D.2", subject + " is ordered before itself in schedule '" + s_.name + "'",
                  {nodes_[from].name}, {source});
            return;
        }
        const std::size_t sf = nodes_[from].stage;
        const std::size_t st = nodes_[to].stage;
        if (sf > st) {
            Error(StartupCode::DE5, "D.2",
                  "'" + nodes_[from].name + "' (stage " + s_.stages[sf] + ") is ordered before '" + nodes_[to].name +
                      "' (stage " + s_.stages[st] + ") in schedule '" + s_.name + "', against stage order",
                  {nodes_[from].name, nodes_[to].name}, {source});
            return;
        }
        if (IsSystem(from) && IsSystem(to)) {
            const auto pair = std::make_pair(nodes_[from].system, nodes_[to].system);
            if (declaredPairs_.insert(pair).second) {
                const AccessSet& u = SystemOf(from).access;
                const AccessSet& v = SystemOf(to).access;
                std::vector<AccessKey> touched = ReadOrMembership(v);
                const std::vector<AccessKey> writtenV = Written(v);
                touched.insert(touched.end(), writtenV.begin(), writtenV.end());
                std::sort(touched.begin(), touched.end());
                const bool dataflow = Intersects<AccessKey>(Written(u), touched, nullptr);
                edges_.push_back(DeclaredEdge{nodes_[from].name, nodes_[to].name, dataflow});
            }
        }
        if (sf == st) AddEdge(from, to);
    }

    void DeclaredEdges() {
        for (const auto& [seq, node] : mounted_) systemByName_[data_.systems[seq].options.name] = node;
        for (const auto& [seq, node] : mounted_) {
            const Mount& mount = data_.systems[seq].options.mounts[nodes_[node].mount];
            for (const std::string& set : mount.sets) setMembers_[set].push_back(node);
        }
        std::set<std::string> knownSets;
        for (const SetRecord& set : data_.sets) {
            if (set.options.schedule == s_.name) knownSets.insert(set.options.name);
        }
        for (const auto& [seq, node] : mounted_) {
            const SystemRecord& system = data_.systems[seq];
            const Mount& mount = system.options.mounts[nodes_[node].mount];
            const std::string subject = "System '" + system.options.name + "'";
            const std::string source = SourceText(system.source);
            for (const std::string& set : mount.sets) {
                if (knownSets.count(set) == 0) {
                    Error(StartupCode::DE4, "D.2", subject + " joins set '" + set + "', which schedule '" + s_.name +
                                                       "' does not have", {system.options.name, set}, {source});
                }
            }
            const auto targets = [&](const std::vector<std::string>& names, bool before) {
                for (const std::string& name : names) {
                    if (!Exists(name)) {
                        Error(StartupCode::DE4, "D.2",
                              subject + " is ordered " + (before ? "before" : "after") + " '" + name +
                                  "', which schedule '" + s_.name + "' does not have (removed plugin? change the "
                                  "reference explicitly)",
                              {system.options.name, name}, {source});
                        continue;
                    }
                    for (std::size_t target : Resolve(name)) {
                        before ? Order(node, target, subject, source) : Order(target, node, subject, source);
                    }
                }
            };
            targets(mount.before, true);
            targets(mount.after, false);
        }
        for (const SetRecord& set : data_.sets) {
            if (set.options.schedule != s_.name) continue;
            const auto members = setMembers_.find(set.options.name);
            if (members == setMembers_.end()) continue;
            const std::string subject = "set '" + set.options.name + "'";
            const auto targets = [&](const std::vector<std::string>& names, bool before) {
                for (const std::string& name : names) {
                    if (!Exists(name)) {
                        Error(StartupCode::DE4, "D.2",
                              subject + " is ordered " + (before ? "before" : "after") + " '" + name +
                                  "', which schedule '" + s_.name + "' does not have",
                              {set.options.name, name});
                        continue;
                    }
                    for (std::size_t member : members->second) {
                        for (std::size_t target : Resolve(name)) {
                            before ? Order(member, target, subject, {}) : Order(target, member, subject, {});
                        }
                    }
                }
            };
            targets(set.options.before, true);
            targets(set.options.after, false);
        }
    }

    bool TopologicalOrder(std::vector<std::size_t>& order) {
        std::vector<std::size_t> indegree(nodes_.size(), 0);
        for (const auto& list : succ_) for (std::size_t to : list) ++indegree[to];
        std::set<std::size_t> ready;
        for (std::size_t node = 0; node < nodes_.size(); ++node) if (indegree[node] == 0) ready.insert(node);
        while (!ready.empty()) {
            const std::size_t node = *ready.begin();
            ready.erase(ready.begin());
            order.push_back(node);
            for (std::size_t to : succ_[node]) if (--indegree[to] == 0) ready.insert(to);
        }
        if (order.size() == nodes_.size()) return true;
        // Report one cycle among the remaining nodes: walk successors that are
        // still blocked from the lowest blocked node until a node repeats.
        std::vector<bool> blocked(nodes_.size(), false);
        for (std::size_t node = 0; node < nodes_.size(); ++node) blocked[node] = indegree[node] > 0;
        std::size_t start = 0;
        while (!blocked[start]) ++start;
        std::vector<std::size_t> path;
        std::vector<std::size_t> seenAt(nodes_.size(), kNone);
        std::size_t current = start;
        while (seenAt[current] == kNone) {
            seenAt[current] = path.size();
            path.push_back(current);
            std::size_t next = kNone;
            for (std::size_t to : succ_[current]) {
                if (blocked[to] && (next == kNone || to < next)) next = to;
            }
            current = next;
        }
        std::vector<std::string> cycle;
        std::string text;
        for (std::size_t i = seenAt[current]; i < path.size(); ++i) {
            cycle.push_back(nodes_[path[i]].name);
            text += nodes_[path[i]].name + " -> ";
        }
        text += nodes_[current].name;
        Error(StartupCode::DE6, "D.2", "order cycle in schedule '" + s_.name + "': " + text, cycle);
        return false;
    }

    void Closure(const std::vector<std::size_t>& order) {
        reach_ = std::make_unique<BitRows>(nodes_.size());
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            for (std::size_t to : succ_[*it]) {
                reach_->Set(*it, to);
                reach_->OrRow(*it, to);
            }
        }
    }

    void AddTieEdge(std::size_t first, std::size_t second) {
        AddEdge(first, second);
        for (std::size_t x = 0; x < nodes_.size(); ++x) {
            if (x == first || reach_->Test(x, first)) {
                reach_->Set(x, second);
                reach_->OrRow(x, second);
            }
        }
    }

    void Conflicts() {
        for (std::size_t stage = 0; stage < s_.stages.size(); ++stage) {
            std::vector<std::size_t> members;
            for (std::size_t node = 0; node < nodes_.size(); ++node) {
                if (IsSystem(node) && nodes_[node].stage == stage) members.push_back(node);
            }
            std::sort(members.begin(), members.end(),
                      [&](std::size_t a, std::size_t b) { return nodes_[a].system < nodes_[b].system; });
            for (std::size_t i = 0; i < members.size(); ++i) {
                for (std::size_t j = i + 1; j < members.size(); ++j) {
                    const std::size_t a = members[i];
                    const std::size_t b = members[j];
                    const SystemRecord& sa = SystemOf(a);
                    const SystemRecord& sb = SystemOf(b);
                    const std::string on = ConflictBetween(data_, sa, sb);
                    if (on.empty()) continue;
                    conflicts_.emplace_back(a, b);
                    if (reach_->Test(a, b) || reach_->Test(b, a)) continue;  // data conflict, ordered
                    if (sa.options.zone == Zone::C || sb.options.zone == Zone::C) {
                        Error(StartupCode::DE8, "D-3",
                              "Systems '" + sa.options.name + "' and '" + sb.options.name + "' in stage " +
                                  s_.stages[stage] + " of schedule '" + s_.name + "' conflict on " + on +
                                  " with no order, and one is zone C: declare before or after",
                              {sa.options.name, sb.options.name, on}, {SourceText(sa.source), SourceText(sb.source)});
                        continue;
                    }
                    AddTieEdge(a, b);
                    data_.ties.push_back(TieBreak{s_.name, sa.options.name, sb.options.name});
                }
            }
        }
    }

    // D-E16 (spec A.1 Derived, S15): in a rollback-enabled schedule every
    // reader of a Derived column or resource has an immediate writer before it.
    void DerivedOrder() {
        if (!s_.rollbackEnabled) return;
        for (std::size_t r = 0; r < nodes_.size(); ++r) {
            if (!IsSystem(r)) continue;
            for (const AccessKey& key : SystemOf(r).access.reads) {
                const bool derived =
                    (key.kind == AccessKind::Component && data_.components[key.id].rollback == Rollback::Derived) ||
                    (key.kind == AccessKind::Resource && data_.resources[key.id].rollback == Rollback::Derived);
                if (!derived) continue;
                bool written = false;
                for (std::size_t w = 0; w < nodes_.size() && !written; ++w) {
                    if (!IsSystem(w) || !reach_->Test(w, r)) continue;
                    const auto& writes = SystemOf(w).access.writes;
                    written = std::binary_search(writes.begin(), writes.end(), key);
                }
                if (!written) {
                    Error(StartupCode::DE16, "A.1",
                          "System '" + SystemOf(r).options.name + "' reads Derived " + KeyName(data_, key) +
                              " in rollback-enabled schedule '" + s_.name + "' with no immediate writer ordered before it",
                          {SystemOf(r).options.name}, {SourceText(SystemOf(r).source)});
                }
            }
        }
    }

    void MakePlan() {
        std::vector<std::size_t> indegree(nodes_.size(), 0);
        for (const auto& list : succ_) for (std::size_t to : list) ++indegree[to];
        // Key: (0, registrationSeq) for a System, (1, node) for a sync point or the trim.
        std::set<std::pair<int, std::size_t>> ready;
        const auto key = [&](std::size_t node) {
            return IsSystem(node) ? std::make_pair(0, static_cast<std::size_t>(nodes_[node].system)) : std::make_pair(1, node);
        };
        std::map<std::pair<int, std::size_t>, std::size_t> nodeOf;
        for (std::size_t node = 0; node < nodes_.size(); ++node) {
            nodeOf[key(node)] = node;
            if (indegree[node] == 0) ready.insert(key(node));
        }
        Plan plan;
        plan.schedule = s_.name;
        plan.domain = s_.domain;
        std::vector<std::size_t> position(nodes_.size(), 0);
        while (!ready.empty()) {
            const std::size_t node = nodeOf[*ready.begin()];
            ready.erase(ready.begin());
            position[node] = plan.steps.size();
            plan.steps.push_back(Step{nodes_[node].kind, IsSystem(node) ? nodes_[node].system : 0U, nodes_[node].name});
            for (std::size_t to : succ_[node]) if (--indegree[to] == 0) ready.insert(key(to));
        }
        GYO_ASSERT(plan.steps.size() == nodes_.size());
        // Spec D-2: every conflict edge has a fixed order in the Plan.
        for (const auto& [a, b] : conflicts_) {
            GYO_ASSERT(reach_->Test(a, b) ? position[a] < position[b] : position[b] < position[a]);
        }
        plan.declaredEdges = std::move(edges_);
        data_.plans.push_back(std::move(plan));
    }

    static constexpr std::size_t kNone = ~std::size_t{0};

    RegistryData& data_;
    const ScheduleOptions& s_;
    std::vector<Node> nodes_;
    std::vector<std::size_t> stageEnd_;
    std::map<std::string, std::size_t> syncNodes_;
    std::map<RegistrationSeq, std::size_t> mounted_;
    std::map<std::string, std::size_t> systemByName_;
    std::map<std::string, std::vector<std::size_t>> setMembers_;
    std::size_t trim_ = kNone;
    std::size_t trimAfter_ = kNone;
    std::unique_ptr<BitRows> direct_;
    std::unique_ptr<BitRows> reach_;
    std::vector<std::vector<std::size_t>> succ_;
    std::set<std::pair<RegistrationSeq, RegistrationSeq>> declaredPairs_;
    std::vector<DeclaredEdge> edges_;
    std::vector<std::pair<std::size_t, std::size_t>> conflicts_;
    bool failed_ = false;
};

} // namespace

void BuildPlans(RegistryData& data) {
    for (const ScheduleRecord& record : data.schedules) {
        ScheduleBuilder builder(data, record);
        builder.Build();
    }
}

} // namespace GYO::Kernel::Detail
