#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/hot_reload/AssetChange.hpp"

namespace Engine::Asset::HotReload {

    // AssetWatcher：
    // - “watch list” を保持
    // - Poll() を呼ぶと変更を検出して AssetChange を返す
    class AssetWatcher final {
    public:
        struct Options final {
            // 変更検出のバースト（エディタ保存など）を抑制するデバウンス
            // 0 なら抑制しない
            std::uint64_t debounceMs = 50;

            bool emitAdded    = true;
            bool emitModified = true;
            bool emitRemoved  = true;

            // AddWatch 時点でファイルが存在しない場合でも監視し続ける
            bool keepWatchingMissing = true;
        };

        struct WatchedInfo final {
            std::string resolvedPath;
            bool existed = false;
            std::uint64_t lastWriteTimeNs = 0;
            std::uint64_t lastEventNs = 0; // debounce 用
        };

    public:
        explicit AssetWatcher(Options opt);
        void SetOptions(Options opt);
        const Options& GetOptions() const noexcept;

        // 監視登録（resolvedPath は AssetCatalog が解決済みのパスを渡す想定）
        // 既に登録済みならパス更新する
        void Watch(const AssetId& id, std::string resolvedPath);

        void Unwatch(const AssetId& id);
        void Clear();

        bool IsWatching(const AssetId& id) const noexcept;

        const WatchedInfo* FindWatched(const AssetId& id) const noexcept;

        // ポーリングして変更を返す（呼び出し側が毎フレーム or 数フレーム毎に呼ぶ）
        std::vector<AssetChange> Poll();

    private:
        static std::uint64_t NowNs();
        static bool ProbeFile(const std::string& path, bool& existsOut, std::uint64_t& writeNsOut);

    private:
        Options opt_{};
        std::unordered_map<AssetId, WatchedInfo> watched_{};
        std::uint64_t seq_ = 0;
    };

} // namespace Engine::Asset::HotReload
