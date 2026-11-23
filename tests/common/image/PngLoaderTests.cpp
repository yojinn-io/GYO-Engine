#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/image/PngLoader.hpp"

#include "AssertTestSupport.hpp"

// The "image"/"png" Data Contract (image.zh-Hant.md section 3): the pixel
// layout (section 2), every colour type and bit depth, the filters, Adam7,
// the ignored chunks, every condition of every rule, the order of the rules,
// the settings, a file from a real encoder, determinism (AS-3) and the way
// through the asset store. The PNG files are written here, byte by byte, with
// stored (uncompressed) deflate blocks, so each test shows the pixels it
// expects.

using namespace GYO::Engine;
using Image::ColorSpace;
using Image::ImageData;
using Image::PngLoader;
using Image::PngLoaderCode;
using Image::PngSettings;

namespace {

using Bytes = std::vector<std::byte>;
using Pixels = std::vector<std::uint8_t>;

void PutByte(Bytes& out, const unsigned value) { out.push_back(static_cast<std::byte>(value & 0xFFu)); }
void Put32(Bytes& out, const std::uint32_t value) {
    PutByte(out, value >> 24);
    PutByte(out, value >> 16);
    PutByte(out, value >> 8);
    PutByte(out, value);
}
void PutText(Bytes& out, const std::string& text) {
    for (const char c : text) PutByte(out, static_cast<unsigned char>(c));
}

std::uint32_t Crc32(const Bytes& bytes, const std::size_t from) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = from; i < bytes.size(); ++i) {
        crc ^= std::to_integer<std::uint32_t>(bytes[i]);
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

// One chunk with its length and CRC. crcDelta is added to the right CRC.
Bytes Chunk(const std::string& type, const Bytes& body, const std::uint32_t crcDelta = 0) {
    Bytes out;
    Put32(out, static_cast<std::uint32_t>(body.size()));
    PutText(out, type);
    out.insert(out.end(), body.begin(), body.end());
    Put32(out, Crc32(out, 4) + crcDelta);
    return out;
}

// A zlib stream of stored deflate blocks.
Bytes Zlib(const Bytes& data) {
    Bytes out;
    PutByte(out, 0x78);
    PutByte(out, 0x01);
    std::size_t at = 0;
    do {
        const std::size_t length = std::min<std::size_t>(data.size() - at, 65535);
        const bool last = at + length == data.size();
        PutByte(out, last ? 1 : 0);
        PutByte(out, static_cast<unsigned>(length));
        PutByte(out, static_cast<unsigned>(length >> 8));
        PutByte(out, static_cast<unsigned>(~length));
        PutByte(out, static_cast<unsigned>(~length >> 8));
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(at),
                   data.begin() + static_cast<std::ptrdiff_t>(at + length));
        at += length;
    } while (at < data.size());
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::byte value : data) {
        a = (a + std::to_integer<std::uint32_t>(value)) % 65521u;
        b = (b + a) % 65521u;
    }
    Put32(out, (b << 16) | a);
    return out;
}

const Bytes kSignature = [] {
    Bytes out;
    for (const unsigned value : {0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au}) PutByte(out, value);
    return out;
}();

Bytes Ihdr(const std::uint32_t width, const std::uint32_t height, const unsigned depth, const unsigned colorType,
           const unsigned interlace = 0) {
    Bytes body;
    Put32(body, width);
    Put32(body, height);
    PutByte(body, depth);
    PutByte(body, colorType);
    PutByte(body, 0);
    PutByte(body, 0);
    PutByte(body, interlace);
    return Chunk("IHDR", body);
}

Bytes Png(const std::vector<Bytes>& chunks) {
    Bytes out = kSignature;
    for (const Bytes& chunk : chunks) out.insert(out.end(), chunk.begin(), chunk.end());
    return out;
}

// Raw rows (without filter bytes) of one image or Adam7 pass, each row given
// as its packed bytes.
using Rows = std::vector<Bytes>;

std::uint8_t At(const Bytes& row, const std::size_t i) {
    return std::to_integer<std::uint8_t>(row[i]);
}

