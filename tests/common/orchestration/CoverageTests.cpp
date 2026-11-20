// Cases the G-8 check (Fable/high, build/evidence/gyo2-orchestration-skeleton/segment1/g8-check-fable-high.md)
// found missing: promised behaviour no test failed on. Each assertion has its own tag.

#include "SaveGame.hpp"

#include "AssertTestSupport.hpp"

#include <new>
#include <stdexcept>

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
    // The default skeleton off (time-events declaration 6.2): with it the orchestration layer's own Register would
    // already make the World's Registry freeze, and the case would no longer test the product's own Register.
    bare.worlds.back().skeleton = O::ScheduleSkeleton::Off;
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

// One broken World ends the Product (v8-arch-gyo2-broken-world-ends-product):
// every entry point checks every World first, runs nothing and changes nothing.

TEST_CASE("O-8: one interrupted World ends the Product: RunSchedule on another World, RunGameInit and InsertSystems run nothing; the failure says what broke") {
    Log().clear();
    // The default skeleton off (time-events declaration 6.2): the sync point number below counts the frame's Tick only.
    O::ProductDescription d = Two();
    for (O::WorldDescription& w : d.worlds) w.skeleton = O::ScheduleSkeleton::Off;
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE(p.Failure() == nullptr);
    const std::uint64_t sideEpoch = p.World("Side").GetWorld().SyncEpoch();
    auto broke = p.RunSchedule("Side", "Bad");
    REQUIRE_FALSE(static_cast<bool>(broke));
    CHECK_MESSAGE(broke.error().code == O::OrchestrationErrorCode::ScheduleFailed, "[or-ended-first-error] code");
    REQUIRE_MESSAGE((broke.error().failure.has_value() && p.Failure() != nullptr), "[or-ended-failure-set]");
    const O::WorldFailure& f = *p.Failure();
    CHECK_MESSAGE((f.product == "game" && f.world == "Side" && f.kind == O::WorldFailureKind::Interrupted), "[or-ended-failure-who] " << f.Text());
    CHECK_MESSAGE((f.entry == "RunSchedule" && f.schedule == "Bad"), "[or-ended-failure-where] " << f.Text());
    // The World's sync point number before the schedule ran (G6: no domain, no tick; the frame's Tick applied one).
    CHECK_MESSAGE((f.syncEpoch == sideEpoch && sideEpoch == 1), "[or-ended-failure-tick] " << f.Text());
    CHECK_MESSAGE((f.kernelCode == K::KernelErrorCode::SystemFailed && f.where.find("FailBad") != std::string::npos), "[or-ended-failure-kernel] " << f.Text());
    CHECK_MESSAGE(f.lastSchedule == "Tick", "[or-ended-failure-last] " << f.Text());
    Log().clear();
    auto other = p.RunSchedule("Main", "A");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(other), "[or-ended-runschedule-other] the healthy World ran");
    CHECK_MESSAGE(other.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-ended-runschedule-other] code");
    CHECK_MESSAGE((other.error().failure.has_value() && other.error().failure->world == "Side"), "[or-ended-names-broken] " << other.error().message);
    CHECK_MESSAGE(!Logged("a"), "[or-ended-runschedule-other] " << Joined());
    auto insert = p.InsertSystems("Main", "console", [](K::Registrar&) {});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(insert), "[or-ended-no-insert] inserted");
    CHECK_MESSAGE(insert.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-ended-no-insert] code");
    auto unknown = p.RunSchedule("Nowhere", "A");
    CHECK_MESSAGE((!unknown && unknown.error().code == O::OrchestrationErrorCode::WorldInterrupted), "[or-ended-before-name]");
}

TEST_CASE("O-8: with a broken World the whole frame does not run: a World before it does not advance") {
    Log().clear();
    auto built = O::Product::Build(Two(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("Side", "Bad")));
    const std::uint64_t mainEpoch = p.World("Main").GetWorld().SyncEpoch();
    Log().clear();
    auto frame = p.RunFrame();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(frame), "[or-ended-no-half-frame] RunFrame succeeded");
    CHECK_MESSAGE(frame.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-ended-no-half-frame] code");
    CHECK_MESSAGE(!Logged("t"), "[or-ended-no-half-frame] the first World ran its frame: " << Joined());
    CHECK_MESSAGE(p.World("Main").GetWorld().SyncEpoch() == mainEpoch, "[or-ended-no-half-frame-tick]");
}

