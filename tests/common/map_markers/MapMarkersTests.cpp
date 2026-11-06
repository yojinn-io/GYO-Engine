#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/map_markers/MapMarkers.hpp"

#include <string>
#include <variant>
#include <vector>

using namespace Engine::MapMarkers;

// Rules of gyo.map_markers version 1 (docs/architecture/map-markers.md).
// Judgment F1 of the v8-arch-big-map-engine-format declaration
// (docs/architecture/plans/big-map/engine-format.zh-Hant.md): one test case per
// rule, named by its rule code. Names in the data are neutral on purpose.

namespace {
struct Doc final {
    std::string schema = R"("gyo.map_markers")";
    std::string version = "1";
    std::string markers = R"([{"id": "m1", "kind": "a", "tags": {}, "point": [0, 16, 0], "yaw": 1.5}])";
    std::string extra;

    [[nodiscard]] std::string Text() const {
        return "{\"schema\": " + schema + ", \"version\": " + version + ", \"markers\": " + markers + extra + "}";
    }
};

std::string One(const std::string& marker) { return "[" + marker + "]"; }

MapMarkersDocument Accepted(const std::string& text) {
    auto parsed = ParseMapMarkers(text);
    REQUIRE_MESSAGE(parsed, (parsed ? std::string() : parsed.error().message + " at " + parsed.error().detail));
    return std::move(*parsed);
}

MapMarkersDocument Accepted(const Doc& doc) { return Accepted(doc.Text()); }

std::string Rejected(const std::string& text) {
    const auto parsed = ParseMapMarkers(text);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == MapMarkersErrorCode::InvalidMapMarkers);
    return parsed.error().detail;
}

std::string Rejected(const Doc& doc) { return Rejected(doc.Text()); }

std::string Message(const Doc& doc) {
    const auto parsed = ParseMapMarkers(doc.Text());
    REQUIRE_FALSE(parsed);
    return parsed.error().message;
}

Doc WithMarkers(const std::string& markers) {
    Doc doc;
    doc.markers = markers;
    return doc;
}
} // namespace

TEST_CASE("map markers: the three shapes are accepted") {
    const auto document = Accepted(WithMarkers(
        R"([{"id": "m1", "kind": "a", "tags": {}, "point": [0, 16, 0], "yaw": 1.5},)"
        R"( {"id": "m2", "kind": "b", "tags": {"k": "v"}, "box": {"min": [0, 0, 0], "max": [32, 32, 32]}},)"
        R"( {"id": "m3", "kind": "c", "tags": {"x": "1", "y": "2"}, "path": [[0, 0, 0], [16, 0, 0]]}])"));
    REQUIRE(document.markers.size() == 3);
    const auto& point = std::get<MarkerPoint>(document.markers[0].shape);
    CHECK(point.position == Engine::Math::Vec3i{0, 16, 0});
    CHECK(point.yaw == 1.5F);
    CHECK(document.markers[1].tags == std::vector<std::pair<std::string, std::string>>{{"k", "v"}});
    const auto& box = std::get<MarkerBox>(document.markers[1].shape);
    CHECK(box.maximum == Engine::Math::Vec3i{32, 32, 32});
    const auto& path = std::get<MarkerPath>(document.markers[2].shape);
    CHECK(path.points.size() == 2);
    CHECK(document.markers[2].kind == "c");
    CHECK(Accepted(WithMarkers("[]")).markers.empty());
}

TEST_CASE("K1: fields and repeated keys") {
    Doc unknown;
    unknown.extra = R"(, "comment": "x")";
    CHECK(Rejected(unknown).empty());
    CHECK(Message(unknown).find("unknown field 'comment'") != std::string::npos);
    Doc repeated;
    repeated.extra = R"(, "version": 1)";
    CHECK(Message(repeated).find("repeated key 'version'") != std::string::npos);
    const auto nested = WithMarkers(One(R"({"id": "m", "id": "n", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]})"));
    CHECK(Message(nested).find("repeated key 'id'") != std::string::npos);
    const auto tagRepeat = WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {"k": "1", "k": "2"}, "path": [[0,0,0],[1,0,0]]})"));
    CHECK(Message(tagRepeat).find("repeated key 'k'") != std::string::npos);
    CHECK(Rejected("[]").empty());
    CHECK(Rejected("not json").empty());
    CHECK(Rejected(R"({"schema": "gyo.map_markers", "version": 1})").empty());
}

