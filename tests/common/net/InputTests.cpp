// Inputs (declaration D-11, C-IN1 to C-IN3): judgement G-6 as declared in section 8 (not loosened after the
// results), and the entry's consistency fingerprint at both ends.
//
// Ticks: the client's inputs are keyed by its own Tick, which the clock sync (3b, D-9) keeps ahead of the Match's
// (3a keyed them by the server's tick read by the test; that temporary arrangement is gone).
#include <doctest/doctest.h>

#include <map>
#include <set>
#include <memory>
#include <string>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

// The player's input: a move and a one-shot fire.
struct Move {
    std::int32_t dx = 0;
    std::uint8_t fire = 0;
    friend bool operator==(const Move&, const Move&) = default;
};
inline void HashValue(K::Hasher& h, const Move& v) {
    HashValue(h, v.dx);
    HashValue(h, v.fire);
}
N::Codec<Move> MoveCodec() {
    return {[](N::NetWriter& w, const Move& v) {
                w.Value(v.dx);
                w.Value(v.fire);
            },
            [](N::NetReader& r, Move& v) { return r.Value(v.dx) && r.Value(v.fire); }, 1};
}
N::InputOptions<Move> MoveOptions() {
    N::InputOptions<Move> options;
    options.codec = MoveCodec();
    options.held = [](Move& move) { move.fire = 0; };
    return options;
}

// What the server's step saw, per tick (FixedUpdate/Update, after the entry in FixedUpdate/First).
struct InputLog {
    std::map<std::uint32_t, Move> seen;
};
inline void HashValue(K::Hasher&, const InputLog&) {}

K::SystemResult RecordMoves(K::Query<K::Read<Move>> moves, K::Res<W::Tick> tick, K::ResMut<InputLog> log) {
    moves.ForEach([&](K::EntityId, const Move& move) { log->seen[static_cast<std::uint32_t>(tick->value)] = move; });
    return {};
}

void RegisterInputGame(O::WorldSetup& s, const bool decision) {
    RegisterGame(s, GameRules{1, false});
    if (decision) {
        A::AuthorityEntry(s).Component<Move>("Move", {.input = true});
    } else {
        s.Registrar().Component<Move>({.name = "Move"});
    }
    s.Registrar().Resource<InputLog>({.name = "InputLog"});
    N::NetEntry(s).Input<Move>("Move", MoveOptions());
    K::SystemOptions record;
    record.name = "RecordMoves";
    K::Mount mount;
    mount.schedule = "FixedUpdate";
    mount.stage = "Update";
    mount.conditions.push_back(N::OnServer());
    record.mounts.push_back(std::move(mount));
    s.Registrar().AddSystem<&RecordMoves>(std::move(record));
    s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(InputLog{});
        return {};
    });
}

struct Clock {
    std::shared_ptr<N::NetTime> now = std::make_shared<N::NetTime>(0);
};

struct InputGame {
    Clock clock;
    std::pair<N::MemoryLink, N::MemoryLink> links;
    End server = GameEnd(N::NetRole::Server, {});
    End client = GameEnd(N::NetRole::Client, {});
    std::unique_ptr<O::Product> serverProduct, clientProduct;
    std::uint64_t lostInputMessages = 0;

    explicit InputGame(const N::MemoryLinkSettings& link, const bool decision = false) : links(N::MemoryLink::Pair(link)) {
        auto reader = [now = clock.now] { return *now; };
        server.net.link = std::make_shared<N::MemoryLink>(links.first);
        server.net.clock = reader;
        server.net.seed = 13;
        server.build = [decision](O::WorldSetup& s) { RegisterInputGame(s, decision); };
        client.net.link = std::make_shared<N::MemoryLink>(links.second);
        client.net.serverEndpoint = N::MemoryLink::EndpointA();
        client.net.userId = 3;
        client.net.clock = reader;
        client.build = [decision](O::WorldSetup& s) { RegisterInputGame(s, decision); };
        if (decision) {
            server.authority = std::make_shared<A::ServerAuthorityStrategy>();
            client.authority = std::make_shared<A::ServerAuthorityStrategy>();
        }
        auto& lost = lostInputMessages;
        links.second.SetInjectionObserver([&lost](N::MemoryLink::Injection kind, std::span<const std::byte> datagram) {
            if (kind != N::MemoryLink::Injection::Loss) return;
            const auto summary = N::Connection::Inspect(datagram);
            if (summary && summary->piecesPerChannel.size() > N::kValuesChannel) lost += summary->piecesPerChannel[N::kValuesChannel];
        });
    }
    void Build() {
        auto s = O::Product::Build(Describe(server), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(s), Reported(s));
        serverProduct = std::move(*s);
        auto c = O::Product::Build(Describe(client), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(c), Reported(c));
        clientProduct = std::move(*c);
    }
    void Frame() {
        REQUIRE(static_cast<bool>(serverProduct->RunFrame()));
        REQUIRE(static_cast<bool>(clientProduct->RunFrame()));
        links.first.Advance();
        *clock.now += Ms(16);
    }
    K::World& ServerWorld() { return serverProduct->World("Net").GetWorld(); }
    K::World& ClientWorld() { return clientProduct->World("Net").GetWorld(); }
    std::uint32_t ServerTick() { return static_cast<std::uint32_t>(ServerWorld().Resource<W::Tick>()->value); }
    K::EntityId ServerPeer() {
        for (const auto& [id, entity] : ServerWorld().Resource<N::NetWorldLink>()->peers) return entity;
        return 0;
    }
};

