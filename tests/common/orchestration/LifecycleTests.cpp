// O-2 (lifecycle), O-5 (one report), O-E1 to O-E7, dependencies (O-E3),
// options and the World declaration (declaration 7.3).

#include "OrchestrationTestSupport.hpp"

#include "AssertTestSupport.hpp"

#include <type_traits>

using namespace OrchTest;

namespace {

O::ProductDescription TwoWorlds() {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", [](O::WorldSetup& s) { Basics(s.Registrar(), {FrameClock("Tick")}); }));
    d.worlds.push_back(World("Side", [](O::WorldSetup& s) { Basics(s.Registrar(), {FrameClock("Tick")}); }));
    d.configure = [](O::ProductSetup&) { Log().push_back("configure game"); };
    return d;
}

struct Strategy {
    std::string name;
};

} // namespace

TEST_CASE("O-2: Configure once each before any Build; per World Build in order, BuildPlans, Finish in order, then Check") {
    Log().clear();
    auto built = O::Product::Build(TwoWorlds(), Units().Module("time").Module("authority", {}, {"time"}).Plugin("window").Take());
    ORCH_REQUIRE_BUILT(built);
    const std::vector<std::string> expected = {
        "configure time", "configure authority", "configure window", "configure game",
        "build time@Main", "build authority@Main", "build window@Main",
        "finish time@Main v1", "finish authority@Main v1", "finish window@Main v1",
        "check time@Main", "check authority@Main", "check window@Main",
        "build time@Side", "build authority@Side", "build window@Side",
        "finish time@Side v1", "finish authority@Side v1", "finish window@Side v1",
        "check time@Side", "check authority@Side", "check window@Side"};
    CHECK_MESSAGE(Log() == expected, "[or-lifecycle-order] " << Joined());
}

TEST_CASE("O-2: a World's units list limits who builds there; a World with no product build still builds") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription only = World("Main", [](O::WorldSetup& s) { Basics(s.Registrar(), {FrameClock("Tick")}); });
    only.units = {"time"};
    d.worlds.push_back(std::move(only));
    d.worlds.push_back(World("Empty"));
    auto built = O::Product::Build(std::move(d), Units().Module("time").Module("audio").Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE((Logged("build time@Main") && !Logged("build audio@Main")), "[or-units-filter] " << Joined());
    CHECK_MESSAGE((Logged("build time@Empty") && Logged("build audio@Empty")), "[or-units-filter] " << Joined());
    CHECK_MESSAGE((*built)->HasWorld("Empty"), "[or-empty-world] the World with nothing registered was not built");
}

