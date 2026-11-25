// Replicated Resources (net-module declaration Q8 (b); net.zh-Hant.md section 12): "resource:" is judgement G-5's
// method on a replicated Resource with the default apply, CCR-8 for a Resource and the protocol hash; "statesync:"
// is a state machine replicated with the state machine's two apply functions (AdoptByRequest,
// AdoptByRequestSilent), CCR-8 for them and when the value changes at the client.
//
// The state machine and the net Module do not depend on each other: this test product links both and joins them
// with ReplicateResource<StateMachine<S>, S>, as a product does.
#include <doctest/doctest.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"
#include "engine/state_machine/AdoptValue.hpp"
#include "engine/state_machine/RegisterStateMachine.hpp"

namespace N = GYO::Engine::Net;
namespace SM = GYO::Engine::StateMachine;
using namespace GYO::Test::Net;

namespace GYO::Test::Net {

// The score of a match: written by the server's rules, replicated with the default apply.
struct Score {
    std::int32_t red = 0;
    std::int32_t blue = 0;
    friend bool operator==(const Score&, const Score&) = default;
};
inline void HashValue(K::Hasher& h, const Score& v) {
    HashValue(h, v.red);
    HashValue(h, v.blue);
}
inline N::Codec<Score> ScoreCodec() {
    N::Codec<Score> codec;
    codec.encode = [](N::NetWriter& w, const Score& v) {
        w.Value(v.red);
        w.Value(v.blue);
    };
    codec.decode = [](N::NetReader& r, Score& v) { return r.Value(v.red) && r.Value(v.blue); };
    return codec;
}

// The sky over the match: a state machine the server changes.
enum class Sky : std::uint8_t { Clear, Fog };

} // namespace GYO::Test::Net

template <>
struct GYO::Engine::StateMachine::StateMachineTraits<GYO::Test::Net::Sky> {
    static constexpr std::string_view name = "Sky";
    static constexpr std::array values{ValueName<GYO::Test::Net::Sky>{GYO::Test::Net::Sky::Clear, "Clear"},
                                       ValueName<GYO::Test::Net::Sky>{GYO::Test::Net::Sky::Fog, "Fog"}};
    static constexpr GYO::Test::Net::Sky initial = GYO::Test::Net::Sky::Clear;
};

namespace {

struct Clock {
    std::shared_ptr<N::NetTime> now = std::make_shared<N::NetTime>(0);
    std::function<N::NetTime()> Reader() const {
        auto shared = now;
        return [shared] { return *shared; };
    }
};

// What the client saw: the enters and exits of Sky that ran, and per frame Sky before and after its apply.
struct SkyLog {
    std::vector<std::string> transitions;
    struct FrameSeen {
        std::optional<Sky> beforeApply;
        std::optional<Sky> pendingBeforeApply;
        std::optional<Sky> afterApply;
        bool changedAfterApply = false;
    };
    std::vector<FrameSeen> frames;
    std::uint64_t scoreChanged = 0;
    std::vector<Score> scoresSeenChanged;
};
inline void HashValue(K::Hasher&, const SkyLog&) {}

template <Sky V, bool Enter>
K::SystemResult NoteSky(K::ResMut<SkyLog> log) {
    log->transitions.push_back(std::string(Enter ? "Enter." : "Exit.") + std::string(*SM::NameOf(V)));
    return {};
}
K::SystemResult SeeBeforeApply(K::Res<SM::StateMachine<Sky>> sky, K::Res<SM::StateMachineRequest<Sky>> request, K::ResMut<SkyLog> log) {
    log->frames.push_back({sky->Current(), request->Pending(), std::nullopt, false});
    return {};
}
K::SystemResult SeeAfterApply(K::Res<SM::StateMachine<Sky>> sky, K::ResMut<SkyLog> log) {
    if (log.Read().frames.empty()) return {};
    log->frames.back().afterApply = sky->Current();
    log->frames.back().changedAfterApply = sky.Changed();
    return {};
}
K::SystemResult SeeScore(K::Res<Score> score, K::ResMut<SkyLog> log) {
    if (!score.Changed()) return {};
    ++log->scoreChanged;
    log->scoresSeenChanged.push_back(*score);
    return {};
}

// The server's rules: a goal every 3 ticks while the game runs; the sky turns to fog at tick 30.
K::SystemResult ScoreGoals(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::ResMut<Score> score) {
    if (!Playing(*rules, *tick) || tick->value % 3 != 0) return {};
    if (tick->value % 2 == 0) {
        ++score->red;
    } else {
        ++score->blue;
    }
    return {};
}
K::SystemResult ScoreAt(K::Res<W::Tick> tick, K::ResMut<Score> score) {
    if (tick->value != 120 && tick->value != 160) return {};
    score->red += 1;
    return {};
}
K::SystemResult FogAt30(K::Res<W::Tick> tick, K::ResMut<SM::StateMachineRequest<Sky>> request) {
    if (tick->value == 30) request->Request(Sky::Fog);
    return {};
}

K::SystemOptions MainAt(std::string name, std::string stage, std::optional<K::ConditionRef> condition) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = "Main";
    mount.stage = std::move(stage);
    if (condition) mount.conditions.push_back(*condition);
    options.mounts.push_back(std::move(mount));
    return options;
}

