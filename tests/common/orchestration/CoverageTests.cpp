// Cases the G-8 check (Fable/high, build/evidence/gyo2-orchestration-skeleton/segment1/g8-check-fable-high.md)
// found missing: promised behaviour no test failed on. Each assertion has its own tag.

#include "SaveGame.hpp"

#include "AssertTestSupport.hpp"

using namespace OrchTest;
using namespace OrchTest::Save;

namespace {

O::WorldDescription Plain(std::string name, std::vector<std::string> frame = {"Tick"}) {
    return World(
        std::move(name),
        [](O::WorldSetup& s) {
            Log().push_back("build product@" + s.WorldName());
            Basics(s.Registrar(), {FrameClock("Tick"), Once("A"), Once("B"), Once("Bad")});
            s.Registrar().AddSystem<&Mark<'t'>>(In("MarkTick", "Tick"));
            s.Registrar().AddSystem<&Mark<'a'>>(In("MarkA", "A"));
            s.Registrar().AddSystem<&Mark<'b'>>(In("MarkB", "B"));
            s.Registrar().AddSystem<&Fail>(In("FailBad", "Bad"));
        },
        std::move(frame));
}

O::ProductDescription Two(std::vector<std::string> mainFrame = {"Tick"}, std::vector<std::string> sideFrame = {"Tick"}) {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(Plain("Main", std::move(mainFrame)));
    d.worlds.push_back(Plain("Side", std::move(sideFrame)));
    return d;
}

O::PrepareFn Refuse() {
    return [](K::World&) -> Engine::Base::Result<void, O::OrchestrationError> {
        return Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "no"));
    };
}

} // namespace

// ---- Startup report (O-5, O-E2, O-E3, O-E6)

TEST_CASE("O-5: a kernel error alone stops the build") {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", [](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick")});
        s.Registrar().AddSystem<&Mark<'x'>>(In("Broken", "Nowhere"));
    }));
    auto built = O::Product::Build(std::move(d), Units().Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-kernel-only] built despite the kernel error");
    CHECK_MESSAGE((built.error().errors.empty() && built.error().KernelReportOf("Main") != nullptr), "[or-kernel-only-report]");
}

TEST_CASE("O-E6: no World; a units entry the Product does not carry; the second of two same-named Worlds is not built") {
    O::ProductDescription none;
    none.name = "game";
    auto empty = O::Product::Build(std::move(none), Units().Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(empty), "[or-oe6-no-world] built with no World");
    CHECK_MESSAGE(empty.error().Has(O::OrchestrationCode::OE6), "[or-oe6-no-world] " << empty.error().detail);

    O::ProductDescription stray = Two();
    stray.worlds[1].units = {"ghost"};
    auto strayBuilt = O::Product::Build(std::move(stray), Units().Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(strayBuilt), "[or-oe6-not-carried] built");
    CHECK_MESSAGE(strayBuilt.error().detail.find("which the Product does not carry") != std::string::npos,
                  "[or-oe6-not-carried] " << strayBuilt.error().detail);

    Log().clear();
    O::ProductDescription dup = Two();
    dup.worlds[1].name = "Main";
    auto dupBuilt = O::Product::Build(std::move(dup), Units().Module("audio").Take());
    REQUIRE_FALSE(static_cast<bool>(dupBuilt));
    CHECK_MESSAGE(CountLogged("build audio@Main") == 1, "[or-oe6-dup-not-built] " << Joined());
}

TEST_CASE("O-E2: refused instances are not configured or built") {
    Log().clear();
    Units units;
    units.Library("base").RowOnly(Row("window", O::UnitKind::Plugin)).Module("window", {}, {}, false).Module("base", {}, {}, false);
    auto built = O::Product::Build(Two(), units.Take());
    REQUIRE_FALSE(static_cast<bool>(built));
    CHECK_MESSAGE((!Logged("configure window") && !Logged("configure base") && !Logged("build window@Main")),
                  "[or-oe2-unused] " << Joined());
}

