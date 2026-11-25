#include "TestSupport.hpp"

#include "voxel_map_editor/ChunkedMap.hpp"
#include "voxel_map_editor/GoxImport.hpp"
#include "voxel_map_editor/Grid.hpp"

#include "engine/voxel_map/VoxelMap.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

// The Goxel import: the format read from its description, the layers that are geometry, the axes, the alpha, the
// colours as blocks, and the bake of the result read back by the voxel_map Module. The .gox files are made here by a
// minimal writer (stored-deflate PNG blocks), so every case says exactly what the file holds; the user's town map
// (fixtures/Kiriyu_Town_Graybox.gox) is the one real file.

using namespace voxel_map_editor;
using voxel_map_editor::test::ReadFixture;

namespace {

// ---- A minimal PNG writer: RGBA8, filter 0, one stored deflate block. -----------------------------------------------

std::uint32_t Crc32(const std::string& bytes) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const char c : bytes) {
        crc ^= static_cast<std::uint8_t>(c);
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void BigEndian(std::string& out, const std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out += static_cast<char>((value >> shift) & 0xFFu);
}

void PngChunk(std::string& out, const std::string& type, const std::string& data) {
    BigEndian(out, static_cast<std::uint32_t>(data.size()));
    out += type + data;
    BigEndian(out, Crc32(type + data));
}

// 64 x 64 RGBA pixels, row-major, voxel i at pixel i.
using BlockPixels = std::array<std::uint8_t, 64 * 64 * 4>;

// An RGBA8 PNG of width x height pixels (row-major, top row first).
std::string Png(const std::uint8_t* pixels, const std::uint32_t width, const std::uint32_t height) {
    std::string raw;
    for (std::size_t row = 0; row < height; ++row) {
        raw += '\0'; // filter type 0
        raw.append(reinterpret_cast<const char*>(pixels) + row * width * 4, width * 4);
    }
    std::string zlib = "\x78\x01";
    zlib += '\x01'; // final stored block
    const auto length = static_cast<std::uint16_t>(raw.size());
    zlib += static_cast<char>(length & 0xFF);
    zlib += static_cast<char>(length >> 8);
    zlib += static_cast<char>(~length & 0xFF);
    zlib += static_cast<char>((~length >> 8) & 0xFF);
    zlib += raw;
    std::uint32_t a = 1, b = 0;
    for (const char c : raw) {
        a = (a + static_cast<std::uint8_t>(c)) % 65521u;
        b = (b + a) % 65521u;
    }
    BigEndian(zlib, (b << 16) | a);
    std::string png = "\x89PNG\r\n\x1a\n";
    std::string header;
    BigEndian(header, width);
    BigEndian(header, height);
    header += std::string("\x08\x06\x00\x00\x00", 5); // 8 bits, RGBA, deflate, filter 0, no interlace
    PngChunk(png, "IHDR", header);
    PngChunk(png, "IDAT", zlib);
    PngChunk(png, "IEND", "");
    return png;
}

std::string Png(const BlockPixels& pixels) { return Png(pixels.data(), 64, 64); }

// ---- A minimal .gox writer. -----------------------------------------------------------------------------------------

void Little(std::string& out, const std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8) out += static_cast<char>((bits >> shift) & 0xFFu);
}

struct GoxVoxel final {
    std::int32_t x, y, z; // Goxel's coordinates
    std::array<std::uint8_t, 4> rgba;
};

struct GoxLayer final {
    std::string name;
    std::vector<GoxVoxel> voxels;
    std::optional<bool> visible = true; // nullopt: no "visible" entry
    bool clone = false;                 // a base_id entry
    bool shape = false;                 // a shape entry
};

struct GoxFile final {
    std::int32_t version = 2;
    std::vector<GoxLayer> layers;
    std::string extraChunks; // written before the blocks

