// The orchestration layer's trace spans (trace declaration docs/architecture/target/v2/trace-declaration.zh-Hant.md,
// D6): one span per entry point around its steps, the kernel's spans inside it; an entry point's error is marked
// on its span, as the origin when the orchestration layer produced it and passing through when the kernel did or a
// World broke (D4 (i)). A recording receiver of the test's own; each CHECK carries a tag "[or-trace-...]".

#include "SaveGame.hpp"
#include "TestRangeAllotter.hpp"

// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Trace.hpp"

using namespace GYO::Test::Orchestration;
using namespace GYO::Test::Orchestration::Save;

namespace {

namespace Base = ::GYO::Engine::Base;

std::vector<std::string>* events = nullptr;
std::uint64_t nextId = 1;

Base::TraceToken Begin(const std::string_view category, const std::string_view name, const std::string_view world) noexcept {
    events->push_back("begin " + std::string(category) + " " + std::string(name) + (world.empty() ? "" : "@" + std::string(world)));
    return Base::TraceToken{.id = nextId++, .generation = 1, .depth = 1};
}
void End(const Base::TraceToken token, bool) noexcept {
    events->push_back("end " + std::to_string(token.id));
}
std::string* lastErrorText = nullptr;
void Error(const Base::TraceToken token, const std::string_view text, const bool origin) noexcept {
    events->push_back("error " + std::to_string(token.id) + (origin ? " origin" : " through"));
    if (lastErrorText != nullptr) *lastErrorText = std::string(text);
}
void MarkCurrent(std::string_view, const bool origin) noexcept {
    events->push_back(origin ? "current origin" : "current through");
}
std::size_t Path(std::span<char>) noexcept {
    return 0;
}
constexpr Base::TraceReceiver kReceiver{.begin = &Begin, .end = &End, .error = &Error, .markCurrent = &MarkCurrent, .path = &Path};

struct Recording {
    std::vector<std::string> log;
    std::string errorText;
    const Base::TraceReceiver* previous;
    Recording() : previous(Base::SetTraceReceiver(&kReceiver)) {
        events = &log;
        lastErrorText = &errorText;
        nextId = 1;
    }
    ~Recording() {
        Base::SetTraceReceiver(previous);
        events = nullptr;
        lastErrorText = nullptr;
    }
    [[nodiscard]] std::string Joined() const {
        std::string out;
        for (const std::string& e : log) out += (out.empty() ? "" : " | ") + e;
        return out;
    }
    [[nodiscard]] bool Has(const std::string& e) const {
        for (const std::string& line : log) {
            if (line == e) return true;
        }
        return false;
    }
};

} // namespace

TEST_CASE("trace-spans: RunFrame is one span with the kernel's spans inside it") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    Recording recording;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE_FALSE(recording.log.empty());
    CHECK_MESSAGE(recording.log.front() == "begin orchestration RunFrame", "[or-trace-frame] " << recording.Joined());
    CHECK_MESSAGE(recording.log.back() == "end 1", "[or-trace-frame] " << recording.Joined());
    CHECK_MESSAGE(recording.Joined().find("begin kernel Play@Main") != std::string::npos, "[or-trace-kernel-inside] " << recording.Joined());
}

TEST_CASE("trace-spans: an error the orchestration layer produces is the origin on the entry point's span") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    Recording recording;
    auto inserted = p.InsertSystems("Main", "console", [](K::Registrar& r) { r.Allotter<GYO::Test::TestRangeAllotter>(); });
    REQUIRE_FALSE(static_cast<bool>(inserted));
    CHECK_MESSAGE(recording.Joined() == "begin orchestration InsertSystems | error 1 origin | end 1", "[or-trace-origin] " << recording.Joined());
    CHECK_MESSAGE(recording.errorText.rfind("InsertBuildFailed: ", 0) == 0, "[or-trace-mark-text] " << recording.errorText);
}

TEST_CASE("trace-spans: a World that breaks passes through the entry point's span (the kernel marked it)") {
    O::ProductDescription d = Game();
    d.worlds[0].build = [base = d.worlds[0].build](O::WorldSetup& setup) {
        if (base) base(setup);
        setup.Registrar().AddSystem<&Fail>(In("Breaks", "Play"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    Recording recording;
    REQUIRE_FALSE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(recording.Has("current origin"), "[or-trace-break-kernel] " << recording.Joined());
    CHECK_MESSAGE(recording.Has("error 1 through"), "[or-trace-break-through] " << recording.Joined());
    // The Product has ended: the next entry point returns WorldInterrupted with the failure, and that passes
    // through too (the World broke earlier; no second path).
    recording.log.clear();
    REQUIRE_FALSE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(recording.Joined() == "begin orchestration RunFrame | error " + std::to_string(nextId - 1) + " through | end " + std::to_string(nextId - 1),
                  "[or-trace-ended-through] " << recording.Joined());
}

TEST_CASE("trace-spans: a DropOldFirst replacement the orchestration layer fails itself is the origin, though it ends the Product") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    O::ReplaceRequest request;
    request.order = O::ReplaceOrder::DropOldFirst;
    request.extra = [](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        return GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "the save is broken"));
    };
    Recording recording;
    auto replaced = p.ReplaceWorld("Main", std::move(request));
    REQUIRE_FALSE(static_cast<bool>(replaced));
    REQUIRE(replaced.error().failure.has_value());
    CHECK_MESSAGE(recording.Has("error 1 origin"), "[or-trace-replace-own-origin] " << recording.Joined());
}

TEST_CASE("trace-spans: RunGameInit and RunSchedule and ReplaceWorld are one span each") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    Recording recording;
    REQUIRE(static_cast<bool>(p.RunGameInit()));
    REQUIRE(static_cast<bool>(p.RunSchedule("Main", "Play")));
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    std::vector<std::string> entries;
    for (const std::string& e : recording.log) {
        if (e.rfind("begin orchestration ", 0) == 0) entries.push_back(e.substr(20));
    }
    CHECK_MESSAGE((entries == std::vector<std::string>{"RunGameInit", "RunSchedule", "ReplaceWorld"}), "[or-trace-entries] " << recording.Joined());
}

TEST_CASE("trace-spans: a beforeSwap schedule that fails in a BuildNewFirst replacement passes through (the kernel marked it)") {
    O::ProductDescription d = Game();
    d.worlds[0].build = [base = d.worlds[0].build](O::WorldSetup& setup) {
        if (base) base(setup);
        setup.Registrar().Schedule(Once("Before"));
        setup.Registrar().AddSystem<&Fail>(In("BreaksBefore", "Before"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    O::ReplaceRequest request;
    request.order = O::ReplaceOrder::BuildNewFirst;
    request.beforeSwap = {"Before"};
    Recording recording;
    auto replaced = p.ReplaceWorld("Main", std::move(request));
    REQUIRE_FALSE(static_cast<bool>(replaced));
    REQUIRE(replaced.error().code == O::OrchestrationErrorCode::ReplaceScheduleFailed);
    CHECK_MESSAGE(recording.Has("current origin"), "[or-trace-before-kernel] " << recording.Joined());
    CHECK_MESSAGE(recording.Has("error 1 through"), "[or-trace-before-through] " << recording.Joined());
}