K::SystemOptions In(std::string name, std::string schedule) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::move(schedule);
    mount.stage = std::string(SM::kEnterExitStage);
    options.mounts.push_back(std::move(mount));
    return options;
}

enum class ScoreRule { Goals, TwoChanges, None };
enum class SkyApply { None, Request, Silent };

struct Setup {
    GameRules rules{};
    ScoreRule score = ScoreRule::Goals;
    bool replicateScore = true;
    SkyApply sky = SkyApply::None;
};

void RegisterEnd(O::WorldSetup& s, const Setup& setup) {
    RegisterGame(s, setup.rules);
    K::Registrar& r = s.Registrar();
    r.Resource<Score>({.name = "Score"});
    r.Resource<SkyLog>({.name = "SkyLog"});
    s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        if (world.Resource<Score>() == nullptr) world.SetResource(Score{});
        world.SetResource(SkyLog{});
        return {};
    });
    N::NetEntry net(s);
    if (setup.replicateScore) {
        N::NetEntry::ResourceOptions<Score> score;
        score.codec = ScoreCodec();
        net.ReplicateResource<Score>("Score", std::move(score));
    }
    if (setup.score == ScoreRule::Goals) r.AddSystem<&ScoreGoals>(Fixed("ScoreGoals", true));
    if (setup.score == ScoreRule::TwoChanges) r.AddSystem<&ScoreAt>(Fixed("ScoreAt", true));
    r.AddSystem<&SeeScore>(MainAt("SeeScore", "Update", N::OnClient()));
    if (setup.sky == SkyApply::None) return;
    SM::RegisterStateMachine<Sky>(s);
    // The enter and exit Systems go into the value's own schedules.
    r.AddSystem<&NoteSky<Sky::Clear, true>>(In("NoteEnterClear", SM::EnterScheduleName(Sky::Clear)));
    r.AddSystem<&NoteSky<Sky::Clear, false>>(In("NoteExitClear", SM::ExitScheduleName(Sky::Clear)));
    r.AddSystem<&NoteSky<Sky::Fog, true>>(In("NoteEnterFog", SM::EnterScheduleName(Sky::Fog)));
    r.AddSystem<&NoteSky<Sky::Fog, false>>(In("NoteExitFog", SM::ExitScheduleName(Sky::Fog)));
    r.AddSystem<&FogAt30>(MainAt("FogAt30", "Update", N::OnServer()));
    r.AddSystem<&SeeBeforeApply>(MainAt("SeeBeforeApply", "PreUpdate", N::OnClient()));
    r.AddSystem<&SeeAfterApply>(MainAt("SeeAfterApply", "Update", N::OnClient()));
    N::NetEntry::ResourceOptions<SM::StateMachine<Sky>, Sky> options;
    options.read = [](const SM::StateMachine<Sky>& machine) { return machine.Current(); };
    options.apply = setup.sky == SkyApply::Silent ? &SM::AdoptByRequestSilent<Sky> : &SM::AdoptByRequest<Sky>;
    net.ReplicateResource<SM::StateMachine<Sky>, Sky>("Sky", options);
}

