// O-9 (the orchestration half of dynamic insertion) (declaration 7.3).

#include "SaveGame.hpp"

#include <stdexcept>

using namespace OrchTest;
using namespace OrchTest::Save;

namespace {

O::InsertFn AddMark() {
    return [](K::Registrar& r) { r.AddSystem<&Mark<'i'>>(In("Inserted", "Play")); };
}

O::ProductDescription Refusing() {
    O::ProductDescription d = Game();
    d.worlds[0].dynamicInsert = false;
    return d;
}

} // namespace

TEST_CASE("O-9: an allowing World: the inserted System runs from the next frame; no Finish or Check runs again") {
    Log().clear();
    auto built = O::Product::Build(Game(), Units().Module("audio").Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    Log().clear();
    REQUIRE(static_cast<bool>(p.InsertSystems("Main", "console", AddMark())));
    CHECK_MESSAGE((!Logged("finish audio@Main v2") && !Logged("check audio@Main")), "[or-insert-no-lifecycle] " << Joined());
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(Logged("i"), "[or-insert-runs] " << Joined());
}

TEST_CASE("O-9: a refusing World: InsertRefused, one warning, the World goes on, the version is unchanged; still so after a replacement") {
    auto built = O::Product::Build(Refusing(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    for (int round = 0; round < 2; ++round) {
        CAPTURE(round);
        WarningCounter warnings;
        const auto version = p.World("Main").GetWorld().Types().Version();
        auto inserted = p.InsertSystems("Main", "console", AddMark());
        REQUIRE_FALSE(static_cast<bool>(inserted));
        CHECK_MESSAGE(inserted.error().code == O::OrchestrationErrorCode::InsertRefused, "[or-insert-refused]");
        CHECK_MESSAGE((inserted.error().kernel.size() == 1 && inserted.error().kernel[0].code == K::KernelErrorCode::DynamicInsertRefused),
                      "[or-insert-refused-kernel]");
        CHECK_MESSAGE(warnings.Warnings() == 1, "[or-insert-warn-once] " << warnings.Warnings());
        CHECK(p.World("Main").GetWorld().Types().Version() == version);
        CHECK(static_cast<bool>(p.RunFrame()));
        REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    }
}

TEST_CASE("O-9: BuildPlans fails (R-E7): InsertBuildFailed, one warning, the World stays on the previous version") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    WarningCounter warnings;
    auto inserted = p.InsertSystems("Main", "console", [](K::Registrar& r) { r.Allotter<K::RangeAllotter>(); });
    REQUIRE_FALSE(static_cast<bool>(inserted));
    CHECK_MESSAGE(inserted.error().code == O::OrchestrationErrorCode::InsertBuildFailed, "[or-insert-build-failed]");
    CHECK_MESSAGE(inserted.error().detail.find("R-E7") != std::string::npos, "[or-insert-build-failed] " << inserted.error().detail);
    CHECK(warnings.Warnings() == 1);
    CHECK(p.World("Main").GetWorld().Types().Version() == 1);
    CHECK(static_cast<bool>(p.RunFrame()));
}

TEST_CASE("O-9: a build that throws: the pending round is dropped, so a replacement afterwards works") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    bool thrown = false;
    try {
        static_cast<void>(p.InsertSystems("Main", "console", [](K::Registrar& r) {
            r.AddSystem<&Mark<'i'>>(In("Inserted", "Play"));
            throw std::runtime_error("bad command");
        }));
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    REQUIRE(thrown);
    auto again = p.InsertSystems("Main", "console", AddMark());
    CHECK_MESSAGE(static_cast<bool>(again), "[or-insert-throw-discards] the half round was kept: " << (again ? "" : again.error().detail));
    CHECK_MESSAGE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})), "[or-insert-throw-discards] replacement");
}

TEST_CASE("O-9: an owner name that is a unit's or the Product's: OwnerTaken") {
    auto built = O::Product::Build(Game(), Units().Module("audio").Take());
    ORCH_REQUIRE_BUILT(built);
    for (const char* owner : {"audio", "rpg"}) {
        auto inserted = (*built)->InsertSystems("Main", owner, AddMark());
        REQUIRE_FALSE(static_cast<bool>(inserted));
        CHECK_MESSAGE(inserted.error().code == O::OrchestrationErrorCode::OwnerTaken, "[or-insert-owner] " << owner);
    }
}
