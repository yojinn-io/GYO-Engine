#include "engine/asset/AssetSnapshot.hpp"

#include <unordered_set>
#include <utility>

namespace GYO::Engine::Asset {

AssetSnapshot::AssetSnapshot(const std::uint64_t version, std::vector<AssetRecord> records)
    : version_(version), records_(std::move(records)) {
    byId_.reserve(records_.size());
    for (std::size_t index = 0; index < records_.size(); ++index) byId_.emplace(records_[index].id, index);
}

const AssetRecord* AssetSnapshot::Find(const AssetId& id) const {
    const auto found = byId_.find(id);
    return found == byId_.end() ? nullptr : &records_[found->second];
}

AssetState AssetSnapshot::State(const AssetId& id) const {
    const AssetRecord* record = Find(id);
    return record == nullptr ? AssetState::NotRequested : record->state;
}

DependencyStatus AssetSnapshot::WithDependencies(const AssetId& id) const {
    DependencyStatus status;
    const AssetRecord* root = Find(id);
    if (root == nullptr) return status;
    bool unfinished = false;
    bool rootFailed = false;
    std::unordered_set<AssetId> reached{id};
    std::vector<const AssetRecord*> stack{root};
    status.ids.push_back(id);
    while (!stack.empty()) {
        const AssetRecord* record = stack.back();
        stack.pop_back();
        switch (record->state) {
        case AssetState::Loaded: break;
        case AssetState::Failed:
            // A dependency's failure is listed at the edge that reached it.
            if (record == root) rootFailed = true;
            continue;
        case AssetState::WaitingForLoader:
            status.waiting.push_back(record->id);
            unfinished = true;
            continue;
        default: unfinished = true; continue;
        }
        for (const AssetEdge& edge : record->edges) {
            switch (edge.status) {
            case EdgeStatus::Skipped: status.skipped.push_back(edge.target); break;
            case EdgeStatus::Failed:
            case EdgeStatus::Cyclic:
                status.failures.push_back(
                    DependencyFailure{record->id, edge.target, edge.reference.writtenPath, *edge.failure});
                break;
            case EdgeStatus::Held: {
                const AssetRecord* target = Find(edge.target);
                if (target == nullptr) {
                    // Held edges keep their target alive; a copy without it
                    // has not seen it yet.
                    unfinished = true;
                    break;
                }
                if (target->state == AssetState::Failed) {
                    status.failures.push_back(DependencyFailure{record->id, edge.target, edge.reference.writtenPath,
                                                                target->failure->code});
                }
                if (reached.insert(target->id).second) {
                    status.ids.push_back(target->id);
                    stack.push_back(target);
                }
                break;
            }
            }
        }
    }
    if (unfinished) {
        status.state = DependencyState::Incomplete;
    } else if (rootFailed || !status.failures.empty()) {
        status.state = DependencyState::Failed;
    } else {
        status.state = DependencyState::Complete;
    }
    return status;
}

std::vector<FailureRecord> AssetSnapshot::Failures() const {
    std::vector<FailureRecord> failures;
    for (const AssetRecord& record : records_) {
        if (record.state == AssetState::Failed) {
            failures.push_back(FailureRecord{record.id, record.attempts, record.failure->code});
        }
    }
    return failures;
}

std::vector<AssetId> AssetSnapshot::WaitingForLoader() const {
    std::vector<AssetId> waiting;
    for (const AssetRecord& record : records_) {
        if (record.state == AssetState::WaitingForLoader) waiting.push_back(record.id);
    }
    return waiting;
}

} // namespace GYO::Engine::Asset