TEST_CASE("O-E3: private dependencies count; a Plugin may depend on a Plugin") {
    Units privateDep;
    O::RegisterRow row = Row("net", O::UnitKind::Module);
    row.privateDepends = {"missing"};
    privateDep.RowOnly(row).Module("net", {}, {}, false);
    auto built = O::Product::Build(Two(), privateDep.Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-oe3-private] a missing private dependency passed");
    CHECK_MESSAGE(built.error().detail.find("'net' depends on 'missing'") != std::string::npos, "[or-oe3-private] " << built.error().detail);

    auto plugins = O::Product::Build(Two(), Units().Plugin("sdl").Plugin("imgui", {}, {"sdl"}).Take());
    CHECK_MESSAGE(static_cast<bool>(plugins), "[or-oe3-plugin-on-plugin] " << Describe(plugins));
}

TEST_CASE("O-5: a Check error names its unit") {
    Calls checker;
    checker.check = [](const K::FrozenRegistry&, O::WorldCheck& c) { c.Report("refused", "no"); };
    auto built = O::Product::Build(Two(), Units().Module("time").Module("authority", checker).Take());
    REQUIRE_FALSE(static_cast<bool>(built));
    REQUIRE(!built.error().unitErrors.empty());
    CHECK_MESSAGE(built.error().unitErrors[0].unit == "authority", "[or-check-unit-name] " << built.error().unitErrors[0].unit);
}

// ---- Lifecycle (O-2)

TEST_CASE("O-2: the product's own build comes after every unit's; a World with no unit and no product build is built") {
    Log().clear();
    auto built = O::Product::Build(Two(), Units().Module("time").Plugin("window").Take());
    ORCH_REQUIRE_BUILT(built);
    const auto at = [](const std::string& line) {
        return std::find(Log().begin(), Log().end(), line) - Log().begin();
    };
    CHECK_MESSAGE((at("build window@Main") < at("build product@Main") && at("build product@Main") < at("finish time@Main v1")),
                  "[or-product-build-last] " << Joined());

    O::ProductDescription bare;
    bare.name = "game";
    bare.worlds.push_back(World("Bare"));
    auto bareBuilt = O::Product::Build(std::move(bare), Units().Take());
    CHECK_MESSAGE(static_cast<bool>(bareBuilt), "[or-bare-world] " << Describe(bareBuilt));
}

// ---- Frame lists (O-1, O-E5)

TEST_CASE("O-1: units insert into a World's frame list; the order is the list's; a missing anchor is O-E5; an anchor that appears twice means the first") {
    Log().clear();
    Calls unit;
    unit.build = [](O::WorldSetup& s) {
        if (s.WorldName() != "Main") return;
        s.Frame().InsertBefore("Tick", "A");
        s.Frame().InsertAfter("Tick", "B");
    };
    O::ProductDescription d = Two({"Tick"}, {});
    auto built = O::Product::Build(std::move(d), Units().Module("ui", unit).Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined().find("a | t | b") != std::string::npos, "[or-frame-order] " << Joined());

    Calls lost;
    lost.build = [](O::WorldSetup& s) { s.Frame().InsertAfter("Nope", "A"); };
    auto refused = O::Product::Build(Two(), Units().Module("ui", lost).Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(refused), "[or-frame-anchor] built");
    CHECK_MESSAGE(refused.error().detail.find("frame list of World 'Main': no anchor 'Nope'") != std::string::npos,
                  "[or-frame-anchor] " << refused.error().detail);

    Log().clear();
    O::ProductDescription twice = Two({"A", "B", "A"}, {});
    Calls inserter;
    inserter.build = [](O::WorldSetup& s) {
        if (s.WorldName() == "Main") s.Frame().InsertAfter("A", "Tick");
    };
    auto twiceBuilt = O::Product::Build(std::move(twice), Units().Module("ui", inserter).Take());
    ORCH_REQUIRE_BUILT(twiceBuilt);
    REQUIRE(static_cast<bool>((*twiceBuilt)->RunFrame()));
    CHECK_MESSAGE(Joined().find("a | t | b | a") != std::string::npos, "[or-anchor-first] " << Joined());

    // InsertBefore resolves the same way: before the first "A", not the last.
    Log().clear();
    O::ProductDescription twiceBefore = Two({"A", "B", "A"}, {});
    Calls beforeInserter;
    beforeInserter.build = [](O::WorldSetup& s) {
        if (s.WorldName() == "Main") s.Frame().InsertBefore("A", "Tick");
    };
    auto twiceBeforeBuilt = O::Product::Build(std::move(twiceBefore), Units().Module("ui", beforeInserter).Take());
    ORCH_REQUIRE_BUILT(twiceBeforeBuilt);
    REQUIRE(static_cast<bool>((*twiceBeforeBuilt)->RunFrame()));
    CHECK_MESSAGE(Joined().find("t | a | b | a") != std::string::npos, "[or-anchor-first] " << Joined());
}