TEST_CASE("K2: schema and version") {
    Doc schema;
    schema.schema = R"("gyo.voxel_map")";
    CHECK(Rejected(schema) == "schema");
    Doc version;
    version.version = "2";
    CHECK(Rejected(version) == "version");
    Doc fraction;
    fraction.version = "1.0";
    CHECK(Rejected(fraction) == "version");
}

TEST_CASE("K3: markers in id order") {
    CHECK(Rejected(WithMarkers(R"({"id": "m"})")) == "markers");
    const auto unordered = WithMarkers(
        R"([{"id": "b", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]},)"
        R"( {"id": "a", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]}])");
    CHECK(Rejected(unordered) == "markers[1].id");
    const auto same = WithMarkers(
        R"([{"id": "a", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]},)"
        R"( {"id": "a", "kind": "b", "tags": {}, "path": [[0,0,0],[1,0,0]]}])");
    CHECK(Rejected(same) == "markers[1].id");
    // Byte order: "a-1" < "a.1" < "a1" < "a_1".
    const auto bytes = WithMarkers(
        R"([{"id": "a-1", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]},)"
        R"( {"id": "a.1", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]},)"
        R"( {"id": "a1", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]},)"
        R"( {"id": "a_1", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]}])");
    CHECK(Accepted(bytes).markers.size() == 4);
    std::string many = "[";
    std::string atLimit;
    for (std::size_t i = 0; i <= MaxMarkers; ++i) {
        if (i == MaxMarkers) atLimit = many + "]";
        if (i > 0) many += ",";
        auto id = std::to_string(i);
        id = std::string(6 - id.size(), '0') + id;
        many += R"({"id":")" + id + R"(","kind":"a","tags":{},"path":[[0,0,0],[1,0,0]]})";
    }
    // Compact, so the text stays within 4 MiB and the count is what fails.
    REQUIRE(WithMarkers(many + "]").Text().size() < MaxTextBytes);
    CHECK(Rejected(WithMarkers(many + "]")) == "markers");
    CHECK(Accepted(WithMarkers(atLimit)).markers.size() == MaxMarkers);
}

TEST_CASE("K4: exactly one shape") {
    CHECK(Rejected(WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}})"))) == "markers[0]");
    // Two shapes are rejected by this rule itself, not only because the
    // second shape's field is unknown to the first (engine-format F2).
    const auto twoShapes = WithMarkers(
        One(R"({"id": "m", "kind": "a", "tags": {}, "point": [0,0,0], "yaw": 0, "path": [[0,0,0],[1,0,0]]})"));
    CHECK(Rejected(twoShapes) == "markers[0]");
    CHECK(Message(twoShapes).find("exactly one of point, box and path") != std::string::npos);
    const auto noShape = WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}})"));
    CHECK(Message(noShape).find("exactly one of point, box and path") != std::string::npos);
    CHECK(Rejected(WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}, "point": [0,0,0]})"))) == "markers[0]");
    const auto yawOnBox = WithMarkers(
        One(R"({"id": "m", "kind": "a", "tags": {}, "box": {"min": [0,0,0], "max": [1,1,1]}, "yaw": 0})"));
    CHECK(Rejected(yawOnBox) == "markers[0]");
    CHECK(Message(yawOnBox).find("unknown field 'yaw'") != std::string::npos);
    CHECK(Rejected(WithMarkers(One(R"({"kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]})"))) == "markers[0]");
    CHECK(Rejected(WithMarkers(One(R"({"id": "m", "kind": "a", "path": [[0,0,0],[1,0,0]]})"))) == "markers[0]");
    CHECK(Rejected(WithMarkers(One(R"({"id": "m", "tags": {}, "path": [[0,0,0],[1,0,0]]})"))) == "markers[0]");
    CHECK(Rejected(WithMarkers(R"(["m"])")) == "markers[0]");
}