int Paeth(const int a, const int b, const int c) {
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

// Filters each row with filter type `filter` (0 to 4); bpp is the bytes per
// complete pixel, at least 1.
Bytes Filter(const Rows& rows, const unsigned filter, const std::size_t bpp) {
    Bytes out;
    const Bytes none;
    for (std::size_t y = 0; y < rows.size(); ++y) {
        const Bytes& row = rows[y];
        const Bytes& up = y == 0 ? none : rows[y - 1];
        PutByte(out, filter);
        for (std::size_t i = 0; i < row.size(); ++i) {
            const int x = At(row, i);
            const int a = i >= bpp ? At(row, i - bpp) : 0;
            const int b = up.empty() ? 0 : At(up, i);
            const int c = (i >= bpp && !up.empty()) ? At(up, i - bpp) : 0;
            int predicted = 0;
            switch (filter) {
            case 1: predicted = a; break;
            case 2: predicted = b; break;
            case 3: predicted = (a + b) / 2; break;
            case 4: predicted = Paeth(a, b, c); break;
            default: break;
            }
            PutByte(out, static_cast<unsigned>(x - predicted));
        }
    }
    return out;
}

// A whole file: IHDR, the extra chunks, one IDAT of the filtered rows, IEND.
Bytes File(const std::uint32_t width, const std::uint32_t height, const unsigned depth, const unsigned colorType,
           const Rows& rows, const std::vector<Bytes>& before = {}, const unsigned filter = 0,
           const std::size_t bpp = 1) {
    std::vector<Bytes> chunks{Ihdr(width, height, depth, colorType)};
    chunks.insert(chunks.end(), before.begin(), before.end());
    chunks.push_back(Chunk("IDAT", Zlib(Filter(rows, filter, bpp))));
    chunks.push_back(Chunk("IEND", {}));
    return Png(chunks);
}

Bytes Of(const std::vector<unsigned>& values) {
    Bytes out;
    for (const unsigned value : values) PutByte(out, value);
    return out;
}

Base::Result<ImageData, Asset::LoaderFailure<PngLoaderCode>> Load(const Bytes& bytes,
                                                                 const PngSettings settings = {}) {
    Asset::LoadContext context(Asset::AssetId("test.image"), "textures/test.png", PngLoader::kDefaultMaxBytes);
    return PngLoader{}.Load(std::span<const std::byte>(bytes), settings, context);
}

Pixels Loaded(const Bytes& bytes) {
    const auto loaded = Load(bytes);
    REQUIRE(loaded.has_value());
    return Pixels(loaded->Pixels().begin(), loaded->Pixels().end());
}

void Expect(const Bytes& bytes, const PngLoaderCode code) {
    const auto loaded = Load(bytes);
    REQUIRE_FALSE(loaded.has_value());
    // By name, so a failure shows which rule answered.
    CHECK(std::string(ToString(loaded.error().code)) == std::string(ToString(code)));
}

// 3 by 2 RGBA8 with every channel different: R = 10x + 1, G = 20 + y, B = 200, A = 50 + 10x + 100y.
Rows Rgba3x2() {
    Rows rows;
    for (unsigned y = 0; y < 2; ++y) {
        Bytes row;
        for (unsigned x = 0; x < 3; ++x) {
            for (const unsigned v : {10 * x + 1, 20 + y, 200u, 50 + 10 * x + 100 * y}) PutByte(row, v);
        }
        rows.push_back(row);
    }
    return rows;
}

Pixels Flatten(const Rows& rows) {
    Pixels out;
    for (const Bytes& row : rows) {
        for (const std::byte value : row) out.push_back(std::to_integer<std::uint8_t>(value));
    }
    return out;
}

Bytes ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Bytes out(chars.size());
    if (!chars.empty()) std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

} // namespace

TEST_CASE("PngLoader gives RGBA8, the top row first, rows packed (section 2)") {
    const Rows rows = Rgba3x2();
    const auto loaded = Load(File(3, 2, 8, 6, rows));
    REQUIRE(loaded.has_value());
    CHECK(loaded->Width() == 3);
    CHECK(loaded->Height() == 2);
    CHECK(loaded->RowPitch() == 12);
    CHECK(loaded->GetColorSpace() == ColorSpace::Srgb);
    // Byte (y * 12 + x * 4 + c) is channel c of pixel (x, y), y = 0 the first row of the file.
    CHECK(Pixels(loaded->Pixels().begin(), loaded->Pixels().end()) == Flatten(rows));
}

