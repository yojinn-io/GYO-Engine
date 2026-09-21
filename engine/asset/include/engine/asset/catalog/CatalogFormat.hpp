#pragma once

#include <string_view>

namespace Engine::Asset::Catalog {

    // catalog.json のフォーマット（キー名/バージョン）をここに集約。
    // こうしておくと、後でスキーマ変更しても parser の修正点が局所化します。
    struct CatalogFormat final {
        static constexpr int kVersion = 1;

        // top-level keys
        static constexpr std::string_view kKeyVersion = "version";
        static constexpr std::string_view kKeyAssets  = "assets";

        // entry keys (array-form)
        static constexpr std::string_view kKeyId   = "id";
        static constexpr std::string_view kKeyType = "type";
        static constexpr std::string_view kKeyPath = "path";

        // 互換性：assets が object-form の場合（id が key になる）
        // "assets": { "player_tex": { "type":"texture", "path":"textures/player.png" } }
        // その inner object でも kKeyType/kKeyPath を使う。

        static constexpr bool IsSupportedVersion(int v) noexcept {
            return v == kVersion;
        }
    };

} // namespace Engine::Asset::Catalog