constexpr K::EntityId kPlayer = 99'999;

Move ScriptAt(const std::uint32_t tick) {
    return Move{static_cast<std::int32_t>((tick * 7) % 11) - 5, static_cast<std::uint8_t>(tick % 10 == 0 ? 1 : 0)};
}

} // namespace

TEST_CASE("input: G-6 the server's step gets the client's input through losses; missing ones are held and counted") {
    N::MemoryLinkSettings lossy;
    lossy.seed = 0x5EED0006u;
    lossy.aToB = {2000, 1000, 2500, 3, 0};
    lossy.bToA = {2000, 1000, 2500, 3, 0};
    InputGame game(lossy);
    game.Build();
    // Connect; the server's product gives the Peer its player.
    for (int i = 0; i < 6 && game.ServerPeer() == 0; ++i) game.Frame();
    REQUIRE(game.ServerPeer() != 0);
    game.ServerWorld().Insert(kPlayer, Move{});
    game.ServerWorld().Insert(game.ServerPeer(), N::PeerControls{kPlayer});
    for (int i = 0; i < 3; ++i) game.Frame();

    constexpr int kRounds = 400;
    std::map<std::uint32_t, Move> sent;
    std::set<std::uint32_t> gaps;
    std::uint32_t firstSent = 0;
    for (int round = 0; round < kRounds; ++round) {
        // The next step of the client's own (synced) Tick; a tick is given once (a backward jump does not redo it).
        const std::uint32_t target = static_cast<std::uint32_t>(game.ClientWorld().Resource<W::Tick>()->value) + 1;
        N::PeerInput<Move>* local = N::LocalInput<Move>(game.ClientWorld());
        REQUIRE(local != nullptr);
        // Every 50 ticks the two ticks after a press get no input at all: the server holds the last one (its press
        // cleared), so the hold and the one-shot rule are exercised whatever the clock sync does to the losses.
        const bool gap = target % 50 == 1 || target % 50 == 2;
        if (gap) {
            gaps.insert(target);
        } else if (sent.count(target) == 0 && (firstSent == 0 || target > firstSent)) {
            if (firstSent == 0) firstSent = target;
            local->Set(target, ScriptAt(target), N::ViewTime{target - 6, 128});
            sent[target] = ScriptAt(target);
        }
        // Once, a slot far beyond the server's window goes out with the others, then is taken back.
        const bool far = round == 200;
        if (far) local->Set(target + 500, Move{99, 0});
        game.Frame();
        if (far) N::LocalInput<Move>(game.ClientWorld())->slots.erase(target + 500);
    }
    for (int i = 0; i < 10; ++i) game.Frame();
    // The link drains: the server runs alone until nothing is on the way to it, so every input message sent is read
    // or was lost.
    for (int i = 0; i < 20 && game.links.first.InFlightToSelf() > 0; ++i) {
        REQUIRE(static_cast<bool>(game.serverProduct->RunFrame()));
        game.links.first.Advance();
    }
    REQUIRE(static_cast<bool>(game.serverProduct->RunFrame()));

    // Each server tick from the first input to the last: the client's value of that tick, or the last one held
    // (fire cleared) when its slot did not come (lost, late, or not given: the gaps).
    const std::uint32_t lastSent = sent.empty() ? 0 : sent.rbegin()->first;
    const auto& seen = game.ServerWorld().Resource<InputLog>()->seen;
    std::uint64_t matched = 0, held = 0, wrong = 0, heldInGaps = 0, firesSent = 0, firesSeen = 0, firesWhereNotSent = 0;
    std::optional<Move> previous;
    for (const auto& [tick, move] : seen) {
        if (tick < firstSent || tick > lastSent) {
            previous = move;
            continue;
        }
        const auto given = sent.find(tick);
        if (given != sent.end()) firesSent += given->second.fire;
        firesSeen += move.fire;
        if (move.fire != 0 && (given == sent.end() || given->second.fire == 0)) ++firesWhereNotSent;
        Move heldValue = previous.value_or(Move{});
        heldValue.fire = 0;
        if (given != sent.end() && move == given->second) {
            ++matched;
        } else if (move == heldValue) {
            ++held;
            heldInGaps += gaps.count(tick);
        } else {
            ++wrong;
        }
        previous = move;
    }
    const auto& counts = game.ServerWorld().Get<N::PeerInput<Move>>(game.ServerPeer())->counts;
    const auto* buffer = game.ServerWorld().Get<N::PeerInput<Move>>(game.ServerPeer());
    std::uint64_t ahead = 0;
    for (const auto& [tick, slot] : buffer->slots) ahead += (!buffer->consumedThrough || tick > *buffer->consumedThrough) ? 1 : 0;
    const auto& clientCounts = N::LocalInput<Move>(game.ClientWorld())->counts;
    const auto& clock = *game.ClientWorld().Resource<N::ClockSync>();
    MESSAGE("G-6 clock: jumps=" << clock.jumps << " last error=" << clock.lastError << " speed=" << clock.scale);
    MESSAGE("G-6: ticks compared=" << (matched + held + wrong) << " matched=" << matched << " held=" << held << " wrong=" << wrong
                                   << " (in the gaps " << heldInGaps << ")" << " | fires sent=" << firesSent << " seen=" << firesSeen << " where not sent=" << firesWhereNotSent
                                   << " | slots received=" << counts.slotsReceived << " stored=" << counts.stored
                                   << " duplicates=" << counts.duplicates << " late=" << counts.late << " outside window="
                                   << counts.outsideWindow << " | consumed=" << counts.consumed << " missing=" << counts.missing
                                   << " ahead=" << ahead << " | messages sent=" << clientCounts.messages << " read="
                                   << counts.messages << " lost with datagrams=" << game.lostInputMessages);
    CHECK_MESSAGE(wrong == 0, "G-6: each server tick has the client's input of that tick, or the last one held");
    // The steps without a slot: those held among the compared ticks, and the ticks before the first input and after
    // the last.
    const std::uint64_t notGiven = seen.size() - (matched + held + wrong);
    CHECK_MESSAGE(counts.missing == held + notGiven, "G-6: every held input is counted as missing");
    CHECK(heldInGaps > 0);
    CHECK_MESSAGE(counts.consumed + counts.missing == seen.size(), "G-6: steps run == consumed + missing");
    CHECK_MESSAGE(firesWhereNotSent == 0, "G-6: a one-shot input fires only at the tick it was given (once)");
    CHECK(firesSeen > 0);
    CHECK_MESSAGE(counts.slotsReceived == counts.stored + counts.duplicates + counts.late + counts.outsideWindow,
                  "G-6: slots received == stored + duplicates + late + outside the window");
    CHECK_MESSAGE(counts.stored == counts.consumed + ahead, "G-6: slots stored == consumed + still ahead");
    CHECK_MESSAGE(clientCounts.messages == counts.messages + game.lostInputMessages,
                  "G-6: input messages sent == read + lost with the injected losses");
    CHECK_MESSAGE(counts.outsideWindow >= 1, "G-6: a slot beyond the window is dropped and counted");
    CHECK(counts.late > 0);
    CHECK(counts.duplicates > 0);
    CHECK(counts.missing > 0);
}

TEST_CASE("input: the entry has the same name, access and mount at both ends, so the fingerprints agree") {
    InputGame game(N::MemoryLinkSettings{}, true);
    game.Build();
    for (int i = 0; i < 6; ++i) game.Frame();
    CHECK_MESSAGE(PeersOf(game.ServerWorld()).size() == 1, "a decision input's entry keeps one consistency fingerprint at both ends");
    const auto* registry = game.ServerWorld().Resource<A::AuthorityRegistry>();
    REQUIRE(registry != nullptr);
    bool entry = false;
    for (const auto& ingress : registry->ingress) entry = entry || ingress.system == "ApplyInput:Move";
    CHECK_MESSAGE(entry, "the entry is added through authority.Ingress where the World has a strategy");
}
