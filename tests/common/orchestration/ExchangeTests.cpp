// Exchange functions (docs/architecture/target/v2/exchange-declaration.zh-Hant.md, section 5.1
// "交換函式的登記" and section 10 cases X-1 to X-8): registered in Build, called by RunFrame before
// and after each World's round, kept until the World is replaced or destroyed.
// Each CHECK carries a tag in brackets ("[or-exch-...]"): the batch's mutants expect it
// (docs/architecture/target/v2/scripts/orchestration_mutations.json).

#include "OrchestrationTestSupport.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using namespace OrchTest;

namespace {

using PResult = ::GYO::Engine::Base::Result<void, O::OrchestrationError>;

// An exchange function that logs "<side> <who>@<World>".
O::ExchangeFn Logs(std::string what) {
    return [what](K::World& w) -> PResult {
        Log().push_back(what + "@" + std::string(w.Name()));
        return {};
    };
}

PResult Refuse(std::string code) {
    return ::GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "the exchange refused", {},
                                                          std::move(code)));
}

// Two Worlds A and B; each frame list runs "Tick" (logs 'a' or 'b'). The product's own build of each
// World registers "in game" and "out game" after the units' ones.
O::ProductDescription TwoWorlds(std::function<void(O::WorldSetup&)> extraA = {}, std::function<void(O::WorldSetup&)> extraB = {}) {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("A", [extraA](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick"), Once("Init"), Once("Other")});
        s.Registrar().AddSystem<&Mark<'a'>>(In("MarkA", "Tick"));
        s.Registrar().AddSystem<&Mark<'i'>>(In("MarkInit", "Init"));
        s.Registrar().AddSystem<&Mark<'o'>>(In("MarkOther", "Other"));
        s.ExchangeIn(Logs("in game"));
        s.ExchangeOut(Logs("out game"));
        if (extraA) extraA(s);
    }, {"Tick"}));
    d.worlds.push_back(World("B", [extraB](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick")});
        s.Registrar().AddSystem<&Mark<'b'>>(In("MarkB", "Tick"));
        s.ExchangeIn(Logs("in game"));
        s.ExchangeOut(Logs("out game"));
        if (extraB) extraB(s);
    }, {"Tick"}));
    d.gameInit = {"Init"};
    return d;
}

Units TwoUnits() {
    Units units;
    units.Module("net", {.build = [](O::WorldSetup& s) {
                     s.ExchangeIn(Logs("in net"));
                     s.ExchangeOut(Logs("out net"));
                 }});
    units.Module("logs", {.build = [](O::WorldSetup& s) { s.ExchangeOut(Logs("out logs")); }});
    return units;
}

std::string LogSince(std::size_t from) {
    std::string out;
    for (std::size_t i = from; i < Log().size(); ++i) out += (out.empty() ? "" : " ") + Log()[i];
    return out;
}

} // namespace

TEST_CASE("exchange: X-1 RunFrame calls each World's in, its round, its out; units in order, the product's last") {
    Log().clear();
    Units units = TwoUnits();
    auto built = O::Product::Build(TwoWorlds(), units.Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunGameInit()));
    const std::size_t from = Log().size();
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(LogSince(from) ==
                      "in net@A in game@A a out net@A out logs@A out game@A "
                      "in net@B in game@B b out net@B out logs@B out game@B",
                  "[or-exch-order] " << LogSince(from));
}

TEST_CASE("exchange: X-1 a World with no exchange function runs its round alone") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Solo", [](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick")});
        s.Registrar().AddSystem<&Mark<'s'>>(In("MarkS", "Tick"));
    }, {"Tick"}));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined() == "s", "[or-exch-none] " << Joined());
}

TEST_CASE("exchange: X-2 RunGameInit and RunSchedule call no exchange function") {
    Log().clear();
    Units units = TwoUnits();
    auto built = O::Product::Build(TwoWorlds(), units.Take());
    ORCH_REQUIRE_BUILT(built);
    const std::size_t from = Log().size();
    REQUIRE(static_cast<bool>((*built)->RunGameInit()));
    REQUIRE(static_cast<bool>((*built)->RunSchedule("A", "Other")));
    CHECK_MESSAGE(LogSince(from) == "i o", "[or-exch-only-runframe] " << LogSince(from));
}

