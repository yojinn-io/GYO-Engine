// The Logger, its filters and entrances ③ and ④ (logging declaration 5.2, 5.5,
// 5.8; Design Invariants LG-7 and LG-8).
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <exception>
#include <vector>

#include "AssertTestSupport.hpp"
#include "TestClock.hpp"
#include "engine/base/Log.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/RecordBuilder.hpp"
#include "engine/logging/Write.hpp"

namespace Logging = GYO::Engine::Logging;
using Logging::Level;

// Design Invariant LG-11: no function of the Sink interface may throw into the code that wrote a record.
static_assert(noexcept(std::declval<Logging::Sink&>().Write(std::declval<const Logging::Record&>())),
              "LG-11: Sink::Write is noexcept");
static_assert(noexcept(std::declval<Logging::Sink&>().WriteFatal(std::declval<const Logging::Record&>(), std::declval<std::string_view>(), std::declval<std::string_view>())),
              "LG-11: Sink::WriteFatal is noexcept");
static_assert(noexcept(std::declval<Logging::Sink&>().Flush(std::declval<std::chrono::steady_clock::time_point>())),
              "LG-11: Sink::Flush is noexcept");
static_assert(noexcept(std::declval<Logging::Sink&>().Close()), "LG-11: Sink::Close is noexcept");

namespace {

struct Installed {
    std::shared_ptr<Logging::MemorySink> memory = std::make_shared<Logging::MemorySink>();
    std::optional<Logging::LoggerHandle> handle;

    explicit Installed(Logging::SinkFilter filter = {.minimum = Level::Trace}, bool takeBaseLog = true) {
        GYO::Test::Logging::ResetClock();
        Logging::LoggerOptions options;
        options.sinks.push_back({.name = "memory", .sink = memory, .filter = std::move(filter)});
        options.takeBaseLog = takeBaseLog;
        options.clock = GYO::Test::Logging::TestClock();
        auto installed = Logging::Install(std::move(options));
        REQUIRE(installed);
        handle.emplace(std::move(installed.value()));
    }
};

} // namespace

TEST_CASE("logger: a record reaches the sink with its level, category, text, fields and both clocks") {
    Installed logger;
    GYO::Test::Logging::monoNow = 42;
    Logging::Write(Level::Info, "net", "sent", {{"bytes", 512}, {"peer", "client-3"}, {"late", false}});
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 1);
    const auto& record = records[0];
    CHECK(record.level == Level::Info);
    CHECK(record.category == "net");
    CHECK(record.text == "sent");
    CHECK(record.monoNs == 42);
    CHECK(record.wall.unixMs == 1'763'600'411'007);
    CHECK(record.wall.utcOffsetSeconds == 9 * 3600);
    REQUIRE(record.fields.size() == 3);
    CHECK(record.fields[0].key == "bytes");
    CHECK(record.fields[0].type == Logging::FieldValue::Type::Int);
    CHECK(record.fields[0].intValue == 512);
    CHECK(record.fields[1].key == "peer");
    CHECK(record.fields[1].text == "client-3");
    CHECK(record.fields[2].type == Logging::FieldValue::Type::Bool);
    CHECK_FALSE(record.fields[2].boolValue);
    CHECK_FALSE(record.fatal);
}

TEST_CASE("logger: field values keep their kind: signed, unsigned, double, bool, text") {
    Installed logger;
    const std::string owned = "owned";
    Logging::Write(Level::Info, "kinds", "",
                   {{"i", -3}, {"u", 7u}, {"u64", std::uint64_t{1} << 63}, {"d", 0.5}, {"f", 1.5f}, {"s", owned}});
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 1);
    const auto& f = records[0].fields;
    REQUIRE(f.size() == 6);
    CHECK(f[0].type == Logging::FieldValue::Type::Int);
    CHECK(f[0].intValue == -3);
    CHECK(f[1].type == Logging::FieldValue::Type::Uint);
    CHECK(f[1].uintValue == 7u);
    CHECK(f[2].uintValue == (std::uint64_t{1} << 63));
    CHECK(f[3].type == Logging::FieldValue::Type::Double);
    CHECK(f[3].doubleValue == 0.5);
    CHECK(f[4].doubleValue == 1.5);
    CHECK(f[5].type == Logging::FieldValue::Type::String);
    CHECK(f[5].text == "owned");
}

TEST_CASE("logger: a record below the sink's minimum level is refused, and so is one of an excluded category") {
    Installed logger({.include = {}, .exclude = {"trace."}, .minimum = Level::Warning});
    Logging::Write(Level::Info, "net", "too quiet");
    Logging::Write(Level::Warning, "trace.net", "excluded");
    Logging::Write(Level::Warning, "net", "taken");
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].text == "taken");
    CHECK_FALSE(Logging::Enabled(Level::Info, "net"));
    CHECK(Logging::Enabled(Level::Error, "net"));
    CHECK_FALSE(Logging::Enabled(Level::Error, "trace.net"));
}

