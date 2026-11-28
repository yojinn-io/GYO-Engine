// A room of the Gateway (Screen::Room; assets/object_fps_pvp/ui/pvp_room.json, canvas "room"; game-flow declaration
// 3.4): its members and who is ready, polled every 0.75 s. The player gets ready (or not); the owner starts it once
// everyone is. When the room has started and gives this player's connect, the game follows, connected to the Match
// (the token is good for two minutes and comes fresh with each read). A room this player is no longer in (gone, its
// match ended, or left elsewhere) sends it back to the lobby.
//
//   Main/Update  RoomActions  pvp.ready (toggles what the player asked last), pvp.start, pvp.leave (-> the lobby once the
//                             Gateway answers)
//                RoomGateway  the calls and their answers (-> the game, or back to the lobby)

#include "client/ScreenBuilds.hpp"

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
constexpr std::string_view kRoomInvitation = "全員が準備したら、部屋主が開始します";

struct RoomState final {
    std::uint64_t id = 0;
    std::optional<Room> room;
    // The readiness the player asked for, until the Gateway answered every ready call (a read of the room asked before
    // them shows the room as it was: it does not count).
    std::optional<bool> asked;
    int readyCalls = 0;  // ready calls asked and not answered
    std::optional<std::int64_t> polled;
    bool leaving = false;
    std::string gateway;
    std::vector<ModeChoice> modes;  // the installed modes (the room's mode is shown by its name)
    // The status line: the newest word -- on the player's last action (a refusal stays to be read: a read that goes
    // well does not clear it), or on a read that went wrong (it takes the line, and goes with the next good read);
    // with neither, the invitation.
    std::optional<std::string> said;
    std::string problem;
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

// Whether this player owns the room (as far as it is known: before the first read, it may).
bool OwnerOf(const RoomState& state, const std::uint64_t self) { return !state.room || state.room->owner == self; }

void ShowRoom(const RoomState& state, const std::uint64_t self, Ui::UiBindings& bindings) {
    Ui::UiList members;
    if (state.room) {
        bindings.Set("mode", "モード: " + ModeText(state.modes, state.room->mode));
        bindings.Set("map", "マップ: " + state.room->map);
        for (const RoomMember& member : state.room->members) {
            Ui::UiListItem item;
            item.fields["line"] = PlayerLabel(member.userId, member.name) + (member.userId == state.room->owner ? "（部屋主）" : "") +
                                  (member.userId == self ? "（自分）" : "") + "  " + (member.ready ? "準備完了" : "準備中");
            members.items.push_back(std::move(item));
        }
    }
    bindings.Set("title", "部屋 #" + std::to_string(state.id));
    bindings.Set("members", std::move(members));
    bindings.Set("ready", std::string(ReadyOf(state, self) ? "準備をやめる" : "準備する"));
    // Only the owner starts the room (the Gateway refuses the others: not_owner).
    bindings.Set("start", std::string(OwnerOf(state, self) ? "開始" : "開始（部屋主のみ）"));
    bindings.Set("status", state.said ? *state.said : state.problem.empty() ? std::string(kRoomInvitation) : state.problem);
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
            ++room.readyCalls;
            room.said.reset();
            gateway->Ask("ready", ReadyRequest(room.id, ready, RandomHex(16)));
        } else if (action->action == "pvp.start") {
            if (OwnerOf(room, SelfOf(*gateway))) {
                gateway->Ask("start", StartRequest(room.id, RandomHex(16)));
                room.said = "開始しています…";
            } else {
                room.said = GatewayErrorText(GatewayResult{"start", 0, {}, "not_owner", {}}, room.gateway);
            }
        } else if (action->action == "pvp.leave") {
            gateway->Ask("leave", LeaveRequest(room.id, RandomHex(16)));
            room.leaving = true;
        }
        if (auto root = bindings.GetMut<Ui::UiBindings>(action->root)) ShowRoom(room, SelfOf(*gateway), *root);
    }
    return {};
}

K::SystemResult RoomGateway(K::Res<WT::RealTime> time, K::ResMut<RoomState> state, K::ResMut<GatewayClient> gateway,
                            K::ResMut<ScreenRequest> request, K::Query<K::Write<Ui::UiBindings>> bindings) {
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
        if (result->tag == "leave") request->next = ScreenChange{.screen = Screen::Lobby};
        return {};
    }
    // The last ready call answered (done or refused): the room's own word on the readiness from now on.
    if (result->tag == "ready" && --room.readyCalls == 0) room.asked.reset();
    // A word on the player's action stays; one on a read lasts until the next read.
    const bool action = result->tag == "ready" || result->tag == "start";
    const auto tell = [&](std::string text) {
        if (action) {
            room.said = std::move(text);
        } else {
            room.problem = std::move(text);
            room.said.reset();
        }
    };
    if (!result->Ok()) {
        // This player is no longer in the room (it is gone, its match ended, or it was left elsewhere): the lobby.
        const bool out = result->status == 404 || result->status == 410 || result->errorCode == "not_member";
        if (out) {
            request->next = ScreenChange{.screen = Screen::Lobby, .status = GatewayErrorText(*result, room.gateway)};
            return {};
        }
        tell(GatewayErrorText(*result, room.gateway));
    } else {
        std::string error;
        if (auto read = ParseRoom(result->body, error)) {
            room.room = std::move(*read);
            if (!action) room.problem.clear();
        } else {
            tell("Gateway の答えを読めません: " + error);
        }
    }
    // Started, with this player's connect: the game, connected to the Match.
    if (room.room && room.room->state == "started" && room.room->connect) {
        const RoomConnect& connect = *room.room->connect;
        const auto server = ParseMatchAddress(connect.server);
        const auto token = data::DecodeHex(connect.token);
        if (server && token) {
            request->next = ScreenChange{.screen = Screen::MultiGame,
                                        .connect = N::NetConnectTarget{*server, connect.userId, *token},
                                        .room = room.id,
                                        .map = room.room->map,
                                        .mode = room.room->mode};
            return {};
        }
        room.problem = "Gateway の接続先を読めません: " + connect.server;
        room.said.reset();
    }
    for (const K::EntityId id : bindings.Entities()) ShowRoom(room, SelfOf(*gateway), *bindings.GetMut<Ui::UiBindings>(id));
    return {};
}

} // namespace

void BuildRoom(O::WorldSetup& setup, const ScreenChange& change, const GatewayContext& context) {
    K::Registrar& r = setup.Registrar();
    r.Resource<RoomState>({.name = "object_fps_pvp.RoomState"});
    r.AddSystem<&RoomActions>(InScreenUpdate("object_fps_pvp.RoomActions"));
    r.AddSystem<&RoomGateway>(InScreenUpdate("object_fps_pvp.RoomGateway", {"object_fps_pvp.RoomActions"}));
    setup.Prepare([change, context](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        RoomState state;
        state.id = change.room;
        state.gateway = context.gateway.ToString();
        state.modes = context.modes;
        if (!change.status.empty()) state.said = change.status;
        Ui::UiBindings bindings;
        // (the mode and the map come with the first read of the room)
        bindings.Set("mode", std::string("モード: —"));
        bindings.Set("map", std::string("マップ: —"));
        const auto* gateway = w.Resource<GatewayClient>();
        ShowRoom(state, gateway != nullptr ? SelfOf(*gateway) : 0, bindings);
        w.SetResource(std::move(state));
        PlaceScreenDocument(w, "object_fps_pvp.ui.room", "room", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
