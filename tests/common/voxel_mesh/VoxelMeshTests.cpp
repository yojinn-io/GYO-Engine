#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <bit>
#include <cstdint>
#include <string>

using namespace Engine::VoxelMesh;

// Rules and expansion of gyo.voxel_mesh version 1 (docs/architecture/voxel-mesh.md).
// Judgments V1 (one test case per rule), V2 (expansion goldens) and V3
// (conversion goldens) of the v8-engine-voxel-mesh-contract declaration in
// docs/tools/plans/v8/voxel-map-editor.md.

namespace {
const std::string Sha(64, 'a');
const std::string Faces = "[[1,2,3,255],[4,5,6,255],[7,8,9,255],[10,11,12,255],[13,14,15,255],[16,17,18,255]]";

struct Doc final {
    std::string schema = R"("gyo.voxel_mesh")";
    std::string version = "1";
    std::string sha = "\"" + Sha + "\"";
    std::string build = R"("voxel_map_editor 1")";
    std::string origin = "[0, 0, 0]";
    std::string cellSize = "1.0";
    std::string palette = R"([{"name": "wall", "faces": )" + Faces + "}]";
    std::string quads = "[[0, 0, 0, 2, 16, 16, 0]]";
    std::string colliders = R"([{"min": [0, 0, 0], "max": [16, 16, 16]}])";
    std::string extra;

    [[nodiscard]] std::string Text() const {
        return "{\"schema\": " + schema + ", \"version\": " + version + ", \"source_sha256\": " + sha +
               ", \"build_version\": " + build + ", \"origin\": " + origin + ", \"cell_size\": " + cellSize +
               ", \"palette\": " + palette + ", \"quads\": " + quads + ", \"colliders\": " + colliders + extra +
               "}";
    }
};

std::string Rejected(const std::string& text) {
    const auto parsed = ParseVoxelMesh(text);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == VoxelMeshErrorCode::InvalidVoxelMesh);
    return parsed.error().detail;
}

std::string Rejected(const Doc& doc) { return Rejected(doc.Text()); }

std::string RejectedMessage(const std::string& text) {
    const auto parsed = ParseVoxelMesh(text);
    REQUIRE_FALSE(parsed);
    return parsed.error().message;
}

VoxelMeshDocument Accepted(const Doc& doc) {
    auto parsed = ParseVoxelMesh(doc.Text());
    REQUIRE_MESSAGE(parsed, (parsed ? std::string() : parsed.error().message + " at " + parsed.error().detail));
    return std::move(*parsed);
}

std::uint32_t Bits(const float value) { return std::bit_cast<std::uint32_t>(value); }

void CheckBits(const Engine::Math::Vec3 actual, const float x, const float y, const float z) {
    CHECK(Bits(actual.x) == Bits(x));
    CHECK(Bits(actual.y) == Bits(y));
    CHECK(Bits(actual.z) == Bits(z));
}