// A server and one client (K1) of the sample game with the score (and the sky) over a MemoryLink.
struct Match {
    Clock clock;
    std::pair<N::MemoryLink, N::MemoryLink> links;
    std::unique_ptr<O::Product> server;
    std::unique_ptr<O::Product> client;
    std::vector<std::uint64_t> lostPieces = std::vector<std::uint64_t>(2, 0);

    Match(const N::MemoryLinkSettings& link, const Setup& serverSetup, const Setup& clientSetup) : links(N::MemoryLink::Pair(link)) {
        auto& pieces = lostPieces;
        links.first.SetInjectionObserver([&pieces](N::MemoryLink::Injection kind, std::span<const std::byte> datagram) {
            if (kind != N::MemoryLink::Injection::Loss) return;
            const auto summary = N::Connection::Inspect(datagram);
            if (!summary) return;
            for (std::size_t c = 0; c < summary->piecesPerChannel.size() && c < pieces.size(); ++c) pieces[c] += summary->piecesPerChannel[c];
        });
        End s = GameEnd(N::NetRole::Server, serverSetup.rules);
        s.build = [serverSetup](O::WorldSetup& setup) { RegisterEnd(setup, serverSetup); };
        s.net.link = std::make_shared<N::MemoryLink>(links.first);
        s.net.clock = clock.Reader();
        s.net.seed = 11;
        End c = GameEnd(N::NetRole::Client, clientSetup.rules);
        c.build = [clientSetup](O::WorldSetup& setup) { RegisterEnd(setup, clientSetup); };
        c.net.link = std::make_shared<N::MemoryLink>(links.second);
        c.target.server = N::MemoryLink::EndpointA();
        c.target.userId = 5;
        c.net.clock = clock.Reader();
        auto builtServer = O::Product::Build(Describe(s), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
        server = std::move(*builtServer);
        auto builtClient = O::Product::Build(Describe(c), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtClient), Reported(builtClient));
        client = std::move(*builtClient);
    }
    void Frame() {
        REQUIRE(static_cast<bool>(server->RunFrame()));
        REQUIRE(static_cast<bool>(client->RunFrame()));
        links.first.Advance();
        *clock.now += Ms(16);
    }
    K::World& ServerWorld() { return server->World("Net").GetWorld(); }
    K::World& ClientWorld() { return client->World("Net").GetWorld(); }
    std::uint64_t ServerTick() { return ServerWorld().Resource<W::Tick>()->value; }
    const N::NetReplicaState& Replica() { return *ClientWorld().Resource<N::NetReplicaState>(); }
    const SkyLog& Log() { return *ClientWorld().Resource<SkyLog>(); }
    N::PeerLedger Ledger() {
        for (const K::EntityId id : ServerWorld().Entities()) {
            if (const auto* ledger = ServerWorld().Get<N::PeerLedger>(id)) return *ledger;
        }
        return {};
    }
};

N::MemoryLinkSettings Lossy() {
    N::MemoryLinkSettings settings;
    settings.seed = 0x5EED0005u;
    settings.aToB = {2000, 1000, 2500, 3, 0};
    settings.bToA = {2000, 1000, 2500, 3, 0};
    return settings;
}
// Confirmations get through rarely: the server sends the same values again and again.
N::MemoryLinkSettings ConfirmationsLost() {
    N::MemoryLinkSettings settings;
    settings.seed = 8;
    settings.bToA = {8000, 0, 0, 0, 0};
    return settings;
}

} // namespace

