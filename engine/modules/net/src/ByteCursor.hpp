#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Little-endian reading and writing on byte spans (declaration section 7, item 4: the net Module's own small
// buffers, not core/io's streams). Internal to the net Module.

namespace GYO::Engine::Net::Detail {

class ByteWriter final {
public:
    explicit ByteWriter(std::vector<std::byte>& out) noexcept : out_(out) {}

    void U8(const std::uint8_t value) { out_.push_back(static_cast<std::byte>(value)); }
    void U16(const std::uint16_t value) {
        U8(static_cast<std::uint8_t>(value));
        U8(static_cast<std::uint8_t>(value >> 8));
    }
    void U32(const std::uint32_t value) {
        U16(static_cast<std::uint16_t>(value));
        U16(static_cast<std::uint16_t>(value >> 16));
    }
    void Bytes(const std::span<const std::byte> bytes) { out_.insert(out_.end(), bytes.begin(), bytes.end()); }

private:
    std::vector<std::byte>& out_;
};

// Every read checks the length first and reports a short input with false; nothing asserts on input bytes.
class ByteReader final {
public:
    explicit ByteReader(const std::span<const std::byte> in) noexcept : in_(in) {}

    [[nodiscard]] std::size_t Remaining() const noexcept { return in_.size() - at_; }

    [[nodiscard]] bool U8(std::uint8_t& value) noexcept {
        if (Remaining() < 1) return false;
        value = static_cast<std::uint8_t>(in_[at_++]);
        return true;
    }
    [[nodiscard]] bool U16(std::uint16_t& value) noexcept {
        std::uint8_t low = 0, high = 0;
        if (Remaining() < 2) return false;
        (void)U8(low);
        (void)U8(high);
        value = static_cast<std::uint16_t>(low | (high << 8));
        return true;
    }
    [[nodiscard]] bool U32(std::uint32_t& value) noexcept {
        std::uint16_t low = 0, high = 0;
        if (Remaining() < 4) return false;
        (void)U16(low);
        (void)U16(high);
        value = static_cast<std::uint32_t>(low) | (static_cast<std::uint32_t>(high) << 16);
        return true;
    }
    [[nodiscard]] bool Bytes(const std::size_t count, std::span<const std::byte>& bytes) noexcept {
        if (Remaining() < count) return false;
        bytes = in_.subspan(at_, count);
        at_ += count;
        return true;
    }

private:
    std::span<const std::byte> in_;
    std::size_t at_ = 0;
};

} // namespace GYO::Engine::Net::Detail