    // One BL16 per 16^3 block a layer touches (blocks are not shared between layers), then one LAYR per layer.
    [[nodiscard]] std::string Bytes() const {
        std::string out = "GOX ";
        Little(out, version);
        out += extraChunks;
        std::string layers;
        std::int32_t next = 0;
        std::string blocks;
        for (const auto& layer : this->layers) {
            std::vector<std::pair<std::array<std::int32_t, 3>, BlockPixels>> owned;
            for (const auto& v : layer.voxels) {
                const auto floor16 = [](const std::int32_t c) { return c >= 0 ? c / 16 * 16 : -((-c + 15) / 16) * 16; };
                const std::array<std::int32_t, 3> at{floor16(v.x), floor16(v.y), floor16(v.z)};
                auto found = std::find_if(owned.begin(), owned.end(), [&](const auto& b) { return b.first == at; });
                if (found == owned.end()) {
                    owned.push_back({at, BlockPixels{}});
                    found = owned.end() - 1;
                }
                const auto i = static_cast<std::size_t>((v.x - at[0]) + 16 * (v.y - at[1]) + 256 * (v.z - at[2]));
                std::memcpy(found->second.data() + i * 4, v.rgba.data(), 4);
            }
            std::string data;
            Little(data, static_cast<std::int32_t>(owned.size()));
            for (const auto& [at, pixels] : owned) {
                Chunk(blocks, "BL16", Png(pixels));
                Little(data, next++);
                for (const auto c : at) Little(data, c);
                Little(data, 0);
            }
            Entry(data, "name", layer.name);
            if (layer.visible) Entry(data, "visible", std::string(1, *layer.visible ? '\x01' : '\x00'));
            if (layer.clone) {
                std::string id;
                Little(id, 7);
                Entry(data, "base_id", id);
            }
            if (layer.shape) Entry(data, "shape", "sphere");
            Little(data, 0);
            Chunk(layers, "LAYR", data);
        }
        return out + blocks + layers;
    }

    static void Chunk(std::string& out, const std::string& type, const std::string& data) {
        out += type;
        Little(out, static_cast<std::int32_t>(data.size()));
        out += data;
        Little(out, 0); // CRC: not checked
    }
    static void Entry(std::string& out, const std::string& key, const std::string& value) {
        Little(out, static_cast<std::int32_t>(key.size()));
        out += key;
        Little(out, static_cast<std::int32_t>(value.size()));
        out += value;
    }
};

constexpr std::array<std::uint8_t, 4> kRed{200, 10, 20, 255};
constexpr std::array<std::uint8_t, 4> kGreen{10, 200, 30, 255};
constexpr std::array<std::uint8_t, 4> kBlue{1, 2, 250, 255};

Result<GoxImport> Import(const GoxFile& file, const GoxImportOptions& options = {}) {
    const auto bytes = file.Bytes();
    return ImportGox(AsBytes(bytes), options);
}

std::string Refused(const GoxFile& file, const GoxImportOptions& options = {}) {
    const auto result = Import(file, options);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::InvalidGox);
    return result.error().message;
}

bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

} // namespace

TEST_CASE("gox: a Goxel voxel (x, y, z) is the cell (x, z, y), at its own coordinates, negative ones too") {
    GoxFile file;
    file.layers.push_back({"geometry", {{1, 2, 3, kRed}, {-16, -1, -5, kRed}}});
    const auto imported = Import(file);
    REQUIRE(imported);
    const auto& grid = imported->grid;
    CHECK(grid.origin == Vec3i{0, 0, 0});
    CHECK(grid.At({1, 3, 2}) == Cell{1, 0});
    CHECK(grid.At({-16, -5, -1}) == Cell{1, 0});
    CHECK(grid.At({1, 2, 3}) == Cell{});
    CHECK(imported->cells == 2);
}

TEST_CASE("gox: right, forward and up stay right, forward and up (the swap is no mirror of the scene)") {
    // Goxel: x right, y forward, z up (right-handed). gyo: x right, y up, z forward (left-handed).
    GoxFile file;
    file.layers.push_back({"geometry", {{0, 0, 0, kRed}, {1, 0, 0, kGreen}, {0, 1, 0, kBlue}, {0, 0, 1, kGreen}}});
    const auto imported = Import(file);
    REQUIRE(imported);
    const auto& grid = imported->grid;
    // Blocks in colour order: blue 010 2fa, green 0ac8 1e, red c80a14.
    CHECK(grid.At({1, 0, 0}).block == 2); // right: Goxel +x, green
    CHECK(grid.At({0, 0, 1}).block == 1); // forward: Goxel +y, blue, is gyo +z
    CHECK(grid.At({0, 1, 0}).block == 2); // up: Goxel +z, green, is gyo +y
    CHECK(grid.At({0, 0, 0}).block == 3);
}

