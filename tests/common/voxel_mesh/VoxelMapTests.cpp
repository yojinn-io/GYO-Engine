#include <doctest/doctest.h>

#include "engine/base/Sha256.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

using namespace Engine::VoxelMesh;

// Rules and expansion of gyo.voxel_map version 1 and gyo.voxel_region version 1
// (docs/architecture/voxel-mesh.md). Judgments J1 (one test case per rule, named
// by its rule code) and J3 (goldens) of the v8-arch-big-map-engine-format
// declaration (docs/architecture/plans/big-map/engine-format.zh-Hant.md).

namespace {
const std::string Faces = "[[1,2,3,255],[4,5,6,255],[7,8,9,255],[10,11,12,255],[13,14,15,255],[16,17,18,255]]";

std::string ShaOf(const std::string& text) {
    return Engine::Base::Sha256(std::as_bytes(std::span(text.data(), text.size())));
}

struct RegionDoc final {
    std::string schema = R"("gyo.voxel_region")";
    std::string version = "1";
    std::string at = "[0, 0]";
    std::string chunks =
        R"([{"at": [0, 0, 0], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": [{"min": [0, 0, 0], "max": [16, 16, 16]}]}])";
    std::string extra;

    [[nodiscard]] std::string Text() const {
        return "{\"schema\": " + schema + ", \"version\": " + version + ", \"at\": " + at + ", \"chunks\": " + chunks +
               extra + "}";
    }
};

struct MapDoc final {
    std::string schema = R"("gyo.voxel_map")";
    std::string version = "1";
    std::string sha = "\"" + std::string(64, 'a') + "\"";
    std::string build = R"("voxel_map_editor 2")";
    std::string origin = "[0, 0, 0]";
    std::string cellSize = "1.0";
    std::string chunkSize = "16";
    std::string regionSize = "8";
    std::string bounds = R"({"min": [-8, 0, -8], "max": [7, 3, 7]})";
    std::string palette = R"([{"name": "a", "faces": )" + Faces + "}]";
    // "@" is replaced by the SHA-256 of the region text the map is paired with.
    std::string regions = R"([{"at": [0, 0], "file": "regions/r_0_0.json", "sha256": "@"}])";
    std::string extra;

    [[nodiscard]] std::string Text(const std::string& regionSha = std::string(64, 'b')) const {
        auto listed = regions;
        for (auto at = listed.find('@'); at != std::string::npos; at = listed.find('@'))
            listed.replace(at, 1, regionSha);
        return "{\"schema\": " + schema + ", \"version\": " + version + ", \"source_sha256\": " + sha +
               ", \"build_version\": " + build + ", \"origin\": " + origin + ", \"cell_size\": " + cellSize +
               ", \"chunk_size\": " + chunkSize + ", \"region_size\": " + regionSize + ", \"bounds\": " + bounds +
               ", \"palette\": " + palette + ", \"regions\": " + listed + extra + "}";
    }
};

VoxelMapDocument AcceptedMap(const MapDoc& doc, const std::string& regionSha = std::string(64, 'b')) {
    auto parsed = ParseVoxelMap(doc.Text(regionSha));
    REQUIRE_MESSAGE(parsed, (parsed ? std::string() : parsed.error().message + " at " + parsed.error().detail));
    return std::move(*parsed);
}

// The map's error: its detail (the path), after checking the code.
std::string RejectedMap(const std::string& text) {
    const auto parsed = ParseVoxelMap(text);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == VoxelMeshErrorCode::InvalidVoxelMap);
    return parsed.error().detail;
}

std::string RejectedMap(const MapDoc& doc) { return RejectedMap(doc.Text()); }

std::string MapMessage(const MapDoc& doc) {
    const auto parsed = ParseVoxelMap(doc.Text());
    REQUIRE_FALSE(parsed);
    return parsed.error().message;
}

// A region read with a map that lists it (with the region text's own hash).
VoxelMeshResult<VoxelRegionDocument> ReadPair(const MapDoc& map, const RegionDoc& region) {
    const auto text = region.Text();
    const auto document = AcceptedMap(map, ShaOf(text));
    return ParseVoxelRegion(text, document);
}

VoxelRegionDocument AcceptedRegion(const RegionDoc& region, const MapDoc& map = MapDoc{}) {
    auto parsed = ReadPair(map, region);
    REQUIRE_MESSAGE(parsed, (parsed ? std::string() : parsed.error().message + " at " + parsed.error().detail));
    return std::move(*parsed);
}

std::string RejectedRegion(const RegionDoc& region, const MapDoc& map = MapDoc{}) {
    const auto parsed = ReadPair(map, region);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == VoxelMeshErrorCode::InvalidVoxelRegion);
    return parsed.error().detail;
}

std::string RegionMessage(const RegionDoc& region, const MapDoc& map = MapDoc{}) {
    const auto parsed = ReadPair(map, region);
    REQUIRE_FALSE(parsed);
    return parsed.error().message;
}

std::string ChunkText(const std::string& at, const std::string& quads, const std::string& colliders) {
    return R"([{"at": )" + at + R"(, "quads": )" + quads + R"(, "colliders": )" + colliders + "}]";
}

