#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/base/Sha256.hpp"
#include "engine/voxel_map/VoxelMap.hpp"
#include "engine/voxel_map/VoxelMapLoaders.hpp"

#include "AssertTestSupport.hpp"

// The chunked map's Data Contract (voxel-map.zh-Hant.md): one test case per rule of the index (M1 to M10), of a region
// (R1 to R5) and of a chunk (C1 to C5), each with the path of the offending value; the two steps of a region against
// ValidateVoxelChunk; the expansion's float bits at both ends of the range; the loaders through the asset store.

using namespace GYO::Engine;
using namespace GYO::Engine::VoxelMap;

namespace {

const std::string kHex64(64, 'a');

// --- Index texts. --------------------------------------------------------------------------------------------------

struct MapText final {
    std::string schema = "\"gyo.voxel_map\"";
    std::string version = "1";
    std::string source = "\"" + kHex64 + "\"";
    std::string build = "\"voxel_map_editor 2\"";
    std::string origin = "[0, 0, 0]";
    std::string cellSize = "0.5";
    std::string chunkSize = "16";
    std::string regionSize = "8";
    std::string bounds = R"({"min": [-1, 0, -1], "max": [8, 1, 8]})";
    std::string palette = R"([{"name": "a", "faces": [[1,2,3,255],[4,5,6,255],[7,8,9,255],[10,11,12,255],[13,14,15,255],[16,17,18,255]]},
                              {"name": "b.c_2", "faces": [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[255,255,255,255]]}])";
    std::string regions;
    std::string extra;

    [[nodiscard]] std::string Text() const {
        return "{\"schema\": " + schema + ", \"version\": " + version + ", \"source_sha256\": " + source +
               ", \"build_version\": " + build + ", \"origin\": " + origin + ", \"cell_size\": " + cellSize +
               ", \"chunk_size\": " + chunkSize + ", \"region_size\": " + regionSize + ", \"bounds\": " + bounds +
               ", \"palette\": " + palette + ", \"regions\": " + regions + extra + "}";
    }
};

std::string RegionEntry(const int x, const int z, const std::string& file, const std::string& sha) {
    return "{\"at\": [" + std::to_string(x) + ", " + std::to_string(z) + "], \"file\": \"" + file + "\", \"sha256\": \"" +
           sha + "\"}";
}

std::string Sha(const std::string& text) { return Base::Sha256(std::as_bytes(std::span<const char>(text.data(), text.size()))); }

// --- Region texts. -------------------------------------------------------------------------------------------------

const std::string kChunkA =
    R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 2, 16, 16, 0], [0, 16, 0, 4, 32, 16, 1]], "colliders": [{"min": [0, 0, 0], "max": [16, 16, 16]}]})";
const std::string kChunkB = R"({"at": [-1, 1, -1], "quads": [], "colliders": [{"min": [0, 0, 0], "max": [256, 8, 256]}]})";

std::string RegionText(const std::string& at, const std::string& chunks) {
    return "{\"schema\": \"gyo.voxel_region\", \"version\": 1, \"at\": " + at + ", \"chunks\": [" + chunks + "]}";
}

const std::string kRegionText = RegionText("[-1, -1]", kChunkA + ", " + kChunkB);
const std::string kRegionText2 = RegionText("[1, 1]", R"({"at": [8, 0, 8], "quads": [[0, 0, 0, 0, 1, 1, 1]], "colliders": []})");

MapText ValidMap() {
    MapText map;
    map.regions = "[" + RegionEntry(-1, -1, "regions/r_-1_-1.json", Sha(kRegionText)) + ", " +
                  RegionEntry(1, 1, "regions/r_1_1.json", Sha(kRegionText2)) + "]";
    return map;
}

VoxelMapDocument Map() {
    auto map = ParseVoxelMap(ValidMap().Text());
    REQUIRE(map.has_value());
    return *map;
}

// The rule's name at the front of the message and the path of the offending value.
void Refused(const VoxelMapResult<VoxelMapDocument>& result, const std::string_view rule, const std::string_view path) {
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == VoxelMapErrorCode::InvalidVoxelMap);
    CHECK(std::string_view(result.error().message).substr(0, rule.size()) == rule);
    CHECK(result.error().detail == path);
}

template <class T>
void RegionRefused(const VoxelMapResult<T>& result, const std::string_view rule, const std::string_view path) {
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == VoxelMapErrorCode::InvalidVoxelRegion);
    CHECK(std::string_view(result.error().message).substr(0, rule.size()) == rule);
    CHECK(result.error().detail == path);
}

void ChunkRefused(const VoxelMapResult<void>& result, const std::string_view rule, const std::string_view path) {
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == VoxelMapErrorCode::InvalidVoxelChunk);
    CHECK(std::string_view(result.error().message).substr(0, rule.size()) == rule);
    CHECK(result.error().detail == path);
}

VoxelMapResult<VoxelMapDocument> MapWith(const std::function<void(MapText&)>& change) {
    MapText map = ValidMap();
    change(map);
    return ParseVoxelMap(map.Text());
}

// A region text whose index entry is rewritten to match it, so the index-dependent rules see the right SHA-256.
VoxelMapResult<VoxelRegionDocument> Region(const std::string& text) {
    MapText index;
    index.regions = "[" + RegionEntry(-1, -1, "regions/r_-1_-1.json", Sha(text)) + "]";
    auto map = ParseVoxelMap(index.Text());
    REQUIRE(map.has_value());
    return ParseVoxelRegion(text, *map, VoxelRegionKey{-1, -1});
}

std::string OneChunk(const std::string& chunk) { return RegionText("[-1, -1]", chunk); }

} // namespace

// --- The index. ----------------------------------------------------------------------------------------------------

