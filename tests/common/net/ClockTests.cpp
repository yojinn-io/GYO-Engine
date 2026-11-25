// The clock sync (declaration D-9, section 18 (A); 3b): a client World that starts at tick 0 jumps to the Match's
// tick (plus its lead) when it connects, then follows by speed. The bounds checked here (+-2 ticks once settled) are
// this test's assertions, not a judgement declared in advance.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

struct ClockGame {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::pair<N::MemoryLink, N::MemoryLink> links;
    std::unique_ptr<O::Product> server, client;

    explicit ClockGame(const N::MemoryLinkSettings& link) : links(N::MemoryLink::Pair(link)) {
        auto reader = [now = clock] { return *now; };
        End s = GameEnd(N::NetRole::Server, GameRules{1, false});
        s.net.link = std::make_shared<N::MemoryLink>(links.first);
        s.net.clock = reader;
        s.net.seed = 23;
        // The Match is at step 5000 when the client comes (a prepare function puts Tick back, WT-3).
        s.build = [](O::WorldSetup& setup) {
            RegisterGame(setup, GameRules{1, false});
            setup.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                world.SetResource(W::Tick{5000});
                return {};
            });
        };
        End c = GameEnd(N::NetRole::Client, GameRules{1, false});
        c.net.link = std::make_shared<N::MemoryLink>(links.second);
        c.target.server = N::MemoryLink::EndpointA();
        c.net.clock = reader;
        auto built = O::Product::Build(Describe(s), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        server = std::move(*built);
        auto other = O::Product::Build(Describe(c), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(other), Reported(other));
        client = std::move(*other);
    }
    void Frame() {
        REQUIRE(static_cast<bool>(server->RunFrame()));
        REQUIRE(static_cast<bool>(client->RunFrame()));
        links.first.Advance();
        // One 64 Hz step of wall time a frame, so the Match's ticks and the wall clock run at one rate.
        *clock += N::NetTime{15'625'000};
    }
    std::uint64_t ClientTick() { return client->World("Net").GetWorld().Resource<W::Tick>()->value; }
    std::uint64_t ServerTick() { return server->World("Net").GetWorld().Resource<W::Tick>()->value; }
    const N::ClockSync& Sync() { return *client->World("Net").GetWorld().Resource<N::ClockSync>(); }
};

// A fixed one-way latency of 3 steps (48 ms) and jitter (a third of the datagrams 1..2 steps later).
N::MemoryLinkSettings Latency(const std::uint32_t steps) {
    N::MemoryLinkSettings link;
    link.seed = 31;
    link.aToB = {0, 0, 3333, 2, steps};
    link.bToA = {0, 0, 3333, 2, steps};
    return link;
}

} // namespace

TEST_CASE("clock: a client at tick 0 jumps to the Match at 5000 on connecting, then stays within 2 ticks of its target") {
    ClockGame game(Latency(3));
    for (int i = 0; i < 20 && game.Sync().jumps == 0; ++i) game.Frame();
    REQUIRE_MESSAGE(game.Sync().jumps == 1, "the first sync after connecting jumps to the target");
    const std::int64_t firstJump = game.Sync().lastJump;
    CHECK(firstJump > 4900);
    // The RTT, the jitter and the smoothed remote estimate settle.
    for (int i = 0; i < 240; ++i) game.Frame();
    double worst = 0.0;
    std::int64_t leadMin = 1 << 20, leadMax = -(1 << 20);
    double remoteWorst = 0.0;  // the remote estimate against the Match's tick
    const std::uint64_t jumpsSettled = game.Sync().jumps;
    for (int i = 0; i < 300; ++i) {
        game.Frame();
        worst = std::max(worst, std::abs(game.Sync().lastError));
        // (SyncClock runs at the client's Last stage; the Match ran this frame before it.)
        remoteWorst = std::max(remoteWorst, std::abs(game.Sync().remoteTick - static_cast<double>(game.ServerTick())));
        const auto lead = static_cast<std::int64_t>(game.ClientTick()) - static_cast<std::int64_t>(game.ServerTick());
        leadMin = std::min(leadMin, lead);
        leadMax = std::max(leadMax, lead);
    }
    MESSAGE("clock: first jump=" << firstJump << " jumps=" << game.Sync().jumps << " worst |error| once settled=" << worst
                                 << " ticks; client lead over the Match " << leadMin << ".." << leadMax << " ticks; speed changes="
                                 << game.Sync().speedChanges);
    CHECK_MESSAGE(worst <= 2.0, "once settled the client stays within 2 ticks of its sync target");
    CHECK_MESSAGE(game.Sync().jumps == jumpsSettled, "once settled the clock follows by speed, without jumping");
    CHECK(leadMin > 0);
    CHECK_MESSAGE(remoteWorst <= 2.0, "[nt-clock-remote] the remote estimate stays within 2 ticks of the Match's tick: " << remoteWorst);
    CHECK_MESSAGE(game.Sync().jitterTicks > 0.0, "[nt-clock-remote] the jitter is kept (a third of the datagrams come late)");
    // Three steps each way plus the late third: the RTT is six ticks and a little more.
    CHECK_MESSAGE((game.Sync().rttTicks >= 5.0 && game.Sync().rttTicks <= 10.0), "[nt-clock-remote] the RTT in ticks: " << game.Sync().rttTicks);
}

TEST_CASE("clock: when the latency grows the speed goes up, and back to 1 once caught up") {
    ClockGame game(Latency(3));
    for (int i = 0; i < 300; ++i) game.Frame();
    REQUIRE(game.Sync().jumps >= 1);
    const std::uint64_t jumps = game.Sync().jumps;
    // One-way 3 -> 5 steps: the target moves a few ticks ahead, within the snap margin.
    game.links.first.SetOutgoingImpairment({0, 0, 3333, 2, 5});
    game.links.second.SetOutgoingImpairment({0, 0, 3333, 2, 5});
    double fastest = 1.0;
    for (int i = 0; i < 600; ++i) {
        game.Frame();
        fastest = std::max(fastest, game.Sync().scale);
    }
    MESSAGE("clock: fastest speed=" << fastest << " final speed=" << game.Sync().scale << " final error=" << game.Sync().lastError
                                    << " jumps after the change=" << (game.Sync().jumps - jumps) << " speed changes="
                                    << game.Sync().speedChanges);
    CHECK_MESSAGE(fastest > 1.0, "a client falling behind its target speeds up");
    CHECK_MESSAGE(game.Sync().scale == 1.0, "back near the target the speed is 1 again");
    CHECK(game.Sync().jumps == jumps);
    CHECK(std::abs(game.Sync().lastError) <= 2.0);
}