// A chunk that ValidateVoxelChunk rejects: its detail (relative to the chunk).
std::string RejectedChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map) {
    const auto checked = ValidateVoxelChunk(chunk, map);
    REQUIRE_FALSE(checked);
    CHECK(checked.error().code == VoxelMeshErrorCode::InvalidVoxelChunk);
    return checked.error().detail;
}

// The default map with any region hash (for ValidateVoxelChunk, which does not
// read text).
VoxelMapDocument DefaultMap() { return AcceptedMap(MapDoc{}); }

VoxelChunkDocument OneChunk() {
    VoxelChunkDocument chunk;
    chunk.at = {0, 0, 0};
    chunk.quads = {{0, 0, 0, VoxelFace::PositiveY, 16, 16, 0}};
    chunk.colliders = {{{0, 0, 0}, {16, 16, 16}}};
    return chunk;
}

std::uint32_t Bits(const float value) { return std::bit_cast<std::uint32_t>(value); }
} // namespace

TEST_CASE("voxel map: the minimal map and region are accepted") {
    const auto region = AcceptedRegion(RegionDoc{});
    CHECK(region.at == VoxelRegionKey{0, 0});
    REQUIRE(region.chunks.size() == 1);
    CHECK(region.chunks[0].at == VoxelChunkKey{0, 0, 0});
    CHECK(region.chunks[0].quads.size() == 1);
    CHECK(region.chunks[0].colliders.size() == 1);
    const auto map = DefaultMap();
    CHECK(map.boundsMinimum == VoxelChunkKey{-8, 0, -8});
    CHECK(map.boundsMaximum == VoxelChunkKey{7, 3, 7});
    REQUIRE(map.regions.size() == 1);
    CHECK(map.regions[0].file == "regions/r_0_0.json");
    CHECK(ValidateVoxelChunk(region.chunks[0], map));
}

TEST_CASE("M1: map fields and repeated keys") {
    MapDoc unknown;
    unknown.extra = R"(, "comment": "x")";
    CHECK(RejectedMap(unknown).empty());
    CHECK(MapMessage(unknown).find("unknown field 'comment'") != std::string::npos);
    MapDoc repeated;
    repeated.extra = R"(, "version": 1)";
    CHECK(RejectedMap(repeated).empty());
    CHECK(MapMessage(repeated).find("repeated key 'version'") != std::string::npos);
    MapDoc nested;
    nested.bounds = R"({"min": [0, 0, 0], "min": [0, 0, 0], "max": [1, 1, 1]})";
    CHECK(MapMessage(nested).find("repeated key 'min'") != std::string::npos);
    CHECK(RejectedMap("[]").empty());
    CHECK(RejectedMap("{").empty());
    const auto text = MapDoc{}.Text();
    const auto truncated = text.substr(0, text.find(", \"regions\"")) + "}";
    CHECK(RejectedMap(truncated).empty());
}

TEST_CASE("M2: map schema and version") {
    MapDoc schema;
    schema.schema = R"("gyo.voxel_mesh")";
    CHECK(RejectedMap(schema) == "schema");
    MapDoc version;
    version.version = "2";
    CHECK(RejectedMap(version) == "version");
    MapDoc fraction;
    fraction.version = "1.0";
    CHECK(RejectedMap(fraction) == "version");
}

TEST_CASE("M3: map source hash and build version") {
    MapDoc shortSha;
    shortSha.sha = "\"" + std::string(63, 'a') + "\"";
    CHECK(RejectedMap(shortSha) == "source_sha256");
    MapDoc upper;
    upper.sha = "\"" + std::string(64, 'A') + "\"";
    CHECK(RejectedMap(upper) == "source_sha256");
    MapDoc empty;
    empty.build = R"("")";
    CHECK(RejectedMap(empty) == "build_version");
}

TEST_CASE("M4: map origin and cell size") {
    MapDoc origin;
    origin.origin = "[0, 0]";
    CHECK(RejectedMap(origin) == "origin");
    MapDoc fraction;
    fraction.origin = "[0, 0.5, 0]";
    CHECK(RejectedMap(fraction) == "origin[1]");
    MapDoc small;
    small.cellSize = "0.00001";
    CHECK(RejectedMap(small) == "cell_size");
    MapDoc large;
    large.cellSize = "70000";
    CHECK(RejectedMap(large) == "cell_size");
    MapDoc text;
    text.cellSize = R"("1")";
    CHECK(RejectedMap(text) == "cell_size");
}

TEST_CASE("M5: chunk and region sizes") {
    MapDoc chunk;
    chunk.chunkSize = "32";
    CHECK(RejectedMap(chunk) == "chunk_size");
    MapDoc chunkFloat;
    chunkFloat.chunkSize = "16.0";
    CHECK(RejectedMap(chunkFloat) == "chunk_size");
    MapDoc region;
    region.regionSize = "16";
    CHECK(RejectedMap(region) == "region_size");
}

