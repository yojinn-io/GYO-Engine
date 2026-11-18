// O-1 (game initialization), O-3 (runner), the frame list and interrupted
// Worlds (declaration 7.3; J-58 rewritten).

#include "OrchestrationTestSupport.hpp"

#include "AssertTestSupport.hpp"

using namespace OrchTest;

namespace {

// Main: one-off schedules A, B, C, Late and a frame clock Tick; Side: its own A and Tick.
O::ProductDescription Game(std::vector<std::string> gameInit = {"A", "C"}) {
    O::ProductDescription d;
    d.name = "game";
    d.gameInit = std::move(gameInit);
    d.worlds.push_back(World(
        "Main",
        [](O::WorldSetup& s) {
            Basics(s.Registrar(), {Once("A"), Once("B"), Once("C"), Once("Late"), FrameClock("Tick")});
            s.Registrar().AddSystem<&Mark<'a'>>(In("MarkA", "A"));
            s.Registrar().AddSystem<&Mark<'b'>>(In("MarkB", "B"));
            s.Registrar().AddSystem<&Mark<'c'>>(In("MarkC", "C"));
            s.Registrar().AddSystem<&Mark<'l'>>(In("MarkLate", "Late"));
            s.Registrar().AddSystem<&Mark<'t'>>(In("MarkTick", "Tick"));
        },
        {"Tick"}));
    d.worlds.push_back(World(
        "Side",
        [](O::WorldSetup& s) {
            Basics(s.Registrar(), {Once("A"), FrameClock("Tick")});
            s.Registrar().AddSystem<&Mark<'x'>>(In("SideA", "A"));
            s.Registrar().AddSystem<&Mark<'s'>>(In("SideTick", "Tick"));
        },
        {"Tick"}));
    return d;
}

K::SystemResult FlipSettings(K::ResMut<K::KernelSettings> settings) {
    settings->breakOnKernelError = true;
    return {};
}
K::SystemResult ReadSettings(K::Res<K::KernelSettings> settings) {
    Log().push_back(settings->breakOnKernelError ? "break on" : "break off");
    return {};
}

} // namespace

