// The trace Module (docs/architecture/target/v2/trace-declaration.zh-Hant.md, D2 to D5; Design Invariants
// TR-3 to TR-7): the receiver of the Base facade keeps one span stack per thread, times a span only when some
// sink takes its record, writes one logging record per span and a path record where an error is produced.
#include <doctest/doctest.h>

// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/base/Trace.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/trace/Trace.hpp"

namespace Base = GYO::Engine::Base;
namespace Logging = GYO::Engine::Logging;
namespace Trace = GYO::Engine::Trace;
using Logging::Level;

namespace {

std::atomic<std::int64_t> now{100};
std::atomic<std::uint64_t> reads{0};
std::int64_t TestNs() noexcept {
    reads.fetch_add(1);
    return now.load();
}

// A Logger with one memory sink taking filter, and Trace with the test clock.
struct Setup {
    std::shared_ptr<Logging::MemorySink> memory = std::make_shared<Logging::MemorySink>();
    std::optional<Logging::LoggerHandle> logger;
    std::optional<Trace::TraceHandle> trace;

    explicit Setup(Logging::SinkFilter filter = {.include = {"span."}, .minimum = Level::Trace}, bool withLogger = true) {
        now = 100;
        reads = 0;
        if (withLogger) {
            Logging::LoggerOptions options;
            options.sinks.push_back({.name = "memory", .sink = memory, .filter = std::move(filter)});
            options.takeAssertions = false;
            options.takeTerminate = false;
            auto installed = Logging::Install(std::move(options));
            REQUIRE(installed);
            logger.emplace(std::move(installed.value()));
        }
        trace.emplace(Trace::Install({.monoNs = &TestNs}));
    }

    [[nodiscard]] std::vector<Logging::StoredRecord> Category(std::string_view category) const {
        std::vector<Logging::StoredRecord> out;
        for (auto& record : memory->Records()) {
            if (record.category == category) out.push_back(std::move(record));
        }
        return out;
    }
};

std::string Text(const Logging::StoredRecord& record, std::string_view key) {
    const auto* field = record.Find(key);
    return field != nullptr ? field->text : std::string("<none>");
}
std::uint64_t Uint(const Logging::StoredRecord& record, std::string_view key) {
    const auto* field = record.Find(key);
    return field != nullptr ? field->uintValue : ~0ull;
}
std::int64_t Int(const Logging::StoredRecord& record, std::string_view key) {
    const auto* field = record.Find(key);
    return field != nullptr ? field->intValue : -1;
}
bool Flag(const Logging::StoredRecord& record, std::string_view key) {
    const auto* field = record.Find(key);
    return field != nullptr && field->boolValue;
}

std::string Path() {
    char buffer[256];
    return std::string(buffer, Base::TraceCurrentPath(buffer));
}

} // namespace

TEST_CASE("trace: nested spans become one record each, inner first, with ids, parent, begin and duration") {
    Setup setup;
    {
        Base::TraceSpan outer;
        outer.Begin("orchestration", "RunFrame");
        now = 150;
        {
            Base::TraceSpan inner;
            inner.Begin("kernel", "Main", "Match");
            now = 170;
        }
        now = 200;
    }
    const auto records = setup.memory->Records();
    REQUIRE(records.size() == 2);
    const auto& inner = records[0];
    const auto& outer = records[1];
    CHECK(inner.category == "span.kernel");
    CHECK(inner.text == "Main");
    CHECK(inner.world == "Match");
    CHECK(inner.level == Level::Trace);
    CHECK(outer.category == "span.orchestration");
    CHECK(outer.text == "RunFrame");
    CHECK_MESSAGE(Uint(inner, "parent_id") == Uint(outer, "span_id"), "TR-4: the parent is the span below on this thread");
    CHECK(Uint(outer, "parent_id") == 0);
    CHECK(Uint(inner, "span_id") != Uint(outer, "span_id"));
    CHECK(Int(outer, "begin_ns") == 100);
    CHECK_MESSAGE(Int(outer, "duration_ns") == 100, "TR-5: duration_ns is the time since begin");
    CHECK(Int(inner, "begin_ns") == 150);
    CHECK(Int(inner, "duration_ns") == 20);
}