Engine::Math::Vec3 Difference(const Engine::Math::Vec3 a, const Engine::Math::Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Engine::Math::Vec3 CrossOf(const Engine::Math::Vec3 a, const Engine::Math::Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
} // namespace

TEST_CASE("voxel mesh: the minimal document is accepted") {
    const auto document = Accepted(Doc{});
    CHECK(document.sourceSha256 == Sha);
    CHECK(document.buildVersion == "voxel_map_editor 1");
    CHECK(document.palette.size() == 1);
    CHECK(document.quads.size() == 1);
    CHECK(document.colliders.size() == 1);
    Doc empty;
    empty.colliders = "[]";
    CHECK(Accepted(empty).colliders.empty());
}

TEST_CASE("voxel mesh rule 1: unknown and repeated fields") {
    Doc unknown;
    unknown.extra = R"(, "comment": "x")";
    CHECK(Rejected(unknown).empty());
    CHECK(RejectedMessage(unknown.Text()).find("unknown field 'comment'") != std::string::npos);
    // A complete document whose only fault is a repeated top-level key.
    Doc topRepeat;
    topRepeat.extra = R"(, "version": 1)";
    CHECK(Rejected(topRepeat).empty());
    CHECK(RejectedMessage(topRepeat.Text()).find("repeated key 'version'") != std::string::npos);
    Doc nestedRepeat;
    nestedRepeat.colliders = R"([{"min": [0, 0, 0], "min": [0, 0, 0], "max": [16, 16, 16]}])";
    CHECK(Rejected(nestedRepeat).empty());
    CHECK(RejectedMessage(nestedRepeat.Text()).find("repeated key 'min'") != std::string::npos);
    CHECK(Rejected("[]").empty());
    CHECK(RejectedMessage("[]").find("the document is an object") != std::string::npos);
    CHECK(Rejected("not json").empty());
    CHECK(RejectedMessage("not json").find("not JSON") != std::string::npos);
    Doc missing;
    missing.extra = "";
    const auto text = missing.Text();
    const auto truncated = text.substr(0, text.find(", \"colliders\"")) + "}";
    CHECK(Rejected(truncated).empty());
    CHECK(RejectedMessage(truncated).find("missing field 'colliders'") != std::string::npos);
}

TEST_CASE("voxel mesh rule 2: schema and version") {
    Doc schema;
    schema.schema = R"("gyo.voxel_grid")";
    CHECK(Rejected(schema) == "schema");
    Doc version;
    version.version = "2";
    CHECK(Rejected(version) == "version");
    version.version = "1.0";
    CHECK(Rejected(version) == "version");
    version.version = "true";
    CHECK(Rejected(version) == "version");
}

TEST_CASE("voxel mesh rule 3: source hash and build version") {
    Doc shortSha;
    shortSha.sha = "\"" + std::string(63, 'a') + "\"";
    CHECK(Rejected(shortSha) == "source_sha256");
    Doc upper;
    upper.sha = "\"" + std::string(63, 'a') + "A\"";
    CHECK(Rejected(upper) == "source_sha256");
    Doc emptyBuild;
    emptyBuild.build = R"("")";
    CHECK(Rejected(emptyBuild) == "build_version");
    Doc longBuild;
    longBuild.build = "\"" + std::string(65, 'v') + "\"";
    CHECK(Rejected(longBuild) == "build_version");
    Doc hexG;
    hexG.sha = "\"" + std::string(63, 'a') + "g\"";
    CHECK(Rejected(hexG) == "source_sha256");
    Doc build64;
    build64.build = "\"" + std::string(64, 'v') + "\"";
    CHECK(Accepted(build64).buildVersion.size() == 64);
    Doc control;
    control.build = R"("a\tb")";
    CHECK(Rejected(control) == "build_version");
}

TEST_CASE("voxel mesh rule 4: origin and cell size") {
    Doc origin;
    origin.origin = "[0, 0]";
    CHECK(Rejected(origin) == "origin");
    origin.origin = "[0, 0.5, 0]";
    CHECK(Rejected(origin) == "origin[1]");
    Doc cell;
    // [2^-16, 2^16]: below, the division by 16 could leave the normal floats;
    // above, a corner 2^20 units away could overflow.
    for (const auto* value : {"0", "-1", "\"1\"", "1e39", "1e33", "65536.01", "1e-5", "1e-40", "1e-50"}) {
        cell.cellSize = value;
        CHECK(Rejected(cell) == "cell_size");
    }
    for (const auto* value : {"65536", "0.0000152587890625", "0.3"}) {
        cell.cellSize = value;
        CHECK(Accepted(cell).quads.size() == 1);
    }
}

TEST_CASE("voxel mesh rule 5: palette") {
    Doc empty;
    empty.palette = "[]";
    CHECK(Rejected(empty) == "palette");
    Doc name;
    name.palette = R"([{"name": "Wall", "faces": )" + Faces + "}]";
    CHECK(Rejected(name) == "palette[0].name");
    Doc repeat;
    repeat.palette = R"([{"name": "wall", "faces": )" + Faces + R"(}, {"name": "wall", "faces": )" + Faces + "}]";
    CHECK(Rejected(repeat) == "palette[1].name");
    Doc fiveFaces;
    fiveFaces.palette = R"([{"name": "wall", "faces": [[1,2,3,4],[1,2,3,4],[1,2,3,4],[1,2,3,4],[1,2,3,4]]}])";
    CHECK(Rejected(fiveFaces) == "palette[0].faces");
    Doc channel;
    channel.palette = R"([{"name": "wall", "faces": [[1,2,3,256],[1,2,3,4],[1,2,3,4],[1,2,3,4],[1,2,3,4],[1,2,3,4]]}])";
    CHECK(Rejected(channel) == "palette[0].faces[0][3]");
    Doc emptyName;
    emptyName.palette = R"([{"name": "", "faces": )" + Faces + "}]";
    CHECK(Rejected(emptyName) == "palette[0].name");
    Doc longName;
    longName.palette = R"([{"name": ")" + std::string(65, 'n') + R"(", "faces": )" + Faces + "}]";
    CHECK(Rejected(longName) == "palette[0].name");
    Doc negative;
    negative.palette = R"([{"name": "wall", "faces": [[1,2,3,4],[1,2,-1,4],[1,2,3,4],[1,2,3,4],[1,2,3,4],[1,2,3,4]]}])";
    CHECK(Rejected(negative) == "palette[0].faces[1][2]");
    Doc threeChannels;
    threeChannels.palette = R"([{"name": "wall", "faces": [[1,2,3,4],[1,2,3],[1,2,3,4],[1,2,3,4],[1,2,3,4],[1,2,3,4]]}])";
    CHECK(Rejected(threeChannels) == "palette[0].faces[1]");
    Doc extraField;
    extraField.palette = R"([{"name": "wall", "solid": true, "faces": )" + Faces + "}]";
    CHECK(Rejected(extraField) == "palette[0]");
    Doc tooMany;
    std::string entries;
    for (int i = 0; i < 257; ++i)
        entries += (i ? "," : "") + std::string(R"({"name": "c)") + std::to_string(i) + R"(", "faces": )" + Faces + "}";
    tooMany.palette = "[" + entries + "]";
    CHECK(Rejected(tooMany) == "palette");
}

TEST_CASE("voxel mesh rule 6: quads") {
    Doc empty;
    empty.quads = "[]";
    CHECK(Rejected(empty) == "quads");
    Doc six;
    six.quads = "[[0, 0, 0, 2, 16, 16]]";
    CHECK(Rejected(six) == "quads[0]");
    Doc dir;
    dir.quads = "[[0, 0, 0, 6, 16, 16, 0]]";
    CHECK(Rejected(dir) == "quads[0][3]");
    Doc width;
    width.quads = "[[0, 0, 0, 2, 0, 16, 0]]";
    CHECK(Rejected(width) == "quads[0][4]");
    Doc heightRule;
    heightRule.quads = "[[0, 0, 0, 2, 16, 0, 0]]";
    CHECK(Rejected(heightRule) == "quads[0][5]");
    Doc palette;
    palette.quads = "[[0, 0, 0, 2, 16, 16, 1]]";
    CHECK(Rejected(palette) == "quads[0][6]");
    Doc fraction;
    fraction.quads = "[[0, 0, 0.5, 2, 16, 16, 0]]";
    CHECK(Rejected(fraction) == "quads[0][2]");
}

TEST_CASE("voxel mesh rule 7: colliders") {
    Doc array;
    array.colliders = "{}";
    CHECK(Rejected(array) == "colliders");
    Doc flat;
    flat.colliders = R"([{"min": [0, 0, 0], "max": [16, 0, 16]}])";
    CHECK(Rejected(flat) == "colliders[0]");
    Doc inverted;
    inverted.colliders = R"([{"min": [0, 0, 32], "max": [16, 16, 16]}])";
    CHECK(Rejected(inverted) == "colliders[0]");
    Doc keys;
    keys.colliders = R"([{"min": [0, 0, 0]}])";
    CHECK(Rejected(keys) == "colliders[0]");
    Doc extraField;
    extraField.colliders = R"([{"min": [0, 0, 0], "max": [16, 16, 16], "solid": true}])";
    CHECK(Rejected(extraField) == "colliders[0]");
}

TEST_CASE("voxel mesh rule 8: coordinate range") {
    constexpr auto limit = CoordinateLimit;
    Doc atLimit;
    atLimit.quads = "[[" + std::to_string(limit) + ", 0, 0, 0, 16, 16, 0]]";
    CHECK(Accepted(atLimit).quads.size() == 1);
    Doc beyond;
    beyond.quads = "[[" + std::to_string(limit + 1) + ", 0, 0, 0, 16, 16, 0]]";
    CHECK(Rejected(beyond) == "quads[0][0]");
    Doc farEdge;
    farEdge.quads = "[[0, " + std::to_string(limit - 15) + ", 0, 0, 16, 16, 0]]";
    CHECK(Rejected(farEdge) == "quads[0]");
    Doc withOrigin;
    withOrigin.origin = "[-1, 0, 0]";
    withOrigin.quads = "[[" + std::to_string(-limit + 15) + ", 0, 0, 0, 16, 16, 0]]";
    CHECK(Rejected(withOrigin) == "quads[0][0]");
    Doc colliderRange;
    colliderRange.colliders = R"([{"min": [0, 0, 0], "max": [16, 16, )" + std::to_string(limit + 1) + "]}]";
    CHECK(Rejected(colliderRange) == "colliders[0].max[2]");
    Doc atNegativeLimit;
    atNegativeLimit.quads = "[[" + std::to_string(-limit) + ", 0, 0, 0, 16, 16, 0]]";
    CHECK(Accepted(atNegativeLimit).quads.size() == 1);
    Doc colliderMin;
    colliderMin.colliders = R"([{"min": [0, )" + std::to_string(-limit - 1) + R"(, 0], "max": [16, 16, 16]}])";
    CHECK(Rejected(colliderMin) == "colliders[0].min[1]");
    // Only the corners count: a far origin with quads that bring them back is valid.
    Doc farOrigin;
    farOrigin.origin = "[70000, 0, 0]";
    farOrigin.quads = "[[-1120000, 0, 0, 0, 16, 16, 0]]";
    farOrigin.colliders = R"([{"min": [-1120000, 0, 0], "max": [-1119984, 16, 16]}])";
    CHECK(Accepted(farOrigin).quads.size() == 1);
    Doc huge;
    huge.quads = "[[18446744073709551615, 0, 0, 0, 16, 16, 0]]";
    CHECK(Rejected(huge) == "quads[0][0]");
}

TEST_CASE("voxel mesh rule 9: canonical order") {
    Doc reversed;
    reversed.quads = "[[0, 0, 16, 2, 16, 16, 0], [0, 0, 0, 2, 16, 16, 0]]";
    CHECK(Rejected(reversed) == "quads[1]");
    Doc repeated;
    repeated.quads = "[[0, 0, 0, 2, 16, 16, 0], [0, 0, 0, 2, 16, 16, 0]]";
    CHECK(Rejected(repeated) == "quads[1]");
    Doc ordered;
    ordered.quads = "[[0, 0, 0, 2, 16, 16, 0], [0, 0, 0, 3, 16, 16, 0]]";
    CHECK(Accepted(ordered).quads.size() == 2);
    Doc boxes;
    boxes.colliders = R"([{"min": [0, 0, 0], "max": [16, 16, 32]}, {"min": [0, 0, 0], "max": [16, 16, 16]}])";
    CHECK(Rejected(boxes) == "colliders[1]");
    Doc sameBoxes;
    sameBoxes.colliders = R"([{"min": [0, 0, 0], "max": [16, 16, 16]}, {"min": [0, 0, 0], "max": [16, 16, 16]}])";
    CHECK(Rejected(sameBoxes) == "colliders[1]");
}

TEST_CASE("voxel mesh rule 10: text size") {
    Doc padded;
    padded.extra = std::string(MaxTextBytes, ' ');
    CHECK(Rejected(padded).empty());
    CHECK(RejectedMessage(padded.Text()).find("16 MiB") != std::string::npos);
    Doc exact;
    exact.extra = std::string(MaxTextBytes - Doc{}.Text().size(), ' ');
    REQUIRE(exact.Text().size() == MaxTextBytes);
    CHECK(Accepted(exact).quads.size() == 1);
}

TEST_CASE("voxel mesh expansion: faces are clockwise seen from outside") {
    Doc all;
    all.quads = "[[0, 0, 0, 0, 16, 16, 0], [0, 0, 0, 1, 16, 16, 0], [0, 0, 0, 2, 16, 16, 0], "
                "[0, 0, 0, 3, 16, 16, 0], [0, 0, 0, 4, 16, 16, 0], [0, 0, 0, 5, 16, 16, 0]]";
    const auto geometry = ExpandVoxelMesh(Accepted(all));
    REQUIRE(geometry.vertices.size() == 24);
    REQUIRE(geometry.indices.size() == 36);
    // Render's unit cube (PrimitiveMesh.cpp, MakeUnitCube) gives
    // cross(v1 - v0, v2 - v0) along the outward normal for every face.
    const Engine::Math::Vec3 normals[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (std::size_t face = 0; face < 6; ++face) {
        const auto* triangle = &geometry.indices[face * 6];
        const auto v0 = geometry.vertices[triangle[0]].position;
        const auto v1 = geometry.vertices[triangle[1]].position;
        const auto v2 = geometry.vertices[triangle[2]].position;
        const auto n = CrossOf(Difference(v1, v0), Difference(v2, v0));
        CHECK(n.x * normals[face].x + n.y * normals[face].y + n.z * normals[face].z > 0.0F);
    }
}

TEST_CASE("voxel mesh expansion: golden vertices, indices and uv") {
    Doc doc;
    doc.palette = R"([{"name": "a", "faces": )" + Faces + R"(}, {"name": "b", "faces": )" + Faces + "}]";
    doc.quads = "[[0, 0, 32, 4, 48, 32, 1], [0, 16, 0, 2, 16, 16, 1], [16, 0, 0, 1, 16, 16, 0]]";
    const auto geometry = ExpandVoxelMesh(Accepted(doc));
    REQUIRE(geometry.vertices.size() == 12);
    REQUIRE(geometry.indices.size() == 18);
    // +Z face at z = 2 cells, 3 cells wide and 2 high, palette 1.
    CheckBits(geometry.vertices[0].position, 0.0F, 0.0F, 2.0F);
    CheckBits(geometry.vertices[1].position, 3.0F, 0.0F, 2.0F);
    CheckBits(geometry.vertices[2].position, 3.0F, 2.0F, 2.0F);
    CheckBits(geometry.vertices[3].position, 0.0F, 2.0F, 2.0F);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(Bits(geometry.vertices[i].uv.x) == Bits(0.75F));
        CHECK(Bits(geometry.vertices[i].uv.y) == Bits(4.5F / 6.0F));
    }
    // +Y face at y = 1 cell, palette 1: column 1 of 2, row 2 of 6.
    CheckBits(geometry.vertices[4].position, 0.0F, 1.0F, 1.0F);
    CheckBits(geometry.vertices[5].position, 1.0F, 1.0F, 1.0F);
    CheckBits(geometry.vertices[6].position, 1.0F, 1.0F, 0.0F);
    CheckBits(geometry.vertices[7].position, 0.0F, 1.0F, 0.0F);
    for (std::size_t i = 4; i < 8; ++i) {
        CHECK(Bits(geometry.vertices[i].uv.x) == Bits(0.75F));
        CHECK(Bits(geometry.vertices[i].uv.y) == Bits(2.5F / 6.0F));
    }
    // -X face at x = 1 cell, palette 0, row 1.
    CheckBits(geometry.vertices[8].position, 1.0F, 0.0F, 0.0F);
    CheckBits(geometry.vertices[9].position, 1.0F, 0.0F, 1.0F);
    CheckBits(geometry.vertices[10].position, 1.0F, 1.0F, 1.0F);
    CheckBits(geometry.vertices[11].position, 1.0F, 1.0F, 0.0F);
    CHECK(Bits(geometry.vertices[8].uv.x) == Bits(0.25F));
    CHECK(Bits(geometry.vertices[8].uv.y) == Bits(1.5F / 6.0F));
    const std::uint32_t expected[] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11};
    for (std::size_t i = 0; i < 18; ++i) CHECK(geometry.indices[i] == expected[i]);
    // Palette image: 2 columns x 6 rows, row = direction.
    CHECK(geometry.paletteWidth == 2);
    CHECK(geometry.paletteHeight == 6);
    REQUIRE(geometry.paletteRgba8.size() == 48);
    CHECK(std::to_integer<int>(geometry.paletteRgba8[(2 * 2 + 1) * 4 + 0]) == 7);
    CHECK(std::to_integer<int>(geometry.paletteRgba8[(5 * 2 + 0) * 4 + 2]) == 18);
}

TEST_CASE("voxel mesh conversion: golden world coordinates") {
    Doc doc;
    doc.origin = "[-1, 0, -2]";
    doc.cellSize = "0.3";
    doc.quads = "[[1, 8, 3, 2, 16, 16, 0]]";
    doc.colliders = R"([{"min": [0, 0, 0], "max": [24, 48, 40]}])";
    const auto geometry = ExpandVoxelMesh(Accepted(doc));
    const float cell = 0.3F;
    // float(units + 16 * origin) * cell / 16, one rounding per step.
    CheckBits(geometry.vertices[3].position, -15.0F * cell / 16.0F, 8.0F * cell / 16.0F, -29.0F * cell / 16.0F);
    CheckBits(geometry.colliders[0].minimum, -16.0F * cell / 16.0F, 0.0F, -32.0F * cell / 16.0F);
    CheckBits(geometry.colliders[0].maximum, 8.0F * cell / 16.0F, 48.0F * cell / 16.0F, 8.0F * cell / 16.0F);
    // Literal bits, so a platform that rounds differently fails here.
    CHECK(Bits(geometry.colliders[0].maximum.y) == 0x3F666667U); // 48 * 0.3f / 16 = 0.90000004
    CHECK(Bits(geometry.colliders[0].minimum.x) == 0xBE99999AU); // -0.3f

    Doc edge;
    edge.cellSize = "1.0";
    edge.quads = "[[" + std::to_string(CoordinateLimit - 16) + ", 0, 0, 2, 16, 16, 0]]";
    const auto atEdge = ExpandVoxelMesh(Accepted(edge));
    CHECK(Bits(atEdge.vertices[1].position.x) == Bits(65536.0F));

    // The negative bound, reached through a negative origin, at cell 0.3.
    Doc negative;
    negative.origin = "[-1, 0, 0]";
    negative.cellSize = "0.3";
    negative.quads = "[[" + std::to_string(-CoordinateLimit + 16) + ", 0, 0, 2, 16, 16, 0]]";
    const auto low = ExpandVoxelMesh(Accepted(negative));
    // -2^20 * 0.3f / 16 = -19660.8 rounded once: 0xC699999A.
    CHECK(Bits(low.vertices[0].position.x) == 0xC699999AU);
}