TEST_CASE("M6: bounds") {
    MapDoc reversed;
    reversed.bounds = R"({"min": [0, 1, 0], "max": [0, 0, 0]})";
    CHECK(RejectedMap(reversed) == "bounds");
    MapDoc single;
    single.bounds = R"({"min": [0, 0, 0], "max": [0, 0, 0]})";
    CHECK(AcceptedMap(single).boundsMaximum == VoxelChunkKey{0, 0, 0});
    MapDoc extra;
    extra.bounds = R"({"min": [0, 0, 0], "max": [0, 0, 0], "size": 1})";
    CHECK(RejectedMap(extra) == "bounds");
    // The far corner of the last chunk: 4096 * 256 units = 2^20.
    MapDoc edge;
    edge.bounds = R"({"min": [-4096, 0, 0], "max": [4095, 0, 0]})";
    CHECK(AcceptedMap(edge).boundsMinimum.x == -4096);
    MapDoc beyondMax;
    beyondMax.bounds = R"({"min": [0, 0, 0], "max": [4096, 0, 0]})";
    CHECK(RejectedMap(beyondMax) == "bounds.max[0]");
    MapDoc beyondMin;
    beyondMin.bounds = R"({"min": [0, -4097, 0], "max": [0, 0, 0]})";
    beyondMin.regions = R"([{"at": [0, 0], "file": "r", "sha256": "@"}])";
    CHECK(RejectedMap(beyondMin) == "bounds.min[1]");
    // The origin counts: one cell more moves the far corner past 2^20.
    MapDoc shifted;
    shifted.origin = "[1, 0, 0]";
    shifted.bounds = R"({"min": [0, 0, 0], "max": [4095, 0, 0]})";
    CHECK(RejectedMap(shifted) == "bounds.max[0]");
}

TEST_CASE("M7: map palette") {
    MapDoc empty;
    empty.palette = "[]";
    CHECK(RejectedMap(empty) == "palette");
    MapDoc repeated;
    repeated.palette =
        R"([{"name": "a", "faces": )" + Faces + R"(}, {"name": "a", "faces": )" + Faces + "}]";
    CHECK(RejectedMap(repeated) == "palette[1].name");
    MapDoc badName;
    badName.palette = R"([{"name": "A", "faces": )" + Faces + "}]";
    CHECK(RejectedMap(badName) == "palette[0].name");
    MapDoc channel;
    channel.palette =
        R"([{"name": "a", "faces": [[1,2,3,256],[4,5,6,255],[7,8,9,255],[10,11,12,255],[13,14,15,255],[16,17,18,255]]}])";
    CHECK(RejectedMap(channel) == "palette[0].faces[0][3]");
}