TEST_CASE("trace: an error marked where it is produced is on the span and gives one path record") {
    Setup setup;
    {
        Base::TraceSpan frame;
        frame.Begin("orchestration", "RunFrame");
        Base::TraceSpan schedule;
        schedule.Begin("kernel", "Main", "Match");
        Base::TraceSpan system;
        system.Begin("kernel", "MoveRacer", "Match");
        system.MarkError("the racer fell");
    }
    const auto spans = setup.Category("span.kernel");
    REQUIRE(spans.size() == 2);
    CHECK(Text(spans[0], "error") == "the racer fell");
    const auto paths = setup.Category("span.path");
    REQUIRE_MESSAGE(paths.size() == 1, "D4 (i): one path record where the error is produced");
    CHECK(paths[0].text == "the racer fell");
    CHECK(Text(paths[0], "path") == "orchestration:RunFrame > kernel:Main@Match > kernel:MoveRacer@Match");
    CHECK(Uint(paths[0], "span_id") == Uint(spans[0], "span_id"));
}

TEST_CASE("trace: an error that only passes through marks the span as failed and writes no path") {
    Setup setup;
    {
        Base::TraceSpan schedule;
        schedule.Begin("kernel", "Main", "Match");
        schedule.MarkError("left by a nested run", false);
    }
    const auto spans = setup.Category("span.kernel");
    REQUIRE(spans.size() == 1);
    CHECK(Text(spans[0], "error") == "left by a nested run");
    CHECK_MESSAGE(setup.Category("span.path").empty(), "D4 (i): passing through writes no path");
}

TEST_CASE("trace: TraceMarkCurrent marks the span on top of this thread's stack") {
    Setup setup;
    {
        Base::TraceSpan frame;
        frame.Begin("orchestration", "RunFrame");
        Base::TraceSpan schedule;
        schedule.Begin("kernel", "Main", "Match");
        {
            Base::TraceSpan system;
            system.Begin("kernel", "Read", "Match");
        }
        Base::TraceMarkCurrent("a sync point failed", true);
    }
    const auto spans = setup.Category("span.kernel");
    REQUIRE(spans.size() == 2);
    CHECK(Text(spans[0], "error") == "<none>");
    CHECK_MESSAGE(Text(spans[1], "error") == "a sync point failed", "D2: the top of the stack is Main once Read ended");
    REQUIRE(setup.Category("span.path").size() == 1);
    CHECK(Text(setup.Category("span.path")[0], "path") == "orchestration:RunFrame > kernel:Main@Match");
    const auto frames = setup.Category("span.orchestration");
    REQUIRE(frames.size() == 1);
    CHECK_MESSAGE(Text(frames[0], "error") == "<none>", "D2: only the top of the stack is marked");
}

TEST_CASE("trace: spans unwound by an exception say so; the deepest one writes the path") {
    Setup setup;
    try {
        Base::TraceSpan frame;
        frame.Begin("orchestration", "RunFrame");
        Base::TraceSpan system;
        system.Begin("kernel", "MoveRacer", "Match");
        throw std::runtime_error("MoveRacer threw");
    } catch (const std::runtime_error&) {
    }
    const auto records = setup.memory->Records();
    std::vector<Logging::StoredRecord> spans;
    for (const auto& record : records) {
        if (record.category != "span.path") spans.push_back(record);
    }
    REQUIRE(spans.size() == 2);
    CHECK(Flag(spans[0], "exception"));
    CHECK(Flag(spans[1], "exception"));
    const auto paths = setup.Category("span.path");
    REQUIRE_MESSAGE(paths.size() == 1, "D4 (i): the first span the exception unwinds is the origin");
    CHECK(Text(paths[0], "path") == "orchestration:RunFrame > kernel:MoveRacer@Match");
    CHECK(paths[0].text == "an exception left the span");
}

TEST_CASE("trace: with only span.path taken, no span is timed and only the path record is written") {
    Setup setup({.include = {"span.path"}, .minimum = Level::Trace});
    {
        Base::TraceSpan schedule;
        schedule.Begin("kernel", "Main", "Match");
        Base::TraceSpan system;
        system.Begin("kernel", "MoveRacer", "Match");
        system.MarkError("the racer fell");
    }
    CHECK_MESSAGE(reads.load() == 0, "TR-6: a span no sink takes reads no clock");
    CHECK(setup.Category("span.kernel").empty());
    REQUIRE(setup.Category("span.path").size() == 1);
}