TEST_CASE("PngLoader turns every colour type and bit depth into RGBA8 (rule 4's conversions)") {
    SUBCASE("greyscale of 8 bits: (g, g, g, 255)") {
        CHECK(Loaded(File(2, 1, 8, 0, {Of({0, 77})})) == Pixels{0, 0, 0, 255, 77, 77, 77, 255});
    }
    SUBCASE("greyscale of 1, 2 and 4 bits is scaled to 0..255") {
        // 1 bit: 1 0 1 1 0 0 0 1 -> 255 or 0.
        CHECK(Loaded(File(3, 1, 1, 0, {Of({0b10100000})})) ==
              Pixels{255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255});
        // 2 bits: 3 1 -> 255, 85.
        CHECK(Loaded(File(2, 1, 2, 0, {Of({0b11010000})})) == Pixels{255, 255, 255, 255, 85, 85, 85, 255});
        // 4 bits: 15 2 -> 255, 34.
        CHECK(Loaded(File(2, 1, 4, 0, {Of({0xF2})})) == Pixels{255, 255, 255, 255, 34, 34, 34, 255});
    }
    SUBCASE("16 bits keep the high byte") {
        CHECK(Loaded(File(1, 1, 16, 0, {Of({0x12, 0xFF})})) == Pixels{0x12, 0x12, 0x12, 255});
        CHECK(Loaded(File(1, 1, 16, 2, {Of({0x01, 0x02, 0x03, 0x04, 0x05, 0x06})}, {}, 0, 6)) ==
              Pixels{0x01, 0x03, 0x05, 255});
        CHECK(Loaded(File(1, 1, 16, 4, {Of({0xAA, 0x01, 0x40, 0xFF})}, {}, 0, 4)) ==
              Pixels{0xAA, 0xAA, 0xAA, 0x40});
        CHECK(Loaded(File(1, 1, 16, 6, {Of({1, 0, 2, 0, 3, 0, 4, 0})}, {}, 0, 8)) == Pixels{1, 2, 3, 4});
    }
    SUBCASE("greyscale with alpha: (g, g, g, a)") {
        CHECK(Loaded(File(1, 1, 8, 4, {Of({90, 30})}, {}, 0, 2)) == Pixels{90, 90, 90, 30});
    }
    SUBCASE("RGB of 8 bits: (r, g, b, 255), Object_Connect's white1x1.png") {
        CHECK(Loaded(File(2, 1, 8, 2, {Of({1, 2, 3, 255, 255, 255})}, {}, 0, 3)) ==
              Pixels{1, 2, 3, 255, 255, 255, 255, 255});
    }
    SUBCASE("palette of 1, 2, 4 and 8 bits: the entry's colour, alpha from tRNS or 255") {
        const Bytes palette = Chunk("PLTE", Of({10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120}));
        const Bytes alpha = Chunk("tRNS", Of({0, 128}));
        // Indices 1 0 at 1 bit; entry 1 has alpha 128, entry 0 alpha 0.
        CHECK(Loaded(File(2, 1, 1, 3, {Of({0b10000000})}, {palette, alpha})) ==
              Pixels{40, 50, 60, 128, 10, 20, 30, 0});
        // Indices 2 3 at 2 bits; tRNS shorter than the palette: 255.
        CHECK(Loaded(File(2, 1, 2, 3, {Of({0b10110000})}, {palette, alpha})) ==
              Pixels{70, 80, 90, 255, 100, 110, 120, 255});
        CHECK(Loaded(File(2, 1, 4, 3, {Of({0x30})}, {palette})) == Pixels{100, 110, 120, 255, 10, 20, 30, 255});
        CHECK(Loaded(File(2, 1, 8, 3, {Of({2, 1})}, {palette})) == Pixels{70, 80, 90, 255, 40, 50, 60, 255});
    }
    SUBCASE("tRNS on greyscale and RGB: the one listed colour gets alpha 0") {
        CHECK(Loaded(File(2, 1, 8, 0, {Of({7, 8})}, {Chunk("tRNS", Of({0, 7}))})) ==
              Pixels{7, 7, 7, 0, 8, 8, 8, 255});
        CHECK(Loaded(File(2, 1, 8, 2, {Of({1, 2, 3, 1, 2, 4})}, {Chunk("tRNS", Of({0, 1, 0, 2, 0, 3}))}, 0, 3)) ==
              Pixels{1, 2, 3, 0, 1, 2, 4, 255});
    }
}

TEST_CASE("PngLoader undoes every filter type") {
    const Rows rows = Rgba3x2();
    for (unsigned filter = 0; filter <= 4; ++filter) {
        CAPTURE(filter);
        CHECK(Loaded(File(3, 2, 8, 6, rows, {}, filter, 4)) == Flatten(rows));
    }
}