TEST_CASE("resource: G-5's method on a replicated Resource: the client's score equals the server's at each observed tick") {
    Match match(Lossy(), Setup{}, Setup{});
    std::map<std::uint64_t, Score> record;
    std::optional<std::uint32_t> observed;
    std::uint64_t observations = 0, mismatches = 0, firstTick = 0;
    std::string firstMismatch;
    const std::uint64_t lastTick = GameRules{}.lastTick;
    while (match.ServerTick() < lastTick) {
        REQUIRE(static_cast<bool>(match.server->RunFrame()));
        record[match.ServerTick()] = *match.ServerWorld().Resource<Score>();
        REQUIRE(static_cast<bool>(match.client->RunFrame()));
        match.links.first.Advance();
        *match.clock.now += Ms(16);
        const auto& replica = match.Replica();
        if (firstTick == 0 && replica.latestConfirmed) firstTick = *replica.latestConfirmed;
        // (1) D-7's two conditions hold for T, and nothing newer than T was written yet.
        if (replica.latestConfirmed && *replica.latestConfirmed == replica.maxAppliedTick && observed != replica.latestConfirmed) {
            observed = replica.latestConfirmed;
            ++observations;
            const auto expected = record.find(*observed);
            REQUIRE(expected != record.end());
            const Score seen = *match.ClientWorld().Resource<Score>();
            if (!(seen == expected->second)) {
                ++mismatches;
                if (firstMismatch.empty()) {
                    firstMismatch = "tick " + std::to_string(*observed) + ": client " + std::to_string(seen.red) + "/" +
                                    std::to_string(seen.blue) + " server " + std::to_string(expected->second.red) + "/" +
                                    std::to_string(expected->second.blue);
                }
            }
        }
    }
    const std::uint64_t ticks = lastTick - firstTick + 1;
    MESSAGE("resource G-5 (1): observed ticks=" << observations << " of " << ticks << ", mismatches=" << mismatches << " "
                                                << firstMismatch);
    REQUIRE_MESSAGE(observations * 10 >= ticks, "resource G-5 (1): the observation holds at 10 % of the ticks or more");
    CHECK_MESSAGE(mismatches == 0, "resource G-5 (1): at each observed tick T the client's score equals the server's tick T");

    // (2) The server stops changing; the link drains and everything is confirmed; compare again.
    int drain = 0;
    for (; drain < 2000; ++drain) {
        match.Frame();
        const auto ledger = match.Ledger();
        const bool settled = match.links.first.InFlightToSelf() == 0 && match.links.second.InFlightToSelf() == 0 &&
                             ledger.confirmationsReceived == match.Replica().confirmationsSent &&
                             *match.ClientWorld().Resource<Score>() == *match.ServerWorld().Resource<Score>();
        if (settled && drain > 20) break;
    }
    const Score final = *match.ServerWorld().Resource<Score>();
    const auto ledger = match.Ledger();
    const auto& replica = match.Replica();
    MESSAGE("resource G-5 (2): drain frames=" << drain << " score=" << final.red << "/" << final.blue
                                              << " | resource values sent=" << ledger.resourceValuesSent
                                              << " applied=" << replica.resourceValuesApplied << " unchanged="
                                              << replica.resourceValuesUnchanged << " | value parts sent=" << ledger.valuePartsSent
                                              << " received=" << replica.valuePartsReceived << " lost with datagrams="
                                              << match.lostPieces[N::kValuesChannel]);
    CHECK_MESSAGE(*match.ClientWorld().Resource<Score>() == final, "resource G-5 (2): after the drain the client's score equals the server's");
    CHECK(final.red + final.blue > 50);
    // (4) The counts: every value part sent was received or lost; none malformed.
    CHECK(ledger.valuePartsSent == replica.valuePartsReceived + match.lostPieces[N::kValuesChannel]);
    CHECK(replica.confirmationsSent == ledger.confirmationsReceived);
    CHECK(replica.drops[static_cast<std::size_t>(N::ReplicaDrop::Malformed)] == 0);
    CHECK(replica.resourceValuesApplied > 0);
    // A confirmed Resource is not sent again while it does not change.
    const auto sentBefore = match.Ledger().resourceValuesSent;
    for (int i = 0; i < 10; ++i) match.Frame();
    CHECK_MESSAGE(match.Ledger().resourceValuesSent == sentBefore, "a confirmed Resource is not sent again");
}

