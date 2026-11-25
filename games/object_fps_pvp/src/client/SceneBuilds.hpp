#pragma once

// The Frame World's registration of each scene (Scenes.hpp): ClientFrameWorld's build calls the common part, then the
// scene's.

#include "object_fps_pvp/client/ClientWorld.hpp"
#include "object_fps_pvp/client/Scenes.hpp"
#include "object_fps_pvp/match/PlayerSave.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/ui/UiRoot.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace object_fps_pvp::client {

namespace K = GYO::Kernel;

// Fixed ids of a menu-like scene's entities (outside the allotters' ranges, as the game scene's: ClientWorld.cpp).
inline constexpr K::EntityId kSceneCamera = 3'000'030;
inline constexpr K::EntityId kSceneRoot = 3'000'031;

// A System in Main/Update (after the given Systems).
[[nodiscard]] K::SystemOptions InSceneUpdate(std::string name, std::vector<std::string> after = {});

// A 2D camera on the window for a scene's document, and the document's root entity (its UiRoot, asked for by its asset
// id, with the text fields' filter, and UiBindings).
void PlaceSceneDocument(K::World& world, std::string_view documentId, std::string_view canvas, GYO::Engine::Ui::UiBindings bindings,
                        GYO::Engine::Ui::UiTextFilter filter);

// Every scene's part: SceneRequest, FrameNotes (carried to the next scene), the frame budget (closes the window when
// it runs out) and the watch of the scene's document (FrameNotes::shown, failure).
void BuildSceneCommon(O::WorldSetup& setup);

// The single player's save (SoloSavePath): what was read, or why it could not be (a file not there is no error: the
// empty save).
struct SoloRecord final {
    std::optional<match::PlayerSave> save;
    std::string error;
};
[[nodiscard]] SoloRecord ReadSoloRecord(const std::string& path);
// The room's line for it.
[[nodiscard]] std::string RecordText(const SoloRecord& record);
// One game settled into the save at path (one more game, these deaths); empty when written, else why not. after: the
// save written.
[[nodiscard]] std::string SettleSolo(const std::string& path, std::uint64_t deaths, match::PlayerSave& after);

// The scenes' own parts.
void BuildMenu(O::WorldSetup& setup, const SceneChange& change);
// dataDirectory: the player's (ClientStart).
void BuildSoloRoom(O::WorldSetup& setup, const SceneChange& change, const std::string& dataDirectory);
// user: the user id the lobby connects with.
void BuildLobby(O::WorldSetup& setup, const SceneChange& change, std::uint64_t user);

} // namespace object_fps_pvp::client