TEST_CASE("logger: include takes only the listed prefixes; the longest matching level rule wins") {
    Installed logger({.include = {"trace."},
                      .exclude = {},
                      .minimum = Level::Error,
                      .levels = {{"trace.", Level::Info}, {"trace.net", Level::Debug}}});
    Logging::Write(Level::Debug, "trace.net.send", "deepest rule");
    Logging::Write(Level::Debug, "trace.move", "only trace. rule");
    Logging::Write(Level::Info, "trace.move", "trace. rule");
    Logging::Write(Level::Fatal, "world", "not included");
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 2);
    CHECK(records[0].text == "deepest rule");
    CHECK(records[1].text == "trace. rule");
}

TEST_CASE("logger: SetLevel changes a rule from the next record on") {
    Installed logger({.minimum = Level::Warning, .levels = {{"net", Level::Warning}}});
    Logging::Write(Level::Debug, "net", "before");
    Logging::SetLevel("memory", "net", Level::Debug);
    Logging::Write(Level::Debug, "net", "after");
    Logging::Write(Level::Debug, "audio", "default still warning");
    Logging::SetLevel("memory", "", Level::Debug);
    Logging::Write(Level::Debug, "audio", "default lowered");
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 2);
    CHECK(records[0].text == "after");
    CHECK(records[1].text == "default lowered");
}

TEST_CASE("logger: SetLevel raising a rule refuses from the next record on") {
    Installed logger({.minimum = Level::Debug});
    Logging::SetLevel("memory", "", Level::Error);
    Logging::Write(Level::Debug, "net", "raised away");
    CHECK_MESSAGE(logger.memory->Records().empty(), "LG-7: the raised level is stored and refuses the next record");
}

TEST_CASE("logger: SetLevel with an unknown sink or prefix, or with no Logger, is a Programmer Error") {
    {
        Installed logger;
        GYO_CHECK_ASSERTS(Logging::SetLevel("nowhere", "", Level::Debug));
        GYO_CHECK_ASSERTS(Logging::SetLevel("memory", "net", Level::Debug));
    }
    GYO_CHECK_ASSERTS(Logging::SetLevel("memory", "", Level::Debug));
}

TEST_CASE("logger: a record that writes one key twice becomes one Error record of category logging") {
    Installed logger;
    Logging::Write(Level::Info, "net", "sent", {{"peer", 1}, {"bytes", 2}, {"peer", 3}});
    const auto records = logger.memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].level == Level::Error);
    CHECK(records[0].category == "logging");
    CHECK(records[0].fields.empty());
    CHECK(records[0].text.find("\"peer\"") != std::string::npos);
    CHECK(records[0].text.find("net") != std::string::npos);
}

TEST_CASE("logger: GYO::Engine::Base::Log lines become records once installed, and the old handler is put back") {
    const auto before = GYO::Engine::Base::SetLogHandler(nullptr);
    GYO::Engine::Base::SetLogHandler(before);
    {
        Installed logger;
        GYO::Engine::Base::Log(GYO::Engine::Base::LogLevel::Warning, "orchestration", "replacement failed");
        const auto records = logger.memory->Records();
        REQUIRE(records.size() == 1);
        CHECK(records[0].level == Level::Warning);
        CHECK(records[0].category == "orchestration");
        CHECK(records[0].text == "replacement failed");
    }
    const auto after = GYO::Engine::Base::SetLogHandler(nullptr);
    GYO::Engine::Base::SetLogHandler(after);
    CHECK(after == before);
}

TEST_CASE("logger: takeBaseLog = false leaves GYO::Engine::Base::Log alone") {
    const auto before = GYO::Engine::Base::SetLogHandler(nullptr);
    GYO::Engine::Base::SetLogHandler(before);
    Installed logger({.minimum = Level::Trace}, false);
    const auto during = GYO::Engine::Base::SetLogHandler(nullptr);
    GYO::Engine::Base::SetLogHandler(during);
    CHECK(during == before);
}

