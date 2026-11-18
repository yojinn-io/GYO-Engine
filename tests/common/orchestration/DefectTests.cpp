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

TEST_CASE("O-8: D1: a failed replacement of an interrupted World returns the error; the carried value waits for the next one") {
    O::ProductDescription d = Game();
    d.worlds[0].build = [](O::WorldSetup& s) {
        Register(s);
        s.Registrar().AddSystem<&Fail>(In("FailJunk", "Junk"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    p.World("Main").GetWorld().ResourceMut<KeyBindings>()->jump = 6;
    REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("Main", "Junk")));  // the World is interrupted now
    auto failed = p.ReplaceWorld("Main", O::ReplaceRequest{Refuse(), {}, O::ReplaceOrder::BuildNewFirst});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(failed), "[or-d1-interrupted-fail] replaced");
    CHECK_MESSAGE(failed.error().code == O::OrchestrationErrorCode::ReplacePrepareFailed, "[or-d1-interrupted-fail]");
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    const KeyBindings* carried = p.World("Main").GetWorld().Resource<KeyBindings>();
    REQUIRE_MESSAGE(carried != nullptr, "[or-d1-held] KeyBindings is gone");
    CHECK_MESSAGE(carried->jump == 6, "[or-d1-held] KeyBindings is " << carried->jump);
}

TEST_CASE("O-8: D3: an exception from extra logs one warning") {
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
    CHECK_MESSAGE(warnings.Warnings() == 1, "[or-d3-throw-warns] " << warnings.Warnings());
}

TEST_CASE("O-1: D4: game initialization with the first World's place empty: WorldMissing, and it runs once the place is filled") {
    BootRuns() = 0;
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE_FALSE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{Refuse(), {}, O::ReplaceOrder::DropOldFirst})));
    auto frame = p.RunFrame();
    REQUIRE_FALSE(static_cast<bool>(frame));
    CHECK(frame.error().code == O::OrchestrationErrorCode::WorldMissing);
    CHECK_MESSAGE(!p.GameInitRan(), "[or-d4-not-marked] game initialization counted as run on an empty place");
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{{}, {}, O::ReplaceOrder::DropOldFirst})));
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(BootRuns() == 1, "[or-d4-runs-later] " << BootRuns());
}