TEST_CASE("trace: with no Logger installed nothing is written and no clock is read; the path is still kept") {
    Setup setup({}, false);
    std::string path;
    {
        Base::TraceSpan schedule;
        schedule.Begin("kernel", "Main", "Match");
        path = Path();
        schedule.MarkError("no one writes this");
    }
    CHECK_MESSAGE(reads.load() == 0, "D5: with no Logger no span is timed");
    CHECK_MESSAGE(path == "kernel:Main@Match", "D5: the stack is kept for the fatal path");
}

TEST_CASE("trace: a span begun under an earlier Install ends without effect after Trace is installed again") {
    Setup setup;
    std::optional<Base::TraceSpan> stale;
    stale.emplace();
    stale->Begin("kernel", "Old", "");
    setup.trace.reset();
    setup.trace.emplace(Trace::Install({.monoNs = &TestNs}));
    stale.reset();
    {
        Base::TraceSpan fresh;
        fresh.Begin("kernel", "New", "");
        CHECK_MESSAGE(Path() == "kernel:New", "TR-4: the old span left nothing on the new stack");
    }
    const auto spans = setup.Category("span.kernel");
    REQUIRE_MESSAGE(spans.size() == 1, "TR-4: the span of the earlier Install wrote nothing");
    CHECK(spans[0].text == "New");
}

TEST_CASE("trace: a span ended while another is on top is still ended and marked unbalanced") {
    Setup setup;
    auto outer = std::make_unique<Base::TraceSpan>();
    outer->Begin("game", "outer");
    auto inner = std::make_unique<Base::TraceSpan>();
    inner->Begin("game", "inner");
    outer.reset();
    CHECK_MESSAGE(Path() == "game:inner", "TR-4: the ended span left the stack");
    inner.reset();
    const auto spans = setup.Category("span.game");
    REQUIRE(spans.size() == 2);
    CHECK(spans[0].text == "outer");
    CHECK_MESSAGE(Flag(spans[0], "unbalanced"), "TR-4: an end not on top is marked unbalanced");
    CHECK_FALSE(Flag(spans[1], "unbalanced"));
}

TEST_CASE("trace: the path keeps the deepest end when it does not fit") {
    Setup setup;
    Base::TraceSpan a;
    a.Begin("game", "first-span-with-a-long-name");
    Base::TraceSpan b;
    b.Begin("game", "second-span-with-a-long-name");
    Base::TraceSpan c;
    c.Begin("game", "deepest");
    char buffer[32];
    const std::string_view path(buffer, Base::TraceCurrentPath(buffer));
    CHECK(path.size() <= sizeof buffer);
    CHECK_MESSAGE(path.ends_with("game:deepest"), "D2: the deepest end is kept");
    CHECK_MESSAGE(path.starts_with("..."), "D2: a cut path starts with ...");
}

TEST_CASE("trace: a second Install while one is installed is a Programmer Error") {
    Setup setup;
    GYO_CHECK_ASSERTS(Trace::Install({}));
}

TEST_CASE("trace: closing the handle uninstalls the receiver") {
    {
        Setup setup;
        const Base::TraceReceiver* installed = Base::SetTraceReceiver(nullptr);
        CHECK(installed != nullptr);
        Base::SetTraceReceiver(installed);
    }
    CHECK_MESSAGE(Base::SetTraceReceiver(nullptr) == nullptr, "TR-7: closing the handle puts the earlier receiver back");
}

TEST_CASE("trace: each thread has its own stack; a span on another thread has no parent from this one") {
    Setup setup;
    Base::TraceSpan outer;
    outer.Begin("game", "main-thread");
    std::string otherPath;
    std::thread other([&] {
        Base::TraceSpan span;
        span.Begin("game", "worker");
        otherPath = Path();
    });
    other.join();
    CHECK_MESSAGE(otherPath == "game:worker", "D3: the other thread's stack holds only its own span");
    const auto spans = setup.Category("span.game");
    REQUIRE(spans.size() == 1);
    CHECK_MESSAGE(Uint(spans[0], "parent_id") == 0, "D3: no parent across threads");
}