TEST_CASE("PngLoader reads Adam7 interlaced files as the same pixels") {
    // 5 by 5 RGBA8; pixel (x, y) = (x, y, x + y, 255).
    constexpr unsigned kSize = 5;
    const auto pixel = [](const unsigned x, const unsigned y) { return Of({x, y, x + y, 255}); };
    Rows plain;
    for (unsigned y = 0; y < kSize; ++y) {
        Bytes row;
        for (unsigned x = 0; x < kSize; ++x) {
            const Bytes p = pixel(x, y);
            row.insert(row.end(), p.begin(), p.end());
        }
        plain.push_back(row);
    }
    // The seven passes: start x, start y, step x, step y.
    constexpr std::array<std::array<unsigned, 4>, 7> kPasses{{
        {0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4}, {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}}};
    Bytes filtered;
    for (const auto& pass : kPasses) {
        Rows rows;
        for (unsigned y = pass[1]; y < kSize; y += pass[3]) {
            Bytes row;
            for (unsigned x = pass[0]; x < kSize; x += pass[2]) {
                const Bytes p = pixel(x, y);
                row.insert(row.end(), p.begin(), p.end());
            }
            if (!row.empty()) rows.push_back(row);
        }
        // Filter type 1 (Sub), so the passes are not read as plain bytes by chance.
        const Bytes pass_bytes = Filter(rows, 1, 4);
        filtered.insert(filtered.end(), pass_bytes.begin(), pass_bytes.end());
    }
    const Bytes interlaced = Png({Ihdr(kSize, kSize, 8, 6, 1), Chunk("IDAT", Zlib(filtered)), Chunk("IEND", {})});
    CHECK(Loaded(interlaced) == Flatten(plain));
}

TEST_CASE("PngLoader ignores the colour chunks, other ancillary chunks, CRCs and split IDAT") {
    const Rows rows = Rgba3x2();
    // Object_Connect's textures carry sRGB, gAMA and pHYs before IDAT.
    const std::vector<Bytes> colour{Chunk("sRGB", Of({0})), Chunk("gAMA", Of({0, 0, 0xB1, 0x8F})),
                                    Chunk("pHYs", Of({0, 0, 0x0E, 0xC3, 0, 0, 0x0E, 0xC3, 1})),
                                    Chunk("tEXt", Of({'a', 0, 'b'}))};
    CHECK(Loaded(File(3, 2, 8, 6, rows, colour, 0, 4)) == Flatten(rows));
    // A wrong CRC on every chunk.
    const Bytes data = Zlib(Filter(rows, 0, 4));
    CHECK(Loaded(Png({Ihdr(3, 2, 8, 6), Chunk("IDAT", data, 1), Chunk("IEND", {}, 1)})) == Flatten(rows));
    // The image data split over two IDAT chunks.
    const Bytes first(data.begin(), data.begin() + 5);
    const Bytes second(data.begin() + 5, data.end());
    CHECK(Loaded(Png({Ihdr(3, 2, 8, 6), Chunk("IDAT", first), Chunk("IDAT", second), Chunk("IEND", {})})) ==
          Flatten(rows));
}

TEST_CASE("PngLoader rule 1: NotPng") {
    Expect(Bytes{}, PngLoaderCode::NotPng);
    Expect(Bytes(kSignature.begin(), kSignature.begin() + 7), PngLoaderCode::NotPng);
    for (std::size_t i = 0; i < kSignature.size(); ++i) {
        CAPTURE(i);
        Bytes bytes = File(3, 2, 8, 6, Rgba3x2());
        bytes[i] ^= std::byte{0x20};
        Expect(bytes, PngLoaderCode::NotPng);
    }
}