TEST_CASE("O-2: options reach the Module in Build; Check sees what the World is built with") {
    Log().clear();
    std::string seen;
    bool checkedInsert = true;
    std::string checkedOption;
    O::ProductDescription d = TwoWorlds();
    d.worlds.resize(1);
    d.worlds[0].dynamicInsert = false;
    d.worlds[0].options.Set(Strategy{"server"});
    Calls calls;
    calls.build = [&](O::WorldSetup& s) {
        if (const Strategy* strategy = s.Options<Strategy>()) seen = strategy->name;
    };
    calls.check = [&](const K::FrozenRegistry&, O::WorldCheck& c) {
        checkedInsert = c.DynamicInsert();
        if (const Strategy* strategy = c.Options<Strategy>()) checkedOption = strategy->name;
    };
    auto built = O::Product::Build(std::move(d), Units().Module("authority", calls).Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(seen == "server", "[or-options] Build read '" << seen << "'");
    CHECK_MESSAGE(checkedOption == "server", "[or-options] Check read '" << checkedOption << "'");
    CHECK_MESSAGE(!checkedInsert, "[or-check-declaration] Check saw dynamicInsert true");
    CHECK_MESSAGE(!(*built)->World("Main").GetWorld().Options().dynamicInsert,
                  "[or-check-declaration] the World was built with another declaration than Check saw");
}

TEST_CASE("O-5: kernel errors of one World, a Check error of another, O-E5 and O-E3 come in one report; no World, no prepare") {
    Log().clear();
    bool prepared = false;
    O::ProductDescription d = TwoWorlds();
    d.worlds[0].build = [](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick")});
        s.Registrar().AddSystem<&Mark<'a'>>(In("Broken", "Nowhere"));  // a mount on no schedule: a kernel error
    };
    d.worlds[1].frame = {"NoSuchSchedule"};
    d.worlds[1].prepare.push_back([&](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        prepared = true;
        return {};
    });
    Calls checker;
    checker.check = [](const K::FrozenRegistry&, O::WorldCheck& c) { c.Report("strategy-check", "refused on " + c.WorldName()); };
    auto built = O::Product::Build(std::move(d), Units().Module("authority", checker, {"time"}).Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-report] the Product was built");
    const O::ProductStartupReport& report = built.error();
    CHECK_MESSAGE(report.KernelReportOf("Main") != nullptr, "[or-report-kernel] " << report.detail);
    CHECK_MESSAGE(report.Has(O::OrchestrationCode::OE5), "[or-report-oe5] " << report.detail);
    CHECK_MESSAGE(report.Has(O::OrchestrationCode::OE3), "[or-report-oe3] " << report.detail);
    CHECK_MESSAGE((report.unitErrors.size() == 1 && report.unitErrors[0].world == "Side"), "[or-report-unit] " << report.detail);
    CHECK_MESSAGE((!Logged("finish authority@Main v1") && !Logged("check authority@Main")),
                  "[or-kernel-fail-no-finish] " << Joined());
    CHECK_MESSAGE(report.detail.find("not checked") != std::string::npos, "[or-not-checked] " << report.detail);
    CHECK_MESSAGE(!prepared, "[or-no-world-on-error] a prepare function ran");
}

TEST_CASE("O-E1: O-E1 to O-E6 and a kernel error come in one report") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription main = World("Main", [](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick")});
        s.Registrar().AddSystem<&Mark<'a'>>(In("Broken", "Nowhere"));
    });
    main.frame = {"Missing"};
    d.worlds.push_back(std::move(main));
    d.worlds.push_back(World("Main"));  // O-E6: two Worlds named Main
    d.configure = [](O::ProductSetup& s) {
        s.InstallRunner("a", [](O::Product&) { return O::RunnerExit{}; });
        s.InstallRunner("b", [](O::Product&) { return O::RunnerExit{}; });  // O-E4
        s.GameInit().InsertBefore("NoAnchor", "Boot");                        // O-E5 (the kernel failed, so the frame list is not checked)
    };
    Units units;
    units.Module("time").Module("time", {}, {}, false)            // O-E1: a second instance of row time
        .Module("ghost", {}, {}, false)                           // O-E2: no row
        .Module("lonely", {}, {"nowhere"});                       // O-E3: a dependency in no file
    auto built = O::Product::Build(std::move(d), units.Take());
    REQUIRE_FALSE(static_cast<bool>(built));
    const O::ProductStartupReport& report = built.error();
    for (O::OrchestrationCode code : {O::OrchestrationCode::OE1, O::OrchestrationCode::OE2, O::OrchestrationCode::OE3,
                                      O::OrchestrationCode::OE4, O::OrchestrationCode::OE5, O::OrchestrationCode::OE6}) {
        CHECK_MESSAGE(report.Has(code), "[or-all-in-one-report] " << O::ToString(code) << " missing: " << report.detail);
    }
    CHECK_MESSAGE(report.KernelReportOf("Main") != nullptr, "[or-all-in-one-report] kernel: " << report.detail);
    CHECK_MESSAGE(CountLogged("configure time") == 1, "[or-oe1-second-unused] " << Joined());
    CHECK_MESSAGE(!Logged("configure ghost"), "[or-oe2-unused] " << Joined());
    CHECK_MESSAGE(Logged("configure lonely"), "[or-oe3-still-built] " << Joined());
}

