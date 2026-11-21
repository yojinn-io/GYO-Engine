// The JSON lines format and the console's text form (logging declaration 5.12;
// Design Invariant LG-5). Every JSON line is read back with GYO::Json's Parse.
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

#include "engine/json/Document.hpp"
#include "engine/logging/ConsoleSink.hpp"
#include "engine/logging/JsonLines.hpp"

namespace Logging = GYO::Engine::Logging;
namespace Json = GYO::Engine::Json;
using Logging::Field;
using Logging::Level;
using Logging::Record;

namespace {

// 2025-11-20T10:00:11.007 at +09:00.
constexpr Logging::WallTime kWall{.unixMs = 1'763'600'411'007, .utcOffsetSeconds = 9 * 3600};

std::string Line(const Record& record) {
    std::string out;
    Logging::AppendJsonLine(out, record);
    return out;
}

} // namespace

TEST_CASE("format: golden: one record with every kind of field, a world and +0900") {
    const Field fields[] = {{"tick", 1207u}, {"delta", -3}, {"speed", 2.5}, {"ok", true}, {"peer", "client \"3\""}};
    const Record record{                        .level = Level::Warning,
                        .category = "world_time",
                        .text = "discarded 59 steps",
                        .wall = kWall,
                        .monoNs = 123456789,
                        .world = "Simulation",
                        .fields = fields};
    CHECK(Line(record) ==
          "{\"schema\":1,\"kind\":\"log\",\"level\":\"warning\",\"category\":\"world_time\","
          "\"wall\":\"2025-11-20T10:00:11.007+0900\",\"mono_ns\":123456789,\"world\":\"Simulation\","
          "\"text\":\"discarded 59 steps\",\"fields\":{\"tick\":1207,\"delta\":-3,\"speed\":2.5,\"ok\":true,"
          "\"peer\":\"client \\\"3\\\"\"}}\n");
}

TEST_CASE("format: golden: a fatal record carries world and world_interrupted; empty fields are {}") {
    const Record record{.level = Level::Fatal,
                        .category = "assert",
                        .text = "broken",
                        .wall = {.unixMs = 0, .utcOffsetSeconds = 0},
                        .monoNs = 7,
                        .world = "Simulation",
                        .worldInterrupted = true};
    CHECK(Line(record) ==
          "{\"schema\":1,\"kind\":\"log\",\"level\":\"fatal\",\"category\":\"assert\","
          "\"wall\":\"1970-01-01T00:00:00.000+0000\",\"mono_ns\":7,\"world\":\"Simulation\","
          "\"world_interrupted\":true,\"text\":\"broken\",\"fields\":{}}\n");
}