TEST_CASE("O-1: insertions place schedules; game initialization runs once, on the first World, before the first frame") {
    Log().clear();
    O::ProductDescription d = Game();
    d.configure = [](O::ProductSetup& s) {
        s.GameInit().InsertBefore("C", "B");
        s.GameInit().InsertAfter("C", "Late");
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& product = **built;
    CHECK((product.GameInitList() == std::vector<std::string>{"A", "B", "C", "Late"}));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(Joined() == "a | b | c | l | t | s | t | s", "[or-gi-order-once] " << Joined());
    CHECK_MESSAGE(!Logged("x"), "[or-gi-first-world] the Side World ran its A");
}

TEST_CASE("O-1: two insertions before one anchor: the later one ends next to it; a missing anchor is O-E5; a name twice runs twice") {
    Log().clear();
    O::ProductDescription d = Game({"A", "C"});
    Calls first;
    first.configure = [](O::ProductSetup& s) { s.GameInit().InsertBefore("C", "B"); };
    Calls second;
    second.configure = [](O::ProductSetup& s) {
        s.GameInit().InsertBefore("C", "Late");
        s.GameInit().Append("A");
    };
    auto built = O::Product::Build(std::move(d), Units().Module("first", first).Module("second", second).Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(((*built)->GameInitList() == std::vector<std::string>{"A", "B", "Late", "C", "A"}), "[or-gi-insert-same-anchor]");
    REQUIRE(static_cast<bool>((*built)->RunGameInit()));
    CHECK_MESSAGE(Joined() == "configure first | configure second | build first@Main | build second@Main | finish first@Main v1 | "
                              "finish second@Main v1 | check first@Main | check second@Main | build first@Side | build second@Side | "
                              "finish first@Side v1 | finish second@Side v1 | check first@Side | check second@Side | a | b | l | c | a",
                  "[or-gi-twice] " << Joined());

    O::ProductDescription missing = Game();
    missing.configure = [](O::ProductSetup& s) { s.GameInit().InsertAfter("Nope", "B"); };
    auto refused = O::Product::Build(std::move(missing), Units().Take());
    REQUIRE_FALSE(static_cast<bool>(refused));
    CHECK_MESSAGE(refused.error().Has(O::OrchestrationCode::OE5), "[or-gi-anchor] " << refused.error().detail);
    O::ProductDescription unknown = Game({"A", "Nowhere"});
    auto unknownBuilt = O::Product::Build(std::move(unknown), Units().Take());
    REQUIRE_FALSE(static_cast<bool>(unknownBuilt));
    CHECK_MESSAGE(unknownBuilt.error().Has(O::OrchestrationCode::OE5), "[or-gi-not-schedule] " << unknownBuilt.error().detail);
}

TEST_CASE("O-1: RunSchedule runs game initialization first; explicit RunGameInit first then RunFrame runs it once") {
    Log().clear();
    {
        auto built = O::Product::Build(Game(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        REQUIRE(static_cast<bool>((*built)->RunSchedule("Main", "B")));
        CHECK_MESSAGE(Joined() == "a | c | b", "[or-gi-runschedule] " << Joined());
    }
    Log().clear();
    {
        auto built = O::Product::Build(Game(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        REQUIRE(static_cast<bool>((*built)->RunGameInit()));
        REQUIRE(static_cast<bool>((*built)->RunFrame()));
        CHECK_MESSAGE(Joined() == "a | c | t | s", "[or-gi-explicit-first] " << Joined());
    }
}

TEST_CASE("O-1: RunGameInit a second time, or after RunFrame: GYO_ASSERT") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunGameInit()));
    GYO_CHECK_ASSERTS((*built)->RunGameInit());
    auto other = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(other);
    REQUIRE(static_cast<bool>((*other)->RunFrame()));
    GYO_CHECK_ASSERTS((*other)->RunGameInit());
}

TEST_CASE("O-1: a failing game initialization schedule: RunFrame returns it, no frame schedule runs, the handler once, then WorldInterrupted") {
    Log().clear();
    O::ProductDescription d = Game({"A", "B"});
    d.worlds[0].build = [](O::WorldSetup& s) {
        Basics(s.Registrar(), {Once("A"), Once("B"), FrameClock("Tick")});
        s.Registrar().AddSystem<&Mark<'a'>>(In("MarkA", "A"));
        s.Registrar().AddSystem<&Fail>(In("FailB", "B"));
        s.Registrar().AddSystem<&Mark<'t'>>(In("MarkTick", "Tick"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    int handled = 0;
    (*built)->SetErrorHandler("Main", [&](const K::KernelError&) { ++handled; });
    auto ran = (*built)->RunFrame();
    REQUIRE_FALSE(static_cast<bool>(ran));
    CHECK_MESSAGE(ran.error().code == O::OrchestrationErrorCode::ScheduleFailed, "[or-gi-error] " << ran.error().message);
    CHECK_MESSAGE(!Logged("t"), "[or-gi-error-no-frame] " << Joined());
    CHECK_MESSAGE(handled == 1, "[or-gi-error-once] the handler ran " << handled << " times");
    auto again = (*built)->RunFrame();
    REQUIRE_FALSE(static_cast<bool>(again));
    CHECK_MESSAGE(again.error().code == O::OrchestrationErrorCode::WorldInterrupted, "[or-interrupted] " << again.error().message);
}

TEST_CASE("O-1: an empty list: RunFrame runs the frame list directly; a setting changed in game initialization counts from the next schedule") {
    Log().clear();
    O::ProductDescription d = Game({"A", "B"});
    d.worlds.resize(1);
    d.worlds[0].build = [](O::WorldSetup& s) {
        Basics(s.Registrar(), {Once("A"), Once("B"), FrameClock("Tick")});
        s.Registrar().AddSystem<&FlipSettings>(In("Flip", "A"));
        s.Registrar().AddSystem<&ReadSettings>(In("ReadB", "B"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    (*built)->World("Main").GetWorld().SetResource(K::KernelSettings{});
    REQUIRE(static_cast<bool>((*built)->RunGameInit()));
    CHECK_MESSAGE(Joined() == "break on", "[or-gi-settings] B (its own Run) saw A's change: " << Joined());

    Log().clear();
    O::ProductDescription empty = Game({});
    auto plain = O::Product::Build(std::move(empty), Units().Take());
    ORCH_REQUIRE_BUILT(plain);
    REQUIRE(static_cast<bool>((*plain)->RunFrame()));
    CHECK_MESSAGE(Joined() == "t | s", "[or-gi-empty] " << Joined());
}

TEST_CASE("O-3: no runner runs one frame; an installed runner is called once with the Product; two are O-E4; Run twice asserts") {
    Log().clear();
    {
        auto built = O::Product::Build(Game(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        const O::RunnerExit exit = (*built)->Run();
        CHECK_MESSAGE(exit.code == O::RunnerExitCode::Success, "[or-runner-default] " << exit.reason);
        CHECK_MESSAGE(Joined() == "a | c | t | s", "[or-runner-default] " << Joined());
        GYO_CHECK_ASSERTS((*built)->Run());
    }
    {
        O::Product* seen = nullptr;
        int calls = 0;
        Calls installer;
        installer.configure = [&](O::ProductSetup& s) {
            s.InstallRunner("loop", [&](O::Product& p) {
                seen = &p;
                ++calls;
                return O::RunnerExit{O::RunnerExitCode::Failure, "stop"};
            });
        };
        auto built = O::Product::Build(Game(), Units().Plugin("window", installer).Take());
        ORCH_REQUIRE_BUILT(built);
        const O::RunnerExit exit = (*built)->Run();
        CHECK_MESSAGE((calls == 1 && seen == built->get() && exit.reason == "stop"), "[or-runner-installed]");
    }
}
