#include "engine/asset/AssetManager.hpp"

#include "engine/asset/AssetCatalog.hpp" // AssetCatalog 実装に合わせて include

namespace Engine::Asset {

    AssetManager::AssetManager(AssetCatalog& catalog,
                               Loading::AssetPipeline& pipeline,
                               Core::AssetStorage& storage,
                               Core::AssetLifetime& lifetime,
                               Core::AssetCachePolicy& cachePolicy,
                               Core::AssetStatistics* stats,
                               HotReload::AssetWatcher* watcher)
        : catalog_(catalog)
        , pipeline_(pipeline)
        , storage_(storage)
        , lifetime_(lifetime)
        , cachePolicy_(cachePolicy)
        , stats_(stats)
        , watcher_(watcher) {}

    void AssetManager::SetOptions(Options opt) { opt_ = opt; }
    const AssetManager::Options& AssetManager::GetOptions() const noexcept { return opt_; }

    void AssetManager::BeginFrame(std::uint64_t frameIndex) {
        frame_ = frameIndex;
    }

    void AssetManager::Update() {
        if (opt_.enableHotReload && watcher_) {
            ProcessHotReload_();
        }
        ProcessQueue_();
    }

    // ---------------- public API ----------------

    Base::Result<AssetHandle, AssetError>
    AssetManager::Load(const AssetId& id, const AssetRequest& request) {
        if (stats_) stats_->OnLoadRequest();

        // 1) catalog から解決
        auto entryR = ResolveEntry_(id, request);
        if (!entryR) return Base::Result<AssetHandle, AssetError>::Err(std::move(entryR.error()));
        const ResolvedEntry e = std::move(entryR.value());

        // 2) record 準備
        Core::AssetRecord& rec = GetOrCreateRecord_(id, e);

        // 3) 既に Ready で reload しないなら、キャッシュヒット
        const bool wantReload = request.IsReload();
        if (rec.IsReady() && !wantReload) {
            if (stats_) stats_->OnCacheHit(id);
            lifetime_.Touch(id, frame_);

            // Acquire 相当
            rec.AddReference(rec.generation);

            // typed handle を使いたい場合は、Load<T>() を別途用意して MakeTyped<T>() を返すのが自然
            return Base::Result<AssetHandle, AssetError>::Ok(
                AssetHandle::Make(id, rec.generation)
            );
        }

        if (!rec.IsReady() && !wantReload) {
            if (stats_) stats_->OnCacheMiss();
        }

        // 4) pin / TTL（必要ならここで適用）
        if (request.pin) {
            lifetime_.Pin(id);
        }
        if (request.keepAliveFramesOverride != 0) {
            // keepAliveFramesOverride は policy 側に “この要求だけ” 反映したいが、
            // ここでは “情報として保持” する場所がないため、運用で policy を切り替えるか、
            // lifetime 側に別mapを持たせて拡張するのがよい。
        }

        // 5) Async ならキューへ
        if (request.IsAsync()) {
            // すでに Loading 中なら二重投入しない
            if (!rec.IsLoading()) {
                EnqueueLoad_(id, request);
                rec.MarkLoading();
                if (stats_) stats_->OnLoadStart();
            }

            // Acquire 相当：呼んだ側はこのhandleを保持する前提
            rec.AddReference(rec.generation);

            return Base::Result<AssetHandle, AssetError>::Ok(
                AssetHandle::Make(id, rec.generation)
            );
        }

        // 6) Sync：その場でロード
        auto loadR = DoLoadSync_(rec, e, request, rec.IsReady());
        if (!loadR) {
            // reload fallback が KeepOldIfAny で、旧データがある場合は rec が Ready のまま
            // その場合は “成功としてhandleを返す” のが開発UX的に強い
            if (request.fallback == AssetRequest::Fallback::KeepOldIfAny && rec.IsReady()) {
                // 失敗理由は rec.error に残す（※ Ready でも error を持つのは「例外運用」）
                return Base::Result<AssetHandle, AssetError>::Ok(
                    AssetHandle::Make(id, rec.generation)
                );
            }
            return Base::Result<AssetHandle, AssetError>::Err(std::move(loadR.error()));
        }

        // Touch / Loaded
        lifetime_.OnLoaded(id, frame_);

        // Acquire 相当
        rec.AddReference(rec.generation);

        return Base::Result<AssetHandle, AssetError>::Ok(
            AssetHandle::Make(id, rec.generation)
        );
    }