TEST_CASE("K5: id and kind") {
    for (const auto* bad : {"", "A", "a b", "a/b", "\\u00e9"}) {
        CHECK_MESSAGE(Rejected(WithMarkers(One(std::string(R"({"id": ")") + bad +
                                               R"(", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]})"))) ==
                          "markers[0].id",
                      bad);
        CHECK_MESSAGE(Rejected(WithMarkers(One(std::string(R"({"id": "m", "kind": ")") + bad +
                                               R"(", "tags": {}, "path": [[0,0,0],[1,0,0]]})"))) ==
                          "markers[0].kind",
                      bad);
    }
    const std::string longest(64, 'a');
    CHECK(Accepted(WithMarkers(One(R"({"id": ")" + longest + R"(", "kind": ")" + longest +
                                   R"(", "tags": {}, "path": [[0,0,0],[1,0,0]]})")))
              .markers[0]
              .id.size() == 64);
    CHECK(Rejected(WithMarkers(One(R"({"id": ")" + longest + R"(a", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]})"))) ==
          "markers[0].id");
    CHECK(Rejected(WithMarkers(One(R"({"id": 1, "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]})"))) ==
          "markers[0].id");
    CHECK(Accepted(WithMarkers(One(R"({"id": "x.y-z_0", "kind": "k.1-2_3", "tags": {}, "path": [[0,0,0],[1,0,0]]})")))
              .markers[0]
              .kind == "k.1-2_3");
}

TEST_CASE("K6: tags") {
    const auto withTags = [](const std::string& tags) {
        return WithMarkers(One(R"({"id": "m", "kind": "a", "tags": )" + tags + R"(, "path": [[0,0,0],[1,0,0]]})"));
    };
    CHECK(Rejected(withTags(R"([])")) == "markers[0].tags");
    CHECK(Rejected(withTags(R"({"b": "1", "a": "2"})")) == "markers[0].tags");
    CHECK(Message(withTags(R"({"b": "1", "a": "2"})")).find("increasing") != std::string::npos);
    CHECK(Accepted(withTags(R"({"a": "2", "b": "1"})")).markers[0].tags.size() == 2);
    CHECK(Rejected(withTags(R"({"A": "1"})")) == "markers[0].tags");
    CHECK(Rejected(withTags(R"({"a-b": "1"})")) == "markers[0].tags");
    CHECK(Rejected(withTags(R"({"": "1"})")) == "markers[0].tags");
    CHECK(Rejected(withTags(R"({"k": 1})")) == "markers[0].tags.k");
    CHECK(Rejected(withTags(R"({"k": "\n"})")) == "markers[0].tags.k");
    CHECK(Accepted(withTags(R"({"k": ""})")).markers[0].tags[0].second.empty());
    CHECK(Accepted(withTags(R"({"k": ")" + std::string(256, 'v') + R"("})")).markers[0].tags[0].second.size() == 256);
    CHECK(Rejected(withTags(R"({"k": ")" + std::string(257, 'v') + R"("})")) == "markers[0].tags.k");
    CHECK(Accepted(withTags(R"({")" + std::string(64, 'k') + R"(": "v"})")).markers[0].tags.size() == 1);
    CHECK(Rejected(withTags(R"({")" + std::string(65, 'k') + R"(": "v"})")) == "markers[0].tags");
    std::string many = "{";
    for (int i = 0; i < 33; ++i) {
        if (i > 0) many += ",";
        many += "\"k" + std::string(i < 10 ? "0" : "") + std::to_string(i) + "\": \"v\"";
    }
    CHECK(Rejected(withTags(many + "}")) == "markers[0].tags");
    const auto thirtyTwo = many.substr(0, many.rfind(',')) + "}";
    CHECK(Accepted(withTags(thirtyTwo)).markers[0].tags.size() == 32);
    // More keys than any valid object has are rejected before the document is
    // built (ordered_json inserts keys with a linear search; review S1 of PR
    // #162), at the path the rules would report.
    CHECK(Message(withTags(many + "}")).find("more than 32 keys") != std::string::npos);
    std::string huge = "{";
    for (int i = 0; i < 20000; ++i) huge += (i > 0 ? ",\"k" : "\"k") + std::to_string(i) + "\": \"v\"";
    CHECK(Rejected(withTags(huge + "}")) == "markers[0].tags");
    std::string extraFields;
    for (int i = 0; i < 33; ++i) extraFields += ", \"x" + std::to_string(i) + "\": 0";
    const auto wideMarker = WithMarkers(
        One(R"({"id": "m", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]])" + extraFields + "}"));
    CHECK(Rejected(wideMarker) == "markers[0]");
    CHECK(Message(wideMarker).find("more than 32 keys") != std::string::npos);
    const auto wideBox = WithMarkers(
        One(R"({"id": "m", "kind": "a", "tags": {}, "box": {"min": [0,0,0], "max": [1,1,1])" + extraFields + "}}"));
    CHECK(Rejected(wideBox) == "markers[0].box");
    const auto secondMarker = WithMarkers(R"([{"id": "a", "kind": "a", "tags": {}, "path": [[0,0,0],[1,0,0]]},)"
                                          R"( {"id": "b", "kind": "a", "tags": )" + many + "}, \"path\": [[0,0,0],[1,0,0]]}]");
    CHECK(Rejected(secondMarker) == "markers[1].tags");
    Doc wideDocument;
    wideDocument.extra = extraFields;
    CHECK(Rejected(wideDocument).empty());
    CHECK(Message(wideDocument).find("more than 32 keys") != std::string::npos);
}

TEST_CASE("K7: point and yaw") {
    const auto withPoint = [](const std::string& point, const std::string& yaw) {
        return WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}, "point": )" + point + R"(, "yaw": )" + yaw + "}"));
    };
    CHECK(Rejected(withPoint("[0, 0]", "0")) == "markers[0].point");
    CHECK(Rejected(withPoint("[0, 0.5, 0]", "0")) == "markers[0].point[1]");
    CHECK(Rejected(withPoint("[0, 0, 0]", R"("0")")) == "markers[0].yaw");
    CHECK(Rejected(withPoint("[0, 0, 0]", "1e39")) == "markers[0].yaw");
    CHECK(Rejected(withPoint("[0, 0, 0]", "-1e39")) == "markers[0].yaw");
    CHECK(std::get<MarkerPoint>(Accepted(withPoint("[0, 0, 0]", "-7")).markers[0].shape).yaw == -7.0F);
    CHECK(std::get<MarkerPoint>(Accepted(withPoint("[0, 0, 0]", "3e38")).markers[0].shape).yaw == 3e38F);
}