TEST_CASE("trace: a span begun after an exception or an error at the same depth starts clean") {
    Setup setup;
    try {
        Base::TraceSpan frame;
        frame.Begin("game", "first");
        Base::TraceSpan inner;
        inner.Begin("inner", "first-inner");
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    // Right after: "third" reuses the slot of "first" (depth 0) with no child of its own, so a kept "a child was
    // unwound" would leave its exception without a path (a child beginning would clear it and hide that).
    try {
        Base::TraceSpan frame;
        frame.Begin("game", "third");
        throw std::runtime_error("again");
    } catch (const std::runtime_error&) {
    }
    {
        // The first exception left an error on the slot of "first-inner" (depth 1); a span there must not keep it;
        // "third" left the exception flag on depth 0.
        Base::TraceSpan frame;
        frame.Begin("game", "second");
        Base::TraceSpan reused;
        reused.Begin("game", "reused");
    }
    std::optional<Logging::StoredRecord> second;
    std::optional<Logging::StoredRecord> reused;
    for (const auto& record : setup.Category("span.game")) {
        if (record.text == "second") second = record;
        if (record.text == "reused") reused = record;
    }
    REQUIRE(second.has_value());
    REQUIRE(reused.has_value());
    CHECK_MESSAGE(!Flag(*second, "exception"), "TR-4: the reused slot does not keep the old exception flag");
    CHECK_MESSAGE(Text(*reused, "error") == "<none>", "TR-4: the reused slot does not keep the old error");
    CHECK_MESSAGE(setup.Category("span.path").size() == 2, "D4 (i): the second exception writes its path too");
}

TEST_CASE("trace: past 64 levels spans are counted but not stored; the stack comes back") {
    Setup setup;
    {
        std::vector<std::unique_ptr<Base::TraceSpan>> spans;
        for (int i = 0; i < 70; ++i) {
            spans.push_back(std::make_unique<Base::TraceSpan>());
            spans.back()->Begin("deep", "level");
        }
        while (!spans.empty()) spans.pop_back();
    }
    CHECK_MESSAGE(setup.Category("span.deep").size() == 64, "TR-4: only the 64 stored levels are written");
    {
        Base::TraceSpan after;
        after.Begin("game", "after");
        CHECK_MESSAGE(Path() == "game:after", "TR-4: the stack is empty again");
    }
    const auto after = setup.Category("span.game");
    REQUIRE(after.size() == 1);
    CHECK_MESSAGE(Uint(after[0], "parent_id") == 0, "TR-4: no parent left over");
}

TEST_CASE("trace: an error text longer than 256 bytes is cut, not dropped") {
    Setup setup;
    const std::string text(300, 'e');
    {
        Base::TraceSpan span;
        span.Begin("game", "long");
        span.MarkError(text, false);
    }
    const auto spans = setup.Category("span.game");
    REQUIRE(spans.size() == 1);
    CHECK_MESSAGE(Text(spans[0], "error") == std::string(256, 'e'), "TR-5: the error is cut at 256 bytes");
}

TEST_CASE("trace: a span still open when Trace is closed ends without effect") {
    Setup setup;
    auto span = std::make_unique<Base::TraceSpan>();
    span->Begin("game", "open");
    setup.trace.reset();
    span.reset();
    CHECK_MESSAGE(setup.Category("span.game").empty(), "TR-4: ended after Close, it writes nothing");
}

TEST_CASE("trace: an error marked before an exception unwinds the span keeps its text") {
    Setup setup;
    try {
        Base::TraceSpan span;
        span.Begin("game", "marked");
        span.MarkError("the first error", false);
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    const auto spans = setup.Category("span.game");
    REQUIRE(spans.size() == 1);
    CHECK_MESSAGE(Text(spans[0], "error") == "the first error", "D4: the unwinding does not overwrite an earlier error");
    CHECK(Flag(spans[0], "exception"));
}

TEST_CASE("trace: an error marked on a span that is not on top writes the path up to that span") {
    Setup setup;
    {
        Base::TraceSpan outer;
        outer.Begin("game", "outer");
        Base::TraceSpan inner;
        inner.Begin("game", "inner");
        outer.MarkError("outer failed");
    }
    const auto paths = setup.Category("span.path");
    REQUIRE(paths.size() == 1);
    CHECK_MESSAGE(Text(paths[0], "path") == "game:outer", "D4: the path ends at the marked span");
}

TEST_CASE("trace: a sink whose minimum is above Trace takes no span, so none is timed") {
    Setup setup({.include = {"span."}, .minimum = Level::Debug});
    {
        Base::TraceSpan span;
        span.Begin("game", "untaken");
    }
    CHECK_MESSAGE(reads.load() == 0, "TR-6: a span is asked for at level Trace");
    CHECK(setup.Category("span.game").empty());
}

TEST_CASE("trace: the path record is at level Trace and the first unwound span carries the unwinding as its error") {
    Setup setup;
    {
        Base::TraceSpan span;
        span.Begin("game", "marked");
        span.MarkError("broken");
    }
    try {
        Base::TraceSpan span;
        span.Begin("game", "unwound");
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    const auto paths = setup.Category("span.path");
    REQUIRE(paths.size() == 2);
    CHECK_MESSAGE(paths[0].level == Level::Trace, "TR-5: the path record is at level Trace");
    const auto spans = setup.Category("span.game");
    REQUIRE(spans.size() == 2);
    CHECK_MESSAGE(Text(spans[1], "error") == "an exception left the span", "D4 (i): the first unwound span says why");
}

TEST_CASE("trace: closing the handle puts back the receiver that was installed before") {
    constexpr Base::TraceReceiver other{.begin = nullptr, .end = nullptr, .error = nullptr, .markCurrent = nullptr, .path = nullptr};
    REQUIRE(Base::SetTraceReceiver(&other) == nullptr);
    {
        auto handle = Trace::Install({});
    }
    CHECK_MESSAGE(Base::SetTraceReceiver(nullptr) == &other, "TR-7: Close restores the earlier receiver, not nullptr");
}

TEST_CASE("trace: after a child's exception is caught, the parent's own later exception writes its path") {
    Setup setup;
    try {
        Base::TraceSpan parent;
        parent.Begin("game", "parent");
        try {
            Base::TraceSpan child;
            child.Begin("game", "child");
            throw std::runtime_error("caught inside");
        } catch (const std::runtime_error&) {
        }
        {
            Base::TraceSpan later;
            later.Begin("game", "later");
        }
        throw std::runtime_error("the parent's own");
    } catch (const std::runtime_error&) {
    }
    CHECK_MESSAGE(setup.Category("span.path").size() == 2, "D4 (i): the parent's own exception writes a path (a new child cleared the old mark)");
}

TEST_CASE("trace: a category longer than 64 bytes is cut, not a failed assertion") {
    Setup setup({.include = {"span."}, .minimum = Level::Trace});
    const std::string category(80, 'c');
    {
        Base::TraceSpan span;
        span.Begin(category, "long-category");
    }
    const auto records = setup.memory->Records();
    REQUIRE_MESSAGE(records.size() == 1, "TR-5: the span is written under the cut category");
    CHECK(records[0].category == "span." + std::string(64, 'c'));
}

TEST_CASE("trace: a span still open when the Logger is uninstalled writes nothing when it ends") {
    Setup setup;
    {
        Base::TraceSpan span;
        span.Begin("game", "open-over-logger-close");
        REQUIRE(reads.load() == 1);
        setup.logger.reset();
    }
    // With no Logger a written record would go to standard error, which a test does not read (OS boundary):
    // the end's clock read shows whether the span was written.
    CHECK_MESSAGE(reads.load() == 1, "D5: no Logger at the end, the span is not written (no clock read at its end)");
    CHECK(setup.memory->Records().empty());
}

TEST_CASE("trace: a parent marked after a child's exception was caught writes the path for its own exception") {
    Setup setup;
    try {
        Base::TraceSpan parent;
        parent.Begin("game", "parent");
        try {
            Base::TraceSpan child;
            child.Begin("game", "child");
            throw std::runtime_error("caught inside");
        } catch (const std::runtime_error&) {
            parent.MarkError("handled the child", false);
        }
        throw std::runtime_error("the parent's own");
    } catch (const std::runtime_error&) {
    }
    CHECK_MESSAGE(setup.Category("span.path").size() == 2, "D4 (i): a mark clears the caught child's flag, so the parent's own exception writes a path");
}