    bool AssetManager::Acquire(const AssetHandle& h) {
        Core::AssetRecord* rec = FindRecord_(h);
        if (!rec) return false;
        if (rec->generation != h.generation()) return false;
        rec->AddReference(rec->generation);
        lifetime_.Touch(h.id(), frame_);
        return true;
    }

    void AssetManager::Release(const AssetHandle& h) {
        Core::AssetRecord* rec = FindRecord_(h);
        if (!rec) return;

        // A successful reload deliberately makes old handles stale for reads.
        // Their Load/Acquire references still have to be released, however.
        // AssetRecord keeps generation-scoped counts so a stale or repeated
        // release cannot consume a reference owned by the current generation.
        (void)rec->ReleaseReference(h.generation());
        // 解放後の eviction は「上位が EvictIfPossible を呼ぶ」運用にしておく（自動evictは後で）
    }

    AssetState AssetManager::GetState(const AssetHandle& h) const {
        const Core::AssetRecord* rec = FindRecordConst_(h);
        if (!rec) return AssetState::Unloaded;
        if (rec->generation != h.generation()) return AssetState::Unloaded; // stale は Unloaded 扱い
        return rec->state;
    }

    const AssetError* AssetManager::GetError(const AssetHandle& h) const {
        const Core::AssetRecord* rec = FindRecordConst_(h);
        if (!rec) return nullptr;
        if (rec->generation != h.generation()) return nullptr;
        if (rec->error.ok()) return nullptr;
        return &rec->error;
    }

    bool AssetManager::EvictIfPossible(const AssetId& id) {
        Core::AssetRecord* rec = storage_.Find(id);
        if (!rec) return false;

        if (!cachePolicy_.IsEvictable(*rec, lifetime_, frame_)) return false;

        // record を消す前に lifetime/statistics を更新
        lifetime_.OnEvicted(id);
        if (stats_) stats_->OnEvict(id);

        // 強制で erase
        storage_.EraseIf(id, true);
        return true;
    }

    void AssetManager::Watch(const AssetId& id, std::string resolvedPath) {
        if (!watcher_) return;
        watcher_->Watch(id, std::move(resolvedPath));
    }

    void AssetManager::Unwatch(const AssetId& id) {
        if (!watcher_) return;
        watcher_->Unwatch(id);
    }

    // ---------------- internal helpers ----------------

    Base::Result<AssetManager::ResolvedEntry, AssetError>
    AssetManager::ResolveEntry_(const AssetId& id, const AssetRequest& req) {
        if (stats_) stats_->OnCatalogLookup();

        // CatalogEntry { AssetType type; std::string resolvedPath; } が引ける
        const auto* entry = catalog_.Find(id); //
        if (!entry) {
            if (stats_) stats_->OnCatalogMiss();
            return Base::Result<ResolvedEntry, AssetError>::Err(
                AssetError::Make(AssetErrorCode::CatalogNotFound, "AssetCatalog: id not found")
            );
        }

        // type hint check
        if (req.useTypeHint && req.expectedType.value != 0) {
            if (req.expectedType.value != entry->type.value) {
                return Base::Result<ResolvedEntry, AssetError>::Err(
                    AssetError::Make(AssetErrorCode::InvalidCatalogEntry, "AssetRequest: expectedType mismatch")
                );
            }
        }

        ResolvedEntry out;
        out.type = entry->type;

        // override path がある場合：ここでは “resolvedPath として扱う”
        // 必要ならここで AssetPathResolver を通して正規化してOK（設計上はCatalog側が担当）
        out.resolvedPath = req.overridePath.empty() ? entry->resolvedPath : req.overridePath;

        if (out.resolvedPath.empty()) {
            return Base::Result<ResolvedEntry, AssetError>::Err(
                AssetError::Make(AssetErrorCode::InvalidPath, "AssetCatalog: resolvedPath is empty")
            );
        }

        return Base::Result<ResolvedEntry, AssetError>::Ok(std::move(out));
    }

    Core::AssetRecord& AssetManager::GetOrCreateRecord_(const AssetId& id, const ResolvedEntry& e) {
        // 初回だけ type/path がセットされる（既存なら保持）
        return storage_.GetOrCreate(id, e.type, e.resolvedPath);
    }

