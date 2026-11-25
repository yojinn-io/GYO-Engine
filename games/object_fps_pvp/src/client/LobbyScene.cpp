// The lobby (Scene::Lobby; assets/object_fps_pvp/ui/pvp_lobby.json, canvas "lobby"; game-flow declaration 3.4): the
// Gateway's open rooms, polled every second; a room is made (mode and map) or joined, and the room's scene follows. The
// player's record (GET /v1/saves/me) is read again every five seconds (the Match writes it when the player leaves, a
// little after the lobby comes back). Coming back from a room's game, the lobby first leaves that room.
//
//   Main/Update  LobbyActions  pvp.prev / pvp.next (the room picked), pvp.mode / pvp.map (what a new room is),
//                              pvp.join, pvp.create (asked of the Gateway), pvp.back (the menu)
//                LobbyGateway  the calls: the list, the record, the join or the creation (-> the room's scene)
//
// Also the Gateway's scenes' common part (SceneBuilds.hpp): the GatewayClient, the error texts, ParseMatchAddress.

#include "client/SceneBuilds.hpp"

#include "object_fps_pvp/match/PlayerSave.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <charconv>
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
constexpr std::size_t kRows = 6;

std::string ModeText(const std::string_view mode) {
    if (mode == "coop") return "協力";
    if (mode == "survival") return "生存";
    return std::string(mode);
}

// The lobby's own state.
struct LobbyState final {
    std::vector<Room> rooms;
    std::size_t picked = 0;
    std::size_t mode = 0;
    std::size_t map = 0;
    std::vector<std::string> maps;
    std::optional<std::int64_t> listed;    // when the list was last asked for
    std::optional<std::int64_t> recorded;  // when the record was last asked for
    bool asking = false;                   // a join or a creation is going
    std::string gateway;                   // its address, for the player's words
};
void HashValue(K::Hasher&, const LobbyState&) {}