TEST_CASE("K8: box") {
    const auto withBox = [](const std::string& box) {
        return WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}, "box": )" + box + "}"));
    };
    CHECK(Rejected(withBox(R"({"min": [0, 0, 0], "max": [1, 0, 1]})")) == "markers[0].box");
    CHECK(Rejected(withBox(R"({"min": [0, 0, 2], "max": [1, 1, 1]})")) == "markers[0].box");
    CHECK(Rejected(withBox(R"({"min": [0, 0, 0], "max": [1, 1, 1], "y": 0})")) == "markers[0].box");
    CHECK(Rejected(withBox(R"([0, 0, 0, 1, 1, 1])")) == "markers[0].box");
    CHECK(Rejected(withBox(R"({"min": [0, 0], "max": [1, 1, 1]})")) == "markers[0].box.min");
    CHECK(std::get<MarkerBox>(Accepted(withBox(R"({"min": [-5, -5, -5], "max": [-4, -4, -4]})")).markers[0].shape)
              .minimum.x == -5);
}

TEST_CASE("K9: path") {
    const auto withPath = [](const std::string& path) {
        return WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}, "path": )" + path + "}"));
    };
    CHECK(Rejected(withPath("[[0, 0, 0]]")) == "markers[0].path");
    CHECK(Rejected(withPath("[]")) == "markers[0].path");
    CHECK(Rejected(withPath("[0, 0, 0]")) == "markers[0].path[0]");
    CHECK(Rejected(withPath("[[0, 0, 0], [1, 1]]")) == "markers[0].path[1]");
    std::string points = "[";
    for (std::size_t i = 0; i < MaxPathPoints; ++i) points += (i > 0 ? ",[" : "[") + std::to_string(i) + ",0,0]";
    CHECK(std::get<MarkerPath>(Accepted(withPath(points + "]")).markers[0].shape).points.size() == MaxPathPoints);
    CHECK(Rejected(withPath(points + ",[0,0,0]]")) == "markers[0].path");
    // Repeated points are form-valid; what a path means is the product's.
    CHECK(Accepted(withPath("[[0, 0, 0], [0, 0, 0]]")).markers.size() == 1);
}

