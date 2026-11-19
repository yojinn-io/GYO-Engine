// The JSON Module's scenarios (declaration 8.4): the three named uses it is
// built for, each run through the public interface only.
//   F1  structured log and trace records: one JSON line per record, written on
//       the producer's thread into a reused string, read back by an analysis tool.
//   F2  a content catalog: walked field by field; broken copies report where.
//   F6  BigMap regions: several threads each parse their own region.
// docs/architecture/target/v2/json-module-declaration.zh-Hant.md
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "engine/json/Document.hpp"
#include "engine/json/Writer.hpp"

using namespace std::string_view_literals;
using GYO::Engine::Json::Document;
using GYO::Engine::Json::JsonErrorCode;
using GYO::Engine::Json::Kind;
using GYO::Engine::Json::Member;
using GYO::Engine::Json::Parse;
using GYO::Engine::Json::Value;
using GYO::Engine::Json::WriteValue;
using GYO::Engine::Json::Writer;

namespace {

struct TraceRecord {
    std::uint64_t tick;
    std::int64_t player;
    std::uint64_t queued;
    double speed;
    std::string source;
    std::vector<std::int64_t> pending;
};

TraceRecord MakeRecord(const std::uint64_t i) {
    TraceRecord record{1200 + i,
                       static_cast<std::int64_t>(i % 7) - 3,
                       i % 5,
                       0.1 * static_cast<double>(i) + 1e-9,
                       "client \"" + std::to_string(i % 3) + "\"\tinput",
                       {}};
    for (std::uint64_t k = 0; k < i % 4; ++k) record.pending.push_back(static_cast<std::int64_t>(i * 10 + k));
    return record;
}

void WriteRecord(Writer& writer, const TraceRecord& record) {
    writer.BeginObject();
    writer.Key("tick");
    writer.Uint(record.tick);
    writer.Key("player");
    writer.Int(record.player);
    writer.Key("queued");
    writer.Uint(record.queued);
    writer.Key("speed");
    writer.Double(record.speed);
    writer.Key("source");
    writer.String(record.source);
    writer.Key("pending");
    writer.BeginArray();
    for (const std::int64_t value : record.pending) writer.Int(value);
    writer.EndArray();
    writer.Key("meta");
    writer.BeginObject();
    writer.Key("schema");
    writer.Uint(1);
    writer.EndObject();
    writer.EndObject();
}

} // namespace

TEST_CASE("Scenario F1: trace records written one per line, read back field by field, rewritten byte for byte") {
    constexpr std::uint64_t kRecords = 1000;
    std::string log;
    std::vector<std::size_t> lineStarts;
    std::string line;  // reused for every record, like a producer's buffer
    for (std::uint64_t i = 0; i < kRecords; ++i) {
        line.clear();
        Writer writer(line);
        WriteRecord(writer, MakeRecord(i));
        REQUIRE(writer.Finish().has_value());
        lineStarts.push_back(log.size());
        log += line;
        log += '\n';
    }

    std::size_t lines = 0;
    std::size_t fieldMismatches = 0;
    std::size_t rewriteMismatches = 0;
    std::size_t start = 0;
    while (start < log.size()) {
        const std::size_t end = log.find('\n', start);
        const std::string_view text(log.data() + start, end - start);
        const TraceRecord expected = MakeRecord(lines);
        auto document = Parse(text);
        REQUIRE(document.has_value());
        auto object = document->Root().AsObject();
        REQUIRE(object.has_value());

        std::vector<std::int64_t> pending;
        for (const Value value : object->Find("pending").value().AsArray().value()) {
            pending.push_back(value.AsInt64().value());
        }
        const bool same = object->Find("tick").value().AsUint64().value() == expected.tick &&
                          object->Find("player").value().AsInt64().value() == expected.player &&
                          object->Find("queued").value().AsUint64().value() == expected.queued &&
                          std::bit_cast<std::uint64_t>(object->Find("speed").value().AsDouble().value()) ==
                              std::bit_cast<std::uint64_t>(expected.speed) &&
                          object->Find("source").value().AsString().value() == expected.source &&
                          pending == expected.pending;
        if (!same) ++fieldMismatches;

        std::vector<std::string_view> keys;
        for (const Member& member : *object) keys.push_back(member.key);
        if (keys != std::vector<std::string_view>{"tick", "player", "queued", "speed", "source", "pending", "meta"}) {
            ++fieldMismatches;
        }

        std::string rewritten;
        Writer writer(rewritten);
        WriteValue(writer, document->Root());
        REQUIRE(writer.Finish().has_value());
        if (rewritten != text) ++rewriteMismatches;

        ++lines;
        start = end + 1;
    }
    CHECK(lines == kRecords);
    CHECK(fieldMismatches == 0);
    CHECK(rewriteMismatches == 0);
}