TEST_CASE("M8: regions") {
    MapDoc empty;
    empty.regions = "[]";
    CHECK(RejectedMap(empty) == "regions");
    MapDoc outside;
    outside.regions = R"([{"at": [1, 0], "file": "r", "sha256": "@"}])";
    CHECK(RejectedMap(outside) == "regions[0].at");
    MapDoc outsideLow;
    outsideLow.regions = R"([{"at": [0, -2], "file": "r", "sha256": "@"}])";
    CHECK(RejectedMap(outsideLow) == "regions[0].at");
    MapDoc negative;
    negative.regions = R"([{"at": [-1, -1], "file": "r", "sha256": "@"}])";
    CHECK(AcceptedMap(negative).regions[0].at == VoxelRegionKey{-1, -1});
    MapDoc three;
    three.regions = R"([{"at": [0, 0, 0], "file": "r", "sha256": "@"}])";
    CHECK(RejectedMap(three) == "regions[0].at");
    MapDoc unordered;
    unordered.regions =
        R"([{"at": [0, 0], "file": "a", "sha256": "@"}, {"at": [-1, 0], "file": "b", "sha256": "@"}])";
    CHECK(RejectedMap(unordered) == "regions[1].at");
    MapDoc same;
    same.regions = R"([{"at": [0, 0], "file": "a", "sha256": "@"}, {"at": [0, 0], "file": "b", "sha256": "@"}])";
    CHECK(RejectedMap(same) == "regions[1].at");
    MapDoc ordered;
    ordered.regions =
        R"([{"at": [-1, 0], "file": "a", "sha256": "@"}, {"at": [0, -1], "file": "b", "sha256": "@"}, {"at": [0, 0], "file": "c", "sha256": "@"}])";
    CHECK(AcceptedMap(ordered).regions.size() == 3);
    MapDoc repeatedFile;
    repeatedFile.regions =
        R"([{"at": [-1, 0], "file": "a", "sha256": "@"}, {"at": [0, 0], "file": "a", "sha256": "@"}])";
    CHECK(RejectedMap(repeatedFile) == "regions[1].file");
    for (const auto* file : {"", "/r", "r/", "a//b", "./r", "a/../b", "..", "R", "a b", "r\\\\x"}) {
        MapDoc bad;
        bad.regions = std::string(R"([{"at": [0, 0], "file": ")") + file + R"(", "sha256": "@"}])";
        CHECK_MESSAGE(RejectedMap(bad) == "regions[0].file", file);
    }
    MapDoc longFile;
    longFile.regions = R"([{"at": [0, 0], "file": ")" + std::string(129, 'a') + R"(", "sha256": "@"}])";
    CHECK(RejectedMap(longFile) == "regions[0].file");
    MapDoc longestFile;
    longestFile.regions = R"([{"at": [0, 0], "file": ")" + std::string(128, 'a') + R"(", "sha256": "@"}])";
    CHECK(AcceptedMap(longestFile).regions[0].file.size() == 128);
    MapDoc dots;
    dots.regions = R"([{"at": [0, 0], "file": "r/.hidden/x-1_2.json", "sha256": "@"}])";
    CHECK(AcceptedMap(dots).regions[0].file == "r/.hidden/x-1_2.json");
    MapDoc badSha;
    badSha.regions = R"([{"at": [0, 0], "file": "r", "sha256": "xyz"}])";
    CHECK(RejectedMap(badSha) == "regions[0].sha256");
    MapDoc extra;
    extra.regions = R"([{"at": [0, 0], "file": "r", "sha256": "@", "size": 1}])";
    CHECK(RejectedMap(extra) == "regions[0]");
}

TEST_CASE("M9: map integers") {
    MapDoc large;
    large.origin = "[1073741825, 0, 0]";
    CHECK(RejectedMap(large) == "origin[0]");
    MapDoc largeRegion;
    largeRegion.regions = R"([{"at": [0, -1073741825], "file": "r", "sha256": "@"}])";
    CHECK(RejectedMap(largeRegion) == "regions[0].at[1]");
    MapDoc largeBounds;
    largeBounds.bounds = R"({"min": [0, 0, -1073741825], "max": [0, 0, 0]})";
    CHECK(RejectedMap(largeBounds) == "bounds.min[2]");
}

TEST_CASE("M10: map text size") {
    MapDoc padded;
    padded.extra = std::string(MaxMapTextBytes, ' ');
    CHECK(RejectedMap(padded).empty());
    CHECK(MapMessage(padded).find("1 MiB") != std::string::npos);
    MapDoc exact;
    exact.extra = std::string(MaxMapTextBytes - MapDoc{}.Text().size(), ' ');
    REQUIRE(exact.Text().size() == MaxMapTextBytes);
    CHECK(AcceptedMap(exact).regions.size() == 1);
}

TEST_CASE("R1: region fields and repeated keys") {
    RegionDoc unknown;
    unknown.extra = R"(, "comment": "x")";
    CHECK(RejectedRegion(unknown).empty());
    CHECK(RegionMessage(unknown).find("unknown field 'comment'") != std::string::npos);
    RegionDoc repeated;
    repeated.extra = R"(, "at": [0, 0])";
    CHECK(RegionMessage(repeated).find("repeated key 'at'") != std::string::npos);
    RegionDoc nested;
    nested.chunks = R"([{"at": [0, 0, 0], "at": [0, 0, 0], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []}])";
    CHECK(RegionMessage(nested).find("repeated key 'at'") != std::string::npos);
    const auto map = DefaultMap();
    const auto notJson = ParseVoxelRegion("not json", map);
    REQUIRE_FALSE(notJson);
    CHECK(notJson.error().code == VoxelMeshErrorCode::InvalidVoxelRegion);
    CHECK(notJson.error().message.find("not JSON") != std::string::npos);
}

TEST_CASE("R2: region schema and version") {
    RegionDoc schema;
    schema.schema = R"("gyo.voxel_map")";
    CHECK(RejectedRegion(schema) == "schema");
    RegionDoc version;
    version.version = "2";
    CHECK(RejectedRegion(version) == "version");
}

TEST_CASE("R3: the region belongs to the map") {
    RegionDoc unlisted;
    unlisted.at = "[-1, 0]";
    CHECK(RejectedRegion(unlisted) == "at");
    RegionDoc three;
    three.at = "[0, 0, 0]";
    CHECK(RejectedRegion(three) == "at");
    // A text other than the one the map lists.
    const auto text = RegionDoc{}.Text();
    const auto map = AcceptedMap(MapDoc{}, ShaOf(text));
    const auto stale = ParseVoxelRegion(text + " ", map);
    REQUIRE_FALSE(stale);
    CHECK(stale.error().code == VoxelMeshErrorCode::InvalidVoxelRegion);
    CHECK(stale.error().message.find("SHA-256") != std::string::npos);
    CHECK(ParseVoxelRegion(text, map));
    // The listed entry among several.
    MapDoc several;
    several.regions =
        R"([{"at": [-1, -1], "file": "a", "sha256": "@"}, {"at": [0, -1], "file": "b", "sha256": "@"}, {"at": [0, 0], "file": "c", "sha256": "@"}])";
    RegionDoc second;
    second.at = "[0, -1]";
    second.chunks = ChunkText("[0, 0, -1]", "[[0, 0, 0, 2, 16, 16, 0]]", "[]");
    CHECK(AcceptedRegion(second, several).at == VoxelRegionKey{0, -1});
}

TEST_CASE("R4: chunks of the region") {
    RegionDoc empty;
    empty.chunks = "[]";
    CHECK(RejectedRegion(empty) == "chunks");
    RegionDoc extra;
    extra.chunks = R"([{"at": [0, 0, 0], "quads": [], "colliders": [], "size": 1}])";
    CHECK(RejectedRegion(extra) == "chunks[0]");
    RegionDoc foreign;
    foreign.chunks = ChunkText("[8, 0, 0]", "[[0, 0, 0, 2, 16, 16, 0]]", "[]");
    MapDoc wide;
    wide.bounds = R"({"min": [0, 0, 0], "max": [15, 3, 7]})";
    CHECK(RejectedRegion(foreign, wide) == "chunks[0].at");
    RegionDoc foreignZ;
    foreignZ.chunks = ChunkText("[0, 0, -1]", "[[0, 0, 0, 2, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(foreignZ) == "chunks[0].at");
    // Every height belongs to the region.
    RegionDoc tall;
    tall.chunks = ChunkText("[7, 3, 7]", "[[0, 0, 0, 2, 16, 16, 0]]", "[]");
    CHECK(AcceptedRegion(tall).chunks[0].at == VoxelChunkKey{7, 3, 7});
    RegionDoc outOfBounds;
    outOfBounds.chunks = ChunkText("[0, 4, 0]", "[[0, 0, 0, 2, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(outOfBounds) == "chunks[0].at");
    RegionDoc unordered;
    unordered.chunks = R"([{"at": [0, 1, 0], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []},)"
                       R"( {"at": [0, 0, 7], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []}])";
    CHECK(RejectedRegion(unordered) == "chunks[1].at");
    RegionDoc same;
    same.chunks = R"([{"at": [0, 0, 0], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []},)"
                  R"( {"at": [0, 0, 0], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []}])";
    CHECK(RejectedRegion(same) == "chunks[1].at");
    RegionDoc ordered;
    ordered.chunks = R"([{"at": [0, 0, 7], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []},)"
                     R"( {"at": [0, 1, 0], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []},)"
                     R"( {"at": [1, 0, 0], "quads": [], "colliders": [{"min": [0, 0, 0], "max": [1, 1, 1]}]}])";
    CHECK(AcceptedRegion(ordered).chunks.size() == 3);
    // A region with negative keys: floor division.
    MapDoc negativeMap;
    negativeMap.regions = R"([{"at": [-1, -1], "file": "a", "sha256": "@"}])";
    RegionDoc negative;
    negative.at = "[-1, -1]";
    negative.chunks = R"([{"at": [-8, 0, -1], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []}])";
    CHECK(AcceptedRegion(negative, negativeMap).chunks[0].at == VoxelChunkKey{-8, 0, -1});
    RegionDoc negativeForeign;
    negativeForeign.at = "[-1, -1]";
    negativeForeign.chunks = R"([{"at": [-9, 0, -1], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": []}])";
    MapDoc wideNegative = negativeMap;
    wideNegative.bounds = R"({"min": [-16, 0, -8], "max": [7, 3, 7]})";
    CHECK(RejectedRegion(negativeForeign, wideNegative) == "chunks[0].at");
}

