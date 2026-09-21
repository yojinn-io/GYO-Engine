#pragma once

#include <cstdint>
#include <unordered_map>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetType.hpp"

namespace Engine::Asset::Core {

    using Hash64 = std::uint64_t;

// AssetStatistics:
// - AssetManager / AssetPipeline がイベント駆動でカウントする
// - 「性能/挙動の可視化」用。ロジックは持たない。
// - bytes は IAssetSource の ReadAll が返すサイズを入れる想定（任意）
class AssetStatistics final {
public:
    struct Counters final {
        Hash64 catalogLookups = 0;
        Hash64 catalogMisses  = 0;

        Hash64 cacheHits   = 0;
        Hash64 cacheMisses = 0;

        Hash64 loadRequests = 0;
        Hash64 loadStarts   = 0;
        Hash64 loadSucceeded= 0;
        Hash64 loadFailed   = 0;

        Hash64 evictions = 0;
        Hash64 reloads   = 0; // hot-reload

        Hash64 bytesReadTotal   = 0; // source bytes
        Hash64 bytesDecodedTotal= 0; // decoded/expanded bytes（分かる範囲で）
    };

    struct PerAsset final {
        AssetType type{};
        Hash64 hits = 0;
        Hash64 lastBytesRead = 0;
        Hash64 lastDecodedBytes = 0;
        Hash64 lastLoadFrame = 0;
        bool lastLoadSucceeded = false;
    };

public:
    void Clear() {
        counters_ = Counters{};
        per_.clear();
    }

    const Counters& GetCounters() const noexcept { return counters_; }

    // --- derived metrics ---
    double CacheHitRate() const noexcept {
        const auto total = counters_.cacheHits + counters_.cacheMisses;
        if (total == 0) return 0.0;
        return static_cast<double>(counters_.cacheHits) / static_cast<double>(total);
    }

    // --- per asset ---
    const PerAsset* Find(const AssetId& id) const noexcept {
        auto it = per_.find(id);
        return (it == per_.end()) ? nullptr : &it->second;
    }

    PerAsset& GetOrCreate(const AssetId& id) {
        return per_[id];
    }

    // --- event hooks (call from manager/pipeline) ---
    void OnCatalogLookup() noexcept { ++counters_.catalogLookups; }
    void OnCatalogMiss() noexcept   { ++counters_.catalogMisses;  }

    void OnCacheHit(const AssetId& id) {
        ++counters_.cacheHits;
        ++per_[id].hits;
    }

    void OnCacheMiss() noexcept { ++counters_.cacheMisses; }

    void OnLoadRequest() noexcept { ++counters_.loadRequests; }
    void OnLoadStart()   noexcept { ++counters_.loadStarts; }

    void OnLoadSuccess(const AssetId& id,
                       AssetType type,
                       Hash64 nowFrame,
                       Hash64 bytesRead = 0,
                       Hash64 decodedBytes = 0) {
        ++counters_.loadSucceeded;
        counters_.bytesReadTotal += bytesRead;
        counters_.bytesDecodedTotal += decodedBytes;

        auto& p = per_[id];
        p.type = type;
        p.lastBytesRead = bytesRead;
        p.lastDecodedBytes = decodedBytes;
        p.lastLoadFrame = nowFrame;
        p.lastLoadSucceeded = true;
    }

    void OnLoadFailure(const AssetId& id, AssetType type, std::uint64_t nowFrame) {
        ++counters_.loadFailed;

        auto& p = per_[id];
        p.type = type;
        p.lastLoadFrame = nowFrame;
        p.lastLoadSucceeded = false;
    }

    void OnEvict(const AssetId& id) {
        ++counters_.evictions;
        // per_ は残しても良い（履歴として）。不要なら erase しても良い。
        // per_.erase(id);
        (void)id;
    }

    void OnReload(const AssetId& id) {
        ++counters_.reloads;
        (void)id;
    }

private:
    Counters counters_{};
    std::unordered_map<AssetId, PerAsset> per_{};
};

} // namespace Engine::Asset::Core