TEST_CASE("O-1: a failing frame schedule stops RunFrame with ScheduleFailed") {
    Log().clear();
    auto built = O::Product::Build(Two({"Bad", "Tick"}), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    auto ran = (*built)->RunFrame();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(ran), "[or-frame-error] RunFrame succeeded");
    CHECK_MESSAGE(ran.error().code == O::OrchestrationErrorCode::ScheduleFailed, "[or-frame-error] code");
    CHECK_MESSAGE(!Logged("t"), "[or-frame-error-stops] " << Joined());
}

// ---- Interrupted and empty Worlds, unknown names

TEST_CASE("O-8: RunSchedule on an interrupted or empty World; another World still runs; RunFrame reports an interrupted World with an empty frame list") {
    Log().clear();
    auto built = O::Product::Build(Two({"Tick"}, {}), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("Side", "Bad")));
    auto interrupted = p.RunSchedule("Side", "A");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(interrupted), "[or-runschedule-interrupted] ran");
    CHECK_MESSAGE(interrupted.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-runschedule-interrupted] code");
    CHECK_MESSAGE(static_cast<bool>(p.RunSchedule("Main", "A")), "[or-other-world-runs]");
    auto frame = p.RunFrame();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(frame), "[or-runframe-interrupted-empty] RunFrame succeeded");
    CHECK_MESSAGE(frame.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-runframe-interrupted-empty] code");

    REQUIRE_FALSE(static_cast<bool>(p.ReplaceWorld("Side", O::ReplaceRequest{Refuse(), {}, O::ReplaceOrder::DropOldFirst})));
    auto missing = p.RunSchedule("Side", "A");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(missing), "[or-runschedule-missing] ran");
    CHECK_MESSAGE(missing.error().code == O::OrchestrationErrorCode::WorldMissing, "[or-runschedule-missing] code");
    auto insert = p.InsertSystems("Side", "console", [](K::Registrar&) {});
    REQUIRE_FALSE(static_cast<bool>(insert));
    CHECK_MESSAGE(insert.error().code == O::OrchestrationErrorCode::WorldMissing, "[or-insert-empty-place]");
}