TEST_CASE("resource: a value sent again unchanged is not written, so Res<T>::Changed() tells only real changes (CCR-8)") {
    Setup setup;
    setup.score = ScoreRule::TwoChanges;
    Match match(ConfirmationsLost(), setup, setup);
    // The score changes at ticks 120 and 160; the connection (its handshake through the lost datagrams) is up before.
    std::uint64_t firstSentAt = 0;
    for (int i = 0; i < 240; ++i) {
        match.Frame();
        if (firstSentAt == 0 && match.Ledger().resourceValuesSent > 0) firstSentAt = match.ServerTick();
    }
    REQUIRE_MESSAGE((firstSentAt > 0 && firstSentAt < 120), "the score was sent before its first change");
    const auto& log = match.Log();
    const auto& replica = match.Replica();
    const auto ledger = match.Ledger();
    MESSAGE("resource CCR-8: resource values sent=" << ledger.resourceValuesSent << " applied=" << replica.resourceValuesApplied
                                                    << " unchanged=" << replica.resourceValuesUnchanged
                                                    << " client Changed() frames=" << log.scoreChanged << " first sent at tick="
                                                    << firstSentAt);
    // The same score arrived again (confirmations lost): more values applied than the score's three values.
    REQUIRE_MESSAGE(replica.resourceValuesApplied > 3, "the same score arrived again (confirmations lost)");
    CHECK(replica.resourceValuesUnchanged > 0);
    // The first frame sees the value put by the prepare function; then one change per real change at the server.
    REQUIRE_MESSAGE(log.scoresSeenChanged.size() == 3, "the client's Changed() is true once per real change of the score (CCR-8)");
    CHECK((log.scoresSeenChanged[1] == Score{1, 0}));
    CHECK_MESSAGE((log.scoresSeenChanged[2] == Score{2, 0}), "Changed() is true when the server really changed the score, and only then");
    CHECK(*match.ClientWorld().Resource<Score>() == *match.ServerWorld().Resource<Score>());
}

TEST_CASE("resource: two ends that replicate different Resources do not connect (ProtocolMismatch)") {
    Setup client;
    client.replicateScore = false;
    Match match({}, Setup{}, client);
    for (int i = 0; i < 4; ++i) match.Frame();
    const auto& seen = SeenIn(match.ClientWorld()).disconnected;
    REQUIRE_MESSAGE(seen.size() == 1, "the protocol hash includes the replicated Resources");
    CHECK(seen[0].reason == N::DisconnectReason::ProtocolMismatch);
}

namespace {

std::vector<std::string> RunSky(const SkyApply apply, const N::MemoryLinkSettings& link, Match*& out, std::unique_ptr<Match>& keep) {
    Setup setup;
    setup.score = ScoreRule::None;
    setup.sky = apply;
    keep = std::make_unique<Match>(link, setup, setup);
    out = keep.get();
    for (int i = 0; i < 90; ++i) keep->Frame();
    return keep->Log().transitions;
}

} // namespace

TEST_CASE("statesync: the server's request reaches the client: AdoptByRequest runs the exit and the enter") {
    Match* match = nullptr;
    std::unique_ptr<Match> keep;
    const auto transitions = RunSky(SkyApply::Request, {}, match, keep);
    CHECK(match->ServerWorld().Resource<SM::StateMachine<Sky>>()->Current() == Sky::Fog);
    CHECK(match->ClientWorld().Resource<SM::StateMachine<Sky>>()->Current() == Sky::Fog);
    const std::vector<std::string> expected{"Enter.Clear", "Exit.Clear", "Enter.Fog"};
    CHECK_MESSAGE(transitions == expected,
                  "the client ran its initial enter, then the exit and enter of the server's change");
}

