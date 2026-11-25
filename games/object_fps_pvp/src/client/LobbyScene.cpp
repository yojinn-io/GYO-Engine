// The lobby (Scene::Lobby; assets/object_fps_pvp/ui/pvp_lobby.json, canvas "lobby"; game-flow declaration 3.4): the
// Gateway's open rooms, polled every second; a room is made (mode and map) or joined, and the room's scene follows. The
// player's record (GET /v1/saves/me) is read again every five seconds (the Match writes it when the player leaves, a
// little after the lobby comes back).
//
//   Main/Update  LobbyActions  pvp.prev / pvp.next (the room picked, kept by its id), pvp.mode / pvp.map (what a new
//                              room is), pvp.join, pvp.create (asked of the Gateway), pvp.back (the menu)
//                LobbyGateway  the calls: the list, the record, the join or the creation (-> the room's scene)

#include "client/SceneBuilds.hpp"

#include "object_fps_pvp/match/PlayerSave.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace Ui = GYO::Engine::Ui;
namespace WT = GYO::Engine::WorldTime;

namespace {

constexpr std::array<std::string_view, 2> kModes = {"coop", "survival"};
constexpr std::int64_t kListEvery = 1'000'000'000;
constexpr std::int64_t kRecordEvery = 5'000'000'000;
// The rows the document's list shows (pvp_lobby.json: room_list's max_items).
constexpr std::size_t kRows = 6;

// The lobby's own state.
struct LobbyState final {
    std::vector<Room> rooms;
    std::uint64_t picked = 0;  // the room picked, by id (0: none); found again in each new list
    std::size_t mode = 0;
    std::size_t map = 0;
    std::vector<std::string> maps;
    std::optional<std::int64_t> listed;    // when the list was last asked for
    std::optional<std::int64_t> recorded;  // when the record was last asked for
    bool asking = false;                   // a join or a creation is going
    std::string gateway;                   // its address, for the player's words
};
void HashValue(K::Hasher&, const LobbyState&) {}

// The rows shown, and where the picked room is among them.
std::size_t Shown(const LobbyState& state) { return std::min(state.rooms.size(), kRows); }
std::optional<std::size_t> PickedRow(const LobbyState& state) {
    for (std::size_t i = 0; i < Shown(state); ++i) {
        if (state.rooms[i].id == state.picked) return i;
    }
    return std::nullopt;
}

Ui::UiList RoomRows(const LobbyState& state) {
    Ui::UiList list;
    for (std::size_t i = 0; i < Shown(state); ++i) {
        const Room& room = state.rooms[i];
        Ui::UiListItem item;
        item.fields["line"] = std::string(room.id == state.picked ? "▶ " : "　 ") + "#" + std::to_string(room.id) + "  " + ModeText(room.mode) +
                              "  " + room.map + "  " + std::to_string(room.players) + "/" + std::to_string(room.capacity);
        list.items.push_back(std::move(item));
    }
    return list;
}

void ShowState(const LobbyState& state, Ui::UiBindings& bindings) {
    bindings.Set("rooms", RoomRows(state));
    bindings.Set("mode", "モード: " + ModeText(kModes[state.mode]));
    bindings.Set("map", "マップ: " + (state.maps.empty() ? std::string("—") : state.maps[state.map]));
    bindings.Set("empty", std::string(state.rooms.empty() ? "開いている部屋はありません" : ""));
}

std::string RecordText(const SaveAnswer& answer) {
    std::string error;
    const auto save = match::DecodePlayerSave(answer.body, error);
    // A save this Client does not read (a newer saveVersion): the Client is older than the game's saves.
    if (!save) return "戦績を読めません。ゲームを更新してください（" + error + "）";
    if (save->matches == 0) return "戦績：まだありません";
    return "戦績：出撃 " + std::to_string(save->matches) + " 回・死亡 " + std::to_string(save->deaths) + " 回";
}

K::SystemResult LobbyActions(K::MessageReader<Ui::UiAction> actions, K::ResMut<LobbyState> state, K::ResMut<GatewayClient> gateway,
                             K::ResMut<SceneRequest> request, K::Query<K::Write<Ui::UiBindings>> bindings) {
    for (const Ui::UiAction* action : actions.Read()) {
        LobbyState& lobby = *state;
        const std::string& id = action->action;
        if (id == "pvp.back") {
            request->next = SceneChange{.scene = Scene::Menu};
        } else if (id == "pvp.prev" || id == "pvp.next") {
            const std::size_t count = Shown(lobby);
            if (count > 0) {
                const std::size_t at = PickedRow(lobby).value_or(id == "pvp.next" ? count - 1 : 0);
                lobby.picked = lobby.rooms[(at + (id == "pvp.next" ? 1 : count - 1)) % count].id;
            }
        } else if (id == "pvp.mode") {
            lobby.mode = (lobby.mode + 1) % kModes.size();
        } else if (id == "pvp.map") {
            if (!lobby.maps.empty()) lobby.map = (lobby.map + 1) % lobby.maps.size();
        } else if ((id == "pvp.join" || id == "pvp.create") && !lobby.asking) {
            if (id == "pvp.join") {
                if (!PickedRow(lobby)) continue;
                gateway->Ask("enter", JoinRequest(lobby.picked, RandomHex(16)));
            } else {
                if (lobby.maps.empty()) continue;
                gateway->Ask("enter", CreateRoomRequest(std::string(kModes[lobby.mode]), lobby.maps[lobby.map], RandomHex(16)));
            }
            lobby.asking = true;
            if (auto root = bindings.GetMut<Ui::UiBindings>(action->root)) root->Set("status", std::string("Gateway に問い合わせています…"));
        }
        if (auto root = bindings.GetMut<Ui::UiBindings>(action->root)) ShowState(lobby, *root);
    }
    return {};
}

K::SystemResult LobbyGateway(K::Res<WT::RealTime> time, K::ResMut<LobbyState> state, K::ResMut<GatewayClient> gateway,
                             K::ResMut<SceneRequest> request, K::Query<K::Write<Ui::UiBindings>> bindings) {
    const std::int64_t now = time->elapsed;
    LobbyState& lobby = *state;
    if (!gateway.Read().Busy()) {
        if (!lobby.listed || now - *lobby.listed >= kListEvery) {
            gateway->Ask("list", ListRoomsRequest());
            lobby.listed = now;
        }
        if (!lobby.recorded || now - *lobby.recorded >= kRecordEvery) {
            gateway->Ask("record", SaveRequest());
            lobby.recorded = now;
        }
    }
    const auto result = gateway->Update(now);
    if (!result || request.Read().next) return {};
    // status: what the player asked (or why the lobby came back); notice: the Gateway's state, from the polls. A call
    // another scene asked (the evacuation's leave) ends here without a word.
    std::optional<std::string> status;
    std::optional<std::string> notice;
    std::optional<std::string> record;
    if (result->tag == "enter") {
        lobby.asking = false;
        std::string error;
        const auto room = result->Ok() ? ParseRoom(result->body, error) : std::nullopt;
        if (room) {
            request->next = SceneChange{.scene = Scene::Room, .room = room->id};
            return {};
        }
        status = result->Ok() ? "Gateway の答えを読めません: " + error : GatewayErrorText(*result, lobby.gateway);
    } else if (result->tag == "list" || result->tag == "record") {
        std::string error;
        if (!result->Ok()) {
            notice = GatewayErrorText(*result, lobby.gateway);
        } else if (result->tag == "list") {
            if (auto rooms = ParseRooms(result->body, error)) {
                lobby.rooms = std::move(*rooms);
                // The picked room where it is now; the first one when it is gone.
                if (!PickedRow(lobby)) lobby.picked = Shown(lobby) > 0 ? lobby.rooms.front().id : 0;
                notice = std::string();
            } else {
                notice = "Gateway の答えを読めません: " + error;
            }
        } else {
            const auto answer = ParseSave(result->body, error);
            record = answer ? RecordText(*answer) : "戦績を読めません: " + error;
        }
    }
    for (const K::EntityId id : bindings.Entities()) {
        auto root = bindings.GetMut<Ui::UiBindings>(id);
        ShowState(lobby, *root);
        if (status) root->Set("status", *status);
        if (notice) root->Set("notice", *notice);
        if (record) root->Set("record", *record);
    }
    return {};
}

} // namespace

void BuildLobby(O::WorldSetup& setup, const SceneChange& change, const GatewayContext& context) {
    K::Registrar& r = setup.Registrar();
    r.Resource<LobbyState>({.name = "object_fps_pvp.LobbyState"});
    r.AddSystem<&LobbyActions>(InSceneUpdate("object_fps_pvp.LobbyActions"));
    r.AddSystem<&LobbyGateway>(InSceneUpdate("object_fps_pvp.LobbyGateway", {"object_fps_pvp.LobbyActions"}));
    setup.Prepare([change, context](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        LobbyState state;
        state.maps = context.maps;
        state.gateway = context.gateway.ToString();
        Ui::UiBindings bindings;
        ShowState(state, bindings);
        bindings.Set("record", std::string("戦績：—"));
        bindings.Set("status", change.status.empty() ? std::string("部屋を選ぶか、作ってください") : change.status);
        bindings.Set("notice", "Gateway: " + state.gateway);
        w.SetResource(std::move(state));
        PlaceSceneDocument(w, "object_fps_pvp.ui.lobby", "lobby", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