TEST_CASE("gox: alpha below 127 is empty, 127 and above solid, and the block keeps the RGB only") {
    GoxFile file;
    file.layers.push_back({"geometry", {{0, 0, 0, {9, 9, 9, 126}}, {1, 0, 0, {9, 9, 9, 127}}, {2, 0, 0, {9, 9, 9, 200}}}});
    const auto imported = Import(file);
    REQUIRE(imported);
    CHECK(imported->grid.At({0, 0, 0}) == Cell{});
    CHECK(imported->grid.At({1, 0, 0}) == Cell{1, 0});
    CHECK(imported->grid.At({2, 0, 0}) == Cell{1, 0});
    REQUIRE(imported->grid.blocks.size() == 1);
    CHECK(imported->grid.blocks[0].color == std::array<std::uint8_t, 4>{9, 9, 9, 255});
}

TEST_CASE("gox: geometry is the visible layers with their own blocks, minus the excluded names, later over earlier") {
    GoxFile file;
    file.layers.push_back({"Terrain", {{0, 0, 0, kRed}, {1, 0, 0, kRed}}});
    file.layers.push_back({"Roads", {{1, 0, 0, kGreen}}}); // over the terrain's red
    file.layers.push_back({"Hidden", {{2, 0, 0, kBlue}}, false});
    file.layers.push_back({"Clone", {{3, 0, 0, kBlue}}, true, true});
    file.layers.push_back({"Sphere", {{4, 0, 0, kBlue}}, true, false, true});
    file.layers.push_back({"M1 | Hooded spirits 2x2x3", {{5, 0, 0, kBlue}}});
    file.layers.push_back({"Spawns and exits", {{6, 0, 0, kBlue}}});
    file.layers.push_back({"No visible entry", {{7, 0, 0, kRed}}, std::nullopt});
    GoxImportOptions options;
    options.excludeLayers = {"M? | *", "Spawns*"};
    const auto imported = Import(file, options);
    REQUIRE(imported);
    const auto& grid = imported->grid;
    // Blocks: green, red (no blue cell is left).
    REQUIRE(grid.blocks.size() == 2);
    CHECK(grid.At({0, 0, 0}).block == 2);
    CHECK(grid.At({1, 0, 0}).block == 1);
    for (std::int32_t x = 2; x <= 6; ++x) CHECK(grid.At({x, 0, 0}) == Cell{});
    CHECK(grid.At({7, 0, 0}).block == 2); // a layer without a "visible" entry is visible
    REQUIRE(imported->layers.size() == 8);
    const std::array<GoxLayerUse, 8> uses{GoxLayerUse::Geometry, GoxLayerUse::Geometry, GoxLayerUse::Hidden,
                                          GoxLayerUse::NoBlocks, GoxLayerUse::NoBlocks, GoxLayerUse::Excluded,
                                          GoxLayerUse::Excluded, GoxLayerUse::Geometry};
    for (std::size_t i = 0; i < uses.size(); ++i) CHECK_MESSAGE(imported->layers[i].use == uses[i], i);
    CHECK(imported->layers[0].name == "Terrain");
    CHECK(imported->layers[0].voxels == 2);
    CHECK(imported->layers[1].voxels == 1);
    CHECK(imported->layers[5].voxels == 0);
    CHECK(imported->cells == 3);
}

