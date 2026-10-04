#pragma once

#include <cstdint>
#include <string_view>

namespace Engine::Base {

// 64-bit FNV-1a of a byte string: a stable, platform-independent,
// non-cryptographic identity hash (asset and input ids, change detection).
// The empty string hashes to the offset basis; callers that reserve 0 for an
// invalid id apply that rule themselves.
[[nodiscard]] constexpr std::uint64_t Fnv1a64(std::string_view bytes) noexcept {
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offsetBasis;
    for (const char byte : bytes) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= prime;
    }
    return hash;
}

} // namespace Engine::Base
