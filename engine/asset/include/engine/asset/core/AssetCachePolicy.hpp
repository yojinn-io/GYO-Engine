#pragma once

#include <cstdint>

#include "engine/asset/AssetState.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStatistics.hpp"
#include "engine/asset/core/AssetRecord.hpp"

namespace Engine::Asset::Core {

// AssetCachePolicy:
// - 「いつevictして良いか」の判断ルール（ポリシー）
// - AssetStorage の走査/削除は AssetManager が行う前提
// - ここは判断だけ（= pure policy）に寄せる
class AssetCachePolicy final {
public:
    enum class Mode : std::uint8_t {
        KeepForever = 0,          // 破棄しない（開発初期向け）
        KeepWhileReferenced,      // refCount==0 になったら破棄可能（TTLはLifetime）
        Budgeted                 // 上限を超えたら破棄を促す（選別はManager側）
    };

    struct Options final {
        Mode mode = Mode::KeepWhileReferenced;

        // TTL（refCount==0 のとき、最後の access から何フレーム保持するか）
        // 0なら即時evict可
        std::uint64_t keepAliveFrames = 0;

        // Failed をキャッシュに残すか（連続リトライ抑止）
        // falseなら refCount==0 & expired で消してOK
        bool keepFailedRecords = true;

        // Budget（mode==Budgeted のときだけ意味を持つ）
        // 0なら無制限
        std::uint32_t maxAssets = 0;          // 「キャッシュ数」の上限（AssetManagerが現数を渡す）
        std::uint64_t maxBytesRead = 0;       // 「読み込み総量」ではなく “現在の推定常駐” を渡す想定（任意）
    };

public:
    explicit AssetCachePolicy(Options opt) : opt_(opt) {}

    void SetOptions(Options opt) { opt_ = opt; }
    const Options& GetOptions() const noexcept { return opt_; }

    // 破棄（evict/erase）して良いか？
    // nowFrame は AssetManager のフレームカウンタを渡す。
    bool IsEvictable(const AssetRecord& rec,
                     const AssetLifetime& lifetime,
                     std::uint64_t nowFrame) const noexcept {
        // ロード中は触らない
        if (rec.state == AssetState::Loading) return false;

        // KeepForever は絶対に破棄しない
        if (opt_.mode == Mode::KeepForever) return false;

        // failed を保持する設定なら、Failed は破棄対象から除外（ただし force evict などは別口で）
        if (rec.state == AssetState::Failed && opt_.keepFailedRecords) return false;

        // refCount + pinned + TTL
        return lifetime.CanEvict(rec.id, nowFrame, rec.refCount, opt_.keepAliveFrames);
    }

    // Budget 超過時に「trim したいか」の判断（選別・何個消すかは Manager）
    // ここは “必要性判定” のみ提供する。
    bool ShouldTrim(std::uint32_t currentAssetCount,
                    std::uint64_t currentResidentBytes = 0) const noexcept {
        if (opt_.mode != Mode::Budgeted) return false;

        if (opt_.maxAssets != 0 && currentAssetCount > opt_.maxAssets) return true;
        if (opt_.maxBytesRead != 0 && currentResidentBytes > opt_.maxBytesRead) return true;
        return false;
    }

private:
    Options opt_{};
};

} // namespace Engine::Asset::Core
