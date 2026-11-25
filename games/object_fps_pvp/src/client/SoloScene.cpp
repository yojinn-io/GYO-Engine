// The single player's room (Scene::SoloRoom; assets/object_fps_pvp/ui/pvp_solo_room.json, canvas "room") and its save
// (game-flow declaration 3.3): the record read before a game, written when the player evacuates.
//
//   Main/Update  SoloRoomActions  pvp.start -> the game (SoloGame); pvp.back -> the menu

#include "client/SceneBuilds.hpp"

#include "object_fps_pvp/client/LocalFiles.hpp"
#include "object_fps_pvp/match/PlayerSave.hpp"

#include "engine/ui/UiRoot.hpp"

#include <utility>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace Ui = GYO::Engine::Ui;

namespace {

constexpr std::string_view kRoomInvitation = "準備ができたら出撃してください";

K::SystemResult SoloRoomActions(K::MessageReader<Ui::UiAction> actions, K::ResMut<SceneRequest> request) {
    for (const Ui::UiAction* action : actions.Read()) {
        if (action->action == "pvp.start") {
            request->next = SceneChange{Scene::SoloGame, {}, std::nullopt, {}};
        } else if (action->action == "pvp.back") {
            request->next = SceneChange{Scene::Menu, {}, std::nullopt, {}};
        }
    }
    return {};
}

} // namespace

SoloRecord ReadSoloRecord(const std::string& path) {
    SoloRecord record;
    if (path.empty()) {
        record.error = "保存先がありません";
        return record;
    }
    std::string error;
    const auto text = ReadLocalText(path, error);
    if (!text) {
        record.error = error;
        return record;
    }
    const auto save = match::DecodePlayerSave(*text, error);
    if (!save) {
        record.error = path + ": " + error;
        return record;
    }
    record.save = *save;
    return record;
}

std::string RecordText(const SoloRecord& record) {
    if (!record.error.empty()) return "記録を読めません: " + record.error;
    if (!record.save || record.save->matches == 0) return "記録はまだありません";
    return "出撃 " + std::to_string(record.save->matches) + " 回・死亡 " + std::to_string(record.save->deaths) + " 回";
}

std::string SettleSolo(const std::string& path, const std::uint64_t deaths, match::PlayerSave& after) {
    if (path.empty()) return "保存先がありません";
    SoloRecord record = ReadSoloRecord(path);
    if (!record.error.empty()) return record.error;
    const match::PlayerSave before = record.save.value_or(match::PlayerSave{});
    // One game is one "match" of the save: the next number after the last one.
    const std::uint64_t game = before.lastMatch + 1;
    after = match::Settled(before, deaths, game, "solo-" + std::to_string(game));
    return WriteLocalText(path, match::EncodePlayerSave(after));
}

void BuildSoloRoom(O::WorldSetup& setup, const SceneChange& change, const std::string& dataDirectory) {
    setup.Registrar().AddSystem<&SoloRoomActions>(InSceneUpdate("object_fps_pvp.SoloRoomActions"));
    setup.Prepare([status = change.status, path = SoloSavePath(dataDirectory)](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        Ui::UiBindings bindings;
        bindings.Set("record", RecordText(ReadSoloRecord(path)));
        bindings.Set("status", status.empty() ? std::string(kRoomInvitation) : status);
        PlaceSceneDocument(w, "object_fps_pvp.ui.solo_room", "room", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