TEST_CASE("M: a valid index reads every field") {
    const VoxelMapDocument map = Map();
    CHECK(map.sourceSha256 == kHex64);
    CHECK(map.buildVersion == "voxel_map_editor 2");
    CHECK(map.origin == Math::Vec3i{0, 0, 0});
    CHECK(map.cellSize == 0.5F);
    CHECK(map.boundsMinimum == VoxelChunkKey{-1, 0, -1});
    CHECK(map.boundsMaximum == VoxelChunkKey{8, 1, 8});
    REQUIRE(map.palette.size() == 2);
    CHECK(map.palette[0].name == "a");
    CHECK(map.palette[0].faces[5] == VoxelColor{16, 17, 18, 255});
    CHECK(map.palette[1].name == "b.c_2");
    REQUIRE(map.regions.size() == 2);
    CHECK(map.regions[0].at == VoxelRegionKey{-1, -1});
    CHECK(map.regions[0].file == "regions/r_-1_-1.json");
    CHECK(map.regions[0].sha256 == Sha(kRegionText));
    CHECK(map.regions[1].at == VoxelRegionKey{1, 1});
}

TEST_CASE("M1: one object with exactly the contract's fields, no repeated key, JSON") {
    Refused(ParseVoxelMap("not json"), "M1", "");
    Refused(ParseVoxelMap("[]"), "M1", "");
    Refused(MapWith([](MapText& m) { m.extra = ", \"more\": 1"; }), "M1", "");
    Refused(MapWith([](MapText& m) { m.extra = ", \"schema\": \"gyo.voxel_map\""; }), "M1", "");
    const std::string text = ValidMap().Text();
    std::string missing = text;
    missing.replace(missing.find("\"cell_size\""), 11, "\"cell_sizes\"");
    Refused(ParseVoxelMap(missing), "M1", "");
}

TEST_CASE("M2: the schema and version 1") {
    Refused(MapWith([](MapText& m) { m.schema = "\"gyo.voxel_mesh\""; }), "M2", "schema");
    Refused(MapWith([](MapText& m) { m.schema = "1"; }), "M2", "schema");
    Refused(MapWith([](MapText& m) { m.version = "2"; }), "M2", "version");
    Refused(MapWith([](MapText& m) { m.version = "\"1\""; }), "M2", "version");
    Refused(MapWith([](MapText& m) { m.version = "1.0"; }), "M2", "version");
}

TEST_CASE("M3: source_sha256 is 64 lowercase hex digits, build_version 1 to 64 printable ASCII") {
    Refused(MapWith([](MapText& m) { m.source = "\"" + std::string(64, 'A') + "\""; }), "M3", "source_sha256");
    Refused(MapWith([](MapText& m) { m.source = "\"" + std::string(63, 'a') + "\""; }), "M3", "source_sha256");
    Refused(MapWith([](MapText& m) { m.build = "\"\""; }), "M3", "build_version");
    Refused(MapWith([](MapText& m) { m.build = "\"" + std::string(65, 'x') + "\""; }), "M3", "build_version");
    Refused(MapWith([](MapText& m) { m.build = "\"a\\tb\""; }), "M3", "build_version");
    CHECK(MapWith([](MapText& m) { m.build = "\"" + std::string(64, '~') + "\""; }).has_value());
}

TEST_CASE("M4: origin three integers with 16 x origin within +-2^20, cell_size within [2^-16, 2^16]") {
    Refused(MapWith([](MapText& m) { m.origin = "[0, 0]"; }), "M4", "origin");
    Refused(MapWith([](MapText& m) { m.origin = "[0, 0.5, 0]"; }), "M4", "origin[1]");
    Refused(MapWith([](MapText& m) {
                m.origin = "[0, 65537, 0]";
                m.bounds = R"({"min": [0, -4096, 0], "max": [0, -4096, 0]})";
            }),
            "M4", "origin[1]");
    Refused(MapWith([](MapText& m) { m.cellSize = "0"; }), "M4", "cell_size");
    Refused(MapWith([](MapText& m) { m.cellSize = "131072"; }), "M4", "cell_size");
    Refused(MapWith([](MapText& m) { m.cellSize = "\"0.5\""; }), "M4", "cell_size");
    CHECK(MapWith([](MapText& m) { m.cellSize = "65536"; }).has_value());
    CHECK(MapWith([](MapText& m) { m.cellSize = "0.0000152587890625"; }).has_value());
}

TEST_CASE("M5: chunk_size is 16 and region_size 8") {
    Refused(MapWith([](MapText& m) { m.chunkSize = "32"; }), "M5", "chunk_size");
    Refused(MapWith([](MapText& m) { m.chunkSize = "16.0"; }), "M5", "chunk_size");
    Refused(MapWith([](MapText& m) { m.regionSize = "4"; }), "M5", "region_size");
}

TEST_CASE("M6: bounds min <= max, every corner within +-2^20 units") {
    Refused(MapWith([](MapText& m) { m.bounds = R"({"min": [0, 2, 0], "max": [8, 1, 8]})"; }), "M6", "bounds.max[1]");
    Refused(MapWith([](MapText& m) { m.bounds = R"({"min": [0, 0, 0], "max": [8, 1, 8], "more": 1})"; }), "M6", "bounds");
    Refused(MapWith([](MapText& m) { m.bounds = R"({"min": [0, 0], "max": [8, 1, 8]})"; }), "M6", "bounds.min");
    // 256 x 4096 + 256 is above 2^20; 256 x 4095 + 256 is 2^20 exactly.
    Refused(MapWith([](MapText& m) { m.bounds = R"({"min": [-1, 0, -1], "max": [4096, 1, 8]})"; }), "M6", "bounds");
    Refused(MapWith([](MapText& m) { m.bounds = R"({"min": [-4097, 0, -1], "max": [8, 1, 8]})"; }), "M6", "bounds");
    CHECK(MapWith([](MapText& m) { m.bounds = R"({"min": [-4096, 0, -1], "max": [4095, 1, 8]})"; }).has_value());
    // The origin counts: 16 x 16 = 256 units more on x.
    Refused(MapWith([](MapText& m) {
                m.origin = "[16, 0, 0]";
                m.bounds = R"({"min": [-1, 0, -1], "max": [4095, 1, 8]})";
            }),
            "M6", "bounds");
}