TEST_CASE("logger: a second Logger, an empty or repeated sink name, a missing sink, a repeated prefix are Programmer Errors") {
    auto memory = std::make_shared<Logging::MemorySink>();
    {
        Installed logger;
        GYO_CHECK_ASSERTS(Logging::Install({}));
    }
    GYO_CHECK_ASSERTS(Logging::Install({.sinks = {{.name = "", .sink = memory, .filter = {}}}}));
    {
        Logging::LoggerOptions options;
        options.sinks.push_back({.name = "a", .sink = memory, .filter = {}});
        options.sinks.push_back({.name = "a", .sink = memory, .filter = {}});
        GYO_CHECK_ASSERTS(Logging::Install(std::move(options)));
    }
    GYO_CHECK_ASSERTS(Logging::Install({.sinks = {{.name = "a", .sink = nullptr, .filter = {}}}}));
    GYO_CHECK_ASSERTS(Logging::Install(
        {.sinks = {{.name = "a", .sink = memory, .filter = {.levels = {{"x", Level::Info}, {"x", Level::Debug}}}}}}));
}

TEST_CASE("logger: closing the handle uninstalls; Close runs on every sink; a new Logger may then be installed") {
    struct ClosingSink final : Logging::Sink {
        int* closes;
        explicit ClosingSink(int* counter) : closes(counter) {}
        void Write(const Logging::Record&) noexcept override {}
        void Close() noexcept override { ++*closes; }
    };
    int closes = 0;
    {
        Logging::LoggerOptions options;
        options.sinks.push_back({.name = "a", .sink = std::make_shared<ClosingSink>(&closes), .filter = {}});
        options.sinks.push_back({.name = "b", .sink = std::make_shared<ClosingSink>(&closes), .filter = {}});
        auto handle = Logging::Install(std::move(options));
        REQUIRE(handle);
        handle.value().Close();
        CHECK(closes == 2);
        handle.value().Close();
        CHECK(closes == 2);
    }
    CHECK(closes == 2);
    Installed again;
    Logging::Write(Level::Info, "x", "y");
    CHECK(again.memory->Records().size() == 1);
}

TEST_CASE("logger: a moved handle uninstalls once, from where it was moved to") {
    auto memory = std::make_shared<Logging::MemorySink>();
    std::optional<Logging::LoggerHandle> outer;
    {
        auto handle = Logging::Install({.sinks = {{.name = "m", .sink = memory, .filter = {}}}});
        REQUIRE(handle);
        outer.emplace(std::move(handle.value()));
    }
    Logging::Write(Level::Info, "x", "still installed");
    CHECK(memory->Records().size() == 1);
    outer.reset();
    Logging::Write(Level::Info, "x", "uninstalled: to standard error");
    CHECK(memory->Records().size() == 1);
}

TEST_CASE("logger: with no Logger installed every category is enabled and Flush is true") {
    CHECK(Logging::Enabled(Level::Trace, "anything"));
    CHECK(Logging::Flush(std::chrono::milliseconds{1}));
}

TEST_CASE("logger: Flush asks every sink with one deadline and reports whether all were flushed") {
    struct FlushSink final : Logging::Sink {
        bool answer;
        std::chrono::steady_clock::time_point* seen;
        FlushSink(bool a, std::chrono::steady_clock::time_point* s) : answer(a), seen(s) {}
        void Write(const Logging::Record&) noexcept override {}
        bool Flush(std::chrono::steady_clock::time_point deadline) noexcept override {
            *seen = deadline;
            return answer;
        }
    };
    std::chrono::steady_clock::time_point first{};
    std::chrono::steady_clock::time_point second{};
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "a", .sink = std::make_shared<FlushSink>(true, &first), .filter = {}});
    options.sinks.push_back({.name = "b", .sink = std::make_shared<FlushSink>(false, &second), .filter = {}});
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    CHECK_FALSE(Logging::Flush(std::chrono::seconds{1}));
    CHECK(first == second);
    CHECK(first != std::chrono::steady_clock::time_point{});
}

TEST_CASE("builder: text and fields are copied; Clear keeps nothing") {
    Logging::RecordBuilder builder;
    {
        std::string key = "peer";
        std::string value = "client-3";
        builder.Text("sent ");
        builder.AppendText("twice");
        builder.Add(key, value);
        builder.Add("bytes", 512);
        key.assign("XXXX");
        value.assign("XXXXXXXX");
    }
    CHECK(builder.TextView() == "sent twice");
    const auto fields = builder.Fields();
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].key == "peer");
    CHECK(fields[0].value.AsString() == "client-3");
    CHECK(fields[1].key == "bytes");
    CHECK(fields[1].value.AsInt() == 512);
    builder.Clear();
    CHECK(builder.TextView().empty());
    CHECK(builder.Fields().empty());
}

TEST_CASE("builder: string views stay right after the arena grows") {
    Logging::RecordBuilder builder;
    for (int i = 0; i < 64; ++i) builder.Add("key_with_a_longer_name", std::string(40, static_cast<char>('a' + i % 26)));
    const auto fields = builder.Fields();
    REQUIRE(fields.size() == 64);
    for (int i = 0; i < 64; ++i) {
        CHECK(fields[static_cast<std::size_t>(i)].value.AsString() == std::string(40, static_cast<char>('a' + i % 26)));
    }
}

