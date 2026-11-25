// Lending a World to another thread (O-13; docs/architecture/target/v2/live-frame-sim-thread-declaration.zh-Hant.md,
// section 4.2 and judgements J1 to J3): Product::Lend, WorldLease::RunRound, Product::Return.
// Each CHECK carries a tag in brackets ("[or-lend-...]"): the batch's mutants expect it.

#include "OrchestrationTestSupport.hpp"

#include <string>
#include <thread>

using namespace GYO::Test::Orchestration;

namespace {

using PResult = ::GYO::Engine::Base::Result<void, O::OrchestrationError>;

// Counts its World's rounds in the World's own Counter (no shared log: two threads run two Worlds at once).
K::SystemResult CountRound(K::ResMut<Counter> counter) {
    ++counter->n;
    return {};
}

// Two Worlds, A and B, each counting its rounds; B's exchange functions count too (in its Counter as well: +100
// for the in, +10000 for the out).
O::ProductDescription Pair(std::function<void(O::WorldSetup&)> extraB = {}) {
    O::ProductDescription d;
    d.name = "game";
    for (const char* name : {"A", "B"}) {
        const bool b = std::string(name) == "B";
        d.worlds.push_back(World(name, [b, extraB](O::WorldSetup& s) {
            Basics(s.Registrar(), {FrameClock("Tick"), Once("Init")});
            s.Registrar().Resource<Counter>({.name = "Counter"});
            s.Registrar().AddSystem<&CountRound>(In("CountRound", "Tick"));
            s.Prepare([](K::World& w) -> PResult {
                w.SetResource(Counter{});
                return {};
            });
            if (b) {
                s.ExchangeIn([](K::World& w) -> PResult {
                    w.ResourceMut<Counter>()->n += 100;
                    return {};
                });
                s.ExchangeOut([](K::World& w) -> PResult {
                    w.ResourceMut<Counter>()->n += 10000;
                    return {};
                });
                if (extraB) extraB(s);
            }
        }, {"Tick"}));
    }
    d.gameInit = {"Init"};
    return d;
}

int CountOf(K::World& w) { return w.Resource<Counter>()->n; }

} // namespace