TEST_CASE("exchange: X-3 a failing exchange function stops RunFrame with ExchangeFailed; the World is healthy, the Product goes on") {
    Log().clear();
    bool refuse = true;
    auto built = O::Product::Build(TwoWorlds({}, [&refuse](O::WorldSetup& s) {
                                       s.ExchangeIn([&refuse](K::World&) -> PResult {
                                           if (refuse) return Refuse("exchange-refused");
                                           return {};
                                       });
                                   }),
                                   Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunGameInit()));
    const std::size_t from = Log().size();
    auto ran = p.RunFrame();
    REQUIRE_FALSE(static_cast<bool>(ran));
    const O::OrchestrationError& error = ran.error();
    CHECK_MESSAGE(error.code == O::OrchestrationErrorCode::ExchangeFailed, "[or-exch-failed] " << O::ToString(error.code));
    CHECK_MESSAGE(error.reporterCode == "exchange-refused", "[or-exch-reporter-code] " << error.reporterCode);
    CHECK_MESSAGE((error.message.find("'B'") != std::string::npos && error.message.find("(in)") != std::string::npos &&
                   error.message.find("'game'") != std::string::npos),
                  "[or-exch-failed-names] " << error.message);
    CHECK_MESSAGE(!error.failure.has_value(), "[or-exch-healthy] the error carries a WorldFailure");
    CHECK_MESSAGE(p.Failure() == nullptr, "[or-exch-healthy] the Product ended");
    CHECK_MESSAGE(p.World("B").GetWorld().State() == K::WorldState::Ready, "[or-exch-healthy] World B is not Ready");
    CHECK_MESSAGE(LogSince(from) == "in game@A a out game@A in game@B", "[or-exch-stops] " << LogSince(from));
    refuse = false;
    CHECK_MESSAGE(static_cast<bool>(p.RunFrame()), "[or-exch-healthy] the next RunFrame failed");
}

TEST_CASE("exchange: X-3 a unit's failing out is named with the unit, the side and the World") {
    Log().clear();
    Units units;
    units.Module("sender", {.build = [](O::WorldSetup& s) {
                      if (s.WorldName() == "B") s.ExchangeOut([](K::World&) -> PResult { return Refuse("out-refused"); });
                  }});
    auto built = O::Product::Build(TwoWorlds(), units.Take());
    ORCH_REQUIRE_BUILT(built);
    auto ran = (*built)->RunFrame();
    REQUIRE_FALSE(static_cast<bool>(ran));
    const std::string& message = ran.error().message;
    CHECK_MESSAGE(message.find("'sender'") != std::string::npos, "[or-exch-names-unit] " << message);
    CHECK_MESSAGE(message.find("(out)") != std::string::npos, "[or-exch-names-out] " << message);
    CHECK_MESSAGE(message.find("'B'") != std::string::npos, "[or-exch-names-world] " << message);
}

TEST_CASE("exchange: X-4 an exception from an exchange function passes through; the World is not broken") {
    Log().clear();
    bool throws = true;
    auto built = O::Product::Build(TwoWorlds([&throws](O::WorldSetup& s) {
                                       s.ExchangeOut([&throws](K::World&) -> PResult {
                                           if (throws) throw std::runtime_error("exchange throws");
                                           return {};
                                       });
                                   }),
                                   Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    CHECK_THROWS_AS(static_cast<void>(p.RunFrame()), std::runtime_error);
    CHECK_MESSAGE(p.Failure() == nullptr, "[or-exch-throw-healthy] the Product ended");
    CHECK_MESSAGE(p.World("A").GetWorld().State() == K::WorldState::Ready, "[or-exch-throw-healthy] World A is not Ready");
    throws = false;
    CHECK(static_cast<bool>(p.RunFrame()));
}

TEST_CASE("exchange: X-5 with a broken World RunFrame calls no exchange function") {
    Log().clear();
    auto built = O::Product::Build(TwoWorlds([](O::WorldSetup& s) {
                                       s.Registrar().Schedule(Once("Break"));
                                       s.Registrar().AddSystem<&Fail>(In("Breaks", "Break"));
                                   }),
                                   Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("A", "Break")));
    const std::size_t from = Log().size();
    REQUIRE_FALSE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(LogSince(from).empty(), "[or-exch-broken] " << LogSince(from));
}

