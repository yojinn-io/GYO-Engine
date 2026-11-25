// Base::Sha256Digest and Base::Sha256 (FIPS 180-4 test vectors): the digest's bytes, and the hex text made from them.
#include <doctest/doctest.h>

#include "engine/base/Sha256.hpp"

#include <string>

namespace {

std::string Hex(const std::array<std::byte, 32>& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (const std::byte b : digest) {
        text += digits[std::to_integer<unsigned>(b) >> 4U];
        text += digits[std::to_integer<unsigned>(b) & 15U];
    }
    return text;
}

std::span<const std::byte> Bytes(const std::string& text) { return std::as_bytes(std::span(text.data(), text.size())); }

} // namespace

TEST_CASE("sha256: the digest bytes and the hex text of the FIPS vectors") {
    const std::string abc = "abc";
    CHECK(Hex(GYO::Engine::Base::Sha256Digest(Bytes(abc))) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(GYO::Engine::Base::Sha256(Bytes(abc)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // Two blocks (56 bytes: the length no longer fits in the first).
    const std::string two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(GYO::Engine::Base::Sha256(Bytes(two)) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(GYO::Engine::Base::Sha256(Bytes(std::string())) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}