TEST_CASE("M7: 1 to 256 palette entries of a unique name and six colours of four components 0 to 255") {
    Refused(MapWith([](MapText& m) { m.palette = "[]"; }), "M7", "palette");
    std::string many = "[";
    for (int i = 0; i < 257; ++i) {
        many += (i > 0 ? "," : "") + std::string("{\"name\": \"n") + std::to_string(i) +
                "\", \"faces\": [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]}";
    }
    Refused(MapWith([&many](MapText& m) { m.palette = many + "]"; }), "M7", "palette");
    const std::string six = "[[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]";
    Refused(MapWith([&six](MapText& m) { m.palette = "[{\"name\": \"A\", \"faces\": " + six + "}]"; }), "M7", "palette[0].name");
    Refused(MapWith([&six](MapText& m) { m.palette = "[{\"name\": \"\", \"faces\": " + six + "}]"; }), "M7", "palette[0].name");
    Refused(MapWith([&six](MapText& m) { m.palette = "[{\"name\": \"" + std::string(65, 'a') + "\", \"faces\": " + six + "}]"; }),
            "M7", "palette[0].name");
    Refused(MapWith([&six](MapText& m) {
                m.palette = "[{\"name\": \"a\", \"faces\": " + six + "}, {\"name\": \"a\", \"faces\": " + six + "}]";
            }),
            "M7", "palette[1].name");
    Refused(MapWith([](MapText& m) { m.palette = R"([{"name": "a", "faces": [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]}])"; }),
            "M7", "palette[0].faces");
    Refused(MapWith([](MapText& m) { m.palette = R"([{"name": "a", "faces": [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,256,0]]}])"; }),
            "M7", "palette[0].faces[5][2]");
    Refused(MapWith([](MapText& m) { m.palette = R"([{"name": "a", "faces": [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0]]}])"; }),
            "M7", "palette[0].faces[5]");
    Refused(MapWith([&six](MapText& m) { m.palette = "[{\"name\": \"a\", \"faces\": " + six + ", \"x\": 1}]"; }), "M7", "palette[0]");
}

TEST_CASE("M8: regions in strictly increasing order, inside the bounds, with an allowed unique file and a sha256") {
    const std::string sha = Sha(kRegionText);
    Refused(MapWith([](MapText& m) { m.regions = "[]"; }), "M8", "regions");
    Refused(MapWith([&sha](MapText& m) { m.regions = "[" + RegionEntry(-2, 0, "r.json", sha) + "]"; }), "M8", "regions[0].at");
    Refused(MapWith([&sha](MapText& m) { m.regions = "[" + RegionEntry(0, 2, "r.json", sha) + "]"; }), "M8", "regions[0].at");
    Refused(MapWith([&sha](MapText& m) {
                m.regions = "[" + RegionEntry(1, 0, "a.json", sha) + ", " + RegionEntry(0, 1, "b.json", sha) + "]";
            }),
            "M8", "regions[1].at");
    Refused(MapWith([&sha](MapText& m) {
                m.regions = "[" + RegionEntry(0, 0, "a.json", sha) + ", " + RegionEntry(0, 0, "b.json", sha) + "]";
            }),
            "M8", "regions[1].at");
    Refused(MapWith([&sha](MapText& m) {
                m.regions = "[" + RegionEntry(0, 0, "a.json", sha) + ", " + RegionEntry(0, 1, "a.json", sha) + "]";
            }),
            "M8", "regions[1].file");
    Refused(MapWith([](MapText& m) { m.regions = "[" + RegionEntry(0, 0, "a.json", std::string(64, 'G')) + "]"; }), "M8",
            "regions[0].sha256");
    Refused(MapWith([](MapText& m) { m.regions = "[{\"at\": [0, 0], \"file\": \"a.json\"}]"; }), "M8", "regions[0]");
    for (const std::string& bad : std::vector<std::string>{"/a.json", "a//b.json", "a/./b.json", "a/../b.json", "../a.json", "a/b.", "a/", "con",
                                  "con.json", "d/aux.x", "com1", "lpt9.json", "A.json", "a b.json", "a\\\\b.json",
                                  std::string(129, 'a')}) {
        CAPTURE(bad);
        Refused(MapWith([&](MapText& m) { m.regions = "[" + RegionEntry(0, 0, bad, sha) + "]"; }), "M8", "regions[0].file");
    }
    for (const std::string& good : std::vector<std::string>{"regions/r_-1_-1.json", "a.b/c-d_e.json", "console.json", "com10.json", "com0",
                                   std::string(128, 'a')}) {
        CAPTURE(good);
        CHECK(MapWith([&](MapText& m) { m.regions = "[" + RegionEntry(0, 0, good, sha) + "]"; }).has_value());
    }
}

TEST_CASE("M9: every integer within +-2^30") {
    Refused(MapWith([](MapText& m) { m.version = "1073741825"; }), "M9", "version");
    Refused(MapWith([](MapText& m) { m.bounds = R"({"min": [-1073741825, 0, -1], "max": [8, 1, 8]})"; }), "M9", "bounds.min[0]");
    Refused(MapWith([](MapText& m) { m.chunkSize = "99999999999999999999"; }), "M9", "chunk_size");
}

TEST_CASE("M10: the text is at most 1 MiB") {
    MapText map = ValidMap();
    const std::size_t room = kMaxMapTextBytes - map.Text().size();
    map.build = "\"voxel_map_editor 2\"" + std::string(room, ' ');
    CHECK(ParseVoxelMap(map.Text()).has_value());
    map.build += " ";
    Refused(ParseVoxelMap(map.Text()), "M10", "");
}

// --- A region. -----------------------------------------------------------------------------------------------------

