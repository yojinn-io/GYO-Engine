#pragma once

#include <cstdint>
#include <unordered_map>

#include "engine/asset/AssetId.hpp"

namespace Engine::Asset::Core {

// AssetLifetime:
// - AssetRecord を汚さずに「寿命メタ情報」を保持する
// - AssetManager が毎フレーム（またはロード/アクセス時）に Touch する想定
//
// 用途：
// - keepAliveFrames（TTL）
// - pin/unpin（強制保持）
// - lastAccessFrame に基づく eviction 判定
class AssetLifetime final {
public:
    struct Info final {
        std::uint64_t lastAccessFrame = 0; // 最後に Get/Use されたフレーム
        std::uint64_t lastLoadedFrame = 0; // 最後にロード完了したフレーム（任意）
        bool pinned = false;              // 強制保持
    };

public:
    void Clear() {
        infos_.clear();
    }

    bool Has(const AssetId& id) const noexcept {
        return infos_.find(id) != infos_.end();
    }

    const Info* Find(const AssetId& id) const noexcept {
        auto it = infos_.find(id);
        return (it == infos_.end()) ? nullptr : &it->second;
    }

    Info* Find(const AssetId& id) noexcept {
        auto it = infos_.find(id);
        return (it == infos_.end()) ? nullptr : &it->second;
    }

    Info& GetOrCreate(const AssetId& id) {
        return infos_[id];
    }

    // 呼び出しポイント：AssetManager::Get / Load のたびに呼ぶのが基本
    void Touch(const AssetId& id, std::uint64_t nowFrame) {
        auto& inf = infos_[id];
        inf.lastAccessFrame = nowFrame;
    }

    // 呼び出しポイント：ロード成功時
    void OnLoaded(const AssetId& id, std::uint64_t nowFrame) {
        auto& inf = infos_[id];
        inf.lastLoadedFrame = nowFrame;
        inf.lastAccessFrame = nowFrame;
    }

    // 呼び出しポイント：evict/erase 時
    void OnEvicted(const AssetId& id) {
        infos_.erase(id);
    }

    void Pin(const AssetId& id) {
        infos_[id].pinned = true;
    }

    void Unpin(const AssetId& id) {
        auto it = infos_.find(id);
        if (it != infos_.end()) it->second.pinned = false;
    }

    bool IsPinned(const AssetId& id) const noexcept {
        auto it = infos_.find(id);
        return (it != infos_.end()) ? it->second.pinned : false;
    }

    // keepAliveFrames:
    // - 0 なら「refCount==0 になったら即evict可」
    // - 60*5 なら「参照が切れても 5秒（60fps想定）保持」
    bool IsExpired(const AssetId& id, std::uint64_t nowFrame, std::uint64_t keepAliveFrames) const noexcept {
        if (keepAliveFrames == 0) return true;

        auto it = infos_.find(id);
        if (it == infos_.end()) {
            // 情報が無い場合は「古い」とみなす（evictしやすく）
            return true;
        }

        const auto last = it->second.lastAccessFrame;
        return (nowFrame >= last) ? ((nowFrame - last) >= keepAliveFrames) : true;
    }

    // refCount と pinned と TTL から「evict可能か」を判断する共通関数
    bool CanEvict(const AssetId& id,
                  std::uint64_t nowFrame,
                  std::uint32_t refCount,
                  std::uint64_t keepAliveFrames) const noexcept {
        if (refCount != 0) return false;
        if (IsPinned(id)) return false;
        return IsExpired(id, nowFrame, keepAliveFrames);
    }

private:
    std::unordered_map<AssetId, Info> infos_;
};

} // namespace Engine::Asset::Core
