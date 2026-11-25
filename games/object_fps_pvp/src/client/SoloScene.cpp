// The single player's room (Scene::SoloRoom; assets/object_fps_pvp/ui/pvp_solo_room.json, canvas "room") and its save
// (game-flow declaration 3.3): the record read before a game, written when the player evacuates.
//
//   Main/Update  SoloRoomActions  pvp.start -> the game (SoloGame); pvp.back -> the menu

#include "client/SceneBuilds.hpp"

#include "object_fps_pvp/client/LocalFiles.hpp"
#include "object_fps_pvp/client/SoloSave.hpp"
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
            request->next = SceneChange{.scene = Scene::SoloGame};
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

void BuildSoloRoom(O::WorldSetup& setup, const SceneChange& change, const std::string& dataDirectory) {
    setup.Registrar().AddSystem<&SoloRoomActions>(InSceneUpdate("object_fps_pvp.SoloRoomActions"));
    setup.Prepare([status = change.status, path = SoloSavePath(dataDirectory)](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        Ui::UiBindings bindings;
        bindings.Set("record", RecordText(ReadSoloSave(path)));
        bindings.Set("status", status.empty() ? std::string(kRoomInvitation) : status);
        PlaceSceneDocument(w, "object_fps_pvp.ui.solo_room", "room", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
