#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <functional>
#include "detail/Hash.hpp"

namespace Engine::Asset {

/// AssetId：カタログ上の "id" をエンジン内部で扱いやすいIDにしたもの
/// - 文字列ID -> 64-bit hash
/// - ENGINE_ASSET_DEBUG_NAME=1 の場合は debugName も保持（衝突調査が容易）
struct AssetId final {
    using ValueType = uint64_t;

    ValueType value{0};

    std::string debugName; // 元のID文字列（例: "player_tex"）

    constexpr AssetId() noexcept = default;
    explicit constexpr AssetId(ValueType v) noexcept : value(v) {}

    static AssetId FromString(std::string_view s) noexcept {
        AssetId id{ Detail::Fnv1a64(s) };
        id.debugName = std::string(s);
        return id;
    }

    constexpr bool IsValid() const noexcept { return value != 0; }

    // 便利：文字列を直接渡せる
    explicit AssetId(std::string_view s) noexcept : AssetId(FromString(s)) {}

    // Asset identity is the hashed value only. debugName is diagnostic metadata
    // and must not change equality, otherwise unordered containers can observe
    // equal IDs with different hashes or non-transitive equality.
    friend constexpr bool operator==(const AssetId& a, const AssetId& b) noexcept {
        return a.value == b.value;
    }

    friend bool operator!=(const AssetId& a, const AssetId& b) noexcept { return !(a == b); }

    friend bool operator<(const AssetId& a, const AssetId& b) noexcept {
        return a.value < b.value;
    }
};

} // namespace Engine::Asset

// unordered_map 用
namespace std {
template <>
struct hash<Engine::Asset::AssetId> {
    size_t operator()(const Engine::Asset::AssetId& id) const noexcept {
        // 64->size_t（環境により 32-bit の場合は縮む）
        return static_cast<size_t>(id.value);
    }
};
} // namespace std