TEST_CASE("R5: region integers and text size") {
    RegionDoc large;
    large.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 2, 16, 1073741825, 0]]", "[]");
    CHECK(RejectedRegion(large) == "chunks[0].quads[0][5]");
    RegionDoc largeAt;
    largeAt.at = "[0, 1073741825]";
    CHECK(RejectedRegion(largeAt) == "at[1]");
    RegionDoc padded;
    padded.extra = std::string(MaxRegionTextBytes, ' ');
    CHECK(RejectedRegion(padded).empty());
    CHECK(RegionMessage(padded).find("16 MiB") != std::string::npos);
    RegionDoc exact;
    exact.extra = std::string(MaxRegionTextBytes - RegionDoc{}.Text().size(), ' ');
    REQUIRE(exact.Text().size() == MaxRegionTextBytes);
    CHECK(AcceptedRegion(exact).chunks.size() == 1);
}

TEST_CASE("C1: a chunk has a quad or a collider") {
    RegionDoc empty;
    empty.chunks = ChunkText("[0, 0, 0]", "[]", "[]");
    CHECK(RejectedRegion(empty) == "chunks[0]");
    RegionDoc boxOnly;
    boxOnly.chunks = ChunkText("[0, 0, 0]", "[]", R"([{"min": [0, 0, 0], "max": [1, 1, 1]}])");
    CHECK(AcceptedRegion(boxOnly).chunks[0].quads.empty());
    RegionDoc quadOnly;
    quadOnly.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 2, 16, 16, 0]]", "[]");
    CHECK(AcceptedRegion(quadOnly).chunks[0].colliders.empty());

    const auto map = DefaultMap();
    VoxelChunkDocument nothing;
    CHECK(RejectedChunk(nothing, map).empty());
    auto outside = OneChunk();
    outside.at = {0, 4, 0};
    CHECK(RejectedChunk(outside, map) == "at");
    outside.at = {-9, 0, 0};
    CHECK(RejectedChunk(outside, map) == "at");
}

TEST_CASE("C2: chunk quads") {
    RegionDoc dir;
    dir.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 6, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(dir) == "chunks[0].quads[0][3]");
    RegionDoc width;
    width.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 2, 0, 16, 0]]", "[]");
    CHECK(RejectedRegion(width) == "chunks[0].quads[0][4]");
    RegionDoc height;
    height.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 2, 16, 0, 0]]", "[]");
    CHECK(RejectedRegion(height) == "chunks[0].quads[0][5]");
    RegionDoc palette;
    palette.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 2, 16, 16, 1]]", "[]");
    CHECK(RejectedRegion(palette) == "chunks[0].quads[0][6]");
    RegionDoc six;
    six.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 2, 16, 16]]", "[]");
    CHECK(RejectedRegion(six) == "chunks[0].quads[0]");
    RegionDoc fraction;
    fraction.chunks = ChunkText("[0, 0, 0]", "[[0, 0.5, 0, 2, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(fraction) == "chunks[0].quads[0][1]");

    const auto map = DefaultMap();
    auto chunk = OneChunk();
    chunk.quads[0].face = static_cast<VoxelFace>(6);
    CHECK(RejectedChunk(chunk, map) == "quads[0][3]");
    chunk = OneChunk();
    chunk.quads[0].width = 0;
    CHECK(RejectedChunk(chunk, map) == "quads[0][4]");
    chunk = OneChunk();
    chunk.quads[0].height = -1;
    CHECK(RejectedChunk(chunk, map) == "quads[0][5]");
    chunk = OneChunk();
    chunk.quads[0].palette = 1;
    CHECK(RejectedChunk(chunk, map) == "quads[0][6]");
}