TEST_CASE("lend: J1 RunFrame skips a lent World; its lease runs its in, round and out; after Return RunFrame runs it again") {
    auto built = O::Product::Build(Pair(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK(CountOf(p.World("B").GetWorld()) == 10101);
    auto lent = p.Lend("B");
    REQUIRE_MESSAGE(static_cast<bool>(lent), (lent ? std::string() : lent.error().message));
    O::WorldLease lease = std::move(lent).value();
    CHECK(lease.Name() == "B");
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK(CountOf(p.World("A").GetWorld()) == 2);
    CHECK_MESSAGE(CountOf(lease.World()) == 10101, "[or-lend-skip] RunFrame did not run the lent World: " << CountOf(lease.World()));
    REQUIRE(static_cast<bool>(lease.RunRound()));
    CHECK_MESSAGE(CountOf(lease.World()) == 20202, "[or-lend-round] in, round, out once each: " << CountOf(lease.World()));
    p.Return(std::move(lease));
    CHECK_FALSE(lease.Valid());
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK(CountOf(p.World("B").GetWorld()) == 30303);
}

TEST_CASE("lend: J1 a lent World runs on another thread while the Product runs the others") {
    auto built = O::Product::Build(Pair(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunGameInit()));
    O::WorldLease lease = std::move(p.Lend("B")).value();
    bool threadOk = true;
    std::thread holder([&lease, &threadOk] {
        for (int i = 0; i < 200; ++i) threadOk = threadOk && static_cast<bool>(lease.RunRound());
    });
    bool mainOk = true;
    for (int i = 0; i < 200; ++i) mainOk = mainOk && static_cast<bool>(p.RunFrame());
    holder.join();
    CHECK(threadOk);
    CHECK(mainOk);
    p.Return(std::move(lease));
    CHECK(CountOf(p.World("A").GetWorld()) == 200);
    CHECK(CountOf(p.World("B").GetWorld()) == 200 * 10101);
}

TEST_CASE("lend: J2 entry points naming a lent World return WorldLent; Lend refuses before game initialization, twice, and Extract's Worlds") {
    auto built = O::Product::Build(Pair(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    auto early = p.Lend("B");
    REQUIRE_FALSE(static_cast<bool>(early));
    CHECK_MESSAGE(early.error().code == O::OrchestrationErrorCode::LendRefused, "[or-lend-init] " << early.error().message);
    REQUIRE(static_cast<bool>(p.RunGameInit()));
    CHECK(p.Lend("C").error().code == O::OrchestrationErrorCode::UnknownWorld);
    O::WorldLease lease = std::move(p.Lend("B")).value();
    CHECK_MESSAGE(p.Lend("B").error().code == O::OrchestrationErrorCode::LendRefused, "[or-lend-twice]");
    const auto lentRefused = [](const PResult& result) { return !result && result.error().code == O::OrchestrationErrorCode::WorldLent; };
    CHECK_MESSAGE(lentRefused(p.RunSchedule("B", "Tick")), "[or-lend-schedule]");
    CHECK_MESSAGE(lentRefused(p.ReplaceWorld("B", {})), "[or-lend-replace]");
    CHECK_MESSAGE(lentRefused(p.InsertSystems("B", "late", [](K::Registrar&) {})), "[or-lend-insert]");
    // The others still work on A.
    CHECK(static_cast<bool>(p.RunSchedule("A", "Tick")));
    p.Return(std::move(lease));
    CHECK(static_cast<bool>(p.RunSchedule("B", "Tick")));

    // An Extract reader and its source are not lent.
    O::ProductDescription d;
    d.name = "drawn";
    d.worlds.push_back(World("Game", [](O::WorldSetup& s) { Basics(s.Registrar(), {FrameClock("Tick")}); }, {"Tick"}));
    O::WorldDescription render = World("Render", [](O::WorldSetup& s) { Basics(s.Registrar(), {FrameClock("Tick")}); }, {"Tick"});
    render.extractFrom = "Game";
    d.worlds.push_back(std::move(render));
    auto drawn = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(drawn);
    REQUIRE(static_cast<bool>((*drawn)->RunGameInit()));
    // (A lease given by mistake is returned, so the Product can be destroyed and the failure is the CHECK's.)
    const auto refused = [&drawn](const std::string_view name) {
        auto lent = (*drawn)->Lend(name);
        if (lent) {
            (*drawn)->Return(std::move(lent).value());
            return false;
        }
        return lent.error().code == O::OrchestrationErrorCode::LendRefused;
    };
    CHECK_MESSAGE(refused("Game"), "[or-lend-source]");
    CHECK_MESSAGE(refused("Render"), "[or-lend-reader]");
}

TEST_CASE("lend: J3 a World that breaks on its holder's thread ends the Product; Return keeps the failure") {
    auto built = O::Product::Build(Pair([](O::WorldSetup& s) { s.Registrar().AddSystem<&Fail>(In("Breaks", "Tick")); }),
                                   Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunGameInit()));
    O::WorldLease lease = std::move(p.Lend("B")).value();
    std::optional<O::OrchestrationError> onThread;
    std::thread holder([&lease, &onThread] {
        auto ran = lease.RunRound();
        if (!ran) onThread = ran.error();
    });
    holder.join();
    REQUIRE(onThread.has_value());
    CHECK(onThread->code == O::OrchestrationErrorCode::ScheduleFailed);
    REQUIRE(onThread->failure.has_value());
    CHECK(onThread->failure->world == "B");
    // The Product's next entry point sees it (O-11) and runs nothing.
    const int aBefore = CountOf(p.World("A").GetWorld());
    auto frame = p.RunFrame();
    CHECK_MESSAGE((!frame && frame.error().code == O::OrchestrationErrorCode::WorldInterrupted), "[or-lend-broken]");
    CHECK(CountOf(p.World("A").GetWorld()) == aBefore);
    // The holder's next round is refused with the same failure (the interrupted World is not run again).
    auto again = lease.RunRound();
    CHECK((!again && again.error().code == O::OrchestrationErrorCode::WorldInterrupted));
    p.Return(std::move(lease));
    REQUIRE(p.Failure() != nullptr);
    CHECK_MESSAGE(p.Failure()->world == "B", "[or-lend-failure]");
    CHECK(p.Failure()->entry == "WorldLease::RunRound");
}
