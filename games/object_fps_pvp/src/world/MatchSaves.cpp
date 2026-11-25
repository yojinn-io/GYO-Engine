#include "object_fps_pvp/world/MatchSaves.hpp"

#include "object_fps_pvp/world/Components.hpp"
#include "object_fps_pvp/world/PvpWorld.hpp"

#include "engine/http/Http.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <map>
#include <utility>

namespace object_fps_pvp::world {

namespace K = GYO::Kernel;
namespace N = GYO::Engine::Net;
namespace H = GYO::Engine::Http;
namespace WT = GYO::Engine::WorldTime;

struct MatchSaves::State final {
    explicit State(const MatchSavesOptions& o) : options(o), sync(o.matchId) {}
    MatchSavesOptions options;
    match::SaveSync sync;
    std::map<std::uint64_t, H::HttpCall> calls;
    std::map<std::uint64_t, std::uint64_t> respawnSeen;
};

match::SaveSyncReport MatchSaves::Report() const { return state ? state->sync.Report() : match::SaveSyncReport{}; }
bool MatchSaves::Busy() const { return state && (state->sync.Busy() || !state->calls.empty()); }

namespace {

H::HttpRequest ToHttp(const MatchSaves::State& state, const match::SaveRequest& request) {
    H::HttpRequest http;
    http.method = request.write ? "PUT" : "GET";
    http.target = "/internal/v1/saves/" + std::to_string(request.user);
    http.host = state.options.service.ToString();
    http.headers.push_back({"X-GYO-User", "0"});
    if (request.write) {
        http.headers.push_back({"Content-Type", "application/json"});
        http.headers.push_back({"Idempotency-Key", request.key});
        http.body = request.body;
    }
    return http;
}

K::SystemResult SyncSaves(K::MessageReader<N::PeerConnected> connected, K::MessageReader<N::PeerDisconnected> disconnected,
                          K::Query<K::Read<Owner>, K::Read<PlayerCombat>> players, K::Res<WT::Tick> tick,
                          K::ResMut<MatchSaves> saves) {
    MatchSaves::State& state = *saves->state;
    // Leaves before joins: a reconnect from the same endpoint (the old connection ends and the new one comes in, in
    // one frame) leaves the player in the match.
    // A connection that ended before its Peer was made (peer 0) had no PeerConnected: it is no leave.
    for (const N::PeerDisconnected* gone : disconnected.Read()) {
        if (gone->peer != 0) state.sync.Left(gone->userId);
    }
    for (const N::PeerConnected* joined : connected.Read()) state.sync.Joined(joined->userId);
    // A death as a save counts it (CountDeath: a respawn tick not seen before).
    players.ForEach([&](K::EntityId, const Owner& owner, const PlayerCombat& combat) {
        if (CountDeath(combat, state.respawnSeen[owner.userId])) state.sync.Died(owner.userId);
    });
    const std::uint64_t now = tick->value;
    for (auto call = state.calls.begin(); call != state.calls.end();) {
        auto polled = call->second.Poll();
        if (polled && !*polled) {
            ++call;
            continue;
        }
        const match::SaveAnswer answer = polled ? match::SaveAnswer{(*polled)->status, (*polled)->body} : match::SaveAnswer{};
        const std::uint64_t id = call->first;
        call = state.calls.erase(call);
        state.sync.Answered(id, answer, now);
    }
    for (const std::uint64_t id : state.sync.TimedOut(now)) state.calls.erase(id);
    for (const match::SaveRequest& request : state.sync.Due(now)) {
        auto call = H::HttpCall::Start(state.options.service, ToHttp(state, request));
        if (call) state.calls.emplace(request.id, std::move(call).value());
        else state.sync.Answered(request.id, match::SaveAnswer{}, now);
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
            w.SetResource(MatchSaves{std::make_shared<MatchSaves::State>(options)});
            return {};
        });
    };
    return world;
}

} // namespace object_fps_pvp::world