TEST_CASE("exchange: X-6 a replacement brings the new build's exchange functions; a failed one keeps the old") {
    Log().clear();
    int build = 0;
    struct Counter2 {
        int frames = 0;
    };
    Units units;
    units.Module("net", {.configure = [](O::ProductSetup& p) { p.Buffer().Create<Counter2>(); },
                         .build = [&build](O::WorldSetup& s) {
                             const std::string tag = "in build" + std::to_string(++build);
                             // Kept after the prepare functions have run: it may point into the Product's buffer.
                             Counter2* counter = s.ProductBuffer().Find<Counter2>();
                             s.ExchangeIn([tag, counter](K::World& w) -> PResult {
                                 ++counter->frames;
                                 Log().push_back(tag + "@" + std::string(w.Name()));
                                 return {};
                             });
                             s.ExchangeOut(Logs("out build" + std::to_string(build)));
                         }});
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", [](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick"), Once("Junk")});
        s.Registrar().AddSystem<&Fail>(In("FailJunk", "Junk"));
    }, {"Tick"}));
    auto built = O::Product::Build(std::move(d), units.Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    std::size_t from = Log().size();
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(LogSince(from) == "in build2@Main out build2@Main", "[or-exch-replaced] " << LogSince(from));
    // A BuildNewFirst replacement that fails (its beforeSwap breaks) keeps the old World and its exchange functions.
    REQUIRE_FALSE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{{}, {"Junk"}, O::ReplaceOrder::BuildNewFirst})));
    from = Log().size();
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(LogSince(from) == "in build2@Main out build2@Main", "[or-exch-replace-failed-keeps] " << LogSince(from));
    CHECK_MESSAGE(p.Buffer().Find<Counter2>()->frames == 3, "[or-exch-replaced] " << p.Buffer().Find<Counter2>()->frames);
}

TEST_CASE("exchange: X-7 a World breaking in its round: neither its out nor any later World's exchange runs") {
    Log().clear();
    auto built = O::Product::Build(TwoWorlds([](O::WorldSetup& s) { s.Registrar().AddSystem<&Fail>(In("BreaksA", "Tick")); }),
                                   Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunGameInit()));
    const std::size_t from = Log().size();
    auto ran = p.RunFrame();
    REQUIRE_FALSE(static_cast<bool>(ran));
    CHECK(ran.error().failure.has_value());
    CHECK_MESSAGE(LogSince(from).rfind("in game@A", 0) == 0, "[or-exch-break-reached] " << LogSince(from));
    CHECK_MESSAGE(LogSince(from).find("out game@A") == std::string::npos, "[or-exch-break-no-out] " << LogSince(from));
    CHECK_MESSAGE(LogSince(from).find("@B") == std::string::npos, "[or-exch-break-no-later] " << LogSince(from));
}

TEST_CASE("exchange: X-8 game initialization before the first in; a failing function stops the ones after it; the default runner fails") {
    Log().clear();
    int laterCalled = 0;
    auto built = O::Product::Build(TwoWorlds([&laterCalled](O::WorldSetup& s) {
                                       s.ExchangeIn([](K::World&) -> PResult { return Refuse("first-refuses"); });
                                       s.ExchangeIn([&laterCalled](K::World&) -> PResult {
                                           ++laterCalled;
                                           return {};
                                       });
                                   }),
                                   Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    // Run() uses the default runner: one RunFrame (game initialization first).
    const O::RunnerExit exit = p.Run();
    CHECK_MESSAGE(exit.code == O::RunnerExitCode::Failure, "[or-exch-runner] the default runner succeeded");
    CHECK_MESSAGE(exit.reason.find("ExchangeFailed") != std::string::npos, "[or-exch-runner] " << exit.reason);
    CHECK_MESSAGE(LogSince(0).rfind("i in game@A", 0) == 0, "[or-exch-init-first] " << LogSince(0));
    CHECK_MESSAGE(laterCalled == 0, "[or-exch-stop-later] the function after the failing one ran");
}