TEST_CASE("PngLoader rule 2: BadHeader") {
    const Bytes good = File(3, 2, 8, 6, Rgba3x2());
    SUBCASE("the signature alone, or the IHDR cut before its CRC ends") {
        Expect(kSignature, PngLoaderCode::BadHeader);
        Expect(Bytes(good.begin(), good.begin() + 32), PngLoaderCode::BadHeader);
    }
    SUBCASE("the first chunk is not IHDR") {
        for (std::size_t i = 12; i < 16; ++i) {
            CAPTURE(i);
            Bytes bytes = good;
            bytes[i] ^= std::byte{0x20};
            Expect(bytes, PngLoaderCode::BadHeader);
        }
        Expect(Png({Chunk("sRGB", Of({0})), Ihdr(3, 2, 8, 6), Chunk("IEND", {})}), PngLoaderCode::BadHeader);
    }
    SUBCASE("an IHDR of another length") {
        Bytes bytes = good;
        bytes[11] = std::byte{14};
        Expect(bytes, PngLoaderCode::BadHeader);
    }
    SUBCASE("a width or a height of 0") {
        Expect(File(0, 2, 8, 6, Rgba3x2()), PngLoaderCode::BadHeader);
        Expect(File(3, 0, 8, 6, Rgba3x2()), PngLoaderCode::BadHeader);
    }
}

TEST_CASE("PngLoader rule 3: TooLarge above 16384, 16384 itself loads") {
    CHECK(PngLoader::kMaxDimension == 16384);
    const Rows wide{Bytes(16384, std::byte{9})};
    const auto loaded = Load(File(16384, 1, 8, 0, wide));
    REQUIRE(loaded.has_value());
    CHECK(loaded->Width() == 16384);
    CHECK(loaded->Pixels()[16383 * 4] == 9);
    Rows tall(16384, Of({9}));
    CHECK(Load(File(1, 16384, 8, 0, tall)).has_value());
    // Only the header is read: the rest need not be there.
    Expect(Png({Ihdr(16385, 1, 8, 0)}), PngLoaderCode::TooLarge);
    Expect(Png({Ihdr(1, 16385, 8, 0)}), PngLoaderCode::TooLarge);
    Expect(Png({Ihdr(0xFFFFFFFFu, 1, 8, 0)}), PngLoaderCode::TooLarge);
}

TEST_CASE("PngLoader rule 4: Undecodable") {
    const Rows rows = Rgba3x2();
    SUBCASE("a colour type or bit depth PNG does not have") {
        Expect(File(3, 2, 8, 5, rows), PngLoaderCode::Undecodable);
        Expect(File(3, 2, 3, 2, rows), PngLoaderCode::Undecodable);
        Expect(File(3, 2, 4, 6, rows), PngLoaderCode::Undecodable);
    }
    SUBCASE("no image data, or too little of it") {
        Expect(Png({Ihdr(3, 2, 8, 6), Chunk("IEND", {})}), PngLoaderCode::Undecodable);
        const Rows short_rows{rows[0]};
        Expect(File(3, 2, 8, 6, short_rows, {}, 0, 4), PngLoaderCode::Undecodable);
    }
    SUBCASE("a broken zlib stream") {
        Bytes data = Zlib(Filter(rows, 0, 4));
        data[0] = std::byte{0x00};
        Expect(Png({Ihdr(3, 2, 8, 6), Chunk("IDAT", data), Chunk("IEND", {})}), PngLoaderCode::Undecodable);
    }
    SUBCASE("a filter type above 4") {
        Expect(File(3, 2, 8, 6, rows, {}, 5, 4), PngLoaderCode::Undecodable);
    }
    SUBCASE("a palette image without PLTE") {
        Expect(File(2, 1, 8, 3, {Of({0, 1})}), PngLoaderCode::Undecodable);
    }
    SUBCASE("a file cut inside IDAT") {
        const Bytes bytes = File(3, 2, 8, 6, rows, {}, 0, 4);
        Expect(Bytes(bytes.begin(), bytes.begin() + 40), PngLoaderCode::Undecodable);
    }
}

TEST_CASE("PngLoader checks its rules in their order") {
    // 1 before 2: no signature and no IHDR.
    Expect(Of({'G', 'I', 'F', '8'}), PngLoaderCode::NotPng);
    // 2 before 3: a zero width with a height above the limit.
    Expect(Png({Ihdr(0, 16385, 8, 0)}), PngLoaderCode::BadHeader);
    // 3 before 4: above the limit and no image data.
    Expect(Png({Ihdr(16385, 16385, 8, 5)}), PngLoaderCode::TooLarge);
}