TEST_CASE("K10: coordinate range and text size") {
    const auto withPath = [](const std::string& path) {
        return WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}, "path": )" + path + "}"));
    };
    CHECK(Accepted(withPath("[[1048576, -1048576, 0], [0, 0, 0]]")).markers.size() == 1);
    CHECK(Rejected(withPath("[[1048577, 0, 0], [0, 0, 0]]")) == "markers[0].path[0][0]");
    CHECK(Rejected(withPath("[[0, 0, 0], [0, 0, -1048577]]")) == "markers[0].path[1][2]");
    CHECK(Rejected(WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}, "point": [0, 1048577, 0], "yaw": 0})"))) ==
          "markers[0].point[1]");
    CHECK(Rejected(WithMarkers(One(R"({"id": "m", "kind": "a", "tags": {}, "box": {"min": [-1048577, 0, 0], "max": [0, 1, 1]}})"))) ==
          "markers[0].box.min[0]");
    CHECK(Rejected(withPath("[[0, 0, 0], [0, 0, 4294967296]]")) == "markers[0].path[1][2]");
    Doc padded;
    padded.extra = std::string(MaxTextBytes, ' ');
    CHECK(Rejected(padded).empty());
    CHECK(Message(padded).find("4 MiB") != std::string::npos);
    Doc exact;
    exact.extra = std::string(MaxTextBytes - Doc{}.Text().size(), ' ');
    REQUIRE(exact.Text().size() == MaxTextBytes);
    CHECK(Accepted(exact).markers.size() == 1);
}

// ---- gyo.marker_kinds version 1 (rules Q1 to Q5, map-markers.md "Marker kinds") ----
// Judgment T6 of v8-arch-big-map-tool-chunked (docs/tools/plans/v8/voxel-map-tool-chunked.md).

namespace {
struct Kinds final {
    std::string schema = R"("gyo.marker_kinds")";
    std::string version = "1";
    std::string kinds = R"(["a", "b.c", "d-e_f"])";
    std::string extra;

    [[nodiscard]] std::string Text() const {
        return "{\"schema\": " + schema + ", \"version\": " + version + ", \"kinds\": " + kinds + extra + "}";
    }
};

std::vector<std::string> KindsAccepted(const std::string& text) {
    auto parsed = ParseMarkerKinds(text);
    REQUIRE_MESSAGE(parsed, (parsed ? std::string() : parsed.error().message + " at " + parsed.error().detail));
    return std::move(*parsed);
}

std::string KindsRejected(const std::string& text) {
    const auto parsed = ParseMarkerKinds(text);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == MapMarkersErrorCode::InvalidMarkerKinds);
    return parsed.error().detail;
}

std::string KindsMessage(const std::string& text) {
    const auto parsed = ParseMarkerKinds(text);
    REQUIRE_FALSE(parsed);
    return parsed.error().message;
}

Kinds WithKinds(const std::string& kinds) {
    Kinds doc;
    doc.kinds = kinds;
    return doc;
}
} // namespace

TEST_CASE("marker kinds: a valid list is returned in text order") {
    CHECK(KindsAccepted(Kinds{}.Text()) == std::vector<std::string>{"a", "b.c", "d-e_f"});
}

TEST_CASE("Q1: kinds fields and repeated keys") {
    Kinds unknown;
    unknown.extra = R"(, "comment": "x")";
    CHECK(KindsRejected(unknown.Text()).empty());
    CHECK(KindsMessage(unknown.Text()).find("unknown field 'comment'") != std::string::npos);
    Kinds repeated;
    repeated.extra = R"(, "kinds": ["a"])";
    CHECK(KindsMessage(repeated.Text()).find("repeated key 'kinds'") != std::string::npos);
    CHECK(KindsRejected(R"({"schema": "gyo.marker_kinds", "version": 1})").empty());
    CHECK(KindsRejected("[]").empty());
    CHECK(KindsRejected("not json").empty());
}

