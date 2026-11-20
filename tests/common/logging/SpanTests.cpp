// The hand-made span (logging declaration 5.13; Design Invariant LG-9).
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <memory>
#include <optional>

#include "TestClock.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/Span.hpp"

namespace Logging = GYO::Engine::Logging;
using Logging::Level;

namespace {

struct Installed {
    std::shared_ptr<Logging::MemorySink> memory = std::make_shared<Logging::MemorySink>();
    std::optional<Logging::LoggerHandle> handle;
    explicit Installed(Level minimum = Level::Info) {
        Logging::Test::ResetClock();
        Logging::LoggerOptions options;
        options.sinks.push_back({.name = "memory", .sink = memory, .filter = {.minimum = minimum}});
        options.clock = Logging::Test::TestClock();
        options.takeAssertions = false;
        options.takeTerminate = false;
        auto installed = Logging::Install(std::move(options));
        REQUIRE(installed);
        handle.emplace(std::move(installed.value()));
    }
};

} // namespace

TEST_CASE("span: one record of kind span with begin_ns, duration_ns and mono_ns at its end, from the destructor") {
    Installed logger;
    Logging::Test::monoNow = 100;
    {
        Logging::Span span("trace.net", "send");
        CHECK(span.Active());
        Logging::Test::monoNow = 350;
    }
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].kind == Logging::RecordKind::Span);
    CHECK(records[0].level == Level::Info);
    CHECK(records[0].category == "trace.net");
    CHECK(records[0].text == "send");
    CHECK(records[0].beginNs == 100);
    CHECK(records[0].durationNs == 250);
    CHECK(records[0].monoNs == 350);
}

TEST_CASE("span: End writes once, with its fields; the destructor then writes nothing") {
    Installed logger;
    Logging::Test::monoNow = 10;
    {
        Logging::Span span("trace.net", "send", Level::Warning);
        Logging::Test::monoNow = 40;
        span.End({{"bytes", 512}, {"peer", "client-3"}});
        span.End({{"bytes", 1}});
    }
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].level == Level::Warning);
    CHECK(records[0].durationNs == 30);
    REQUIRE(records[0].fields.size() == 2);
    CHECK(records[0].fields[0].key == "bytes");
    CHECK(records[0].fields[0].intValue == 512);
    CHECK(records[0].fields[1].text == "client-3");
}

TEST_CASE("span: a span no sink takes reads no clock and writes nothing") {
    Installed logger(Level::Warning);
    {
        Logging::Span span("trace.net", "send", Level::Debug);
        CHECK_FALSE(span.Active());
        span.End({{"bytes", 512}});
    }
    CHECK(Logging::Test::clockReads.load() == 0);
    CHECK(logger.memory->Records().empty());
}

TEST_CASE("span: a span that writes one key twice becomes the Error record of category logging") {
    Installed logger;
    {
        Logging::Span span("trace.net", "send");
        span.End({{"a", 1}, {"a", 2}});
    }
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].category == "logging");
    CHECK(records[0].level == Level::Error);
}