TEST_CASE("PngLoader settings: the colour space") {
    const PngLoader loader;
    const auto parse = [&](const std::optional<std::string_view> json) { return loader.ParseSettings(json); };
    REQUIRE(parse(std::nullopt).has_value());
    CHECK(parse(std::nullopt)->colorSpace == ColorSpace::Srgb);
    CHECK(parse("{}")->colorSpace == ColorSpace::Srgb);
    CHECK(parse(R"({"colorSpace":"srgb"})")->colorSpace == ColorSpace::Srgb);
    CHECK(parse(R"({"colorSpace":"linear"})")->colorSpace == ColorSpace::Linear);
    for (const std::string_view bad :
         {R"({"colorSpace":"sRGB"})", R"({"colorSpace":1})", R"({"colorSpace":null})", R"({"colourSpace":"srgb"})",
          R"({"colorSpace":"linear","flip":true})", "[]", "not json"}) {
        CAPTURE(bad);
        const auto parsed = parse(bad);
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == PngLoaderCode::BadSettings);
    }
    // The colour space is a label: the bytes are the same.
    const Bytes bytes = File(3, 2, 8, 6, Rgba3x2());
    const auto linear = Load(bytes, PngSettings{ColorSpace::Linear});
    REQUIRE(linear.has_value());
    CHECK(linear->GetColorSpace() == ColorSpace::Linear);
    CHECK(Pixels(linear->Pixels().begin(), linear->Pixels().end()) == Loaded(bytes));
}

TEST_CASE("PngLoader reads a file from a real encoder (the contract example)") {
    // contract/gradient_rgba_16x8.png: written by contract/make_example.py with zlib
    // (compressed blocks); pixel (x, y) = (16x, 32y, 255 - 16x, 128 + 8y).
    const auto loaded = Load(ReadFile(std::filesystem::path(GYO_IMAGE_CONTRACT_DIR) / "gradient_rgba_16x8.png"));
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->Width() == 16);
    REQUIRE(loaded->Height() == 8);
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            const std::size_t at = std::size_t{y} * loaded->RowPitch() + x * 4;
            CAPTURE(x);
            CAPTURE(y);
            CHECK(loaded->Pixels()[at] == 16 * x);
            CHECK(loaded->Pixels()[at + 1] == 32 * y);
            CHECK(loaded->Pixels()[at + 2] == 255 - 16 * x);
            CHECK(loaded->Pixels()[at + 3] == 128 + 8 * y);
        }
    }
}

TEST_CASE("PngLoader gives the same image for the same bytes, on any thread (AS-3)") {
    const Bytes bytes = File(3, 2, 8, 6, Rgba3x2(), {}, 4, 4);
    const auto first = Load(bytes);
    REQUIRE(first.has_value());
    CHECK(*Load(bytes) == *first);
    int mismatches[2] = {0, 0};
    std::thread threads[2];
    for (int t = 0; t < 2; ++t) {
        threads[t] = std::thread([&, t] {
            for (int i = 0; i < 1000; ++i) {
                const auto loaded = Load(bytes);
                if (!loaded || !(*loaded == *first)) ++mismatches[t];
                // A failure in between leaves nothing behind for the next load.
                if (Load(Png({Ihdr(3, 2, 8, 6), Chunk("IEND", {})})).has_value()) ++mismatches[t];
            }
        });
    }
    for (std::thread& thread : threads) thread.join();
    CHECK(mismatches[0] == 0);
    CHECK(mismatches[1] == 0);
}

TEST_CASE("ImageData: pixels that do not fill width by height by 4 are a Programmer Error") {
    const ImageData empty;
    CHECK(empty.Width() == 0);
    CHECK(empty.Pixels().empty());
    const ImageData one(1, 1, ColorSpace::Linear, Pixels{1, 2, 3, 4});
    CHECK(one.RowPitch() == 4);
    CHECK(one.GetColorSpace() == ColorSpace::Linear);
    GYO::Test::ScopedAssertionHandler handler;
    CHECK_THROWS_AS(ImageData(2, 1, ColorSpace::Srgb, Pixels{1, 2, 3, 4}), Base::AssertionFailure);
}