TEST_CASE("R: a valid region reads its chunks in key order and its text's SHA-256") {
    const VoxelMapDocument map = Map();
    const auto region = ParseVoxelRegion(kRegionText, map, VoxelRegionKey{-1, -1});
    REQUIRE(region.has_value());
    CHECK(region->at == VoxelRegionKey{-1, -1});
    CHECK(region->textSha256 == Sha(kRegionText));
    REQUIRE(region->chunks.size() == 2);
    CHECK(region->chunks[0].at == VoxelChunkKey{-1, 0, -1});
    REQUIRE(region->chunks[0].quads.size() == 2);
    CHECK(region->chunks[0].quads[1] == VoxelQuad{0, 16, 0, VoxelFace::PositiveZ, 32, 16, 1});
    CHECK(region->chunks[0].colliders[0] == VoxelBox{{0, 0, 0}, {16, 16, 16}});
    CHECK(region->chunks[1].quads.empty());
    // The first step alone gives the same document.
    const auto first = ParseVoxelRegion(kRegionText);
    REQUIRE(first.has_value());
    CHECK(*first == *region);
}

TEST_CASE("R1: one object with exactly schema, version, at and chunks, JSON") {
    RegionRefused(ParseVoxelRegion(std::string_view("{")), "R1", "");
    RegionRefused(ParseVoxelRegion(std::string_view("[]")), "R1", "");
    RegionRefused(ParseVoxelRegion(std::string_view(R"({"schema": "gyo.voxel_region", "version": 1, "at": [0, 0]})")), "R1", "");
    RegionRefused(ParseVoxelRegion(std::string_view(
                      R"({"schema": "gyo.voxel_region", "version": 1, "at": [0, 0], "chunks": [], "x": 1})")),
                  "R1", "");
}

TEST_CASE("R2: the schema and version 1") {
    RegionRefused(ParseVoxelRegion(std::string_view(R"({"schema": "gyo.voxel_map", "version": 1, "at": [0, 0], "chunks": []})")),
                  "R2", "schema");
    RegionRefused(ParseVoxelRegion(std::string_view(R"({"schema": "gyo.voxel_region", "version": 2, "at": [0, 0], "chunks": []})")),
                  "R2", "version");
}

TEST_CASE("R3: at is the region the index entry names and the text has that entry's SHA-256") {
    const VoxelMapDocument map = Map();
    RegionRefused(ParseVoxelRegion(std::string_view(RegionText("[0]", kChunkA))), "R3", "at");
    // kRegionText2's own text, read for the entry of (-1, -1): its at is (1, 1).
    RegionRefused(ParseVoxelRegion(kRegionText2, map, VoxelRegionKey{-1, -1}), "R3", "at");
    // The right at, another text (one more space).
    const std::string changed = kRegionText + " ";
    RegionRefused(ParseVoxelRegion(changed, map, VoxelRegionKey{-1, -1}), "R3", "");
    const auto first = ParseVoxelRegion(changed);
    REQUIRE(first.has_value());
    RegionRefused(CheckVoxelRegion(*first, map, VoxelRegionKey{-1, -1}), "R3", "");
    // An expected key the index does not list is the caller's error.
    GYO::Test::ScopedAssertionHandler handler;
    CHECK_THROWS_AS((void)CheckVoxelRegion(*first, map, VoxelRegionKey{0, 0}), Base::AssertionFailure);
}

TEST_CASE("CheckVoxelMap: every region against its index entry, in index order, paths under regions[i]") {
    const VoxelMapDocument map = Map();
    const auto first = ParseVoxelRegion(kRegionText);
    const auto second = ParseVoxelRegion(kRegionText2);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    const std::array<const VoxelRegionDocument*, 2> both{&*first, &*second};
    CHECK(CheckVoxelMap(map, both).has_value());
    // The second region's text read for the first entry and the other way round: R3 at the first one.
    const std::array<const VoxelRegionDocument*, 2> swapped{&*second, &*first};
    RegionRefused(CheckVoxelMap(map, swapped), "R3", "regions[0].at");
    // A region whose text is not the entry's: R3 with the whole region's path.
    const auto changed = ParseVoxelRegion(kRegionText2 + " ");
    REQUIRE(changed.has_value());
    const std::array<const VoxelRegionDocument*, 2> stale{&*first, &*changed};
    RegionRefused(CheckVoxelMap(map, stale), "R3", "regions[1]");
    // A palette index outside a smaller palette in the first region's first chunk: C2 under regions[0].
    VoxelMapDocument small = map;
    small.palette.resize(1);
    RegionRefused(CheckVoxelMap(small, both), "C2", "regions[0].chunks[0].quads[1][6]");
}

TEST_CASE("R4: one or more chunks of exactly at, quads and colliders, in this region, in strictly increasing key order") {
    RegionRefused(Region(RegionText("[-1, -1]", "")), "R4", "chunks");
    RegionRefused(Region(OneChunk(R"({"at": [0, 0, -1], "quads": [], "colliders": [{"min": [0,0,0], "max": [1,1,1]}]})")), "R4",
                  "chunks[0].at");
    RegionRefused(Region(OneChunk(R"({"at": [-9, 0, -1], "quads": [], "colliders": [{"min": [0,0,0], "max": [1,1,1]}]})")), "R4",
                  "chunks[0].at");
    RegionRefused(Region(RegionText("[-1, -1]", kChunkB + ", " + kChunkA)), "R4", "chunks[1].at");
    RegionRefused(Region(RegionText("[-1, -1]", kChunkA + ", " + kChunkA)), "R4", "chunks[1].at");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [], "colliders": [], "x": 1})")), "R4", "chunks[0]");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0], "quads": [], "colliders": []})")), "R4", "chunks[0].at");
    // Inside the region but outside the map's bounds (y 2 > 1): the index's rule, in the second step.
    RegionRefused(Region(OneChunk(R"({"at": [-1, 2, -1], "quads": [], "colliders": [{"min": [0,0,0], "max": [1,1,1]}]})")), "C1",
                  "chunks[0].at");
}