TEST_CASE("C3: chunk colliders") {
    RegionDoc flat;
    flat.chunks = ChunkText("[0, 0, 0]", "[]", R"([{"min": [0, 0, 0], "max": [16, 0, 16]}])");
    CHECK(RejectedRegion(flat) == "chunks[0].colliders[0]");
    RegionDoc extra;
    extra.chunks = ChunkText("[0, 0, 0]", "[]", R"([{"min": [0, 0, 0], "max": [1, 1, 1], "id": 1}])");
    CHECK(RejectedRegion(extra) == "chunks[0].colliders[0]");
    RegionDoc pair;
    pair.chunks = ChunkText("[0, 0, 0]", "[]", R"([{"min": [0, 0], "max": [1, 1, 1]}])");
    CHECK(RejectedRegion(pair) == "chunks[0].colliders[0].min");

    const auto map = DefaultMap();
    auto chunk = OneChunk();
    chunk.colliders[0].maximum.z = 0;
    CHECK(RejectedChunk(chunk, map) == "colliders[0]");
}

TEST_CASE("C4: chunk coordinates lie in the chunk") {
    // The face on the chunk's far plane (256) is inside.
    RegionDoc farPlane;
    farPlane.chunks = ChunkText("[0, 0, 0]", "[[256, 0, 0, 0, 256, 256, 0]]",
                                R"([{"min": [0, 0, 0], "max": [256, 256, 256]}])");
    CHECK(AcceptedRegion(farPlane).chunks[0].quads[0].x == 256);
    RegionDoc negative;
    negative.chunks = ChunkText("[0, 0, 0]", "[[0, -1, 0, 2, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(negative) == "chunks[0].quads[0][1]");
    RegionDoc beyond;
    beyond.chunks = ChunkText("[0, 0, 0]", "[[257, 0, 0, 0, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(beyond) == "chunks[0].quads[0][0]");
    // The far corner: width 16 from 241 reaches 257 on Z.
    RegionDoc farCorner;
    farCorner.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 241, 0, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(farCorner) == "chunks[0].quads[0]");
    RegionDoc boxBeyond;
    boxBeyond.chunks = ChunkText("[0, 0, 0]", "[]", R"([{"min": [0, 0, 0], "max": [16, 257, 16]}])");
    CHECK(RejectedRegion(boxBeyond) == "chunks[0].colliders[0].max[1]");
    RegionDoc boxBelow;
    boxBelow.chunks = ChunkText("[0, 0, 0]", "[]", R"([{"min": [0, 0, -1], "max": [16, 16, 16]}])");
    CHECK(RejectedRegion(boxBelow) == "chunks[0].colliders[0].min[2]");
    CHECK(RegionMessage(boxBelow).find("outside the chunk") != std::string::npos);

    const auto map = DefaultMap();
    auto chunk = OneChunk();
    chunk.quads[0] = {0, 0, 241, VoxelFace::PositiveX, 16, 16, 0};
    CHECK(RejectedChunk(chunk, map) == "quads[0]");
    chunk = OneChunk();
    chunk.quads[0] = {-1, 0, 0, VoxelFace::PositiveY, 16, 16, 0};
    CHECK(RejectedChunk(chunk, map) == "quads[0][0]");
    chunk = OneChunk();
    chunk.colliders[0] = {{0, 0, 0}, {257, 16, 16}};
    CHECK(RejectedChunk(chunk, map) == "colliders[0].max[0]");
    chunk = OneChunk();
    chunk.quads[0] = {256, 0, 0, VoxelFace::NegativeX, 256, 256, 0};
    CHECK(ValidateVoxelChunk(chunk, map));
}

TEST_CASE("C5: chunk canonical order") {
    RegionDoc quads;
    quads.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 1, 2, 16, 16, 0], [0, 0, 0, 2, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(quads) == "chunks[0].quads[1]");
    RegionDoc sameQuads;
    sameQuads.chunks = ChunkText("[0, 0, 0]", "[[0, 0, 0, 2, 16, 16, 0], [0, 0, 0, 2, 16, 16, 0]]", "[]");
    CHECK(RejectedRegion(sameQuads) == "chunks[0].quads[1]");
    RegionDoc boxes;
    boxes.chunks = ChunkText("[0, 0, 0]", "[]",
                             R"([{"min": [0, 0, 0], "max": [16, 16, 32]}, {"min": [0, 0, 0], "max": [16, 16, 16]}])");
    CHECK(RejectedRegion(boxes) == "chunks[0].colliders[1]");

    const auto map = DefaultMap();
    auto chunk = OneChunk();
    chunk.quads.push_back(chunk.quads[0]);
    CHECK(RejectedChunk(chunk, map) == "quads[1]");
    chunk = OneChunk();
    chunk.colliders.insert(chunk.colliders.begin(), VoxelBox{{0, 0, 0}, {16, 16, 32}});
    CHECK(RejectedChunk(chunk, map) == "colliders[1]");
}

