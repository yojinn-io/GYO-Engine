#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

#include "engine/asset/AssetError.hpp"
#include "engine/asset/AssetHandle.hpp"
#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/AssetState.hpp"
#include "engine/asset/AssetType.hpp"

#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStatistics.hpp"
#include "engine/asset/core/AssetStorage.hpp"

#include "engine/base/Result.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/LoadContext.hpp"

#include "engine/asset/hot_reload/AssetWatcher.hpp"

// forward
namespace Engine::Asset {
class AssetCatalog;
}

namespace Engine::Asset {
    using AssetError = Base::Error<AssetErrorCode>;

    class AssetManager final {
    public:
        struct Options final {
            // Asyncキューを1フレームに何件処理するか（擬似async / ポーリング）
            std::uint32_t maxLoadsPerFrame = 2;

            // HotReload を AssetManager 側で Poll して Reload を投げるか
            bool enableHotReload = false;

            // Reload するときは基本 KeepOldIfAny にする（開発中のUX優先）
            bool reloadKeepOldIfAny = true;
        };

        // Dependencies are injected by the application composition root.
        AssetManager(AssetCatalog& catalog,
                     Loading::AssetPipeline& pipeline,
                     Core::AssetStorage& storage,
                     Core::AssetLifetime& lifetime,
                     Core::AssetCachePolicy& cachePolicy,
                     Core::AssetStatistics* stats = nullptr,
                     HotReload::AssetWatcher* watcher = nullptr);

        void SetOptions(Options opt);
        const Options& GetOptions() const noexcept;

        // フレーム境界（寿命/統計/ホットリロードのため）
        void BeginFrame(std::uint64_t frameIndex);

        // 1フレーム処理：asyncキュー消化 + (任意) hot-reload poll
        void Update();

        // ---- Public API ----

        // Load:
        // - Sync: その場で読み込み、失敗なら Err(AssetError)
        // - Async: キューへ積み、すぐ Ok(handle) を返す（後で Ready になる）
        Base::Result<AssetHandle, AssetError> Load(const AssetId& id, const AssetRequest& request);

        // 参照カウント（AssetStorage.refCount）操作
        // - Load() は内部で Acquire 相当（refCount++）する設計
        // - Release() accepts stale generations only to balance their original
        //   reference; read/query APIs continue to reject stale handles.
        bool Acquire(const AssetHandle& h);
        void Release(const AssetHandle& h);

        // 状態・エラー参照
        AssetState GetState(const AssetHandle& h) const;
        const AssetError* GetError(const AssetHandle& h) const;

        // 型安全取得（shared_ptr を返すのが安全）
        template <class T>
        std::shared_ptr<T> GetShared(const AssetHandle& h) {
            Core::AssetRecord* rec = FindRecord_(h);
            if (!rec) return {};
            if (!rec->IsReady()) return {};
            // stale / type hint check
            if (rec->generation != h.generation()) return {};
            if (h.has_type_hint() && rec->asset.type() != h.type_hint()) return {};
            return rec->asset.ShareAs<T>();
        }

        template <class T>
        const std::shared_ptr<const T> GetSharedConst(const AssetHandle& h) const {
            const Core::AssetRecord* rec = FindRecordConst_(h);
            if (!rec) return {};
            if (!rec->IsReady()) return {};
            if (rec->generation != h.generation()) return {};
            if (h.has_type_hint() && rec->asset.type() != h.type_hint()) return {};
            auto sp = rec->asset.ShareAs<T>();
            return std::const_pointer_cast<const T>(sp);
        }

        // 低レベル：evict を “1つだけ” 試す（Budgeted運用などで上位がループする想定）
        bool EvictIfPossible(const AssetId& id);

        // HotReload 用：外部から watch 登録したい場合
        void Watch(const AssetId& id, std::string resolvedPath);
        void Unwatch(const AssetId& id);

    private:
        struct PendingLoad final {
            AssetId id;
            AssetRequest req;
            bool hadReadyAsset = false;
        };

    private:
        // ---- internal helpers ----
        struct ResolvedEntry final {
            AssetType type{};
            std::string resolvedPath;
        };

        // AssetCatalog から (type, resolvedPath) を引く
        Base::Result<ResolvedEntry, AssetError> ResolveEntry_(const AssetId& id, const AssetRequest& req);

        // Record 取得/作成
        Core::AssetRecord& GetOrCreateRecord_(const AssetId& id, const ResolvedEntry& e);

        // 実ロード（Sync）
        Base::Result<void, AssetError> DoLoadSync_(Core::AssetRecord& rec,
                                                   const ResolvedEntry& e,
                                                   const AssetRequest& req,
                                                   bool hadReadyAsset);

        // Async キュー操作
        void EnqueueLoad_(const AssetId& id, const AssetRequest& req);
        void ProcessQueue_();

        // Hot reload
        void ProcessHotReload_();

        // Record検索（staleチェックは呼び出し側）
        Core::AssetRecord* FindRecord_(const AssetHandle& h);
        const Core::AssetRecord* FindRecordConst_(const AssetHandle& h) const;

    private:
        AssetCatalog& catalog_;
        Loading::AssetPipeline& pipeline_;
        Core::AssetStorage& storage_;
        Core::AssetLifetime& lifetime_;
        Core::AssetCachePolicy& cachePolicy_;
        Core::AssetStatistics* stats_ = nullptr;
        HotReload::AssetWatcher* watcher_ = nullptr;

        Options opt_{};
        std::uint64_t frame_ = 0;

        std::deque<PendingLoad> queue_;
        std::unordered_set<AssetId> queued_; // 重複防止
    };

} // namespace Engine::Asset