TEST_CASE("R5: every integer within +-2^30, the text at most 16 MiB") {
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 1073741825, 1, 0]], "colliders": []})")), "R5",
                  "chunks[0].quads[0][4]");
    RegionRefused(ParseVoxelRegion(std::string_view(RegionText("[-1, -1073741825]", kChunkA))), "R5", "at[1]");
    std::string big = kRegionText;
    big.insert(big.size() - 1, std::string(kMaxRegionTextBytes - big.size(), ' '));
    CHECK(ParseVoxelRegion(std::string_view(big)).has_value());
    big.insert(big.size() - 1, " ");
    RegionRefused(ParseVoxelRegion(std::string_view(big)), "R5", "");
}

// --- A chunk, through a region's text and through ValidateVoxelChunk (one rule implementation). --------------------

namespace {

// The same violation as text (one chunk of the region (-1, -1)) and as a document: the same rule, the region's path
// under chunks[0], the chunk's relative.
void BothRefuse(const std::string& chunkText, const VoxelChunkDocument& chunk, const std::string_view rule,
                const std::string_view path) {
    const std::string regionPath = path.empty() ? std::string("chunks[0]") : "chunks[0]." + std::string(path);
    RegionRefused(Region(OneChunk(chunkText)), rule, regionPath);
    ChunkRefused(ValidateVoxelChunk(chunk, Map()), rule, path);
}

VoxelChunkDocument ChunkWith(std::vector<VoxelQuad> quads, std::vector<VoxelBox> boxes, VoxelChunkKey at = {-1, 0, -1}) {
    return {at, std::move(quads), std::move(boxes)};
}

} // namespace

TEST_CASE("C1: a chunk has a quad or a collider and lies in the map's bounds") {
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [], "colliders": []})", ChunkWith({}, {}), "C1", "");
    ChunkRefused(ValidateVoxelChunk(ChunkWith({}, {{{0, 0, 0}, {1, 1, 1}}}, {-2, 0, -1}), Map()), "C1", "at");
    CHECK(ValidateVoxelChunk(ChunkWith({}, {{{0, 0, 0}, {1, 1, 1}}}, {8, 1, 8}), Map()).has_value());
}

TEST_CASE("C2: a quad is seven integers, direction 0 to 5, sizes at least 1, a palette index into the map's palette") {
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 6, 1, 1, 0]], "colliders": []})",
               ChunkWith({{0, 0, 0, static_cast<VoxelFace>(6), 1, 1, 0}}, {}), "C2", "quads[0][3]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 0, 1, 0]], "colliders": []})",
               ChunkWith({{0, 0, 0, VoxelFace::PositiveX, 0, 1, 0}}, {}), "C2", "quads[0][4]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 1, -1, 0]], "colliders": []})",
               ChunkWith({{0, 0, 0, VoxelFace::PositiveX, 1, -1, 0}}, {}), "C2", "quads[0][5]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 1, 1, 2]], "colliders": []})",
               ChunkWith({{0, 0, 0, VoxelFace::PositiveX, 1, 1, 2}}, {}), "C2", "quads[0][6]");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, -1, 1, 1, 0]], "colliders": []})")), "C2",
                  "chunks[0].quads[0][3]");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 1, 1, -1]], "colliders": []})")), "C2",
                  "chunks[0].quads[0][6]");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 1, 1]], "colliders": []})")), "C2",
                  "chunks[0].quads[0]");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 1, 1, 0.5]], "colliders": []})")), "C2",
                  "chunks[0].quads[0][6]");
    CHECK(ValidateVoxelChunk(ChunkWith({{0, 0, 0, VoxelFace::NegativeZ, 1, 1, 1}}, {}), Map()).has_value());
}

TEST_CASE("C3: a collider has exactly min and max, three integers each, min < max on every axis") {
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [0, 0, 0], "max": [1, 0, 1]}]})",
               ChunkWith({}, {{{0, 0, 0}, {1, 0, 1}}}), "C3", "colliders[0]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [0, 0, 2], "max": [1, 1, 1]}]})",
               ChunkWith({}, {{{0, 0, 2}, {1, 1, 1}}}), "C3", "colliders[0]");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [0, 0, 0]}]})")), "C3",
                  "chunks[0].colliders[0]");
    RegionRefused(Region(OneChunk(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [0, 0, 0], "max": [1, 1]}]})")), "C3",
                  "chunks[0].colliders[0].max");
}

TEST_CASE("C4: every corner of a quad or collider within [0, 256]") {
    // A Y face along X (width) and Z (height): its far corner is (x + w, y, z + h).
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 256, 200, 2, 256, 57, 0]], "colliders": []})",
               ChunkWith({{0, 256, 200, VoxelFace::PositiveY, 256, 57, 0}}, {}), "C4", "quads[0]");
    // An X face runs along Z (width) and Y (height).
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[256, 0, 255, 0, 2, 1, 0]], "colliders": []})",
               ChunkWith({{256, 0, 255, VoxelFace::PositiveX, 2, 1, 0}}, {}), "C4", "quads[0]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 255, 0, 5, 1, 2, 0]], "colliders": []})",
               ChunkWith({{0, 255, 0, VoxelFace::NegativeZ, 1, 2, 0}}, {}), "C4", "quads[0]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[-1, 0, 0, 2, 1, 1, 0]], "colliders": []})",
               ChunkWith({{-1, 0, 0, VoxelFace::PositiveY, 1, 1, 0}}, {}), "C4", "quads[0]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [0, 0, 0], "max": [1, 257, 1]}]})",
               ChunkWith({}, {{{0, 0, 0}, {1, 257, 1}}}), "C4", "colliders[0]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [-1, 0, 0], "max": [1, 1, 1]}]})",
               ChunkWith({}, {{{-1, 0, 0}, {1, 1, 1}}}), "C4", "colliders[0]");
    CHECK(ValidateVoxelChunk(ChunkWith({{256, 0, 0, VoxelFace::PositiveX, 256, 256, 0}}, {{{0, 0, 0}, {256, 256, 256}}}), Map())
              .has_value());
}