TEST_CASE("voxel map keys: total order and regions") {
    CHECK(VoxelChunkKey{0, 5, 5} < VoxelChunkKey{1, 0, 0});
    CHECK(VoxelChunkKey{0, 0, 5} < VoxelChunkKey{0, 1, 0});
    CHECK(VoxelChunkKey{-1, 9, 9} < VoxelChunkKey{0, 0, 0});
    CHECK(VoxelRegionKey{-1, 5} < VoxelRegionKey{0, -5});
    CHECK(RegionOf({0, 0, 0}) == VoxelRegionKey{0, 0});
    CHECK(RegionOf({7, 99, 7}) == VoxelRegionKey{0, 0});
    CHECK(RegionOf({8, 0, -1}) == VoxelRegionKey{1, -1});
    CHECK(RegionOf({-8, 0, -9}) == VoxelRegionKey{-1, -2});
}

TEST_CASE("voxel map expansion: golden chunk vertices, uv and boxes") {
    MapDoc doc;
    doc.origin = "[-1, 0, 2]";
    doc.cellSize = "0.5";
    doc.palette = R"([{"name": "a", "faces": )" + Faces + R"(}, {"name": "b", "faces": )" + Faces + "}]";
    const auto map = AcceptedMap(doc);
    VoxelChunkDocument chunk;
    chunk.at = {-1, 0, 1};
    chunk.quads = {{16, 32, 48, VoxelFace::PositiveZ, 16, 16, 1}};
    chunk.colliders = {{{16, 32, 48}, {32, 48, 64}}};
    REQUIRE(ValidateVoxelChunk(chunk, map));
    const auto geometry = ExpandVoxelChunk(chunk, map);
    // Units: x = 16 + 256 * -1 + 16 * -1 = -256 (and -240), y = 32 (48),
    // z = 48 + 256 + 32 = 336; world = units * 0.5 / 16.
    REQUIRE(geometry.vertices.size() == 4);
    const std::uint32_t xs[4] = {0xc1000000, 0xc0f00000, 0xc0f00000, 0xc1000000}; // -8, -7.5
    const std::uint32_t ys[4] = {0x3f800000, 0x3f800000, 0x3fc00000, 0x3fc00000}; // 1, 1.5
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(Bits(geometry.vertices[i].position.x) == xs[i]);
        CHECK(Bits(geometry.vertices[i].position.y) == ys[i]);
        CHECK(Bits(geometry.vertices[i].position.z) == 0x41280000); // 10.5
        CHECK(Bits(geometry.vertices[i].uv.x) == Bits(1.5F / 2.0F));
        CHECK(Bits(geometry.vertices[i].uv.y) == Bits(4.5F / 6.0F));
    }
    CHECK(geometry.indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3});
    REQUIRE(geometry.colliders.size() == 1);
    CHECK(Bits(geometry.colliders[0].minimum.x) == 0xc1000000);
    CHECK(Bits(geometry.colliders[0].maximum.x) == 0xc0f00000);
    CHECK(Bits(geometry.colliders[0].minimum.z) == 0x41280000);
    const auto origin = ChunkWorldOrigin({-1, 0, 1}, map);
    CHECK(origin.x == -8.5F); // (-256 - 16) * 0.5 / 16
    CHECK(origin.y == 0.0F);
    CHECK(origin.z == 9.0F); // (256 + 32) * 0.5 / 16
    const auto palette = ExpandVoxelPalette(map);
    CHECK(palette.width == 2);
    CHECK(palette.height == 6);
    REQUIRE(palette.rgba8.size() == 48);
    CHECK(palette.rgba8[4] == std::byte{1}); // row 0, column 1
    CHECK(palette.rgba8[8] == std::byte{4}); // row 1, column 0
}

TEST_CASE("voxel map expansion: golden world coordinates at both ends of the range") {
    MapDoc doc;
    doc.cellSize = "0.1";
    doc.bounds = R"({"min": [-4096, 0, 0], "max": [4095, 0, 0]})";
    doc.regions = R"([{"at": [0, 0], "file": "r", "sha256": "@"}])";
    const auto map = AcceptedMap(doc);
    VoxelChunkDocument high;
    high.at = {4095, 0, 0};
    high.quads = {{256, 0, 3, VoxelFace::PositiveX, 1, 1, 0}};
    REQUIRE(ValidateVoxelChunk(high, map));
    const auto highGeometry = ExpandVoxelChunk(high, map);
    // x = 4095 * 256 + 256 = 2^20 units; z = 3 units (and 4); cell 0.1f.
    CHECK(Bits(highGeometry.vertices[1].position.x) == 0x45cccccd); // 6553.6001
    CHECK(Bits(highGeometry.vertices[1].position.z) == 0x3c99999a); // 3 * 0.1f / 16
    VoxelChunkDocument low;
    low.at = {-4096, 0, 0};
    low.colliders = {{{0, 0, 0}, {3, 1, 1}}};
    REQUIRE(ValidateVoxelChunk(low, map));
    const auto lowGeometry = ExpandVoxelChunk(low, map);
    CHECK(Bits(lowGeometry.colliders[0].minimum.x) == 0xc5cccccd); // -2^20 units
    CHECK(Bits(lowGeometry.colliders[0].maximum.x) == Bits(static_cast<float>(-1048573) * 0.1F / 16.0F));
    CHECK(Bits(ChunkWorldOrigin({4095, 0, 0}, map).x) == Bits(static_cast<float>(1048320) * 0.1F / 16.0F));
    CHECK(Bits(MapUnitsToWorld({1048576, 3, -1048576}, map).x) == 0x45cccccd);
    CHECK(Bits(MapUnitsToWorld({1048576, 3, -1048576}, map).y) == 0x3c99999a);
    CHECK(Bits(MapUnitsToWorld({1048576, 3, -1048576}, map).z) == 0xc5cccccd);
    MapDoc shifted;
    shifted.origin = "[2, -1, 0]";
    shifted.cellSize = "0.3";
    const auto shiftedMap = AcceptedMap(shifted);
    // units + 16 * origin: (-1048605 + 32, ...) = -1048573 units at cell 0.3f.
    CHECK(Bits(MapUnitsToWorld({-1048605, 16, 0}, shiftedMap).x) == 0xc699997d);
    CHECK(MapUnitsToWorld({-1048605, 16, 0}, shiftedMap).y == 0.0F);
}

