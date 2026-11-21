// The trace facade (docs/architecture/target/v2/trace-declaration.zh-Hant.md, D1 and D2): one atomic pointer
// to a receiver; TraceSpan starts when Begin is called (so a step can declare it first and start it only when
// it runs) and ends in its destructor, telling the receiver whether an exception was unwinding it; with no
// receiver everything is a no-op.
#include <doctest/doctest.h>

// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>

#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Trace.hpp"

namespace Base = Engine::Base;

namespace {

// Records every call in one sequence.
struct Calls {
    std::vector<std::string> events;
    std::uint64_t nextId = 1;
};
Calls* calls = nullptr;

Base::TraceToken Begin(const std::string_view category, const std::string_view name, const std::string_view world) noexcept {
    calls->events.push_back("begin " + std::string(category) + " " + std::string(name) + " " + std::string(world));
    return Base::TraceToken{.id = calls->nextId++, .generation = 7, .depth = 1};
}
void End(const Base::TraceToken token, const bool byException) noexcept {
    calls->events.push_back("end " + std::to_string(token.id) + (byException ? " exception" : ""));
}
void Error(const Base::TraceToken token, const std::string_view text, const bool origin) noexcept {
    calls->events.push_back("error " + std::to_string(token.id) + " " + std::string(text) + (origin ? " origin" : ""));
}
void MarkCurrent(const std::string_view text, const bool origin) noexcept {
    calls->events.push_back("current " + std::string(text) + (origin ? " origin" : ""));
}
std::size_t Path(const std::span<char> buffer) noexcept {
    constexpr std::string_view path = "a > b";
    const std::size_t size = path.size() < buffer.size() ? path.size() : buffer.size();
    std::memcpy(buffer.data(), path.data(), size);
    return size;
}

constexpr Base::TraceReceiver kReceiver{.begin = &Begin, .end = &End, .error = &Error, .markCurrent = &MarkCurrent, .path = &Path};

// Installs kReceiver and a fresh call log while alive; puts back what was there.
struct Installed {
    Calls log;
    const Base::TraceReceiver* previous;
    Installed() : previous(Base::SetTraceReceiver(&kReceiver)) { calls = &log; }
    ~Installed() {
        Base::SetTraceReceiver(previous);
        calls = nullptr;
    }
};

} // namespace

TEST_CASE("trace facade: with no receiver a span, a mark and a path do nothing") {
    REQUIRE(Base::SetTraceReceiver(nullptr) == nullptr);
    {
        Base::TraceSpan span;
        span.Begin("kernel", "Main", "Match");
        CHECK_FALSE(span.Active());
        span.MarkError("ignored");
    }
    Base::TraceMarkCurrent("ignored", true);
    char buffer[16];
    CHECK(Base::TraceCurrentPath(buffer) == 0);
}

TEST_CASE("trace facade: Begin starts the span with category, name and World; the destructor ends it") {
    Installed installed;
    {
        Base::TraceSpan span;
        CHECK_FALSE(span.Active());
        span.Begin("kernel", "MoveRacer", "Match");
        CHECK(span.Active());
        span.MarkError("the racer fell");
        span.MarkError("propagated", false);
    }
    CHECK(installed.log.events == std::vector<std::string>{"begin kernel MoveRacer Match", "error 1 the racer fell origin",
                                                           "error 1 propagated", "end 1"});
}

TEST_CASE("trace facade: a span declared but never begun calls nothing (a skipped step)") {
    Installed installed;
    { Base::TraceSpan span; }
    CHECK(installed.log.events.empty());
}

TEST_CASE("trace facade: a span ended by an exception unwinding it says so; the exception goes on") {
    Installed installed;
    bool caught = false;
    try {
        Base::TraceSpan span;
        span.Begin("kernel", "Main", "");
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    CHECK(caught);
    CHECK(installed.log.events == std::vector<std::string>{"begin kernel Main ", "end 1 exception"});
}

TEST_CASE("trace facade: a span begun inside a catch block ends normally (only a new unwinding counts)") {
    Installed installed;
    try {
        throw std::runtime_error("outer");
    } catch (...) {
        Base::TraceSpan span;
        span.Begin("orchestration", "RunFrame", "");
    }
    CHECK(installed.log.events == std::vector<std::string>{"begin orchestration RunFrame ", "end 1"});
}

TEST_CASE("trace facade: a span ends at the receiver it began with, even if the receiver changed meanwhile") {
    Installed installed;
    {
        Base::TraceSpan span;
        span.Begin("kernel", "Main", "");
        Base::SetTraceReceiver(nullptr);
    }
    Base::SetTraceReceiver(&kReceiver);
    CHECK(installed.log.events == std::vector<std::string>{"begin kernel Main ", "end 1"});
}

TEST_CASE("trace facade: TraceMarkCurrent and TraceCurrentPath go to the receiver") {
    Installed installed;
    Base::TraceMarkCurrent("sync point failed", true);
    Base::TraceMarkCurrent("from below", false);
    char buffer[16];
    CHECK(Base::TraceCurrentPath(buffer) == 5);
    CHECK(std::string_view(buffer, 5) == "a > b");
    CHECK(installed.log.events == std::vector<std::string>{"current sync point failed origin", "current from below"});
}

TEST_CASE("trace facade: SetTraceReceiver returns the receiver it replaced") {
    Installed installed;
    CHECK(Base::SetTraceReceiver(nullptr) == &kReceiver);
    CHECK(Base::SetTraceReceiver(&kReceiver) == nullptr);
}

TEST_CASE("trace facade: a span begun and ended inside a destructor that runs during unwinding ends normally") {
    Installed installed;
    struct Cleanup {
        ~Cleanup() {
            Base::TraceSpan span;
            span.Begin("game", "cleanup", "");
        }
    };
    try {
        Cleanup cleanup;
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    CHECK_MESSAGE((installed.log.events == std::vector<std::string>{"begin game cleanup ", "end 1"}),
                  "D2: only an unwinding that started after Begin marks the span");
}

TEST_CASE("trace facade: a second Begin on the same span is ignored") {
    Installed installed;
    {
        Base::TraceSpan span;
        span.Begin("kernel", "Main", "");
        span.Begin("kernel", "Other", "");
    }
    CHECK_MESSAGE((installed.log.events == std::vector<std::string>{"begin kernel Main ", "end 1"}),
                  "D2: one span, one begin and one end");
}