TEST_CASE("O-8: unknown World names: UnknownWorld from RunSchedule, ReplaceWorld and InsertSystems") {
    auto built = O::Product::Build(Two(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    auto ran = p.RunSchedule("Nope", "A");
    CHECK_MESSAGE((!ran && ran.error().code == O::OrchestrationErrorCode::UnknownWorld), "[or-unknown-world-run]");
    auto replaced = p.ReplaceWorld("Nope", O::ReplaceRequest{});
    CHECK_MESSAGE((!replaced && replaced.error().code == O::OrchestrationErrorCode::UnknownWorld), "[or-unknown-world-replace]");
    auto inserted = p.InsertSystems("Nope", "console", [](K::Registrar&) {});
    CHECK_MESSAGE((!inserted && inserted.error().code == O::OrchestrationErrorCode::UnknownWorld), "[or-unknown-world-insert]");
}

// ---- Runner and game initialization (O-1, O-3)

TEST_CASE("O-3: the default runner reports a failing frame") {
    auto built = O::Product::Build(Two({"Bad"}), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    const O::RunnerExit exit = (*built)->Run();
    CHECK_MESSAGE(exit.code == O::RunnerExitCode::Failure, "[or-runner-default-failure] " << exit.reason);
}

TEST_CASE("O-1: a failed game initialization counts as run (RunGameInit afterwards asserts)") {
    O::ProductDescription d = Two();
    d.gameInit = {"Bad"};
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE_FALSE(static_cast<bool>((*built)->RunFrame()));
    GYO_CHECK_ASSERTS((*built)->RunGameInit());
}

// ---- Replacement (O-8) and carry over (O-10), with the save game

TEST_CASE("O-8: replacing a World that is not the first leaves the first alone and runs no game initialization again") {
    BootRuns() = 0;
    O::ProductDescription d = Game();
    d.worlds.push_back(Plain("Side"));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    p.World("Main").GetWorld().ResourceMut<KeyBindings>()->jump = 3;
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Side", O::ReplaceRequest{})));
    const KeyBindings* kb = p.World("Main").GetWorld().Resource<KeyBindings>();
    REQUIRE_MESSAGE(kb != nullptr, "[or-replace-other] the first World lost KeyBindings");
    CHECK_MESSAGE(kb->jump == 3, "[or-replace-other] the first World was touched");
    CHECK_MESSAGE(BootRuns() == 1, "[or-replace-other-no-gi] " << BootRuns());
}

TEST_CASE("O-8: replacing before game initialization ran: on the new first World, or on the first World before another is swapped in") {
    BootRuns() = 0;
    {
        auto built = O::Product::Build(Game(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        REQUIRE(static_cast<bool>((*built)->ReplaceWorld("Main", O::ReplaceRequest{})));
        CHECK_MESSAGE((BootRuns() == 1 && (*built)->GameInitRan()), "[or-replace-first-before-gi] " << BootRuns());
        CHECK_MESSAGE(JumpOf(**built) == 1, "[or-replace-first-before-gi] on the new World");
    }
    BootRuns() = 0;
    {
        O::ProductDescription d = Game();
        d.worlds.push_back(Plain("Side"));
        auto built = O::Product::Build(std::move(d), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        REQUIRE(static_cast<bool>((*built)->ReplaceWorld("Side", O::ReplaceRequest{})));
        CHECK_MESSAGE((BootRuns() == 1 && (*built)->GameInitRan()), "[or-replace-other-before-gi] " << BootRuns());
    }
}

TEST_CASE("O-8: a failing beforeSwap gives the carried value back and warns once; the report of a failed registration is carried") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    p.World("Main").GetWorld().ResourceMut<KeyBindings>()->jump = 4;
    SaveData broken = TheSave();
    broken.unreadable = true;
    WarningCounter warnings;
    auto failed = p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(broken), {"Load"}});
    REQUIRE_FALSE(static_cast<bool>(failed));
    const KeyBindings* kb = p.World("Main").GetWorld().Resource<KeyBindings>();
    REQUIRE_MESSAGE(kb != nullptr, "[or-beforeswap-giveback] KeyBindings is gone");
    CHECK_MESSAGE(kb->jump == 4, "[or-beforeswap-giveback] " << kb->jump);
    CHECK_MESSAGE(warnings.Warnings() == 1, "[or-beforeswap-warn] " << warnings.Warnings());

    int checks = 0;
    Calls strategy;
    strategy.check = [&](const K::FrozenRegistry&, O::WorldCheck& c) {
        if (++checks == 2) c.Report("strategy-refused", "the second registration");
    };
    auto other = O::Product::Build(Game(), Units().Module("authority", strategy).Take());
    ORCH_REQUIRE_BUILT(other);
    auto refused = (*other)->ReplaceWorld("Main", O::ReplaceRequest{});
    REQUIRE_FALSE(static_cast<bool>(refused));
    CHECK_MESSAGE(refused.error().detail.find("strategy-refused") != std::string::npos, "[or-replace-report] " << refused.error().detail);
}

TEST_CASE("O-8: a registration that differs: the new frame list and prepare are used; a list error refuses") {
    Log().clear();
    int registrations = 0;
    O::ProductDescription d = Game();
    d.worlds[0].build = [&](O::WorldSetup& s) {
        Register(s);
        ++registrations;
        if (registrations == 2) {
            s.Registrar().Schedule(Once("Second"));
            s.Registrar().AddSystem<&Mark<'2'>>(In("MarkSecond", "Second"));
            s.Frame().Append("Second");
            s.Prepare([](K::World&) -> Engine::Base::Result<void, O::OrchestrationError> {
                Log().push_back("prepare 2");
                return {};
            });
        }
        if (registrations == 3) s.Frame().Append("Ghost");
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    CHECK_MESSAGE(Logged("prepare 2"), "[or-swap-new-prepare] " << Joined());
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(Logged("2"), "[or-swap-new-frame] " << Joined());
    auto listError = p.ReplaceWorld("Main", O::ReplaceRequest{});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(listError), "[or-replace-list-error] replaced with a frame name that is no schedule");
    CHECK_MESSAGE(listError.error().code == O::OrchestrationErrorCode::ReplaceRegistrationFailed, "[or-replace-list-error] code");
}

TEST_CASE("O-10: a value carries over from an interrupted World") {
    O::ProductDescription d = Game();
    d.worlds[0].build = [](O::WorldSetup& s) {
        Register(s);
        s.Registrar().AddSystem<&Fail>(In("FailJunk", "Junk"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    p.World("Main").GetWorld().ResourceMut<KeyBindings>()->jump = 5;
    REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("Main", "Junk")));
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    const KeyBindings* kb = p.World("Main").GetWorld().Resource<KeyBindings>();
    REQUIRE_MESSAGE(kb != nullptr, "[or-carry-from-interrupted] gone");
    CHECK_MESSAGE(kb->jump == 5, "[or-carry-from-interrupted] " << kb->jump);
}

TEST_CASE("O-9: the refusal warning names the World and the owner") {
    O::ProductDescription d = Game();
    d.worlds[0].dynamicInsert = false;
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    WarningCounter warnings;
    REQUIRE_FALSE(static_cast<bool>((*built)->InsertSystems("Main", "console", [](K::Registrar&) {})));
    CHECK_MESSAGE((WarningCounter::Last().find("'Main'") != std::string::npos && WarningCounter::Last().find("'console'") != std::string::npos),
                  "[or-insert-warning-text] " << WarningCounter::Last());
}

// ---- Kernel rules that game initialization schedules are under (G-7, groups A and E)

K::SystemResult WriteTagBoot(K::Query<K::Write<Tag>>) { return {}; }
K::SystemResult WriteTagFrame(K::Query<K::Write<Tag>>) { return {}; }

TEST_CASE("O-1: until batch 3 a game initialization schedule belongs to a domain (D-E12); game initialization and the frame both writing one column is D-E2") {
    Log().clear();
    O::ProductDescription noDomain;
    noDomain.name = "game";
    noDomain.gameInit = {"Boot"};
    noDomain.worlds.push_back(World("Main", [](O::WorldSetup& s) {
        K::ScheduleOptions boot = Once("Boot");
        boot.domain = K::Domain::None;
        Basics(s.Registrar(), {boot});
    }));
    auto refused = O::Product::Build(std::move(noDomain), Units().Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(refused), "[or-gi-domain] built");
    CHECK_MESSAGE(refused.error().detail.find("'Boot' belongs to no clock domain") != std::string::npos, "[or-gi-domain] " << refused.error().detail);

    O::ProductDescription twoWriters;
    twoWriters.name = "game";
    twoWriters.gameInit = {"Boot"};
    twoWriters.worlds.push_back(World("Main", [](O::WorldSetup& s) {
        Basics(s.Registrar(), {Once("Boot"), FrameClock("Tick")});
        s.Registrar().AddSystem<&WriteTagBoot>(In("WriteBoot", "Boot"));
        s.Registrar().AddSystem<&WriteTagFrame>(In("WriteFrame", "Tick"));
    }, {"Tick"}));
    auto counted = O::Product::Build(std::move(twoWriters), Units().Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(counted), "[or-gi-writer-counted] built");
    CHECK_MESSAGE(counted.error().detail.find("D-E2") != std::string::npos, "[or-gi-writer-counted] " << counted.error().detail);
}