TEST_CASE("logger: SetLevel from two threads leaves the floor low enough for every sink (review S3)") {
    auto a = std::make_shared<Logging::MemorySink>();
    auto b = std::make_shared<Logging::MemorySink>();
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "a", .sink = a, .filter = {.minimum = Level::Error}});
    options.sinks.push_back({.name = "b", .sink = b, .filter = {.minimum = Level::Error}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    std::atomic<bool> stop{false};
    std::thread toggler([&] {
        while (!stop.load()) {
            Logging::SetLevel("a", "", Level::Warning);
            Logging::SetLevel("a", "", Level::Error);
        }
    });
    int refused = 0;
    for (int i = 0; i < 200'000; ++i) {
        Logging::SetLevel("b", "", Level::Trace);
        if (!Logging::Enabled(Level::Trace, "x")) ++refused;
        Logging::SetLevel("b", "", Level::Error);
    }
    stop.store(true);
    toggler.join();
    CHECK_MESSAGE(refused == 0, "S3: Enabled(Trace) was false right after SetLevel(b, Trace)");
}

TEST_CASE("logger: a record with a repeated key reaches the sinks that would have taken it, as the Error record (review m1)") {
    auto relay = std::make_shared<Logging::MemorySink>();
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "relay", .sink = relay, .filter = {.include = {"relay."}, .minimum = Level::Info}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    Logging::Write(Level::Info, "relay.movement", "moved", {{"i", 1}, {"i", 2}});
    const auto records = relay->Records();
    REQUIRE_MESSAGE(records.size() == 1, "m1: the sink that takes relay. got the replacement, not nothing");
    CHECK(records[0].level == Level::Error);
    CHECK(records[0].category == "logging");
}

TEST_CASE("logger: a later sink with a lower minimum still gets what the first sink refuses (the floor covers all sinks)") {
    auto strict = std::make_shared<Logging::MemorySink>();
    auto loose = std::make_shared<Logging::MemorySink>();
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "strict", .sink = strict, .filter = {.minimum = Level::Error}});
    options.sinks.push_back({.name = "loose", .sink = loose, .filter = {.minimum = Level::Debug}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    Logging::Write(Level::Debug, "net", "fine detail");
    CHECK(strict->Records().empty());
    CHECK_MESSAGE(loose->Records().size() == 1, "LG-7: the fast refusal looks at every sink's minimum");
}

TEST_CASE("logger: the Error record that replaces a record with a repeated key keeps its World") {
    auto memory = std::make_shared<Logging::MemorySink>();
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "memory", .sink = memory, .filter = {}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    const Logging::Field fields[] = {{"i", 1}, {"i", 2}};
    Logging::Write(Logging::Entry{.level = Level::Info, .category = "relay", .text = "x", .fields = fields, .world = "Match"});
    const auto records = memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].category == "logging");
    CHECK_MESSAGE(records[0].world == "Match", "LG-5: the replacement keeps the World");
}

TEST_CASE("logger: uninstalling puts the handlers back before closing the sinks, and closes them in the order given") {
    const std::terminate_handler before = std::get_terminate();
    std::vector<std::string> order;
    struct ClosingSink final : Logging::Sink {
        std::vector<std::string>* order = nullptr;
        std::string name;
        std::terminate_handler before = nullptr;
        bool handlerBack = false;
        void Write(const Logging::Record&) noexcept override {}
        void Close() noexcept override {
            order->push_back(name);
            handlerBack = std::get_terminate() == before;
        }
    };
    auto first = std::make_shared<ClosingSink>();
    auto second = std::make_shared<ClosingSink>();
    first->order = second->order = &order;
    first->name = "first";
    second->name = "second";
    first->before = second->before = before;
    {
        Logging::LoggerOptions options;
        options.sinks.push_back({.name = "first", .sink = first, .filter = {}});
        options.sinks.push_back({.name = "second", .sink = second, .filter = {}});
        options.takeAssertions = false;
        options.takeTerminate = true;
        auto handle = Logging::Install(std::move(options));
        REQUIRE(handle);
    }
    CHECK_MESSAGE((order == std::vector<std::string>{"first", "second"}), "LG-8: the sinks close in the order given");
    CHECK_MESSAGE(first->handlerBack, "LG-8: the handlers are put back before the sinks are closed");
}

TEST_CASE("logger: Installed tells whether a Logger is installed (Enabled cannot: it is true with none)") {
    CHECK_FALSE(Logging::Installed());
    {
        Installed logger;
        CHECK(Logging::Installed());
    }
    CHECK_MESSAGE(!Logging::Installed(), "trace declaration D5: Installed is false again once the Logger is uninstalled");
}
