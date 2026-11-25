#pragma once

// The admission token the matches service signs for each player of a Match (gateway declaration D5; the Go side is
// services/token/token.go, and both are checked against services/token/testdata/admission_vectors.txt). Integers
// big-endian:
//
//   version u8 (= 1) | matchId u64 | userId u64 | expiresAt u64 (Unix seconds) | reserved u32 | mac [32]
//   mac = HMAC-SHA256(key, the 29 bytes before it)
//
// The key is the Match's own (the matches service draws one per Match and hands it to the process). The check needs
// nobody else: no lookup, no call to the services.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace object_fps_pvp::match {

inline constexpr std::uint8_t kAdmissionVersion = 1;
inline constexpr std::size_t kAdmissionSigned = 1 + 8 + 8 + 8 + 4;
inline constexpr std::size_t kAdmissionSize = kAdmissionSigned + 32;

// What the check found, in the order it checks.
enum class AdmissionRefusal : std::uint8_t { Length = 1, Version, Signature, Match, User, Expired };

// The refusal's name, for the Refused detail the Client sees.
[[nodiscard]] std::string_view AdmissionRefusalName(AdmissionRefusal refusal) noexcept;

// HMAC-SHA256 (RFC 2104) over the engine's SHA-256.
[[nodiscard]] std::array<std::byte, 32> HmacSha256(std::span<const std::byte> key, std::span<const std::byte> message);

// The layout, the signature (compared in constant time), then the Match, the user and the expiry (refused from
// expiresAt on). nullopt: admitted.
[[nodiscard]] std::optional<AdmissionRefusal> VerifyAdmission(std::span<const std::byte> key, std::span<const std::byte> token,
                                                              std::int64_t nowUnixSeconds, std::uint64_t matchId,
                                                              std::uint64_t userId);

// Hexadecimal text (either case) as bytes; nullopt for an odd length or another character.
[[nodiscard]] std::optional<std::vector<std::byte>> DecodeHex(std::string_view text);

} // namespace object_fps_pvp::match