Ui::UiList RoomRows(const LobbyState& state) {
    Ui::UiList list;
    for (std::size_t i = 0; i < state.rooms.size() && i < kRows; ++i) {
        const Room& room = state.rooms[i];
        Ui::UiListItem item;
        item.fields["line"] = std::string(i == state.picked ? "▶ " : "　 ") + "#" + std::to_string(room.id) + "  " + ModeText(room.mode) +
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
    if (!save) return "戦績を読めません: " + error;
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
            const std::size_t count = std::min(lobby.rooms.size(), kRows);
            if (count > 0) lobby.picked = (lobby.picked + (id == "pvp.next" ? 1 : count - 1)) % count;
        } else if (id == "pvp.mode") {
            lobby.mode = (lobby.mode + 1) % kModes.size();
        } else if (id == "pvp.map") {
            if (!lobby.maps.empty()) lobby.map = (lobby.map + 1) % lobby.maps.size();
        } else if ((id == "pvp.join" || id == "pvp.create") && !lobby.asking) {
            if (id == "pvp.join") {
                if (lobby.picked >= lobby.rooms.size()) continue;
                gateway->Ask("enter", JoinRequest(lobby.rooms[lobby.picked].id, RandomHex(16)));
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
                             K::ResMut<GatewayState> carried, K::ResMut<SceneRequest> request,
                             K::Query<K::Write<Ui::UiBindings>> bindings) {
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
    if (gateway.Read().Session() && (!carried.Read().session || carried.Read().session->token != gateway.Read().Session()->token)) {
        carried->session = gateway.Read().Session();
    }
    if (!result) return {};
    // status: what the player asked (or why the lobby came back); notice: the Gateway's state, from the polls.
    std::optional<std::string> status;
    std::optional<std::string> notice;
    std::optional<std::string> record;
    if (!result->Ok()) {
        if (result->tag == "enter") {
            lobby.asking = false;
            status = GatewayErrorText(*result, lobby.gateway);
        } else if (result->tag != "leave") {
            // The leave of a room the lobby came back from: nothing to tell (the room may be gone already).
            notice = GatewayErrorText(*result, lobby.gateway);
        }
    } else if (result->tag == "list") {
        notice = std::string();
        std::string error;
        if (auto rooms = ParseRooms(result->body, error)) {
            lobby.rooms = std::move(*rooms);
            if (lobby.picked >= std::min(lobby.rooms.size(), kRows)) lobby.picked = 0;
        } else {
            notice = "Gateway の答えを読めません: " + error;
        }
    } else if (result->tag == "record") {
        std::string error;
        const auto answer = ParseSave(result->body, error);
        record = answer ? RecordText(*answer) : "戦績を読めません: " + error;
    } else if (result->tag == "enter") {
        lobby.asking = false;
        std::string error;
        if (const auto room = ParseRoom(result->body, error)) {
            request->next = SceneChange{.scene = Scene::Room, .room = room->id};
        } else {
            status = "Gateway の答えを読めません: " + error;
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

std::optional<GYO::Engine::OS::DatagramAddress> ParseMatchAddress(const std::string_view text) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string_view::npos) return std::nullopt;
    const std::string_view portText = text.substr(colon + 1);
    std::uint16_t port = 0;
    const auto parsed = std::from_chars(portText.data(), portText.data() + portText.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != portText.data() + portText.size() || port == 0) return std::nullopt;
    auto address = GYO::Engine::OS::DatagramAddress::Parse(text.substr(0, colon), port);
    if (!address) return std::nullopt;
    return *address;
}

std::string GatewayErrorText(const GatewayResult& result, const std::string& gateway) {
    const std::string& code = result.errorCode;
    if (code == "unreachable") return "Gateway（" + gateway + "）に接続できません";
    if (code == "timeout") return "Gateway（" + gateway + "）から応答がありません";
    if (code == "room_full") return "部屋が満員です";
    if (code == "room_started" || code == "room_starting") return "その部屋はもう始まっています";
    if (code == "room_not_found") return "部屋がありません";
    if (code == "match_ended") return "対戦は終わりました";
    if (code == "not_owner") return "開始できるのは部屋を作った人だけです";
    if (code == "not_all_ready") return "全員の準備がまだです";
    if (code == "no_match_available" || code == "allocation_failed") return "対戦を始められませんでした。もう一度開始してください";
    if (code == "unknown_map" || code == "unknown_mode") return "Gateway がこのマップかモードを知りません";
    if (code.empty()) return "Gateway が答えられませんでした（HTTP " + std::to_string(result.status) + "）" + (result.detail.empty() ? "" : ": " + result.detail);
    return "Gateway: " + code + (result.detail.empty() ? "" : "（" + result.detail + "）");
}

std::string DeviceIdPath(const std::string& dataDirectory) {
    return dataDirectory.empty() ? std::string() : dataDirectory + "/device-id";
}

void RegisterGatewayClient(O::WorldSetup& setup) {
    setup.Registrar().Resource<GatewayClient>({.name = "object_fps_pvp.GatewayClient"});
}

bool PlaceGatewayClient(K::World& world, const GatewayContext& context, std::string& error) {
    const std::string device = LoadDeviceId(DeviceIdPath(context.dataDirectory), error);
    if (device.empty()) return false;
    const auto* carried = world.Resource<GatewayState>();
    world.SetResource(GatewayClient(context.gateway, device, carried != nullptr ? carried->session : std::nullopt));
    return true;
}

void BuildLobby(O::WorldSetup& setup, const SceneChange& change, const GatewayContext& context) {
    K::Registrar& r = setup.Registrar();
    r.Resource<LobbyState>({.name = "object_fps_pvp.LobbyState"});
    RegisterGatewayClient(setup);
    r.AddSystem<&LobbyActions>(InSceneUpdate("object_fps_pvp.LobbyActions"));
    r.AddSystem<&LobbyGateway>(InSceneUpdate("object_fps_pvp.LobbyGateway", {"object_fps_pvp.LobbyActions"}));
    setup.Prepare([change, context](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        LobbyState state;
        state.maps = context.maps;
        state.gateway = context.gateway.ToString();
        std::string error;
        const bool placed = PlaceGatewayClient(w, context, error);
        if (placed && change.room != 0) w.ResourceMut<GatewayClient>()->Ask("leave", LeaveRequest(change.room, RandomHex(16)));
        Ui::UiBindings bindings;
        ShowState(state, bindings);
        bindings.Set("record", std::string("戦績：—"));
        bindings.Set("status", change.status.empty() ? std::string("部屋を選ぶか、作ってください") : change.status);
        bindings.Set("notice", !placed ? "この端末の ID を用意できません: " + error : std::string("Gateway: ") + state.gateway);
        // Without a device id the lobby shows why and calls nothing (its System waits for a client that is not there).
        if (!placed) w.SetResource(GatewayClient(context.gateway, std::string(), std::nullopt));
        if (!placed) state.listed = state.recorded = std::numeric_limits<std::int64_t>::max();
        w.SetResource(std::move(state));
        PlaceSceneDocument(w, "object_fps_pvp.ui.lobby", "lobby", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