TEST_CASE("gox: one block per colour, named by it, a cube unless --block gives it a shape") {
    GoxFile file;
    file.layers.push_back({"geometry", {{0, 0, 0, kRed}, {1, 0, 0, kGreen}, {2, 0, 0, kBlue}}});
    GoxImportOptions options;
    options.blockShapes = {{{200, 10, 20}, "stair"}, {{10, 200, 30}, "slab"}, {{7, 7, 7}, "slab"}};
    options.cellSize = 0.25F;
    const auto imported = Import(file, options);
    REQUIRE(imported);
    const auto& grid = imported->grid;
    CHECK(grid.shapes == DefaultShapes());
    CHECK(grid.cellSize == 0.25F);
    REQUIRE(grid.blocks.size() == 3);
    CHECK(grid.blocks[0] == Block{"c_0102fa", 0, {1, 2, 250, 255}});
    CHECK(grid.blocks[1] == Block{"c_0ac81e", 1, {10, 200, 30, 255}});
    CHECK(grid.blocks[2] == Block{"c_c80a14", 2, {200, 10, 20, 255}});
    // A colour --block names that no geometry voxel has is reported, not an error.
    CHECK(imported->unusedShapeColours == std::vector<Rgb>{{7, 7, 7}});
    options.blockShapes = {{{200, 10, 20}, "ramp"}};
    CHECK(Contains(Refused(file, options), "no shape is named 'ramp'"));
    options.blockShapes = {{{7, 7, 7}, "ramp"}};
    CHECK(Contains(Refused(file, options), "no shape is named 'ramp'"));
}

TEST_CASE("gox: more than 256 colours is refused, 256 are a palette") {
    GoxFile file;
    file.layers.push_back({"geometry", {}});
    for (std::int32_t i = 0; i < 256; ++i)
        file.layers[0].voxels.push_back({i % 16, (i / 16) % 16, 0, {static_cast<std::uint8_t>(i), 1, 2, 255}});
    const auto full = Import(file);
    REQUIRE(full);
    CHECK(full->grid.blocks.size() == 256);
    file.layers[0].voxels.push_back({0, 0, 1, {0, 0, 0, 255}});
    CHECK(Contains(Refused(file), "257 colours"));
}

TEST_CASE("gox: what is not a .gox of version 2 is refused") {
    GoxFile empty;
    CHECK(Contains(Refused(empty), "no geometry layer has a voxel"));
    GoxFile version;
    version.version = 1;
    version.layers.push_back({"geometry", {{0, 0, 0, kRed}}});
    CHECK(Contains(Refused(version), "version 2"));
    const std::string notGox = "GOXX\x02\0\0\0";
    const auto magic = ImportGox(AsBytes(notGox), {});
    REQUIRE_FALSE(magic);
    CHECK(Contains(magic.error().message, "magic"));
    // A chunk longer than the file.
    GoxFile truncated;
    truncated.extraChunks = std::string("IMG ") + std::string("\xff\x00\x00\x00", 4);
    CHECK(Contains(Refused(truncated), "a chunk does not fit"));
    // A BL16 that is not a PNG.
    GoxFile notPng;
    GoxFile::Chunk(notPng.extraChunks, "BL16", "not a png");
    CHECK(Contains(Refused(notPng), "not a PNG"));
}

TEST_CASE("gox: a chunk of another type is skipped") {
    GoxFile file;
    GoxFile::Chunk(file.extraChunks, "MATE", "anything");
    file.layers.push_back({"geometry", {{0, 0, 0, kRed}}});
    CHECK(Import(file));
}

TEST_CASE("gox: a layer naming a block that is not there, or a block list longer than its chunk, is refused") {
    std::string bytes = "GOX ";
    Little(bytes, 2);
    std::string layer;
    Little(layer, 1);
    for (const std::int32_t value : {0, 0, 0, 0, 0}) Little(layer, value); // block 0, but no BL16 came before
    Little(layer, 0);
    GoxFile::Chunk(bytes, "LAYR", layer);
    const auto missing = ImportGox(AsBytes(bytes), {});
    REQUIRE_FALSE(missing);
    CHECK(Contains(missing.error().message, "names a block"));

    std::string longer = "GOX ";
    Little(longer, 2);
    std::string list;
    Little(list, 3); // three blocks, none written
    GoxFile::Chunk(longer, "LAYR", list);
    const auto overrun = ImportGox(AsBytes(longer), {});
    REQUIRE_FALSE(overrun);
    CHECK(Contains(overrun.error().message, "block list does not fit"));
}