namespace {

constexpr std::string_view kCatalog = R"({
  "version": 2,
  "assets": [
    {"id": "wall_brick", "kind": "texture", "size": [256, 256], "scale": 0.5},
    {"id": "door_oak", "kind": "model", "lods": [1.0, 0.25], "tags": ["door", "wood"]},
    {"id": "step_sound", "kind": "audio", "gain": -3.5, "loop": false}
  ]
})";

} // namespace

TEST_CASE("Scenario F2: a content catalog is walked field by field") {
    const Document document = [] {
        auto parsed = Parse(kCatalog);
        REQUIRE(parsed.has_value());
        return std::move(*parsed);
    }();
    auto root = document.Root().AsObject();
    REQUIRE(root.has_value());
    CHECK(root->Find("version").value().AsUint64().value() == 2u);
    std::vector<std::string_view> ids;
    std::vector<std::string_view> kinds;
    double scale = 0.0;
    double gain = 0.0;
    std::size_t tags = 0;
    for (const Value asset : root->Find("assets").value().AsArray().value()) {
        auto fields = asset.AsObject();
        REQUIRE(fields.has_value());
        ids.push_back(fields->Find("id").value().AsString().value());
        kinds.push_back(fields->Find("kind").value().AsString().value());
        if (auto value = fields->Find("scale")) scale = value->AsDouble().value();
        if (auto value = fields->Find("gain")) gain = value->AsDouble().value();
        if (auto value = fields->Find("tags")) tags = value->AsArray().value().Size();
    }
    CHECK(ids == std::vector<std::string_view>{"wall_brick", "door_oak", "step_sound"});
    CHECK(kinds == std::vector<std::string_view>{"texture", "model", "audio"});
    CHECK(scale == 0.5);
    CHECK(gain == -3.5);
    CHECK(tags == 2);
}

TEST_CASE("Scenario F2: broken copies of the catalog say what and where") {
    std::string repeated(kCatalog);
    repeated.replace(repeated.find("\"scale\""), 7, "\"kind\"");
    auto first = Parse(repeated);
    REQUIRE_FALSE(first.has_value());
    CHECK(first.error().code == JsonErrorCode::RepeatedKey);
    CHECK(first.error().detail == "line 4, column 66; /assets/0/kind");

    std::string tooLarge(kCatalog);
    tooLarge.replace(tooLarge.find("-3.5"), 4, "-3e999");
    auto second = Parse(tooLarge);
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code == JsonErrorCode::NumberOutOfRange);
    CHECK(second.error().detail == "line 6, column 51; /assets/2/gain");

    // A field of the wrong kind parses; reading it reports.
    std::string wrongKind(kCatalog);
    wrongKind.replace(wrongKind.find("[256, 256]"), 10, "\"256x256\"");
    auto third = Parse(wrongKind);
    REQUIRE(third.has_value());
    auto assets = third->Root().AsObject().value().Find("assets").value().AsArray().value();
    auto size = (*assets.begin()).AsObject().value().Find("size").value().AsArray();
    REQUIRE_FALSE(size.has_value());
    CHECK(size.error().code == JsonErrorCode::WrongKind);
    CHECK(size.error().detail == "expected Array, found String");
}

namespace {

// A synthetic region: mostly numbers in wide arrays, like a chunk of voxel data.
std::string MakeRegion(const int seed) {
    std::string text = "{\"region\":" + std::to_string(seed) + ",\"cells\":[";
    for (int i = 0; i < 20000; ++i) {
        if (i != 0) text += ',';
        text += "[" + std::to_string((i * 7 + seed) % 64) + "," + std::to_string((i * 13 + seed) % 64) + "," +
                std::to_string(seed) + "." + std::to_string(i % 100) + "]";
    }
    text += "]}";
    return text;
}

double RegionSum(const std::string& text) {
    auto document = Parse(text);
    if (!document) return -1.0;
    double sum = 0.0;
    for (const Value cell : document->Root().AsObject().value().Find("cells").value().AsArray().value()) {
        for (const Value value : cell.AsArray().value()) sum += value.AsDouble().value();
    }
    return sum;
}

} // namespace

TEST_CASE("Scenario F6: several threads parse their own regions at once, as one thread would") {
    constexpr int kRegions = 8;
    std::vector<std::string> regions;
    for (int seed = 0; seed < kRegions; ++seed) regions.push_back(MakeRegion(seed));
    std::vector<double> alone;
    for (const std::string& region : regions) alone.push_back(RegionSum(region));
    std::vector<double> together(kRegions, 0.0);
    std::vector<std::thread> threads;
    for (int i = 0; i < kRegions; ++i) {
        threads.emplace_back([&, i] { together[i] = RegionSum(regions[i]); });
    }
    for (std::thread& thread : threads) thread.join();
    for (int i = 0; i < kRegions; ++i) {
        CHECK(alone[i] > 0.0);
        CHECK(std::bit_cast<std::uint64_t>(together[i]) == std::bit_cast<std::uint64_t>(alone[i]));
    }
}
