#pragma once

// A value's hash function (authority-rollback declaration section 3): what
// World::Summary hashes for each value, and what the authority Module's digest
// hashes. One customization point, found by argument-dependent lookup:
//
//     void HashValue(GYO::Kernel::Hasher& h, const T& value);
//
// An author writes it for an own type next to the type, field by field:
//
//     struct Health { std::int32_t current; std::int32_t max; };
//     void HashValue(GYO::Kernel::Hasher& h, const Health& v) { HashValue(h, v.current); HashValue(h, v.max); }
//
// The kernel writes the ones below: integers (as 64 bits, little-endian, so a
// value hashes the same whatever the integer's width on the platform), bool,
// enums (their underlying integer), float and double bit by bit (+0 and -0
// differ, a NaN hashes its bits; an author who wants them equal normalizes in
// the own function, e.g. HashValue(h, v.x + 0.0f)), and the contents of
// std::array, std::vector, std::string, std::optional (whether it holds a
// value, then the value; never the bytes of an empty one) and std::pair.
//
// HashAsBytes hashes a value's object bytes as they are. It is never chosen
// for a type by itself: an author picks it in the own function, and so
// promises the bytes are the value (no padding, no optional or union leftover,
// no pointer or handle, no integer whose width depends on the platform; LIM-8).
//
// A type with no HashValue still registers: World::Summary returns an error
// naming it when a value of it is in the World (the user's decision (d),
// 2026-11-20). A missing function is the author's to catch, by a test.
//
// Every write goes into the Hasher's bytes in a fixed width and byte order, so
// the bytes, and the SHA-256 the caller takes over them, are the same on every
// platform.

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace GYO::Kernel {

// The byte sink a hash function writes into. The caller decides what to do
// with the bytes (World::Summary takes one SHA-256 over all of them).
class Hasher final {
public:
    // value as 8 bytes, little-endian.
    void U64(std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8)
            bytes_.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
    // The bytes as they are (no length).
    void Bytes(std::span<const std::byte> raw) { bytes_.insert(bytes_.end(), raw.begin(), raw.end()); }

    [[nodiscard]] std::span<const std::byte> Data() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
};

template <std::integral I>
void HashValue(Hasher& h, I value) {
    // char's signedness depends on the platform: hashed as a byte, as in std::string.
    if constexpr (std::same_as<I, char>)
        h.U64(static_cast<unsigned char>(value));
    else if constexpr (std::is_signed_v<I>)
        h.U64(static_cast<std::uint64_t>(static_cast<std::int64_t>(value)));
    else
        h.U64(static_cast<std::uint64_t>(value));
}

template <class E>
    requires std::is_enum_v<E>
void HashValue(Hasher& h, E value) {
    HashValue(h, static_cast<std::underlying_type_t<E>>(value));
}

inline void HashValue(Hasher& h, float value) { h.U64(std::bit_cast<std::uint32_t>(value)); }
inline void HashValue(Hasher& h, double value) { h.U64(std::bit_cast<std::uint64_t>(value)); }

// Whether T has a hash function the kernel can find.
template <class T>
concept Hashable = requires(Hasher& h, const T& value) { HashValue(h, value); };

template <class T>
    requires std::is_trivially_copyable_v<T>
void HashAsBytes(Hasher& h, const T& value) {
    h.Bytes(std::span<const std::byte>(reinterpret_cast<const std::byte*>(&value), sizeof(T)));
}

inline void HashValue(Hasher& h, const std::string& value) {
    h.U64(value.size());
    h.Bytes(std::as_bytes(std::span<const char>(value.data(), value.size())));
}

template <Hashable T, std::size_t N>
void HashValue(Hasher& h, const std::array<T, N>& value) {
    for (const T& element : value) HashValue(h, element);
}

template <Hashable T, class A>
void HashValue(Hasher& h, const std::vector<T, A>& value) {
    h.U64(value.size());
    for (const T& element : value) HashValue(h, element);
}

template <Hashable T>
void HashValue(Hasher& h, const std::optional<T>& value) {
    h.U64(value.has_value() ? 1U : 0U);
    if (value) HashValue(h, *value);
}

template <Hashable A, Hashable B>
void HashValue(Hasher& h, const std::pair<A, B>& value) {
    HashValue(h, value.first);
    HashValue(h, value.second);
}

} // namespace GYO::Kernel
