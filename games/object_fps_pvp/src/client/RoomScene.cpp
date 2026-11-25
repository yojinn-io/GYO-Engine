// A room of the Gateway (Scene::Room; assets/object_fps_pvp/ui/pvp_room.json, canvas "room"; game-flow declaration
// 3.4): its members and who is ready, polled every 0.75 s. The player gets ready (or not); the owner starts it once
// everyone is. When the room has started and gives this player's connect, the game follows, connected to the Match
// (the token is good for two minutes and comes fresh with each read). A room this player is no longer in (gone, its
// match ended, or left elsewhere) sends it back to the lobby.
//
//   Main/Update  RoomActions  pvp.ready (toggles what the player asked last), pvp.start, pvp.leave (-> the lobby once the
//                             Gateway answers)
//                RoomGateway  the calls and their answers (-> the game, or back to the lobby)

#include "client/SceneBuilds.hpp"

#include "object_fps_pvp/data/Hex.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <utility>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace N = GYO::Engine::Net;
namespace Ui = GYO::Engine::Ui;
namespace WT = GYO::Engine::WorldTime;

namespace {

constexpr std::int64_t kPollEvery = 750'000'000;

struct RoomState final {
    std::uint64_t id = 0;
    std::optional<Room> room;
    std::optional<bool> asked;  // the readiness the player asked for, until a read of the room shows it
    std::optional<std::int64_t> polled;
    bool leaving = false;
    std::string gateway;
};
void HashValue(K::Hasher&, const RoomState&) {}

bool ReadyOf(const RoomState& state, const std::uint64_t self) {
    if (state.asked) return *state.asked;
    if (!state.room) return false;
    for (const RoomMember& member : state.room->members) {
        if (member.userId == self) return member.ready;
    }
    return false;
}

void ShowRoom(const RoomState& state, const std::uint64_t self, Ui::UiBindings& bindings) {
    std::string title = "部屋 #" + std::to_string(state.id);
    Ui::UiList members;
    if (state.room) {
        title += "  " + ModeText(state.room->mode) + "  " + state.room->map;
        for (const RoomMember& member : state.room->members) {
            Ui::UiListItem item;
            item.fields["line"] = "Player " + std::to_string(member.userId) + (member.userId == state.room->owner ? "（部屋主）" : "") +
                                  (member.userId == self ? "（自分）" : "") + "  " + (member.ready ? "準備完了" : "準備中");
            members.items.push_back(std::move(item));
        }
    }
    bindings.Set("title", title);
    bindings.Set("members", std::move(members));
    bindings.Set("ready", std::string(ReadyOf(state, self) ? "準備をやめる" : "準備する"));
}

std::uint64_t SelfOf(const GatewayClient& gateway) { return gateway.Session() ? gateway.Session()->userId : 0; }

K::SystemResult RoomActions(K::MessageReader<Ui::UiAction> actions, K::ResMut<RoomState> state, K::ResMut<GatewayClient> gateway,
                            K::Query<K::Write<Ui::UiBindings>> bindings) {
    for (const Ui::UiAction* action : actions.Read()) {
        RoomState& room = *state;
        if (room.leaving) continue;
        if (action->action == "pvp.ready") {
            const bool ready = !ReadyOf(room, SelfOf(*gateway));
            room.asked = ready;
            gateway->Ask("ready", ReadyRequest(room.id, ready, RandomHex(16)));
        } else if (action->action == "pvp.start") {
            gateway->Ask("start", StartRequest(room.id, RandomHex(16)));
            if (auto root = bindings.GetMut<Ui::UiBindings>(action->root)) root->Set("status", std::string("開始しています…"));
        } else if (action->action == "pvp.leave") {
            gateway->Ask("leave", LeaveRequest(room.id, RandomHex(16)));
            room.leaving = true;
        }
        if (auto root = bindings.GetMut<Ui::UiBindings>(action->root)) ShowRoom(room, SelfOf(*gateway), *root);
    }
    return {};
}

K::SystemResult RoomGateway(K::Res<WT::RealTime> time, K::ResMut<RoomState> state, K::ResMut<GatewayClient> gateway,
                            K::ResMut<SceneRequest> request, K::Query<K::Write<Ui::UiBindings>> bindings) {
    const std::int64_t now = time->elapsed;
    RoomState& room = *state;
    if (!gateway.Read().Busy() && !room.leaving && (!room.polled || now - *room.polled >= kPollEvery)) {
        gateway->Ask("room", RoomRequest(room.id));
        room.polled = now;
    }
    const auto result = gateway->Update(now);
    if (!result || request.Read().next) return {};
    if (room.leaving) {
        // Leaving: only the leave's end counts (gone either way: an error says the room is gone already).
        if (result->tag == "leave") request->next = SceneChange{.scene = Scene::Lobby};
        return {};
    }
    std::optional<std::string> status;
    if (!result->Ok()) {
        // This player is no longer in the room (it is gone, its match ended, or it was left elsewhere): the lobby.
        const bool out = result->status == 404 || result->status == 410 || result->errorCode == "not_member";
        if (out) {
            request->next = SceneChange{.scene = Scene::Lobby, .status = GatewayErrorText(*result, room.gateway)};
            return {};
        }
        status = GatewayErrorText(*result, room.gateway);
        if (result->tag == "ready") room.asked.reset();
    } else {
        std::string error;
        if (auto read = ParseRoom(result->body, error)) {
            room.room = std::move(*read);
            if (result->tag == "room") room.asked.reset();
            status = std::string();
        } else {
            status = "Gateway の答えを読めません: " + error;
        }
    }
    // Started, with this player's connect: the game, connected to the Match.
    if (room.room && room.room->state == "started" && room.room->connect) {
        const RoomConnect& connect = *room.room->connect;
        const auto server = ParseMatchAddress(connect.server);
        const auto token = data::DecodeHex(connect.token);
        if (server && token) {
            request->next = SceneChange{.scene = Scene::MultiGame,
                                        .connect = N::NetConnectTarget{*server, connect.userId, *token},
                                        .room = room.id,
                                        .map = room.room->map};
            return {};
        }
        status = "Gateway の接続先を読めません: " + connect.server;
    }
    for (const K::EntityId id : bindings.Entities()) {
        auto root = bindings.GetMut<Ui::UiBindings>(id);
        ShowRoom(room, SelfOf(*gateway), *root);
        if (status && !status->empty()) root->Set("status", *status);
        if (status && status->empty()) root->Set("status", std::string("全員が準備したら、部屋主が開始します"));
    }
    return {};
}

} // namespace

void BuildRoom(O::WorldSetup& setup, const SceneChange& change, const GatewayContext& context) {
    K::Registrar& r = setup.Registrar();
    r.Resource<RoomState>({.name = "object_fps_pvp.RoomState"});
    r.AddSystem<&RoomActions>(InSceneUpdate("object_fps_pvp.RoomActions"));
    r.AddSystem<&RoomGateway>(InSceneUpdate("object_fps_pvp.RoomGateway", {"object_fps_pvp.RoomActions"}));
    setup.Prepare([change, context](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        RoomState state;
        state.id = change.room;
        state.gateway = context.gateway.ToString();
        Ui::UiBindings bindings;
        const auto* gateway = w.Resource<GatewayClient>();
        ShowRoom(state, gateway != nullptr ? SelfOf(*gateway) : 0, bindings);
        bindings.Set("status", change.status.empty() ? std::string("全員が準備したら、部屋主が開始します") : change.status);
        w.SetResource(std::move(state));
        PlaceSceneDocument(w, "object_fps_pvp.ui.room", "room", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
