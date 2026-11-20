// Defects the G-8 check (Fable/high, segment1/g8-check-fable-high.md, D1, D3, D4)
// found by reading; each fixed after this test failed (AGENTS 14.3).

#include "SaveGame.hpp"

#include <stdexcept>

using namespace OrchTest;
using namespace OrchTest::Save;

namespace {

O::PrepareFn Refuse() {
    return [](K::World&) -> Engine::Base::Result<void, O::OrchestrationError> {
        return Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "no", {}, "save-version-unsupported"));
    };
}

} // namespace

// D1 (a failed replacement of an interrupted World) left with the property it
// guarded: an interrupted World is not replaced any more, the Product has ended
// (v8-arch-gyo2-broken-world-ends-product; "O-8: a replacement does not mend a broken World").

TEST_CASE("O-8: D3: an exception from extra goes on to the caller and nothing is logged") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    WarningCounter warnings;
    bool thrown = false;
    try {
        static_cast<void>((*built)->ReplaceWorld("Main", O::ReplaceRequest{[](K::World&) -> Engine::Base::Result<void, O::OrchestrationError> {
            throw std::runtime_error("disk gone");
        }, {}, O::ReplaceOrder::BuildNewFirst}));
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    REQUIRE(thrown);
    // Error levels D2: the exception itself tells the caller; the orchestration layer logs nothing.
    CHECK_MESSAGE(warnings.Warnings() == 0, "[or-d3-throw-silent] " << warnings.Warnings());
}

// D4 now: the empty place ends the Product, so game initialization never runs
// (the place is not filled again; v8-arch-gyo2-broken-world-ends-product).
TEST_CASE("O-1: D4: game initialization with the first World's place empty: WorldMissing, not counted as run") {
    BootRuns() = 0;
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE_FALSE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{Refuse(), {}, O::ReplaceOrder::DropOldFirst})));
    auto frame = p.RunFrame();
    REQUIRE_FALSE(static_cast<bool>(frame));
    CHECK(frame.error().code == O::OrchestrationErrorCode::WorldMissing);
    CHECK_MESSAGE(!p.GameInitRan(), "[or-d4-not-marked] game initialization counted as run on an empty place");
    CHECK_MESSAGE(BootRuns() == 0, "[or-d4-not-run] " << BootRuns());
}

// ---- Error levels P12: DropOldFirst, an exception after the old World is gone and before the
// replacement's own try (the registration step): the place is empty, so the Product has ended and
// its failure is recorded before the exception goes on

namespace {

bool& BuildThrows() {
    static bool throws = false;
    return throws;
}

} // namespace

TEST_CASE("O-8: P12: DropOldFirst, the registration throws after the old World is gone: the failure is recorded before the exception goes on") {
    BuildThrows() = false;
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World(
        "Main",
        [](O::WorldSetup& s) {
            if (BuildThrows()) throw std::runtime_error("the World's build throws");
            Basics(s.Registrar(), {FrameClock("Tick")});
        },
        {"Tick"}));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    BuildThrows() = true;
    bool thrown = false;
    try {
        static_cast<void>(p.ReplaceWorld("Main", O::ReplaceRequest{{}, {}, O::ReplaceOrder::DropOldFirst}));
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    BuildThrows() = false;
    REQUIRE_MESSAGE(thrown, "[el-p12-throw-goes-on] the exception did not leave ReplaceWorld");
    REQUIRE_MESSAGE(p.Failure() != nullptr, "[el-p12-dropold-throw-before-try] the place is empty and no failure was recorded");
    CHECK_MESSAGE(!p.HasWorld("Main"), "[el-p12-place-empty]");
    auto after = p.RunFrame();
    CHECK_MESSAGE(!static_cast<bool>(after), "[el-p12-ended] the next entry point ran");
}
