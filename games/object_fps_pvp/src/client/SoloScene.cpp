// The single player's room (Scene::SoloRoom; the multiplayer's room document, assets/object_fps_pvp/ui/pvp_room.json,
// canvas "solo_room"; game-flow declaration 3.3) and its save: the room as the multiplayer's, with its own buttons (the
// game starts at once: no one else to be ready), the map chosen here, the first installed mode; the record read before a game,
// written when the player evacuates.
//
//   Main/Update  SoloRoomActions  pvp.map (the next installed arena), pvp.start -> the game (SoloGame, on that map);
//                                 pvp.back -> the menu

#include "client/SceneBuilds.hpp"

#include "object_fps_pvp/client/LocalFiles.hpp"
#include "object_fps_pvp/client/SoloSave.hpp"
#include "object_fps_pvp/match/PlayerSave.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/ui/UiRoot.hpp"

#include <string>
#include <utility>
#include <vector>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace Ui = GYO::Engine::Ui;

namespace {

constexpr std::string_view kRoomInvitation = "マップを選んで「開始」を押してください";
// The room's own state: the maps it offers (the installed arenas, by id) and the one chosen, and the mode it plays (the
// first installed mode: the single player does not choose it).
struct SoloRoomState final {
    std::vector<std::string> maps;
    std::size_t map = 0;
    std::optional<ModeChoice> mode;
};
void HashValue(K::Hasher&, const SoloRoomState&) {}

std::string MapText(const SoloRoomState& state) { return "マップ: " + (state.maps.empty() ? std::string("—") : state.maps[state.map]); }

K::SystemResult SoloRoomActions(K::MessageReader<Ui::UiAction> actions, K::ResMut<SoloRoomState> state, K::ResMut<SceneRequest> request,
                                K::Query<K::Write<Ui::UiBindings>> bindings) {
    for (const Ui::UiAction* action : actions.Read()) {
        SoloRoomState& room = *state;
        if (action->action == "pvp.map") {
            if (!room.maps.empty()) room.map = (room.map + 1) % room.maps.size();
            if (auto root = bindings.GetMut<Ui::UiBindings>(action->root)) root->Set("map", MapText(room));
        } else if (action->action == "pvp.start") {
            request->next = SceneChange{.scene = Scene::SoloGame, .map = room.maps.empty() ? std::string() : room.maps[room.map],
                                        .mode = room.mode ? room.mode->id : std::string()};
        } else if (action->action == "pvp.back") {
            request->next = SceneChange{.scene = Scene::Menu};
        }
    }
    return {};
}

} // namespace

const char* ToString(const SoloSaveFailureCode code) noexcept {
    switch (code) {
    case SoloSaveFailureCode::NoDirectory: return "NoDirectory";
    case SoloSaveFailureCode::ReadFailed: return "ReadFailed";
    case SoloSaveFailureCode::BadSave: return "BadSave";
    case SoloSaveFailureCode::WriteFailed: return "WriteFailed";
    }
    return "Unknown";
}

Base::Result<std::optional<match::PlayerSave>, SoloSaveFailure> ReadSoloSave(const std::string& path) {
    if (path.empty()) return Base::Err(SoloSaveFailure::Make(SoloSaveFailureCode::NoDirectory, "no directory for the save"));
    auto text = ReadLocalText(path);
    if (!text) return Base::Err(SoloSaveFailure::Make(SoloSaveFailureCode::ReadFailed, path, Base::Describe(text.error())));
    if (!*text) return std::optional<match::PlayerSave>{};
    std::string error;
    auto save = match::DecodePlayerSave(**text, error);
    if (!save) return Base::Err(SoloSaveFailure::Make(SoloSaveFailureCode::BadSave, path, error));
    return std::optional<match::PlayerSave>(std::move(*save));
}

Base::Result<match::PlayerSave, SoloSaveFailure> SettleSolo(const std::string& path, const std::uint64_t deaths) {
    auto read = ReadSoloSave(path);
    if (!read) return Base::Err(std::move(read).error());
    const match::PlayerSave before = read->value_or(match::PlayerSave{});
    // One game is one "match" of the save (match::Settled counts a match once by its number): the next number after the
    // last one. lastWrite only names the write here (a local file has no conflicting writer to tell it from).
    const std::uint64_t game = before.lastMatch + 1;
    match::PlayerSave after = match::Settled(before, deaths, game, "solo-" + std::to_string(game));
    auto written = WriteLocalText(path, match::EncodePlayerSave(after));
    if (!written) return Base::Err(SoloSaveFailure::Make(SoloSaveFailureCode::WriteFailed, path, Base::Describe(written.error())));
    return after;
}

std::string SoloSaveText(const SoloSaveFailure& failure) {
    switch (failure.code) {
    case SoloSaveFailureCode::NoDirectory: return "保存先がありません";
    case SoloSaveFailureCode::ReadFailed: return "記録を読めません（" + failure.detail + "）";
    case SoloSaveFailureCode::BadSave: return "記録の形式が違います（" + failure.detail + "）";
    case SoloSaveFailureCode::WriteFailed: return "記録を書けません（" + failure.detail + "）";
    }
    return failure.message;
}

namespace {

std::string RecordText(const Base::Result<std::optional<match::PlayerSave>, SoloSaveFailure>& read) {
    if (!read) return SoloSaveText(read.error());
    if (!*read || (*read)->matches == 0) return "記録はまだありません";
    return "出撃 " + std::to_string((*read)->matches) + " 回・死亡 " + std::to_string((*read)->deaths) + " 回";
}

} // namespace

void BuildSoloRoom(O::WorldSetup& setup, const SceneChange& change, const std::string& dataDirectory, const std::vector<std::string>& maps,
                   const std::vector<ModeChoice>& modes) {
    K::Registrar& r = setup.Registrar();
    r.Resource<SoloRoomState>({.name = "object_fps_pvp.SoloRoomState"});
    r.AddSystem<&SoloRoomActions>(InSceneUpdate("object_fps_pvp.SoloRoomActions"));
    SoloRoomState state{maps, 0, modes.empty() ? std::nullopt : std::optional<ModeChoice>(modes.front())};
    // Back from a game: the map it was played on.
    for (std::size_t i = 0; i < maps.size(); ++i) {
        if (maps[i] == change.map) state.map = i;
    }
    setup.Prepare([status = change.status, path = SoloSavePath(dataDirectory), state](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        Ui::UiBindings bindings;
        bindings.Set("title", std::string("シングルプレイ"));
        Ui::UiList members;
        Ui::UiListItem self;
        self.fields["line"] = std::string("自分");
        members.items.push_back(std::move(self));
        bindings.Set("members", std::move(members));
        bindings.Set("mode", "モード: " + (state.mode ? state.mode->name : std::string("—")) + "（固定）");
        bindings.Set("map", MapText(state));
        bindings.Set("record", RecordText(ReadSoloSave(path)));
        bindings.Set("status", status.empty() ? std::string(kRoomInvitation) : status);
        w.SetResource(state);
        PlaceSceneDocument(w, "object_fps_pvp.ui.room", "solo_room", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
