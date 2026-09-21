#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetType.hpp"

#include "engine/asset/core/AssetRecord.hpp"

namespace Engine::Asset::Core {

// AssetStorage:
// - map<AssetId, AssetRecord> の所有者
// - record のアドレス安定性を確保するため、unique_ptr で保持する
//   （unordered_map の rehash で参照が壊れる問題を避ける）
class AssetStorage final {
public:
    AssetStorage() = default;

    void Clear() {
        for (const auto& [id, record] : records_) {
            RetireGeneration_(id, record->generation);
        }
        records_.clear();
    }

    std::size_t Size() const noexcept { return records_.size(); }

    AssetRecord* Find(const AssetId& id) noexcept {
        auto it = records_.find(id);
        return it == records_.end() ? nullptr : it->second.get();
    }

    const AssetRecord* Find(const AssetId& id) const noexcept {
        auto it = records_.find(id);
        return it == records_.end() ? nullptr : it->second.get();
    }

    bool Contains(const AssetId& id) const noexcept {
        return records_.find(id) != records_.end();
    }

    // 無ければ作る。type/path は「初回作成時のみ」設定する（既存なら保持）
    AssetRecord& GetOrCreate(const AssetId& id, const AssetType& type, std::string resolvedPath = {}) {
        auto it = records_.find(id);
        if (it != records_.end()) {
            return *it->second;
        }

        auto rec = std::make_unique<AssetRecord>();
        rec->id = id;
        rec->type = type;
        rec->resolvedPath = std::move(resolvedPath);
        rec->state = AssetState::Unloaded;
        if (const auto retired = retiredGenerations_.find(id);
            retired != retiredGenerations_.end()) {
            rec->generation = NextGeneration_(retired->second);
        }

        auto* ptr = rec.get();
        records_.emplace(id, std::move(rec));
        return *ptr;
    }

    // “pathだけ後から埋めたい” 用（Catalog構築→Storage作成の順序差に対応）
    void SetResolvedPathIfEmpty(const AssetId& id, std::string resolvedPath) {
        if (auto* r = Find(id)) {
            if (r->resolvedPath.empty()) r->resolvedPath = std::move(resolvedPath);
        }
    }

    // “解放可能か” の判定は CachePolicy/Lifetime と組み合わせて AssetManager が行う想定
    bool CanEvict(const AssetId& id) const noexcept {
        const auto* r = Find(id);
        if (!r) return false;
        return r->refCount == 0;
    }

    void EraseIf(const AssetId& id, bool force = false) {
        auto it = records_.find(id);
        if (it == records_.end()) return;

        if (force || it->second->refCount == 0) {
            RetireGeneration_(id, it->second->generation);
            records_.erase(it);
        }
    }

private:
    static std::uint32_t NextGeneration_(std::uint32_t generation) noexcept {
        ++generation;
        // AssetHandle reserves zero for Invalid().
        return generation == 0 ? 1 : generation;
    }

    void RetireGeneration_(const AssetId& id, std::uint32_t generation) {
        retiredGenerations_[id] = generation;
    }

    std::unordered_map<AssetId, std::unique_ptr<AssetRecord>> records_;

    // Keep the last incarnation after eviction so a newly loaded record cannot
    // make a previously issued handle valid again (generation ABA).
    std::unordered_map<AssetId, std::uint32_t> retiredGenerations_;
};

} // namespace Engine::Asset::Core