TEST_CASE("O-8: a replacement does not mend a broken World: an interrupted one, an empty place; a BuildNewFirst failure ends nothing") {
    SUBCASE("interrupted") {
        auto built = O::Product::Build(Two(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("Side", "Bad")));
        for (const O::ReplaceOrder order : {O::ReplaceOrder::BuildNewFirst, O::ReplaceOrder::DropOldFirst}) {
            CAPTURE(static_cast<int>(order));
            auto replaced = p.ReplaceWorld("Side", O::ReplaceRequest{{}, {}, order});
            REQUIRE_FALSE_MESSAGE(static_cast<bool>(replaced), "[or-ended-no-replace] the interrupted World was replaced");
            CHECK_MESSAGE(replaced.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-ended-no-replace] code");
            CHECK_MESSAGE((p.HasWorld("Side") && p.World("Side").GetWorld().State() == K::WorldState::Interrupted), "[or-ended-no-replace-kept]");
            CHECK_MESSAGE(!p.ReplaceWorld("Main", O::ReplaceRequest{{}, {}, order}), "[or-ended-no-replace-other]");
        }
    }
    SUBCASE("empty place") {
        Log().clear();
        auto built = O::Product::Build(Two(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        REQUIRE(static_cast<bool>(p.RunFrame()));
        auto failed = p.ReplaceWorld("Side", O::ReplaceRequest{Refuse(), {}, O::ReplaceOrder::DropOldFirst});
        REQUIRE_FALSE(static_cast<bool>(failed));
        CHECK_MESSAGE(failed.error().code == O::OrchestrationErrorCode::ReplacePrepareFailed, "[or-ended-drop-code]");
        REQUIRE_MESSAGE((failed.error().failure.has_value() && p.Failure() != nullptr), "[or-ended-drop-failure]");
        const O::WorldFailure& f = *p.Failure();
        CHECK_MESSAGE((f.world == "Side" && f.kind == O::WorldFailureKind::ReplacementFailed && f.entry == "ReplaceWorld" &&
                       f.schedule.empty() && f.where.find("ReplacePrepareFailed") != std::string::npos && f.lastSchedule == "Tick"),
                      "[or-ended-drop-failure-fields] " << f.Text());
        Log().clear();
        auto other = p.RunSchedule("Main", "A");
        CHECK_MESSAGE((!other && other.error().code == O::OrchestrationErrorCode::WorldMissing && !Logged("a")), "[or-ended-missing-runs-nothing] " << Joined());
        for (const O::ReplaceOrder order : {O::ReplaceOrder::BuildNewFirst, O::ReplaceOrder::DropOldFirst}) {
            CAPTURE(static_cast<int>(order));
            auto refill = p.ReplaceWorld("Side", O::ReplaceRequest{{}, {}, order});
            REQUIRE_FALSE_MESSAGE(static_cast<bool>(refill), "[or-ended-no-refill] the empty place was filled");
            CHECK_MESSAGE(refill.error().code == O::OrchestrationErrorCode::WorldMissing, "[or-ended-no-refill] code");
            CHECK_MESSAGE(!p.HasWorld("Side"), "[or-ended-no-refill]");
        }
    }
    SUBCASE("BuildNewFirst failure") {
        auto built = O::Product::Build(Two(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        auto failed = p.ReplaceWorld("Side", O::ReplaceRequest{Refuse(), {}, O::ReplaceOrder::BuildNewFirst});
        REQUIRE_FALSE(static_cast<bool>(failed));
        CHECK_MESSAGE((!failed.error().failure.has_value() && p.Failure() == nullptr), "[or-build-new-fail-goes-on]");
        CHECK(static_cast<bool>(p.RunFrame()));
        // The new World ran nothing yet: the old World's last schedule is not its.
        REQUIRE(static_cast<bool>(p.ReplaceWorld("Side", O::ReplaceRequest{})));
        REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("Side", "Bad")));
        REQUIRE(p.Failure() != nullptr);
        CHECK_MESSAGE(p.Failure()->lastSchedule.empty(), "[or-failure-last-new-world] " << p.Failure()->Text());
    }
}

TEST_CASE("O-8: the failure is returned, not logged: later refusals carry the same failure; Text() names every part") {
    // The default skeleton off (time-events declaration 6.2): "sync point 1" counts the frame's Tick only.
    O::ProductDescription d = Two();
    for (O::WorldDescription& w : d.worlds) w.skeleton = O::ScheduleSkeleton::Off;
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    WarningCounter log;
    auto broke = p.RunSchedule("Side", "Bad");
    REQUIRE_FALSE(static_cast<bool>(broke));
    auto frame = p.RunFrame();
    auto other = p.RunSchedule("Main", "A");
    REQUIRE((!frame && !other && broke.error().failure && frame.error().failure && other.error().failure));
    CHECK_MESSAGE((log.Errors() == 0 && log.Warnings() == 0), "[or-failure-not-logged] errors " << log.Errors() << " warnings " << log.Warnings());
    const std::string text = broke.error().failure->Text();
    CHECK_MESSAGE((frame.error().failure->Text() == text && other.error().failure->Text() == text && p.Failure()->Text() == text),
                  "[or-failure-same] " << frame.error().failure->Text());
    CHECK_MESSAGE(frame.error().detail == text, "[or-failure-detail] " << frame.error().detail);
    for (const char* part : {"'game'", "'Side'", "Interrupted", "RunSchedule", "'Bad'", "sync point 1", "SystemFailed", "'Tick'", "FailBad"}) {
        CHECK_MESSAGE(text.find(part) != std::string::npos, "[or-failure-text] " << part << " missing: " << text);
    }
}

TEST_CASE("O-8: a World interrupted outside the Product ends it too; RunGameInit checks first and is not counted as run") {
    auto built = O::Product::Build(Two(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE_FALSE(static_cast<bool>(p.World("Side").Run("Bad")));
    auto init = p.RunGameInit();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(init), "[or-ended-gameinit] RunGameInit succeeded");
    CHECK_MESSAGE(init.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-ended-gameinit] code");
    CHECK_MESSAGE(!p.GameInitRan(), "[or-ended-gameinit] counted as run");
    REQUIRE(p.Failure() != nullptr);
    CHECK_MESSAGE((p.Failure()->world == "Side" && p.Failure()->entry == "outside the Product" && p.Failure()->lastSchedule.empty() &&
                   !p.Failure()->kernelCode.has_value()),
                  "[or-ended-outside-fields] " << p.Failure()->Text());
    CHECK_MESSAGE(!p.RunFrame(), "[or-ended-outside] RunFrame succeeded");
}

namespace {

K::SystemResult ThrowAlloc(K::Query<K::Read<Tag>>) {
    throw std::bad_alloc();
}

} // namespace

TEST_CASE("O-8: std::bad_alloc leaving a schedule ends the Product as ResourceExhausted, recorded by the entry point") {
    O::ProductDescription d = Two();
    d.worlds[1].build = [](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick"), Once("Boom")});
        s.Registrar().AddSystem<&ThrowAlloc>(In("ThrowAlloc", "Boom"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_THROWS_AS(static_cast<void>(p.RunSchedule("Side", "Boom")), std::bad_alloc);
    REQUIRE_MESSAGE(p.Failure() != nullptr, "[or-run-throw] no failure recorded");
    CHECK_MESSAGE((p.Failure()->kind == O::WorldFailureKind::ResourceExhausted && p.Failure()->entry == "RunSchedule" &&
                   p.Failure()->schedule == "Boom"),
                  "[or-run-throw-kind] " << p.Failure()->Text());
    CHECK_MESSAGE(!p.RunFrame(), "[or-run-throw-runs-nothing]");
}

TEST_CASE("O-8: an exception that leaves a DropOldFirst replacement ends the Product; std::bad_alloc is ResourceExhausted") {
    for (const bool alloc : {false, true}) {
        CAPTURE(alloc);
        auto built = O::Product::Build(Two(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        bool thrown = false;
        try {
            static_cast<void>(p.ReplaceWorld("Side", O::ReplaceRequest{[alloc](K::World&) -> Engine::Base::Result<void, O::OrchestrationError> {
                if (alloc) throw std::bad_alloc();
                throw std::runtime_error("disk gone");
            }, {}, O::ReplaceOrder::DropOldFirst}));
        } catch (...) {
            thrown = true;
        }
        REQUIRE(thrown);
        REQUIRE_MESSAGE(p.Failure() != nullptr, "[or-ended-throw] no failure recorded");
        CHECK_MESSAGE(p.Failure()->kind == (alloc ? O::WorldFailureKind::ResourceExhausted : O::WorldFailureKind::ReplacementFailed),
                      "[or-ended-throw-kind] " << p.Failure()->Text());
        CHECK_MESSAGE(!p.RunFrame(), "[or-ended-throw-runs-nothing]");
    }
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

TEST_CASE("O-8: a failing beforeSwap gives the carried value back and logs nothing; the report of a failed registration is carried") {
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
    // Error levels D2: the orchestration layer logs nothing; the error went back to the caller.
    CHECK_MESSAGE(warnings.Warnings() == 0, "[or-beforeswap-silent] " << warnings.Warnings());

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

TEST_CASE("O-9: the refusal error names the World and the owner; nothing is logged") {
    O::ProductDescription d = Game();
    d.worlds[0].dynamicInsert = false;
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    WarningCounter warnings;
    auto refused = (*built)->InsertSystems("Main", "console", [](K::Registrar&) {});
    REQUIRE_FALSE(static_cast<bool>(refused));
    // Error levels D2: the returned error names the World and the owner, so whoever handles it can say so.
    CHECK_MESSAGE((refused.error().message.find("'Main'") != std::string::npos && refused.error().message.find("'console'") != std::string::npos),
                  "[or-insert-error-text] " << refused.error().message);
    CHECK_MESSAGE(warnings.Warnings() == 0, "[or-insert-silent] " << warnings.Warnings());
}

// ---- Kernel rules that game initialization schedules are under (G-7, groups A and E)

K::SystemResult WriteTagBoot(K::Query<K::Write<Tag>>) { return {}; }
K::SystemResult WriteTagFrame(K::Query<K::Write<Tag>>) { return {}; }

// "A game initialization schedule belongs to a domain" (D-E12, the O-1 transition sentence) left with the clock
// in batch 3 (G2, v8-arch-gyo2-clock-out-of-kernel).
TEST_CASE("O-1: game initialization and the frame both writing one column is D-E2") {
    Log().clear();
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