TEST_CASE("format: the wall offset is written as %z writes it: negative, half hours") {
    Record record{.category = "c", .wall = {.unixMs = 1'763'600'411'007, .utcOffsetSeconds = -5 * 3600}};
    CHECK(Line(record).find("\"wall\":\"2025-11-19T20:00:11.007-0500\"") != std::string::npos);
    record.wall.utcOffsetSeconds = 5 * 3600 + 1800;
    CHECK(Line(record).find("\"wall\":\"2025-11-20T06:30:11.007+0530\"") != std::string::npos);
    record.wall = {.unixMs = 951'782'400'000 + 999, .utcOffsetSeconds = 0}; // 2000-02-29, a leap day
    CHECK(Line(record).find("\"wall\":\"2000-02-29T00:00:00.999+0000\"") != std::string::npos);
}

TEST_CASE("format: NaN and both infinities become the agreed strings; -0.0 stays a number") {
    const Field fields[] = {{"nan", std::numeric_limits<double>::quiet_NaN()},
                            {"pos", std::numeric_limits<double>::infinity()},
                            {"neg", -std::numeric_limits<double>::infinity()},
                            {"zero", -0.0}};
    const Record record{.category = "c", .wall = kWall, .fields = fields};
    const std::string line = Line(record);
    CHECK(line.find("\"fields\":{\"nan\":\"NaN\",\"pos\":\"Infinity\",\"neg\":\"-Infinity\",\"zero\":-0.0}") !=
          std::string::npos);
    auto document = Json::Parse(line);
    REQUIRE(document);
    const auto fieldsObject = document.value().Root().AsObject().value().Find("fields").value().AsObject().value();
    const double zero = fieldsObject.Find("zero").value().AsDouble().value();
    CHECK(zero == 0.0);
    CHECK(std::signbit(zero));
}

TEST_CASE("format: control characters and non-ASCII UTF-8 are written so that Parse reads them back") {
    const std::string text = "line\nbreak\ttab \x01 \xE6\x97\xA5\xE6\x9C\xAC";
    const Field fields[] = {{"s", std::string_view(text)}};
    const Record record{.category = "c", .text = text, .wall = kWall, .fields = fields};
    auto document = Json::Parse(Line(record));
    REQUIRE(document);
    const auto root = document.value().Root().AsObject().value();
    CHECK(root.Find("text").value().AsString().value() == text);
    CHECK(root.Find("fields").value().AsObject().value().Find("s").value().AsString().value() == text);
}

TEST_CASE("format: a record JSON cannot represent becomes an Error record of category logging, same world") {
    const std::string bad = "bad \xFF byte";
    const Field fields[] = {{"s", std::string_view(bad)}};
    const Record record{.level = Level::Info,
                        .category = "net",
                        .text = "fine",
                        .wall = kWall,
                        .monoNs = 5,
                        .world = "Simulation",
                        .fields = fields};
    std::string out = "before\n";
    CHECK_FALSE(Logging::AppendJsonLine(out, record));
    REQUIRE(out.starts_with("before\n"));
    const std::string line = out.substr(7);
    auto document = Json::Parse(line);
    REQUIRE(document);
    const auto root = document.value().Root().AsObject().value();
    CHECK(root.Find("kind").value().AsString().value() == "log");
    CHECK(root.Find("level").value().AsString().value() == "error");
    CHECK(root.Find("category").value().AsString().value() == "logging");
    CHECK(root.Find("world").value().AsString().value() == "Simulation");
    CHECK(root.Find("text").value().AsString().value().find("NotRepresentable") != std::string_view::npos);
    CHECK(root.Find("fields").value().AsObject().value().Size() == 0);
    CHECK(line.find("\xFF") == std::string::npos);
}

TEST_CASE("format: an invalid world name too still gives a line that reads back") {
    const Record record{.category = "c", .text = "t", .wall = kWall, .world = "bad\xFF"};
    std::string out;
    CHECK_FALSE(Logging::AppendJsonLine(out, record));
    REQUIRE(Json::Parse(out));
    CHECK(out.find("\"category\":\"logging\"") != std::string::npos);
}

TEST_CASE("format: golden: the end line") {
    std::string out;
    Logging::AppendJsonEndLine(out, 99, {.written = 10, .droppedFull = 2, .droppedClosed = 1, .fatalDirect = 1});
    CHECK(out == "{\"schema\":1,\"kind\":\"end\",\"mono_ns\":99,\"written\":10,\"dropped_full\":2,"
                 "\"dropped_closed\":1,\"fatal_direct\":1}\n");
}

TEST_CASE("format: each line appends to what out already holds") {
    std::string out;
    Logging::AppendJsonLine(out, Record{.category = "a", .wall = kWall});
    Logging::AppendJsonLine(out, Record{.category = "b", .wall = kWall});
    CHECK(std::count(out.begin(), out.end(), '\n') == 2);
    const auto split = out.find('\n');
    CHECK(Json::Parse(std::string_view(out).substr(0, split)));
    CHECK(Json::Parse(std::string_view(out).substr(split + 1)));
}

TEST_CASE("console: the text form goes to its stream: [category] level: text key=value") {
    std::FILE* stream = std::tmpfile();
    REQUIRE(stream != nullptr);
    {
        Logging::ConsoleSink console(stream);
        const Field fields[] = {{"bytes", 512}, {"peer", "client 3"}, {"rate", std::numeric_limits<double>::infinity()}};
        console.Write(Record{.level = Level::Warning, .category = "net", .text = "sent", .world = "Sim", .fields = fields});
        CHECK(console.Failed() == 0);
        CHECK(console.WriteFatal(Record{}, "{}\n", "[world] fatal: broken\n"));
    }
    std::rewind(stream);
    char buffer[512] = {};
    const std::size_t size = std::fread(buffer, 1, sizeof buffer - 1, stream);
    std::fclose(stream);
    CHECK(std::string_view(buffer, size) ==
          "[net] warning: sent world=Sim bytes=512 peer=\"client 3\" rate=Infinity\n[world] fatal: broken\n");
}

TEST_CASE("format: every level has its agreed name in the envelope") {
    const std::pair<Level, std::string_view> names[] = {{Level::Trace, "trace"}, {Level::Debug, "debug"}, {Level::Info, "info"},
                                                        {Level::Warning, "warning"}, {Level::Error, "error"}, {Level::Fatal, "fatal"}};
    for (const auto& [level, name] : names) {
        const Record record{.level = level, .category = "c", .wall = kWall};
        CHECK_MESSAGE(Line(record).find("\"level\":\"" + std::string(name) + "\"") != std::string::npos, "LG-5: level " << name);
    }
}

TEST_CASE("console: a write that fails is counted in Failed") {
    const auto path = std::filesystem::temp_directory_path() / "gyo-console-readonly.txt";
    { std::ofstream(path) << "x"; }
    std::FILE* stream = std::fopen(path.string().c_str(), "r");
    REQUIRE(stream != nullptr);
    {
        Logging::ConsoleSink console(stream);
        console.Write(Record{.level = Level::Warning, .category = "net", .text = "cannot be written"});
        CHECK_MESSAGE(console.Failed() == 1, "LG-11: a failed write is counted");
    }
    std::fclose(stream);
    std::filesystem::remove(path);
}
