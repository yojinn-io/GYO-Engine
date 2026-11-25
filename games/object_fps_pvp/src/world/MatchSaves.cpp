#include "object_fps_pvp/world/MatchSaves.hpp"

#include "object_fps_pvp/world/Components.hpp"

#include "engine/http/Http.hpp"
#include "engine/json/Document.hpp"
#include "engine/json/Writer.hpp"
#include "engine/net/NetModule.hpp"

#include <string_view>

#include <map>
#include <optional>
#include <utility>

namespace object_fps_pvp::world {

namespace K = GYO::Kernel;
namespace N = GYO::Engine::Net;
namespace H = GYO::Engine::Http;
namespace J = GYO::Engine::Json;

namespace {

// A call that got no answer is sent this many times in all.
constexpr int kAttempts = 3;

enum class CallKind : std::uint8_t { Read, Write };

struct Player final {
    bool leaving = false;
    bool loaded = false;
    // The save service refused, or never answered: nothing more is sent for this player.
    bool gaveUp = false;
    std::uint64_t version = 0;
    match::PlayerSave save;
    std::uint64_t sessionDeaths = 0;
    std::uint64_t lastRespawnTick = 0;
    std::uint32_t leaves = 0;
    // A call wanted (read or write), the call in flight, how many times it went out.
    bool pending = false;
    CallKind kind = CallKind::Read;
    std::optional<H::HttpCall> call;
    int attempts = 0;
    std::string writeBody;
};

} // namespace

struct MatchSaves::State final {
    MatchSavesOptions options;
    std::map<std::uint64_t, Player> players;
    MatchSavesReport report;
};

MatchSavesReport MatchSaves::Report() const { return state ? state->report : MatchSavesReport{}; }

namespace {

void Failed(MatchSaves::State& state, Player& player, std::string why) {
    ++state.report.failures;
    state.report.lastFailure = std::move(why);
    player.call.reset();
    player.pending = false;
    player.gaveUp = true;
}

std::optional<J::Value> Member(const J::Document& document, const std::string_view key) {
    auto object = document.Root().AsObject();
    if (!object) return std::nullopt;
    return object->Find(key);
}

std::optional<std::uint64_t> UnsignedMember(const J::Document& document, const std::string_view key) {
    auto value = Member(document, key);
    if (!value) return std::nullopt;
    auto number = value->AsUint64();
    if (!number) return std::nullopt;
    return *number;
}

// Sends the player's call (again); one the OS refused to start stays pending for the next frame.
void Send(MatchSaves::State& state, Player& player, const std::uint64_t user) {
    if (player.attempts >= kAttempts) {
        Failed(state, player, "the save service did not answer");
        return;
    }
    H::HttpRequest request;
    request.method = player.kind == CallKind::Read ? "GET" : "PUT";
    request.target = "/internal/v1/saves/" + std::to_string(user);
    request.host = state.options.service.ToString();
    request.headers.push_back({"X-GYO-User", "0"});
    if (player.kind == CallKind::Write) {
        request.headers.push_back({"Content-Type", "application/json"});
        request.headers.push_back({"Idempotency-Key", "match-" + std::to_string(state.options.matchId) + "-user-" +
                                                          std::to_string(user) + "-leave-" + std::to_string(player.leaves + 1)});
        request.body = player.writeBody;
    }
    ++player.attempts;
    auto call = H::HttpCall::Start(state.options.service, request);
    if (call) player.call.emplace(std::move(call).value());
}

void Want(MatchSaves::State& state, Player& player, const std::uint64_t user, const CallKind kind) {
    player.kind = kind;
    player.pending = true;
    player.attempts = 0;
    if (kind == CallKind::Write) {
        // {"version": the version read, "body": the settled save}
        player.writeBody = "{\"version\":" + std::to_string(player.version) +
                           ",\"body\":" + match::EncodePlayerSave(match::Settled(player.save, player.sessionDeaths)) + "}";
    }
    Send(state, player, user);
}

void TakeRead(MatchSaves::State& state, Player& player, const H::HttpResponse& answer) {
    if (answer.status != 200) return Failed(state, player, "the save service answered " + std::to_string(answer.status) + " to a read: " + answer.body);
    auto document = J::Parse(answer.body);
    if (!document) return Failed(state, player, "the save service's read is not JSON");
    const auto version = UnsignedMember(*document, "version");
    const auto body = Member(*document, "body");
    if (!version || !body) return Failed(state, player, "the save service's read has no version or body");
    std::string text;
    J::Writer writer(text);
    J::WriteValue(writer, *body);
    (void)writer.Finish();
    std::string error;
    auto save = match::DecodePlayerSave(text, error);
    if (!save) return Failed(state, player, "the save does not read: " + error);
    player.version = *version;
    player.save = *save;
    player.loaded = true;
    ++state.report.reads;
}

void TakeWrite(MatchSaves::State& state, Player& player, const H::HttpResponse& answer) {
    if (answer.status != 200) return Failed(state, player, "the save service answered " + std::to_string(answer.status) + " to a write: " + answer.body);
    auto document = J::Parse(answer.body);
    const auto version = document ? UnsignedMember(*document, "version") : std::nullopt;
    if (!version) return Failed(state, player, "the save service's write has no version");
    player.save = match::Settled(player.save, player.sessionDeaths);
    player.version = *version;
    player.sessionDeaths = 0;
    player.leaving = false;
    ++player.leaves;
    ++state.report.writes;
}

// One step of the player's pending call: poll it, or send it again when it got no answer.
void Step(MatchSaves::State& state, Player& player, const std::uint64_t user) {
    if (!player.call) return Send(state, player, user);
    auto polled = player.call->Poll();
    if (!polled) {
        player.call.reset();
        return Send(state, player, user);
    }
    if (!*polled) return;
    const H::HttpResponse answer = std::move(**polled);
    player.call.reset();
    player.pending = false;
    if (player.kind == CallKind::Read) TakeRead(state, player, answer);
    else TakeWrite(state, player, answer);
}

K::SystemResult SyncSaves(K::MessageReader<N::PeerConnected> connected, K::MessageReader<N::PeerDisconnected> disconnected,
                          K::Query<K::Read<Owner>, K::Read<PlayerCombat>> players, K::ResMut<MatchSaves> saves) {
    MatchSaves::State& state = *saves->state;
    for (const N::PeerConnected* joined : connected.Read()) {
        Player& player = state.players[joined->userId];
        player.leaving = false;
        if (!player.loaded && !player.pending && !player.gaveUp) Want(state, player, joined->userId, CallKind::Read);
    }
    // A death is a respawn tick not seen before (set when the player dies, the tick they come back).
    players.ForEach([&](K::EntityId, const Owner& owner, const PlayerCombat& combat) {
        auto found = state.players.find(owner.userId);
        if (found == state.players.end()) return;
        if (combat.life == sim::LifeState::Dead && combat.respawnTick > found->second.lastRespawnTick) {
            found->second.lastRespawnTick = combat.respawnTick;
            ++found->second.sessionDeaths;
        }
    });
    for (const N::PeerDisconnected* gone : disconnected.Read()) {
        auto found = state.players.find(gone->userId);
        if (found != state.players.end()) found->second.leaving = true;
    }
    for (auto& [user, player] : state.players) {
        if (player.pending) Step(state, player, user);
        if (player.leaving && player.loaded && !player.pending && !player.gaveUp) Want(state, player, user, CallKind::Write);
    }
    return {};
}

} // namespace

O::WorldDescription WithMatchSaves(O::WorldDescription world, MatchSavesOptions options) {
    world.build = [inner = std::move(world.build), options](O::WorldSetup& setup) {
        inner(setup);
        setup.Registrar().Resource<MatchSaves>({.name = "MatchSaves"});
        K::SystemOptions sync;
        sync.name = "SyncSaves";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::last);
        mount.conditions.push_back(N::OnServer());
        sync.mounts.push_back(std::move(mount));
        setup.Registrar().AddSystem<&SyncSaves>(std::move(sync));
        setup.Prepare([options](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            auto state = std::make_shared<MatchSaves::State>();
            state->options = options;
            w.SetResource(MatchSaves{std::move(state)});
            return {};
        });
    };
    return world;
}

} // namespace object_fps_pvp::world