TEST_CASE("voxel map expansion: a chunk expands to the same bits as the single-file format") {
    // The same quads and boxes, placed once in gyo.voxel_mesh units and once
    // in chunks, give the same floats.
    const std::string origin = "[-3, 2, 5]";
    const std::string cell = "0.3";
    const std::string quadsV1 = "[[-300, 40, 17, 1, 3, 5, 0], [-17, 0, 600, 4, 16, 2, 0], [255, 256, 511, 2, 1, 1, 0]]";
    const std::string boxesV1 = R"([{"min": [-300, 40, 17], "max": [-290, 41, 18]}])";
    const std::string text = "{\"schema\": \"gyo.voxel_mesh\", \"version\": 1, \"source_sha256\": \"" +
                             std::string(64, 'a') + "\", \"build_version\": \"x\", \"origin\": " + origin +
                             ", \"cell_size\": " + cell + R"(, "palette": [{"name": "a", "faces": )" + Faces +
                             "}], \"quads\": " + quadsV1 + ", \"colliders\": " + boxesV1 + "}";
    const auto single = ParseVoxelMesh(text);
    REQUIRE(single);
    const auto expected = ExpandVoxelMesh(*single);
    MapDoc doc;
    doc.origin = origin;
    doc.cellSize = cell;
    doc.bounds = R"({"min": [-2, 0, 0], "max": [1, 1, 2]})";
    doc.regions = R"([{"at": [-1, 0], "file": "a", "sha256": "@"}])";
    const auto map = AcceptedMap(doc);
    const auto floorDiv = [](const std::int32_t v) { return v >= 0 ? v / 256 : -((-v + 255) / 256); };
    std::size_t vertex = 0;
    for (const auto& quad : single->quads) {
        VoxelChunkDocument chunk;
        chunk.at = {floorDiv(quad.x), floorDiv(quad.y), floorDiv(quad.z)};
        auto local = quad;
        local.x -= chunk.at.x * 256;
        local.y -= chunk.at.y * 256;
        local.z -= chunk.at.z * 256;
        chunk.quads = {local};
        REQUIRE(ValidateVoxelChunk(chunk, map));
        const auto geometry = ExpandVoxelChunk(chunk, map);
        for (std::size_t i = 0; i < 4; ++i, ++vertex) {
            CHECK(Bits(geometry.vertices[i].position.x) == Bits(expected.vertices[vertex].position.x));
            CHECK(Bits(geometry.vertices[i].position.y) == Bits(expected.vertices[vertex].position.y));
            CHECK(Bits(geometry.vertices[i].position.z) == Bits(expected.vertices[vertex].position.z));
            CHECK(Bits(geometry.vertices[i].uv.x) == Bits(expected.vertices[vertex].uv.x));
            CHECK(Bits(geometry.vertices[i].uv.y) == Bits(expected.vertices[vertex].uv.y));
        }
    }
    const auto& box = single->colliders[0];
    VoxelChunkDocument boxChunk;
    boxChunk.at = {floorDiv(box.minimum.x), floorDiv(box.minimum.y), floorDiv(box.minimum.z)};
    boxChunk.colliders = {{{box.minimum.x - boxChunk.at.x * 256, box.minimum.y - boxChunk.at.y * 256,
                            box.minimum.z - boxChunk.at.z * 256},
                           {box.maximum.x - boxChunk.at.x * 256, box.maximum.y - boxChunk.at.y * 256,
                            box.maximum.z - boxChunk.at.z * 256}}};
    REQUIRE(ValidateVoxelChunk(boxChunk, map));
    const auto boxGeometry = ExpandVoxelChunk(boxChunk, map);
    CHECK(Bits(boxGeometry.colliders[0].minimum.x) == Bits(expected.colliders[0].minimum.x));
    CHECK(Bits(boxGeometry.colliders[0].maximum.y) == Bits(expected.colliders[0].maximum.y));
    CHECK(Bits(boxGeometry.colliders[0].maximum.z) == Bits(expected.colliders[0].maximum.z));
}