TEST_CASE("C5: quads and colliders each in strictly increasing order") {
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 0, 1, 0, 1, 1, 0], [0, 0, 0, 0, 1, 1, 0]], "colliders": []})",
               ChunkWith({{0, 0, 1, VoxelFace::PositiveX, 1, 1, 0}, {0, 0, 0, VoxelFace::PositiveX, 1, 1, 0}}, {}), "C5",
               "quads[1]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [[0, 0, 0, 0, 1, 1, 0], [0, 0, 0, 0, 1, 1, 0]], "colliders": []})",
               ChunkWith({{0, 0, 0, VoxelFace::PositiveX, 1, 1, 0}, {0, 0, 0, VoxelFace::PositiveX, 1, 1, 0}}, {}), "C5",
               "quads[1]");
    // The palette index is the last key: (.., 1) after (.., 0) is in order.
    CHECK(ValidateVoxelChunk(ChunkWith({{0, 0, 0, VoxelFace::PositiveX, 1, 1, 0}, {0, 0, 0, VoxelFace::PositiveX, 1, 1, 1}}, {}),
                             Map())
              .has_value());
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [0, 0, 0], "max": [2, 2, 2]}, {"min": [0, 0, 0], "max": [1, 2, 2]}]})",
               ChunkWith({}, {{{0, 0, 0}, {2, 2, 2}}, {{0, 0, 0}, {1, 2, 2}}}), "C5", "colliders[1]");
    BothRefuse(R"({"at": [-1, 0, -1], "quads": [], "colliders": [{"min": [0, 0, 0], "max": [1, 1, 1]}, {"min": [0, 0, 0], "max": [1, 1, 1]}]})",
               ChunkWith({}, {{{0, 0, 0}, {1, 1, 1}}, {{0, 0, 0}, {1, 1, 1}}}), "C5", "colliders[1]");
}

// --- Expansion. ----------------------------------------------------------------------------------------------------

namespace {

Math::Vec3 Minus(const Math::Vec3& a, const Math::Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
bool SameBits(const Math::Vec3& a, const Math::Vec3& b) {
    return std::bit_cast<std::uint32_t>(a.x) == std::bit_cast<std::uint32_t>(b.x) &&
           std::bit_cast<std::uint32_t>(a.y) == std::bit_cast<std::uint32_t>(b.y) &&
           std::bit_cast<std::uint32_t>(a.z) == std::bit_cast<std::uint32_t>(b.z);
}

} // namespace

TEST_CASE("Expansion: world coordinates, corners, normals, winding, uv and indices") {
    VoxelMapDocument map = Map();
    map.origin = {-2, 1, 3};
    map.cellSize = 0.5F;
    // A Z face at (16, 32, 48) in chunk (-1, 0, -1): 2 x 3 cells.
    const VoxelChunkDocument chunk = ChunkWith({{16, 32, 48, VoxelFace::PositiveZ, 32, 48, 1}}, {{{0, 0, 0}, {16, 16, 16}}});
    const VoxelGeometry geometry = ExpandVoxelChunk(chunk, map);
    REQUIRE(geometry.positions.size() == 4);
    // float(local + 256 key + 16 origin) x 0.5 / 16 per axis.
    const auto world = [](const std::int64_t units) { return static_cast<float>(units) * 0.5F / 16.0F; };
    const float x0 = world(16 - 256 - 32), x1 = world(16 + 32 - 256 - 32);
    const float y0 = world(32 + 16), y1 = world(32 + 48 + 16);
    const float z = world(48 - 256 + 48);
    CHECK(SameBits(geometry.positions[0], {x0, y0, z}));
    CHECK(SameBits(geometry.positions[1], {x0, y1, z}));
    CHECK(SameBits(geometry.positions[2], {x1, y1, z}));
    CHECK(SameBits(geometry.positions[3], {x1, y0, z}));
    for (const Math::Vec3& normal : geometry.normals) CHECK(SameBits(normal, {0.0F, 0.0F, 1.0F}));
    // Palette entry 1 of 2 columns, row 4 (+Z) of 6.
    for (const Math::Vec2& uv : geometry.uvs) {
        CHECK(uv.x == 0.75F);
        CHECK(uv.y == 4.5F / 6.0F);
    }
    CHECK(geometry.indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3});
    REQUIRE(geometry.colliders.size() == 1);
    CHECK(SameBits(geometry.colliders[0].minimum, {world(-256 - 32), world(16), world(-256 + 48)}));
    CHECK(SameBits(geometry.colliders[0].maximum, {world(16 - 256 - 32), world(16 + 16), world(16 - 256 + 48)}));
}

