#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace Engine::Base {

// Portable content fingerprint shared by offline tools and asset consumers.
[[nodiscard]] inline std::string Sha256(std::span<const std::byte> bytes) {
    constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,8> hash{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const auto compress = [&](const std::byte* block) {
        std::array<std::uint32_t,64> words{};
        for (std::size_t i=0;i<16;++i) {
            for (std::size_t j=0;j<4;++j)
                words[i]=(words[i]<<8U)|std::to_integer<std::uint32_t>(block[i*4+j]);
        }
        for (std::size_t i=16;i<64;++i) {
            const auto x=words[i-15], y=words[i-2];
            words[i]=words[i-16]+(std::rotr(x,7)^std::rotr(x,18)^(x>>3U))+
                words[i-7]+(std::rotr(y,17)^std::rotr(y,19)^(y>>10U));
        }
        auto [a,b,c,d,e,f,g,h]=hash;
        for (std::size_t i=0;i<64;++i) {
            const auto t1=h+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+
                ((e&f)^((~e)&g))+constants[i]+words[i];
            const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+
                ((a&b)^(a&c)^(b&c));
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        const std::array next{a,b,c,d,e,f,g,h};
        for (std::size_t i=0;i<8;++i) hash[i]+=next[i];
    };
    std::size_t offset=0;
    while (bytes.size()-offset>=64) { compress(bytes.data()+offset); offset+=64; }
    std::array<std::byte,128> tail{};
    const auto remainder=bytes.size()-offset;
    for (std::size_t i=0;i<remainder;++i) tail[i]=bytes[offset+i];
    tail[remainder]=std::byte{0x80};
    const std::size_t padded=remainder<56 ? 64 : 128;
    const auto bits=static_cast<std::uint64_t>(bytes.size())*8U;
    for (std::size_t i=0;i<8;++i) tail[padded-1-i]=static_cast<std::byte>((bits>>(8*i))&255U);
    compress(tail.data());
    if (padded==128) compress(tail.data()+64);
    constexpr char digits[]="0123456789abcdef";
    std::string result(64,'0');
    for (std::size_t i=0;i<8;++i)
        for (std::size_t j=0;j<8;++j) result[i*8+j]=digits[(hash[i]>>(28-4*j))&15U];
    return result;
}

} // namespace Engine::Base