    Base::Result<void, AssetError>
    AssetManager::DoLoadSync_(Core::AssetRecord& rec,
                              const ResolvedEntry& e,
                              const AssetRequest& req,
                              bool hadReadyAsset) {
        // ForceReload のときは “読み込み前に Loading へ”
        rec.MarkLoading();

        Loading::LoadContext ctx;
        ctx.id = rec.id;
        ctx.type = e.type;
        ctx.resolvedPath = e.resolvedPath;
        ctx.request = &req;
        ctx.statistics = stats_;
        ctx.nowFrame = frame_;

        auto r = pipeline_.Load(ctx);
        if (!r) {
            // Reload + KeepOldIfAny + 旧データあり => 旧キャッシュ維持
            if (req.fallback == AssetRequest::Fallback::KeepOldIfAny && hadReadyAsset) {
                // 旧 asset は rec.asset に残っているので state を Ready に戻す
                // ただしエラー情報は “最後のreload失敗” として残しておく（デバッグ優先）
                rec.state = AssetState::Ready;
                rec.error = std::move(r.error());
                if (stats_) stats_->OnReload(rec.id);
                return Base::Result<void, AssetError>::Ok();
            }

            // A destructive reload invalidated a previously readable payload.
            // Retire that generation even though no replacement was produced;
            // otherwise a later recovery load could resurrect old handles.
            if (req.IsReload() && hadReadyAsset) {
                rec.AdvanceGeneration();
            }

            rec.SetFailed(std::move(r.error()));
            return Base::Result<void, AssetError>::Err(rec.error);
        }

        // 成功：AnyAsset を格納
        // Reload で既に Ready だった場合のみ generation を進める（stale handle を弾く）
        if (req.IsReload() && hadReadyAsset) {
            rec.AdvanceGeneration();
            if (stats_) stats_->OnReload(rec.id);
        }

        rec.SetReady(std::move(r.value()));
        // resolvedPath を record に持たせておく（便利）
        if (rec.resolvedPath.empty()) rec.resolvedPath = e.resolvedPath;

        return Base::Result<void, AssetError>::Ok();
    }

    void AssetManager::EnqueueLoad_(const AssetId& id, const AssetRequest& req) {
        // 重複投入を防ぐ（同じIDがキューにいるならスキップ）
        if (queued_.find(id) != queued_.end()) return;

        const Core::AssetRecord* record = storage_.Find(id);
        queue_.push_back(PendingLoad{ id, req, record != nullptr && record->IsReady() });
        queued_.insert(id);
    }

    void AssetManager::ProcessQueue_() {
        if (queue_.empty()) return;

        std::uint32_t budget = opt_.maxLoadsPerFrame;
        while (budget > 0 && !queue_.empty()) {
            PendingLoad job = std::move(queue_.front());
            queue_.pop_front();
            queued_.erase(job.id);

            // catalog resolve
            auto entryR = ResolveEntry_(job.id, job.req);
            if (!entryR) {
                // catalog 失敗：record があれば Failed に落とす
                if (auto* rec = storage_.Find(job.id)) {
                    rec->SetFailed(std::move(entryR.error()));
                }
                --budget;
                continue;
            }

            const ResolvedEntry e = std::move(entryR.value());
            Core::AssetRecord& rec = GetOrCreateRecord_(job.id, e);

            // 実ロード（sync実行）
            (void)DoLoadSync_(rec, e, job.req, job.hadReadyAsset);

            // 成功なら寿命更新
            if (rec.IsReady()) lifetime_.OnLoaded(rec.id, frame_);

            --budget;
        }
    }

    void AssetManager::ProcessHotReload_() {
        if (!watcher_) return;

        auto changes = watcher_->Poll();
        if (changes.empty()) return;

        for (auto& c : changes) {
            AssetRequest r = AssetRequest::Reload();
            r.sync = AssetRequest::SyncWith::Async;
            r.fallback = opt_.reloadKeepOldIfAny ? AssetRequest::Fallback::KeepOldIfAny
                                                : AssetRequest::Fallback::None;
            EnqueueLoad_(c.id, r);
        }
    }

    Core::AssetRecord* AssetManager::FindRecord_(const AssetHandle& h) {
        Core::AssetRecord* rec = storage_.Find(h.id());
        if (!rec) return nullptr;
        return rec;
    }

    const Core::AssetRecord* AssetManager::FindRecordConst_(const AssetHandle& h) const {
        const Core::AssetRecord* rec = storage_.Find(h.id());
        if (!rec) return nullptr;
        return rec;
    }

} // namespace Engine::Asset