TEST_CASE("Expansion: every face's triangles go counter-clockwise on the screen seen from the side it looks at") {
    const VoxelMapDocument map = Map();
    std::vector<VoxelQuad> quads;
    for (std::uint8_t face = 0; face < kFaceCount; ++face) quads.push_back({8, 8, 8, static_cast<VoxelFace>(face), 4, 2, 0});
    const VoxelGeometry geometry = ExpandVoxelChunk(ChunkWith(quads, {}), map);
    REQUIRE(geometry.indices.size() == 6 * kFaceCount);
    const std::array<Math::Vec3, kFaceCount> normals{{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
    for (std::size_t face = 0; face < kFaceCount; ++face) {
        CAPTURE(face);
        CHECK(SameBits(geometry.normals[face * 4], normals[face]));
        for (std::size_t triangle = 0; triangle < 2; ++triangle) {
            const std::uint32_t* corner = &geometry.indices[face * 6 + triangle * 3];
            const Math::Vec3& a = geometry.positions[corner[0]];
            const Math::Vec3 turn = Math::Cross(Minus(geometry.positions[corner[1]], a), Minus(geometry.positions[corner[2]], a));
            // Left-handed space (math.md): seen from the side the normal points to, a triangle counter-clockwise on
            // the screen (render's front face) has its cross product pointing away from the viewer.
            CHECK(Math::Dot(turn, normals[face]) < 0.0F);
        }
        // The face's sizes: X faces along Z and Y, Y faces along X and Z, Z faces along X and Y (4 and 2 units).
        Math::Vec3 low = geometry.positions[face * 4], high = low;
        for (std::size_t v = 0; v < 4; ++v) {
            const Math::Vec3& p = geometry.positions[face * 4 + v];
            low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
            high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
        }
        const Math::Vec3 extent = Minus(high, low);
        const float w = 4.0F * map.cellSize / 16.0F, h = 2.0F * map.cellSize / 16.0F;
        const Math::Vec3 expected = face < 2 ? Math::Vec3{0, h, w} : face < 4 ? Math::Vec3{w, 0, h} : Math::Vec3{w, h, 0};
        CHECK(SameBits(extent, expected));
    }
}

TEST_CASE("Expansion: world coordinates at both ends of +-2^20 units are exact") {
    MapText text = ValidMap();
    text.bounds = R"({"min": [-4096, 0, -1], "max": [4095, 1, 8]})";
    text.cellSize = "0.75";
    auto parsed = ParseVoxelMap(text.Text());
    REQUIRE(parsed.has_value());
    const VoxelMapDocument& map = *parsed;
    const VoxelGeometry low = ExpandVoxelChunk(ChunkWith({}, {{{0, 0, 0}, {1, 1, 1}}}, {-4096, 0, -1}), map);
    CHECK(SameBits(low.colliders[0].minimum, {-1048576.0F * 0.75F / 16.0F, 0.0F, -256.0F * 0.75F / 16.0F}));
    const VoxelGeometry high = ExpandVoxelChunk(ChunkWith({}, {{{255, 0, 0}, {256, 1, 1}}}, {4095, 0, -1}), map);
    CHECK(SameBits(high.colliders[0].maximum, {1048576.0F * 0.75F / 16.0F, 1.0F * 0.75F / 16.0F, -255.0F * 0.75F / 16.0F}));
    CHECK(high.colliders[0].maximum.x == 49152.0F);
    CHECK(SameBits(MapUnitsToWorld({1048576, -1048576, 3}, map), {49152.0F, -49152.0F, 3.0F * 0.75F / 16.0F}));
    CHECK(SameBits(ChunkWorldOrigin({4095, 1, -1}, map), {(1048576.0F - 256.0F) * 0.75F / 16.0F, 12.0F, -12.0F}));
    const Math::Aabb box = ChunkWorldBounds({4095, 1, -1}, map);
    CHECK(SameBits(box.maximum, {49152.0F, 24.0F, 0.0F}));
}

TEST_CASE("Expansion: AppendVoxelChunk continues the indices; a quad it cannot expand is a programmer error") {
    const VoxelMapDocument map = Map();
    VoxelGeometry geometry;
    AppendVoxelChunk(geometry, ChunkWith({{0, 0, 0, VoxelFace::PositiveY, 1, 1, 0}}, {}), map);
    AppendVoxelChunk(geometry, ChunkWith({{0, 0, 0, VoxelFace::NegativeY, 1, 1, 1}}, {{{0, 0, 0}, {1, 1, 1}}}, {-1, 1, -1}), map);
    CHECK(geometry.positions.size() == 8);
    CHECK(geometry.normals.size() == 8);
    CHECK(geometry.uvs.size() == 8);
    CHECK(geometry.indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7});
    CHECK(geometry.colliders.size() == 1);
    GYO::Test::ScopedAssertionHandler handler;
    CHECK_THROWS_AS(AppendVoxelChunk(geometry, ChunkWith({{0, 0, 0, VoxelFace::PositiveY, 1, 1, 9}}, {}), map),
                    Base::AssertionFailure);
    CHECK_THROWS_AS((void)ChunkWorldOrigin({9, 0, 0}, map), Base::AssertionFailure);
}

TEST_CASE("Expansion: the palette image, one column per entry and one row per direction") {
    const VoxelPaletteImage image = ExpandVoxelPalette(Map());
    CHECK(image.width == 2);
    CHECK(image.height == 6);
    REQUIRE(image.pixels.size() == 2 * 6 * 4);
    CHECK(std::vector<std::uint8_t>(image.pixels.begin(), image.pixels.begin() + 8) ==
          std::vector<std::uint8_t>{1, 2, 3, 255, 0, 0, 0, 0});
    CHECK(std::vector<std::uint8_t>(image.pixels.end() - 8, image.pixels.end()) ==
          std::vector<std::uint8_t>{16, 17, 18, 255, 255, 255, 255, 255});
}

TEST_CASE("RegionOf: floor(x / 8), floor(z / 8)") {
    CHECK(RegionOf({0, 5, 0}) == VoxelRegionKey{0, 0});
    CHECK(RegionOf({7, 0, 8}) == VoxelRegionKey{0, 1});
    CHECK(RegionOf({-1, 0, -8}) == VoxelRegionKey{-1, -1});
    CHECK(RegionOf({-9, 0, -16}) == VoxelRegionKey{-2, -2});
    CHECK(RegionOf({-2147483647 - 1, 0, 2147483647}) == VoxelRegionKey{-268435456, 268435455});
}

// --- The loaders. --------------------------------------------------------------------------------------------------

namespace {

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

struct Content final {
    std::filesystem::path root;
    Content() {
        root = std::filesystem::temp_directory_path() / ("gyo_voxel_map_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(root / "maps" / "m" / "regions");
    }
    void Write(const std::string& path, const std::string& text) const {
        std::ofstream out(root / path, std::ios::binary);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
    Content(const Content&) = delete;
    Content& operator=(const Content&) = delete;
    ~Content() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

Asset::CatalogEntry Entry(const std::string& id, const std::string& type, const std::string& path,
                          std::optional<std::string> settings = std::nullopt) {
    Asset::CatalogEntry entry;
    entry.id = Asset::AssetId(id);
    entry.type = type;
    entry.path = path;
    entry.settings = std::move(settings);
    return entry;
}

} // namespace

TEST_CASE("Loaders: through the asset store the index references its regions in order and both read as Parse does") {
    const Content content;
    content.Write("maps/m/map.json", ValidMap().Text());
    content.Write("maps/m/regions/r_-1_-1.json", kRegionText);
    content.Write("maps/m/regions/r_1_1.json", kRegionText2);
    // A region's text where an index is expected (a file of its own: two entries on one file make a reference to it
    // ambiguous).
    content.Write("maps/m/not_an_index.json", kRegionText2);
    InlineExecutor executor;
    Asset::AssetStoreOptions options;
    options.contentRoot = content.root.string();
    const std::vector<Asset::CatalogEntry> entries{
        Entry("game.map", "voxel_map", "maps/m/map.json"),
        Entry("game.map.r1", "voxel_region", "maps/m/regions/r_1_1.json"),
        Entry("game.map.r0", "voxel_region", "maps/m/regions/r_-1_-1.json"),
        Entry("game.map_bad", "voxel_map", "maps/m/not_an_index.json"),
        Entry("game.map_settings", "voxel_map", "maps/m/map.json", R"({"x":1})"),
        Entry("game.region_bad", "voxel_region", "maps/m/map.json"),
    };
    Asset::AssetStore store(entries, options, &executor);
    REQUIRE(store.Loaders().Register<VoxelMapLoader>().has_value());
    REQUIRE(store.Loaders().Register<VoxelRegionLoader>().has_value());

    const auto index = store.Read(store.Request<VoxelMapAsset>(Asset::AssetId("game.map")));
    REQUIRE(index != nullptr);
    CHECK(index->document == Map());
    REQUIRE(index->regions.size() == 2);
    // In index order, by file relative to the index: (-1, -1) first.
    CHECK(index->regions[0].Id() == Asset::AssetId("game.map.r0"));
    CHECK(index->regions[1].Id() == Asset::AssetId("game.map.r1"));
    for (std::size_t i = 0; i < 2; ++i) {
        const auto region = store.Read(store.Request(index->regions[i]));
        REQUIRE(region != nullptr);
        const auto direct = ParseVoxelRegion(i == 0 ? kRegionText : kRegionText2, index->document, index->document.regions[i].at);
        REQUIRE(direct.has_value());
        CHECK(*region == *direct);
        CHECK(CheckVoxelRegion(*region, index->document, index->document.regions[i].at).has_value());
    }

    CHECK(store.Read(store.Request<VoxelMapAsset>(Asset::AssetId("game.map_bad"))) == nullptr);
    CHECK(store.Read(store.Request<VoxelMapAsset>(Asset::AssetId("game.map_settings"))) == nullptr);
    CHECK(store.Read(store.Request<VoxelRegionDocument>(Asset::AssetId("game.region_bad"))) == nullptr);
    const Asset::AssetSnapshot snapshot = store.Snapshot();
    const Asset::AssetRecord* const bad = snapshot.Find(Asset::AssetId("game.map_bad"));
    REQUIRE(bad != nullptr);
    REQUIRE(bad->failure.has_value());
    CHECK(bad->failure->code == Asset::AssetLoadFailureCode::LoaderFailed);
    const Asset::AssetRecord* const settings = snapshot.Find(Asset::AssetId("game.map_settings"));
    REQUIRE(settings != nullptr);
    REQUIRE(settings->failure.has_value());
    CHECK(settings->failure->code == Asset::AssetLoadFailureCode::BadSettings);
}

TEST_CASE("Loaders: the codes and an empty settings object") {
    const VoxelMapLoader maps;
    const VoxelRegionLoader regions;
    CHECK(maps.ParseSettings(std::nullopt).has_value());
    CHECK(maps.ParseSettings(std::string_view("{}")).has_value());
    REQUIRE_FALSE(maps.ParseSettings(std::string_view(R"({"a":1})")).has_value());
    CHECK(maps.ParseSettings(std::string_view(R"({"a":1})")).error().code == VoxelMapLoaderCode::BadSettings);
    REQUIRE_FALSE(regions.ParseSettings(std::string_view(R"({"a":1})")).has_value());
    CHECK(regions.ParseSettings(std::string_view(R"({"a":1})")).error().code == VoxelRegionLoaderCode::BadSettings);
    Asset::LoadContext context(Asset::AssetId("game.map"), "maps/m/map.json", kMaxMapTextBytes);
    const std::string bad = "{}";
    const auto failed = maps.Load(std::as_bytes(std::span<const char>(bad.data(), bad.size())), {}, context);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code == VoxelMapLoaderCode::NotVoxelMap);
    CHECK(std::string_view(failed.error().message).substr(0, 2) == "M1");
    const std::string text = ValidMap().Text();
    const auto loaded = maps.Load(std::as_bytes(std::span<const char>(text.data(), text.size())), {}, context);
    REQUIRE(loaded.has_value());
    // Without a ContentPathIndex the references carry their paths only.
    REQUIRE(context.References().size() == 2);
    CHECK(context.References()[0].path == "maps/m/regions/r_-1_-1.json");
    CHECK(context.References()[1].typeName == "voxel_region");
    Asset::LoadContext regionContext(Asset::AssetId("game.r"), "maps/m/regions/r.json", kMaxRegionTextBytes);
    const auto region = regions.Load(std::as_bytes(std::span<const char>(bad.data(), bad.size())), {}, regionContext);
    REQUIRE_FALSE(region.has_value());
    CHECK(region.error().code == VoxelRegionLoaderCode::NotVoxelRegion);
}
