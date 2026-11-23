#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <type_traits>
#include <vector>

// How a replicated value goes on the wire (declaration D-5): each field written on its own, little-endian, so
// padding never goes on the wire and both ends read the same bytes on every platform. There is no "copy the
// object's bytes" codec: a type that is not a number or an enum gets the author's encode and decode.

namespace GYO::Engine::Net {

class NetWriter final {
public:
    explicit NetWriter(std::vector<std::byte>& out) noexcept : out_(out) {}

    // A number (integers as their width, floating point by its bits), an enum (its underlying integer) or bool.
    template <class V>
        requires(std::is_arithmetic_v<V> || std::is_enum_v<V>)
    void Value(const V value) {
        if constexpr (std::is_enum_v<V>) {
            Value(static_cast<std::underlying_type_t<V>>(value));
        } else if constexpr (std::is_same_v<V, bool>) {
            Raw(static_cast<std::uint64_t>(value ? 1 : 0), 1);
        } else if constexpr (std::is_floating_point_v<V>) {
            if constexpr (sizeof(V) == 4) {
                Raw(std::bit_cast<std::uint32_t>(value), 4);
            } else {
                Raw(std::bit_cast<std::uint64_t>(value), 8);
            }
        } else {
            Raw(static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<V>>(value)), sizeof(V));
        }
    }
    void Bytes(const std::span<const std::byte> bytes) { out_.insert(out_.end(), bytes.begin(), bytes.end()); }
    [[nodiscard]] std::size_t Size() const noexcept { return out_.size(); }

private:
    void Raw(const std::uint64_t value, const std::size_t width) {
        for (std::size_t i = 0; i < width; ++i) out_.push_back(static_cast<std::byte>(value >> (8 * i)));
    }
    std::vector<std::byte>& out_;
};

class NetReader final {
public:
    explicit NetReader(const std::span<const std::byte> in) noexcept : in_(in) {}

    // false, value untouched, when the input is shorter than the value (nothing asserts on input bytes).
    template <class V>
        requires(std::is_arithmetic_v<V> || std::is_enum_v<V>)
    [[nodiscard]] bool Value(V& value) {
        if constexpr (std::is_enum_v<V>) {
            std::underlying_type_t<V> raw{};
            if (!Value(raw)) return false;
            value = static_cast<V>(raw);
            return true;
        } else if constexpr (std::is_same_v<V, bool>) {
            std::uint64_t raw = 0;
            if (!Raw(raw, 1) || raw > 1) return false;
            value = raw == 1;
            return true;
        } else if constexpr (std::is_floating_point_v<V>) {
            std::uint64_t raw = 0;
            if (!Raw(raw, sizeof(V))) return false;
            if constexpr (sizeof(V) == 4) {
                value = std::bit_cast<float>(static_cast<std::uint32_t>(raw));
            } else {
                value = std::bit_cast<double>(raw);
            }
            return true;
        } else {
            std::uint64_t raw = 0;
            if (!Raw(raw, sizeof(V))) return false;
            value = static_cast<V>(static_cast<std::make_unsigned_t<V>>(raw));
            return true;
        }
    }
    [[nodiscard]] bool Bytes(const std::size_t count, std::span<const std::byte>& bytes) {
        if (Remaining() < count) return false;
        bytes = in_.subspan(at_, count);
        at_ += count;
        return true;
    }
    [[nodiscard]] std::size_t Remaining() const noexcept { return in_.size() - at_; }

private:
    [[nodiscard]] bool Raw(std::uint64_t& value, const std::size_t width) {
        if (Remaining() < width) return false;
        value = 0;
        for (std::size_t i = 0; i < width; ++i) value |= std::to_integer<std::uint64_t>(in_[at_ + i]) << (8 * i);
        at_ += width;
        return true;
    }
    std::span<const std::byte> in_;
    std::size_t at_ = 0;
};

// The codec of one replicated type. version goes into the protocol hash: change it when the encoding changes, so
// two ends with different encodings do not connect.
template <class T>
struct Codec final {
    std::function<void(NetWriter&, const T&)> encode;
    // false: the bytes are not a T (the value is dropped and counted).
    std::function<bool(NetReader&, T&)> decode;
    std::uint32_t version = 1;
};

// The default codec: numbers and enums only. Any other type has none (empty functions): the author gives one.
template <class T>
[[nodiscard]] Codec<T> DefaultCodec() {
    Codec<T> codec;
    if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
        codec.encode = [](NetWriter& writer, const T& value) { writer.Value(value); };
        codec.decode = [](NetReader& reader, T& value) { return reader.Value(value); };
    }
    return codec;
}

} // namespace GYO::Engine::Net
