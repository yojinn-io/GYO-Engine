// Two outputs appending to one file at once (logging declaration 5.6 step 3,
// section 12, RC-L5): the writer thread's output and the fatal output are two
// handles of one file, both opened to append. That their writes never
// interleave inside a line is knowledge (POSIX O_APPEND, a Windows
// append-only handle), not something engine/os promises; this test checks it
// on every L1 platform with 100,000 lines from each, and every line must read
// back with Parse. If it fails on any platform the batch stops (declaration
// section 15) and the fatal path moves to a file of its own.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include "engine/json/Document.hpp"
#include "engine/logging/JsonLines.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"

namespace Logging = GYO::Engine::Logging;
namespace Json = GYO::Engine::Json;

TEST_CASE("append: the writer thread and the fatal output, 100,000 lines each into one file, never interleave") {
    constexpr int kLines = 100'000;
    // In the build directory, never the working directory (a run from the repository root left files there).
    const auto dir = std::filesystem::path(GYO_LOGGING_TEST_BINARY_DIR) / "logging-test-files";
    std::filesystem::create_directories(dir);
    const auto path = dir / "append.jsonl";
    std::filesystem::remove(path);

    auto opened = Logging::JsonLinesFileSink::Open({.path = path.string(), .capacity = kLines + 1});
    REQUIRE(opened);
    auto sink = opened.value();

    // Lines of about the size of a trace record (master: about 420 bytes).
    const std::string padding(360, 'q');
    std::thread queued([&] {
        for (int i = 0; i < kLines; ++i) {
            const Logging::Field fields[] = {{"i", i}, {"pad", std::string_view(padding)}};
            sink->Write(Logging::Record{.level = Logging::Level::Info, .category = "queued", .monoNs = i,
                                        .fields = fields});
        }
    });
    std::thread fatal([&] {
        std::string json;
        for (int i = 0; i < kLines; ++i) {
            json.clear();
            const Logging::Field fields[] = {{"i", i}, {"pad", std::string_view(padding)}};
            Logging::AppendJsonLine(json, Logging::Record{.level = Logging::Level::Fatal, .category = "fatal",
                                                          .monoNs = i, .fields = fields});
            sink->WriteFatal(Logging::Record{}, json, "unused\n");
        }
    });
    queued.join();
    fatal.join();
    sink->Close();
    const auto counts = sink->Counts();
    REQUIRE(counts.written == kLines);
    REQUIRE(counts.fatalDirect == kLines);

    std::ifstream in(path, std::ios::binary);
    int queuedLines = 0;
    int fatalLines = 0;
    int endLines = 0;
    int unreadable = 0;
    for (std::string line; std::getline(in, line);) {
        auto document = Json::Parse(line);
        if (!document) {
            ++unreadable;
            continue;
        }
        const auto root = document.value().Root().AsObject().value();
        const auto category = root.Find("category");
        if (!category) {
            ++endLines;
        } else if (category->AsString().value() == "queued") {
            ++queuedLines;
        } else {
            ++fatalLines;
        }
    }
    CHECK(unreadable == 0);
    CHECK(queuedLines == kLines);
    CHECK(fatalLines == kLines);
    CHECK(endLines == 1);
}
