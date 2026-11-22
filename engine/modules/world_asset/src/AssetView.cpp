#include "engine/world_asset/AssetView.hpp"

#include <algorithm>
#include <typeindex>

namespace GYO::Engine::WorldAsset {

namespace {

void AddOnce(std::vector<Asset::AssetId>& list, const Asset::AssetId& id) {
    if (std::find(list.begin(), list.end(), id) == list.end()) list.push_back(id);
}

bool Finished(const Asset::AssetState state) {
    return state == Asset::AssetState::Loaded || state == Asset::AssetState::Failed;
}

const Asset::AssetId& EmptyId() {
    static const Asset::AssetId empty;
    return empty;
}

} // namespace

const Asset::AssetId& UntypedHandle::Id() const noexcept {
    return body_ ? body_->id : EmptyId();
}

const Asset::AssetRecord* AssetView::Matching(const Asset::Detail::HandleBody* body) const {
    // No core (names nothing, or refused at the request level): no item.
    if (body == nullptr || body->failure || body->life == 0) return nullptr;
    const Asset::AssetRecord* record = snapshot_.Find(body->id);
    // Another life of the id is not this Handle's item (5.7, 1622 S-5).
    if (record != nullptr && record->life == body->life) return record;
    return nullptr;
}

Asset::AssetState AssetView::StateOf(const Asset::Detail::HandleBody* body) const {
    if (body == nullptr) return Asset::AssetState::NotRequested;
    if (body->failure) return Asset::AssetState::Failed;
    const Asset::AssetRecord* record = Matching(body);
    return record != nullptr ? record->state : Asset::AssetState::Queued;
}

const Asset::AssetLoadFailure* AssetView::FailureOf(const Asset::Detail::HandleBody* body) const {
    if (body == nullptr) return nullptr;
    if (body->failure) return &*body->failure;
    const Asset::AssetRecord* record = Matching(body);
    return record != nullptr && record->failure ? &*record->failure : nullptr;
}

const void* AssetView::ValueOf(const Asset::AssetRecord* record, const std::type_info& type) {
    if (record == nullptr || record->state != Asset::AssetState::Loaded || !record->value || !record->valueType ||
        *record->valueType != std::type_index(type)) {
        return nullptr;
    }
    return record->value.get();
}

const std::string* AssetView::DigestOf(const Asset::AssetRecord* record) {
    return record != nullptr && record->sourceDigest ? &*record->sourceDigest : nullptr;
}

AssetReadiness AssetView::Ready(const std::span<const UntypedHandle> handles) const {
    AssetReadiness readiness;
    for (const UntypedHandle& handle : handles) {
        const Asset::Detail::HandleBody* body = handle.body_.get();
        const Asset::AssetState state = StateOf(body);
        const Asset::AssetId& id = handle.Id();
        if (!id.Empty()) AddOnce(readiness.ids, id);
        if (body != nullptr && body->failure) {
            // Refused at the request level: finished, failed, no item and no dependency.
            ++readiness.failed;
            readiness.failures.push_back(AssetReadyFailure{id, {}, id, {}, body->failure->code});
            readiness.complete = false;
            continue;
        }
        const Asset::AssetRecord* record = Matching(body);
        if (record == nullptr) {
            // Names nothing (NotRequested), or a life this view has not seen (Queued).
            readiness.allFinished = false;
            readiness.complete = false;
            readiness.dependenciesFinished = false;
            continue;
        }
        if (state == Asset::AssetState::Loaded) {
            ++readiness.loaded;
        } else if (state == Asset::AssetState::Failed) {
            ++readiness.failed;
            readiness.failures.push_back(AssetReadyFailure{id, {}, id, {}, record->failure->code});
        }
        if (!Finished(state)) readiness.allFinished = false;
        const Asset::DependencyStatus dependencies = snapshot_.WithDependencies(id);
        for (const Asset::AssetId& reached : dependencies.ids) AddOnce(readiness.ids, reached);
        for (const Asset::AssetId& skipped : dependencies.skipped) AddOnce(readiness.skipped, skipped);
        for (const Asset::AssetId& waiting : dependencies.waiting) AddOnce(readiness.waitingForLoader, waiting);
        for (const Asset::DependencyFailure& failure : dependencies.failures) {
            readiness.failures.push_back(
                AssetReadyFailure{id, failure.parent, failure.reference, failure.writtenPath, failure.code});
        }
        if (state != Asset::AssetState::Loaded || dependencies.state != Asset::DependencyState::Complete) {
            readiness.complete = false;
        }
        if (dependencies.state == Asset::DependencyState::Incomplete) readiness.dependenciesFinished = false;
    }
    return readiness;
}

std::size_t AssetView::Unfinished(const std::span<const UntypedHandle> handles) const {
    std::size_t unfinished = 0;
    for (const UntypedHandle& handle : handles) {
        if (!Finished(StateOf(handle.body_.get()))) ++unfinished;
    }
    return unfinished;
}

} // namespace GYO::Engine::WorldAsset
