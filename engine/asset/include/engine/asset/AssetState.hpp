#pragma once

#include <cstdint>

namespace Engine::Asset {

// AssetState：AssetRecord のライフサイクル状態
// 最小4状態で運用し、必要になったら増やす（HotReload等）
enum class AssetState : std::uint8_t {
    Unloaded = 0,  // 未ロード（キャッシュ無し）
    Loading,       // 読み込み中（非同期も想定）
    Ready,         // 利用可能（assetが有効）
    Failed         // 失敗（errorが有効）
};

inline constexpr const char* ToString(AssetState s) noexcept {
    switch (s) {
    case AssetState::Unloaded: return "Unloaded";
    case AssetState::Loading:  return "Loading";
    case AssetState::Ready:    return "Ready";
    case AssetState::Failed:   return "Failed";
    default:                   return "Unknown";
    }
}

} // namespace Engine::Asset