TEST_CASE("statesync: AdoptByRequestSilent changes the value without the exit and the enter; Changed() is true") {
    Match* match = nullptr;
    std::unique_ptr<Match> keep;
    const auto transitions = RunSky(SkyApply::Silent, {}, match, keep);
    CHECK(match->ClientWorld().Resource<SM::StateMachine<Sky>>()->Current() == Sky::Fog);
    const std::vector<std::string> expected{"Enter.Clear"};
    CHECK_MESSAGE(transitions == expected, "a silent apply runs no exit and no enter");
    std::size_t changedToFog = 0;
    for (const auto& frame : match->Log().frames) {
        if (frame.changedAfterApply && frame.afterApply == Sky::Fog) ++changedToFog;
    }
    CHECK_MESSAGE(changedToFog == 1, "Res<StateMachine<Sky>>::Changed() is true once, when the value changed");
}

TEST_CASE("statesync: the same value sent again changes nothing: no exit, no enter, Changed() not true (CCR-8)") {
    for (const SkyApply apply : {SkyApply::Request, SkyApply::Silent}) {
        CAPTURE(static_cast<int>(apply));
        Match* match = nullptr;
        std::unique_ptr<Match> keep;
        const auto transitions = RunSky(apply, ConfirmationsLost(), match, keep);
        const auto& replica = match->Replica();
        MESSAGE("statesync CCR-8: kind=" << static_cast<int>(apply) << " resource values applied=" << replica.resourceValuesApplied
                                         << " sent=" << match->Ledger().resourceValuesSent);
        REQUIRE_MESSAGE(replica.resourceValuesApplied > 3, "the same sky arrived again (confirmations lost)");
        CHECK(match->ClientWorld().Resource<SM::StateMachine<Sky>>()->Current() == Sky::Fog);
        const std::vector<std::string> expected = apply == SkyApply::Request
                                                      ? std::vector<std::string>{"Enter.Clear", "Exit.Clear", "Enter.Fog"}
                                                      : std::vector<std::string>{"Enter.Clear"};
        CHECK_MESSAGE(transitions == expected, "a value sent again runs no exit and no enter");
        std::size_t changed = 0;
        const auto& frames = match->Log().frames;
        for (std::size_t i = 1; i < frames.size(); ++i) changed += frames[i].changedAfterApply ? 1 : 0;
        CHECK_MESSAGE(changed == 1, "Changed() is true only for the frame the value changed, not for the values sent again");
        CHECK(match->ClientWorld().Resource<SM::StateMachineRequest<Sky>>()->Overwritten() == 0);
    }
}

TEST_CASE("statesync: the value changes the frame it arrives, at the state's apply: earlier Systems see the old one") {
    for (const SkyApply apply : {SkyApply::Request, SkyApply::Silent}) {
        CAPTURE(static_cast<int>(apply));
        Match* match = nullptr;
        std::unique_ptr<Match> keep;
        (void)RunSky(apply, {}, match, keep);
        const auto& frames = match->Log().frames;
        std::optional<std::size_t> first;
        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (frames[i].afterApply == Sky::Fog) {
                first = i;
                break;
            }
        }
        REQUIRE(first.has_value());
        REQUIRE(*first > 0);
        const auto& at = frames[*first];
        // Received in Main/First this frame (the request is pending in PreUpdate), applied after PreUpdate.
        CHECK_MESSAGE(at.pendingBeforeApply == Sky::Fog, "the request was written this frame, before PreUpdate (the net Module receives in First)");
        CHECK_MESSAGE(at.beforeApply == Sky::Clear, "a System before the apply (PreUpdate) still sees the old value");
        CHECK_MESSAGE(at.afterApply == Sky::Fog, "a System after the apply (Update) sees the new value the same frame");
        CHECK(!frames[*first - 1].pendingBeforeApply.has_value());
        CHECK(frames[*first - 1].afterApply == Sky::Clear);
    }
}
