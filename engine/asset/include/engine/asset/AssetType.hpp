#pragma once


#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <functional>
#include "detail/Hash.hpp"

namespace Engine::Asset {

/// AssetType：アセットの種別（"texture", "sound" など）
/// - 文字列 -> 64-bit hash
/// - エンジンが知らない type でも表現できる（ImporterRegistry 等で使える）
struct AssetType final {
    using ValueType = uint64_t;

    ValueType value{0};

    std::string debugName; // 元のtype文字列（例: "texture"）


    constexpr AssetType() noexcept = default;
    explicit constexpr AssetType(ValueType v) noexcept : value(v) {}



    static AssetType FromString(std::string_view s) noexcept {
        AssetType t{ Detail::Fnv1a64(s) };

        t.debugName = std::string(s);

        return t;
    }

    constexpr bool IsValid() const noexcept { return value != 0; }

    // 文字列を直接渡せる
    explicit AssetType(std::string_view s) noexcept : AssetType(FromString(s)) {}

    // よく使うエンジン標準タイプ（必要に応じて追加）
    static constexpr AssetType Texture() noexcept { return AssetType(Detail::Fnv1a64("texture", 7)); }
    static constexpr AssetType Sound()   noexcept { return AssetType(Detail::Fnv1a64("sound",   5)); }
    static constexpr AssetType Font()    noexcept { return AssetType(Detail::Fnv1a64("font",    4)); }
    static constexpr AssetType Text()    noexcept { return AssetType(Detail::Fnv1a64("text",    4)); }
    static constexpr AssetType Binary()  noexcept { return AssetType(Detail::Fnv1a64("binary",  6)); }
    static constexpr AssetType Data()  noexcept { return AssetType(Detail::Fnv1a64("data",  4)); }
    static constexpr AssetType Invalid() noexcept { return {}; }

    // The hashed value is the identity. debugName is diagnostic metadata and
    // must not change equality because hashing and loader dispatch use value.
    friend constexpr bool operator==(const AssetType& a,
                                     const AssetType& b) noexcept {
        return a.value == b.value;
    }

    friend bool operator!=(const AssetType& a, const AssetType& b) noexcept { return !(a == b); }

    friend bool operator<(const AssetType& a, const AssetType& b) noexcept {
        return a.value < b.value;
    }
};

} // namespace Engine::Asset

namespace std {
    template <>
    struct hash<Engine::Asset::AssetType> {
        size_t operator()(const Engine::Asset::AssetType& t) const noexcept {
            return static_cast<size_t>(t.value);
        }
    };
} // namespace std