TEST_CASE("Q2: kinds schema and version") {
    Kinds schema;
    schema.schema = R"("gyo.map_markers")";
    CHECK(KindsRejected(schema.Text()) == "schema");
    Kinds version;
    version.version = "2";
    CHECK(KindsRejected(version.Text()) == "version");
    version.version = "1.0";
    CHECK(KindsRejected(version.Text()) == "version");
}

TEST_CASE("Q3: kinds count and order") {
    CHECK(KindsRejected(WithKinds("[]").Text()) == "kinds");
    CHECK(KindsRejected(WithKinds(R"("a")").Text()) == "kinds");
    CHECK(KindsRejected(WithKinds(R"(["b", "a"])").Text()) == "kinds[1]");
    CHECK(KindsRejected(WithKinds(R"(["a", "a"])").Text()) == "kinds[1]");
    // Byte order: "a-" (0x2D) comes before "a." (0x2E) and "a0".
    CHECK(KindsAccepted(WithKinds(R"(["a-", "a.", "a0"])").Text()).size() == 3);
    std::string many = "[";
    for (std::size_t i = 0; i < MaxKinds; ++i) {
        const auto name = std::string(1, static_cast<char>('a' + i / 26 % 26)) + static_cast<char>('a' + i % 26) +
                          std::to_string(i / 676);
        many += (i > 0 ? ", \"" : "\"") + name + "\"";
    }
    CHECK(KindsAccepted(WithKinds(many + "]").Text()).size() == MaxKinds);
    CHECK(KindsRejected(WithKinds(many + ", \"zzz\"]").Text()) == "kinds");
}

TEST_CASE("Q4: each kind follows rule K5") {
    CHECK(KindsRejected(WithKinds(R"(["a", "B"])").Text()) == "kinds[1]");
    CHECK(KindsRejected(WithKinds(R"([""])").Text()) == "kinds[0]");
    CHECK(KindsRejected(WithKinds(R"(["a b"])").Text()) == "kinds[0]");
    CHECK(KindsRejected(WithKinds(R"([1])").Text()) == "kinds[0]");
    CHECK(KindsAccepted(WithKinds("[\"" + std::string(MaxNameLength, 'a') + "\"]").Text()).size() == 1);
    CHECK(KindsRejected(WithKinds("[\"" + std::string(MaxNameLength + 1, 'a') + "\"]").Text()) == "kinds[0]");
}

TEST_CASE("Q5: kinds text size") {
    Kinds exact;
    exact.extra = std::string(MaxKindsTextBytes - Kinds{}.Text().size(), ' ');
    REQUIRE(exact.Text().size() == MaxKindsTextBytes);
    CHECK(KindsAccepted(exact.Text()).size() == 3);
    Kinds padded;
    padded.extra = std::string(MaxKindsTextBytes, ' ');
    CHECK(KindsRejected(padded.Text()).empty());
    CHECK(KindsMessage(padded.Text()).find("64 KiB") != std::string::npos);
}

TEST_CASE("marker kinds: unknown kinds are found in markers order") {
    const auto document = Accepted(WithMarkers(
        R"([{"id": "m1", "kind": "c", "tags": {}, "point": [0, 0, 0], "yaw": 0},)"
        R"( {"id": "m2", "kind": "a", "tags": {}, "point": [0, 0, 0], "yaw": 0},)"
        R"( {"id": "m3", "kind": "z", "tags": {}, "point": [0, 0, 0], "yaw": 0}])"));
    const std::vector<std::string> kinds{"a", "b"};
    CHECK(UnknownMarkerKinds(document, kinds) == std::vector<std::string>{"markers[0].kind", "markers[2].kind"});
    const std::vector<std::string> all{"a", "c", "z"};
    CHECK(UnknownMarkerKinds(document, all).empty());
    CHECK(UnknownMarkerKinds(Accepted(WithMarkers("[]")), kinds).empty());
}
