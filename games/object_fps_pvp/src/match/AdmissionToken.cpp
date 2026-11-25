#include "object_fps_pvp/match/AdmissionToken.hpp"

#include "engine/base/Sha256.hpp"

namespace object_fps_pvp::match {

namespace {

std::uint64_t ReadU64(const std::span<const std::byte> bytes, const std::size_t at) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8U) | std::to_integer<std::uint64_t>(bytes[at + i]);
    return value;
}

std::array<std::byte, 32> Sha256Digest(const std::span<const std::byte> bytes) {
    const std::string text = GYO::Engine::Base::Sha256(bytes);
    const auto decoded = DecodeHex(text);
    std::array<std::byte, 32> digest{};
    for (std::size_t i = 0; i < digest.size(); ++i) digest[i] = (*decoded)[i];
    return digest;
}

} // namespace

std::string_view AdmissionRefusalName(const AdmissionRefusal refusal) noexcept {
    switch (refusal) {
    case AdmissionRefusal::Length: return "token length";
    case AdmissionRefusal::Version: return "token version";
    case AdmissionRefusal::Signature: return "token signature";
    case AdmissionRefusal::Match: return "token match";
    case AdmissionRefusal::User: return "token user";
    case AdmissionRefusal::Expired: return "token expired";
    }
    return "token";
}

std::array<std::byte, 32> HmacSha256(std::span<const std::byte> key, const std::span<const std::byte> message) {
    constexpr std::size_t kBlock = 64;
    std::array<std::byte, 32> hashedKey{};
    if (key.size() > kBlock) {
        hashedKey = Sha256Digest(key);
        key = hashedKey;
    }
    std::vector<std::byte> inner(kBlock + message.size());
    std::vector<std::byte> outer(kBlock + 32);
    for (std::size_t i = 0; i < kBlock; ++i) {
        const std::byte k = i < key.size() ? key[i] : std::byte{0};
        inner[i] = k ^ std::byte{0x36};
        outer[i] = k ^ std::byte{0x5c};
    }
    for (std::size_t i = 0; i < message.size(); ++i) inner[kBlock + i] = message[i];
    const auto innerDigest = Sha256Digest(inner);
    for (std::size_t i = 0; i < innerDigest.size(); ++i) outer[kBlock + i] = innerDigest[i];
    return Sha256Digest(outer);
}

std::optional<AdmissionRefusal> VerifyAdmission(const std::span<const std::byte> key, const std::span<const std::byte> token,
                                                const std::int64_t nowUnixSeconds, const std::uint64_t matchId,
                                                const std::uint64_t userId) {
    if (token.size() != kAdmissionSize) return AdmissionRefusal::Length;
    if (std::to_integer<std::uint8_t>(token[0]) != kAdmissionVersion) return AdmissionRefusal::Version;
    const auto expected = HmacSha256(key, token.first(kAdmissionSigned));
    std::uint8_t difference = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        difference |= std::to_integer<std::uint8_t>(expected[i] ^ token[kAdmissionSigned + i]);
    }
    if (difference != 0) return AdmissionRefusal::Signature;
    if (ReadU64(token, 1) != matchId) return AdmissionRefusal::Match;
    if (ReadU64(token, 9) != userId) return AdmissionRefusal::User;
    const std::uint64_t expiresAt = ReadU64(token, 17);
    if (nowUnixSeconds < 0 || static_cast<std::uint64_t>(nowUnixSeconds) >= expiresAt) return AdmissionRefusal::Expired;
    return std::nullopt;
}

std::optional<std::vector<std::byte>> DecodeHex(const std::string_view text) {
    if (text.size() % 2 != 0) return std::nullopt;
    const auto nibble = [](const char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<std::byte> bytes(text.size() / 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const int high = nibble(text[2 * i]);
        const int low = nibble(text[2 * i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        bytes[i] = static_cast<std::byte>((high << 4) | low);
    }
    return bytes;
}

} // namespace object_fps_pvp::match
