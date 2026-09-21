#pragma once

#include <cstdint>
#include <string>

#include "engine/asset/AssetId.hpp"

namespace Engine::Asset::HotReload {

enum class AssetChangeKind : std::uint8_t {
    Added = 0,
    Modified,
    Removed
};

inline constexpr const char* ToString(AssetChangeKind k) noexcept {
    switch (k) {
    case AssetChangeKind::Added:    return "Added";
    case AssetChangeKind::Modified: return "Modified";
    case AssetChangeKind::Removed:  return "Removed";
    default:                        return "Unknown";
    }
}

// AssetChange：監視結果として上位へ通知するイベント
// - reload のキーは基本 AssetId（AssetManager がそれを使う）
// - resolvedPath も載せておくとログ/デバッグが楽
struct AssetChange final {
    AssetId id{};
    AssetChangeKind kind = AssetChangeKind::Modified;

    std::string resolvedPath;     // "assets/..."（AssetCatalog が解決済みのものを渡す想定）
    std::uint64_t writeTimeNs = 0; // ファイルの最終更新時刻（ns, best-effort）
    std::uint64_t detectedNs  = 0; // 変更検出時刻（ns, system_clock）

    // 任意：同じフレーム内での順序が欲しい場合
    std::uint64_t seq = 0;
};

} // namespace Engine::Asset::HotReload
