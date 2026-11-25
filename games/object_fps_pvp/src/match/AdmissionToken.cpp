#include "object_fps_pvp/match/AdmissionToken.hpp"

#include "engine/base/Sha256.hpp"

namespace object_fps_pvp::match {

namespace {

std::uint64_t ReadU64(const std::span<const std::byte> bytes, const std::size_t at) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8U) | std::to_integer<std::uint64_t>(bytes[at + i]);
    return value;
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
        hashedKey = GYO::Engine::Base::Sha256Digest(key);
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
    const auto innerDigest = GYO::Engine::Base::Sha256Digest(inner);
    for (std::size_t i = 0; i < innerDigest.size(); ++i) outer[kBlock + i] = innerDigest[i];
    return GYO::Engine::Base::Sha256Digest(outer);
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

} // namespace object_fps_pvp::match
