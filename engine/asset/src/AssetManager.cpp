#include "engine/asset/AssetManager.hpp"

#include "engine/asset/AssetCatalog.hpp"

#include <algorithm>

namespace Engine::Asset {

AssetManager::AssetManager(AssetCatalog& catalog,
                           Loading::AssetPipeline& pipeline,
                           Core::AssetStorage& storage,
                           Core::AssetLifetime& lifetime,
                           Core::AssetCachePolicy& cachePolicy,
                           Core::AssetStatistics* stats,
                           HotReload::AssetWatcher* watcher)
    : catalog_(catalog), pipeline_(pipeline), storage_(storage), lifetime_(lifetime),
      cachePolicy_(cachePolicy), stats_(stats), watcher_(watcher) {}

void AssetManager::SetOptions(Options opt) { opt_ = opt; }
const AssetManager::Options& AssetManager::GetOptions() const noexcept { return opt_; }
void AssetManager::BeginFrame(std::uint64_t frameIndex) { frame_ = frameIndex; }

void AssetManager::Update() {
    if (opt_.enableHotReload && watcher_) ProcessHotReload_();
    ProcessQueue_();
}

Base::Result<AssetHandle, AssetError>
AssetManager::Load(const AssetId& id, const AssetRequest& request) {
    using Result = Base::Result<AssetHandle, AssetError>;
    // Reserved hints must never silently change the request's meaning, including
    // on cache hits. Reject before catalog access, records, pinning or references.
    if (request.priority != 0 || request.keepAliveFramesOverride != 0) {
        return Result::Err(AssetError::Make(AssetErrorCode::UnsupportedRequest,
            "AssetManager: nonzero priority and per-request TTL are not supported"));
    }
    if (stats_) stats_->OnLoadRequest();
    auto entry = ResolveEntry_(id, request);
    if (!entry) return Result::Err(std::move(entry.error()));

    auto pending = std::find_if(queue_.begin(), queue_.end(),
        [&](const PendingLoad& job) { return job.id == id; });
    const auto* existing = storage_.Find(id);
    if (pending != queue_.end() && (!existing || !existing->IsLoading() ||
        existing->candidateGeneration != pending->generation)) {
        // Low-level forced eviction/Clear retires all issued generations. Do
        // not let the old queue entry attach its handle to a new incarnation.
        queue_.erase(pending);
        pending = queue_.end();
    }
    const bool cached = existing && existing->IsReady() && !request.IsReload();
    if (pending == queue_.end() && !cached && !storage_.CanReserveGeneration(id)) {
        return Result::Err(AssetError::Make(AssetErrorCode::GenerationExhausted,
            "AssetManager: all handle generations for this asset have been issued"));
    }
    auto& record = GetOrCreateRecord_(id, entry.value());
    const auto acquire = [&](std::uint32_t generation) {
        record.AddReference(generation);
        lifetime_.Touch(id, frame_);
        return Result::Ok(AssetHandle::Make(id, generation));
    };

    // A normal lookup can keep using the published asset during replacement.
    // An explicit different path is not allowed to overwrite a pending request.
    if (record.IsReady() && !request.IsReload() &&
        (pending == queue_.end() || (record.resolvedPath == entry.value().resolvedPath &&
                                    record.type == entry.value().type))) {
        if (request.pin) lifetime_.Pin(id);
        if (stats_) stats_->OnCacheHit(id);
        return acquire(record.generation);
    }

    if (pending != queue_.end()) {
        if (!Compatible_(*pending, entry.value(), request)) {
            return Result::Err(AssetError::Make(AssetErrorCode::RequestInProgress,
                "AssetManager: another request for this asset is already queued"));
        }
        if (request.pin) lifetime_.Pin(id);
        if (request.IsAsync()) return acquire(pending->generation);

        // The queue is polled on this thread: promote the existing operation,
        // remove it first, and execute it exactly once with its original inputs.
        PendingLoad job = std::move(*pending);
        queue_.erase(pending);
        auto completed = CompleteLoad_(job);
        if (!completed && !(request.fallback == AssetRequest::Fallback::KeepOldIfAny && record.IsReady()))
            return Result::Err(std::move(completed.error()));
        return acquire(record.generation);
    }

    if (request.pin) lifetime_.Pin(id);
    if (!record.IsReady() && stats_) stats_->OnCacheMiss();
    PendingLoad job{id, request, std::move(entry.value()), record.ReserveGeneration()};
    if (request.IsAsync()) {
        const auto generation = job.generation;
        queue_.push_back(std::move(job));
        return acquire(generation);
    }

    auto completed = CompleteLoad_(job);
    if (!completed && !(request.fallback == AssetRequest::Fallback::KeepOldIfAny && record.IsReady()))
        return Result::Err(std::move(completed.error()));
    return acquire(record.generation);
}

bool AssetManager::Acquire(const AssetHandle& handle) {
    auto* record = FindRecord_(handle);
    if (!record || record->StateFor(handle.generation()) == AssetState::Unloaded) return false;
    record->AddReference(handle.generation());
    lifetime_.Touch(handle.id(), frame_);
    return true;
}

void AssetManager::Release(const AssetHandle& handle) {
    if (auto* record = FindRecord_(handle)) {
        // Stale handles still balance their own generation's references.
        (void)record->ReleaseReference(handle.generation());
    }
}

AssetState AssetManager::GetState(const AssetHandle& handle) const {
    const auto* record = FindRecordConst_(handle);
    return record ? record->StateFor(handle.generation()) : AssetState::Unloaded;
}

const AssetError* AssetManager::GetError(const AssetHandle& handle) const {
    const auto* record = FindRecordConst_(handle);
    return record ? record->ErrorFor(handle.generation()) : nullptr;
}

bool AssetManager::EvictIfPossible(const AssetId& id) {
    auto* record = storage_.Find(id);
    if (!record || record->IsLoading()) return false;
    if (!cachePolicy_.IsEvictable(*record, lifetime_, frame_)) return false;
    lifetime_.OnEvicted(id);
    if (stats_) stats_->OnEvict(id);
    storage_.EraseIf(id, true);
    return true;
}

void AssetManager::Watch(const AssetId& id, std::string resolvedPath) {
    if (watcher_) watcher_->Watch(id, std::move(resolvedPath));
}

void AssetManager::Unwatch(const AssetId& id) {
    if (watcher_) watcher_->Unwatch(id);
}

Base::Result<AssetManager::ResolvedEntry, AssetError>
AssetManager::ResolveEntry_(const AssetId& id, const AssetRequest& request) {
    using Result = Base::Result<ResolvedEntry, AssetError>;
    if (stats_) stats_->OnCatalogLookup();
    const auto* entry = catalog_.Find(id);
    if (!entry) {
        if (stats_) stats_->OnCatalogMiss();
        return Result::Err(AssetError::Make(AssetErrorCode::CatalogNotFound,
            "AssetCatalog: id not found"));
    }
    if (request.useTypeHint && request.expectedType.value != 0 && request.expectedType != entry->type) {
        return Result::Err(AssetError::Make(AssetErrorCode::InvalidCatalogEntry,
            "AssetRequest: expectedType mismatch"));
    }
    ResolvedEntry resolved{entry->type,
        request.overridePath.empty() ? entry->resolvedPath : request.overridePath};
    if (resolved.resolvedPath.empty()) {
        return Result::Err(AssetError::Make(AssetErrorCode::InvalidPath,
            "AssetCatalog: resolvedPath is empty"));
    }
    return Result::Ok(std::move(resolved));
}

Core::AssetRecord& AssetManager::GetOrCreateRecord_(const AssetId& id, const ResolvedEntry& entry) {
    return storage_.GetOrCreate(id, entry.type, entry.resolvedPath);
}

bool AssetManager::Compatible_(const PendingLoad& job, const ResolvedEntry& entry,
                               const AssetRequest& request) {
    // Scheduling and pinning do not change the bytes/loader operation. Type
    // hints have already been checked against the catalog before this point.
    return job.entry.type == entry.type && job.entry.resolvedPath == entry.resolvedPath &&
           job.req.mode == request.mode && job.req.fallback == request.fallback &&
           job.req.tag == request.tag;
}

Base::Result<void, AssetError> AssetManager::CompleteLoad_(const PendingLoad& job) {
    using Result = Base::Result<void, AssetError>;
    auto* record = storage_.Find(job.id);
    // A forcibly removed record must not be recreated by a stale queued job.
    if (!record || !record->IsLoading() || record->candidateGeneration != job.generation) {
        return Result::Err(AssetError::Make(AssetErrorCode::RequestInProgress,
            "AssetManager: queued generation is no longer pending"));
    }
    const bool hadReadyAsset = record->IsReady();
    Loading::LoadContext context;
    context.id = job.id;
    context.type = job.entry.type;
    context.resolvedPath = job.entry.resolvedPath;
    context.request = &job.req;
    context.statistics = stats_;
    context.nowFrame = frame_;
    auto loaded = pipeline_.Load(context);
    if (job.req.IsReload() && hadReadyAsset && stats_) stats_->OnReload(job.id);

    if (!loaded) {
        const auto error = std::move(loaded.error());
        record->failedGeneration = job.generation;
        record->failedError = error;
        record->ClearCandidate();
        if (job.req.fallback == AssetRequest::Fallback::KeepOldIfAny && hadReadyAsset) {
            // Published generation stays readable. The candidate has its own
            // failure so its caller can observe this load's actual outcome.
            record->error = error;
        } else {
            record->generation = job.generation;
            record->SetFailed(error);
        }
        return Result::Err(error);
    }

    record->generation = job.generation;
    record->type = job.entry.type;
    record->resolvedPath = job.entry.resolvedPath;
    record->ClearCandidate();
    record->SetReady(std::move(loaded.value()));
    lifetime_.OnLoaded(job.id, frame_);
    return Result::Ok();
}

void AssetManager::ProcessQueue_() {
    auto budget = opt_.maxLoadsPerFrame;
    while (budget > 0 && !queue_.empty()) {
        PendingLoad job = std::move(queue_.front());
        queue_.pop_front();
        (void)CompleteLoad_(job);
        --budget;
    }
}

void AssetManager::ProcessHotReload_() {
    for (const auto& change : watcher_->Poll()) {
        auto request = AssetRequest::Reload();
        request.sync = AssetRequest::SyncWith::Async;
        request.overridePath = change.resolvedPath;
        request.fallback = opt_.reloadKeepOldIfAny ? AssetRequest::Fallback::KeepOldIfAny
                                                  : AssetRequest::Fallback::None;
        // Use exactly the public admission, reservation and merge rules. The
        // watcher owns no handle reference; the pending operation blocks eviction.
        auto queued = Load(change.id, request);
        if (queued) Release(queued.value());
    }
}

Core::AssetRecord* AssetManager::FindRecord_(const AssetHandle& handle) {
    return storage_.Find(handle.id());
}

const Core::AssetRecord* AssetManager::FindRecordConst_(const AssetHandle& handle) const {
    return storage_.Find(handle.id());
}

} // namespace Engine::Asset