TEST_CASE("gox: a BL16 image that is not 64 x 64 is refused") {
    const std::vector<std::uint8_t> pixels(32 * 64 * 4, 255);
    const auto png = Png(pixels.data(), 32, 64);
    GoxFile file;
    GoxFile::Chunk(file.extraChunks, "BL16", png);
    CHECK(Contains(Refused(file), "64 x 64"));
}

TEST_CASE("gox: an imported grid bakes into a map the voxel_map Module reads back") {
    GoxFile file;
    GoxLayer layer{"geometry", {}};
    for (std::int32_t x = -20; x < 20; ++x)
        for (std::int32_t y = -20; y < 20; ++y) layer.voxels.push_back({x, y, 0, kRed});
    layer.voxels.push_back({0, 0, 1, kGreen});
    file.layers.push_back(layer);
    GoxImportOptions options;
    options.blockShapes = {{{10, 200, 30}, "slab"}};
    const auto imported = Import(file, options);
    REQUIRE(imported);
    const auto text = WriteGrid(imported->grid);
    const auto map = BakeMap(imported->grid);
    REQUIRE(map);
    const auto written = WriteMap(*map, AsBytes(text));
    REQUIRE(written);
    const auto index = GYO::Engine::VoxelMap::ParseVoxelMap(written->index);
    REQUIRE(index);
    // 40 x 40 cells from -20: chunks -2 to 1 on x and z, the layer at gyo y = 0.
    CHECK(index->boundsMinimum == GYO::Engine::VoxelMap::VoxelChunkKey{-2, 0, -2});
    CHECK(index->boundsMaximum == GYO::Engine::VoxelMap::VoxelChunkKey{1, 0, 1});
    for (std::size_t r = 0; r < written->regions.size(); ++r)
        CHECK(GYO::Engine::VoxelMap::ParseVoxelRegion(written->regions[r].text, *index, index->regions[r].at));
}

TEST_CASE("gox: the town map imports and bakes (the counts an independent reader of the format gives)") {
    // fixtures/Kiriyu_Town_Graybox.gox: 11 layers, the last "Spawns and exits" (scene markers). A reader written
    // separately (Python, from the format's description) counts 644121 cells and 80 colours for the other layers.
    const auto bytes = ReadFixture("Kiriyu_Town_Graybox.gox");
    GoxImportOptions options;
    options.excludeLayers = {"Spawns*"};
    const auto imported = ImportGox(AsBytes(bytes), options);
    REQUIRE_MESSAGE(imported, (imported ? std::string() : imported.error().message));
    CHECK(imported->layers.size() == 11);
    CHECK(imported->layers.back().use == GoxLayerUse::Excluded);
    CHECK(imported->cells == 644121);
    CHECK(imported->grid.blocks.size() == 80);
    const auto text = WriteGrid(imported->grid);
    const auto map = BakeMap(imported->grid);
    REQUIRE(map);
    CHECK(WriteMap(*map, AsBytes(text)));
}

TEST_CASE("gox: layer name patterns, colours and block names") {
    CHECK(MatchesPattern("Spawns*", "Spawns and exits"));
    CHECK(MatchesPattern("*", ""));
    CHECK(MatchesPattern("M? | *", "M1 | Hooded spirits 2x2x3"));
    CHECK(MatchesPattern("*exclude*", "M6 | Boss hands only - exclude from count"));
    CHECK(MatchesPattern("a*b*c", "a__b__b__c"));
    CHECK_FALSE(MatchesPattern("M? | *", "M10 | x"));
    CHECK_FALSE(MatchesPattern("spawns*", "Spawns and exits")); // case-sensitive
    CHECK_FALSE(MatchesPattern("Spawns", "Spawns and exits"));
    CHECK_FALSE(MatchesPattern("a*c", "abcd"));
    CHECK(ParseRgb("51544a") == Rgb{0x51, 0x54, 0x4a});
    CHECK(ParseRgb("ABCDEF") == Rgb{0xab, 0xcd, 0xef});
    CHECK_FALSE(ParseRgb("51544"));
    CHECK_FALSE(ParseRgb("51544g"));
    CHECK_FALSE(ParseRgb("#51544a"));
    CHECK(ColourBlockName({0x51, 0x04, 0xff}) == "c_5104ff");
}
