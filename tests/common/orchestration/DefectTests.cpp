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
