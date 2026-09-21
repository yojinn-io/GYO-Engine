#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Engine::Asset::Detail  {

    using Hash64 = std::uint64_t;

    constexpr Hash64 Fnv1a64(const void* data, std::size_t size) noexcept {

        constexpr Hash64 kOffsetBasis = 14695981039346656037ull;
        constexpr Hash64 kPrime       = 1099511628211ull;

        Hash64 h = kOffsetBasis;
        const auto* p = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i) {
            h ^= static_cast<Hash64>(p[i]);
            h *= kPrime;
        }
        return h;
    }

    constexpr Hash64 Fnv1a64(std::string_view sv) noexcept {
        return Fnv1a64(sv.data(), sv.size());
    }

    constexpr Hash64 HashCombine(Hash64 a, Hash64 b) noexcept {
        // よくある 64-bit combine（簡易）
        // ※安定性/衝突耐性は用途次第。必要なら差し替え可能。
        a ^= b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2);
        return a;
    }
}