namespace {

// Runs each job at once; a job submitted from a job waits in the queue.
class InlineExecutor final : public Asset::AssetExecutor {
public:
    void Submit(Job job) override {
        queue_.push_back(std::move(job));
        if (running_) return;
        running_ = true;
        while (!queue_.empty()) {
            Job next = std::move(queue_.front());
            queue_.pop_front();
            next();
        }
        running_ = false;
    }

private:
    std::deque<Job> queue_;
    bool running_ = false;
};

// A content root in the temporary directory holding textures/node.png.
struct Content final {
    std::filesystem::path root;
    explicit Content(const Bytes& png) {
        root = std::filesystem::temp_directory_path() /
               ("gyo_image_png_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(root / "textures");
        std::ofstream out(root / "textures" / "node.png", std::ios::binary);
        out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    }
    Content(const Content&) = delete;
    Content& operator=(const Content&) = delete;
    ~Content() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

Asset::CatalogEntry Entry(const std::string& id, std::optional<std::string> settings = std::nullopt) {
    Asset::CatalogEntry entry;
    entry.id = Asset::AssetId(id);
    entry.type = "image";
    entry.path = "textures/node.png";
    entry.settings = std::move(settings);
    return entry;
}

const Asset::AssetRecord& Record(const Asset::AssetSnapshot& snapshot, const std::string& id) {
    const Asset::AssetRecord* const record = snapshot.Find(Asset::AssetId(id));
    REQUIRE(record != nullptr);
    return *record;
}

} // namespace

TEST_CASE("PngLoader through the asset store: loaded with its settings, bad settings, a bad file, the size limit") {
    const Rows rows = Rgba3x2();
    const Bytes png = File(3, 2, 8, 6, rows);
    const Content content(png);
    InlineExecutor executor;
    const std::vector<Asset::CatalogEntry> entries{Entry("game.node"),
                                                   Entry("game.node_linear", R"({"colorSpace":"linear"})"),
                                                   Entry("game.node_bad", R"({"colorSpace":"cmyk"})")};

    SUBCASE("loaded, with each entry's settings") {
        Asset::AssetStoreOptions options;
        options.contentRoot = content.root.string();
        Asset::AssetStore store(entries, options, &executor);
        REQUIRE(store.Loaders().Register<PngLoader>().has_value());
        const auto node = store.Read(store.Request<ImageData>(Asset::AssetId("game.node")));
        REQUIRE(node != nullptr);
        CHECK(node->GetColorSpace() == ColorSpace::Srgb);
        CHECK(Pixels(node->Pixels().begin(), node->Pixels().end()) == Flatten(rows));
        const auto linear = store.Read(store.Request<ImageData>(Asset::AssetId("game.node_linear")));
        REQUIRE(linear != nullptr);
        CHECK(linear->GetColorSpace() == ColorSpace::Linear);
        CHECK(store.Read(store.Request<ImageData>(Asset::AssetId("game.node_bad"))) == nullptr);
        const Asset::AssetSnapshot snapshot = store.Snapshot();
        const Asset::AssetRecord& bad = Record(snapshot, "game.node_bad");
        REQUIRE(bad.failure.has_value());
        CHECK(bad.failure->code == Asset::AssetLoadFailureCode::BadSettings);
    }

    SUBCASE("a file above the limit is TooLarge and the loader is not called") {
        Asset::AssetStoreOptions options;
        options.contentRoot = content.root.string();
        options.byteLimits.push_back(Asset::AssetByteLimit{"image", "png", png.size() - 1});
        Asset::AssetStore store(entries, options, &executor);
        REQUIRE(store.Loaders().Register<PngLoader>().has_value());
        CHECK(store.Read(store.Request<ImageData>(Asset::AssetId("game.node"))) == nullptr);
        const Asset::AssetSnapshot snapshot = store.Snapshot();
        const Asset::AssetRecord& record = Record(snapshot, "game.node");
        REQUIRE(record.failure.has_value());
        CHECK(record.failure->code == Asset::AssetLoadFailureCode::TooLarge);
    }
}

TEST_CASE("PngLoader through the asset store: a file the loader refuses is LoaderFailed with its code") {
    Bytes broken = File(3, 2, 8, 6, Rgba3x2());
    broken[0] = std::byte{0};
    const Content content(broken);
    InlineExecutor executor;
    Asset::AssetStoreOptions options;
    options.contentRoot = content.root.string();
    const std::vector<Asset::CatalogEntry> entries{Entry("game.node")};
    Asset::AssetStore store(entries, options, &executor);
    REQUIRE(store.Loaders().Register<PngLoader>().has_value());
    CHECK(store.Read(store.Request<ImageData>(Asset::AssetId("game.node"))) == nullptr);
    const Asset::AssetSnapshot snapshot = store.Snapshot();
    const Asset::AssetRecord& record = Record(snapshot, "game.node");
    REQUIRE(record.failure.has_value());
    CHECK(record.failure->code == Asset::AssetLoadFailureCode::LoaderFailed);
    CHECK(record.failure->detail.find("NotPng") != std::string::npos);
}