TEST_CASE("O-E2: an instance in the wrong file and one of a library-only row") {
    Units units;
    units.Library("base").RowOnly(Row("window", O::UnitKind::Plugin)).Module("window", {}, {}, false).Module("base", {}, {}, false);
    O::ProductDescription d = TwoWorlds();
    auto built = O::Product::Build(std::move(d), units.Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-oe2] built with a wrong-file and a library-only instance");
    CHECK_MESSAGE(built.error().Count(O::OrchestrationCode::OE2) == 2, "[or-oe2] " << built.error().detail);
}

TEST_CASE("O-E3: missing, later (not reordered), a Module on a Plugin, a library dependency, a World leaving it out") {
    SUBCASE("later") {
        auto built = O::Product::Build(TwoWorlds(), Units().Module("authority", {}, {"time"}).Module("time").Take());
        REQUIRE_FALSE(static_cast<bool>(built));
        CHECK_MESSAGE(built.error().detail.find("comes later") != std::string::npos, "[or-oe3-later] " << built.error().detail);
    }
    SUBCASE("a Module on a Plugin") {
        auto built = O::Product::Build(TwoWorlds(), Units().Plugin("window").Module("ui", {}, {"window"}).Take());
        REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-oe3-plugin] built");
        CHECK_MESSAGE(built.error().detail.find("a Module does not depend on a Plugin") != std::string::npos,
                      "[or-oe3-plugin] the rule was not named: " << built.error().detail);
    }
    SUBCASE("a Plugin on a Module, and a library dependency, pass") {
        auto built = O::Product::Build(TwoWorlds(), Units().Library("kernel").Module("time", {}, {"kernel"}).Plugin("window", {}, {"time"}).Take());
        CHECK_MESSAGE(static_cast<bool>(built), "[or-oe3-library] " << Describe(built));
    }
    SUBCASE("a World that leaves the dependency out; another World having it does not count") {
        O::ProductDescription d = TwoWorlds();
        d.worlds[1].units = {"authority"};
        auto built = O::Product::Build(std::move(d), Units().Module("time").Module("authority", {}, {"time"}).Take());
        REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-oe3-world] built with a World leaving the dependency out");
        CHECK_MESSAGE(built.error().detail.find("not used by World 'Side'") != std::string::npos,
                      "[or-oe3-world] " << built.error().detail);
    }
}

TEST_CASE("O-E7: a prepare function that fails at startup comes in its own report") {
    O::ProductDescription d = TwoWorlds();
    d.worlds[0].prepare.push_back([](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        return GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "no config", {}, "config-missing"));
    });
    auto built = O::Product::Build(std::move(d), Units().Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-oe7] built despite the failing prepare");
    CHECK_MESSAGE(built.error().Has(O::OrchestrationCode::OE7), "[or-oe7] " << built.error().detail);
    CHECK_MESSAGE(built.error().detail.find("config-missing") != std::string::npos, "[or-oe7] " << built.error().detail);
}

TEST_CASE("O-2: the World context names each World; a Finish error is reported with its unit and World") {
    Calls calls;
    calls.finish = [](const K::FrozenRegistry&, O::WorldFinish& f) {
        if (f.WorldName() == "Side") f.Report("bad", "side only");
    };
    auto built = O::Product::Build(TwoWorlds(), Units().Module("net", calls).Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-finish-report] built despite a reported error");
    REQUIRE(built.error().unitErrors.size() == 1);
    CHECK_MESSAGE((built.error().unitErrors[0].unit == "net" && built.error().unitErrors[0].world == "Side" &&
                   built.error().unitErrors[0].code == "bad"),
                  "[or-finish-report] " << built.error().detail);
}

TEST_CASE("O-2: the contexts cannot be copied or moved out of their call (J-19, moved from FinishContext)") {
    static_assert(!std::is_copy_constructible_v<O::WorldFinish>);
    static_assert(!std::is_move_constructible_v<O::WorldFinish>);
    static_assert(!std::is_copy_constructible_v<O::WorldCheck>);
    static_assert(!std::is_copy_constructible_v<O::WorldSetup>);
    static_assert(!std::is_copy_constructible_v<O::ProductSetup>);
    CHECK(true);
}
