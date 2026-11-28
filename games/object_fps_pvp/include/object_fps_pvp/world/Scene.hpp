#pragma once

// A scene file (scenes/*.json; the data contract games/object_fps_pvp/docs/scene-format.zh-Hant.md): what furnishes an
// arena -- doors and the like -- apart from its map (the map is the bare walls, the scene what stands in them). The
// format is the game's: each object belongs to a gameplay set, whose readScene reads its data. Both ends read the same
// file, so an object's entity has the same id at both ends (SceneEntity): a scene's objects are not spawned over the
// wire, they are put at every end before the first frame.

#include "object_fps_pvp/world/GameplaySets.hpp"

#include "engine/kernel/Kernel.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace object_fps_pvp::world {

// The entities of a scene's objects: SceneEntity(id) = kSceneEntityBase + id, id 1 to kMaxSceneObjectId. Outside the
// allotter ranges (kMatchIds, kClientIds, a Client's kFrameIds) and the game's fixed ids (3'000'001 to 3'000'032).
inline constexpr K::EntityId kSceneEntityBase = 3'100'000;
inline constexpr std::uint32_t kMaxSceneObjectId = 99'999;

[[nodiscard]] constexpr K::EntityId SceneEntity(const std::uint32_t id) noexcept { return kSceneEntityBase + id; }

// One object: its id in the scene (its stable identity: a tool never gives a removed one's id to another), the set it
// belongs to (its slot in the sets' table) and what the set's reader read.
struct SceneObject final {
    std::uint32_t id = 0;
    std::size_t slot = 0;
    SetData data;
};

// A scene as read: its id and its objects in the file's order. An arena without a scene has none (an empty id).
struct SceneContent final {
    std::string id;
    std::vector<SceneObject> objects;
};

// Reads a scene file with the sets' table. nullopt with error naming the object (its index and id) and what was wrong:
// a member missing or of the wrong kind, the version is not 1, the id is not 1 to 64 characters, an object's id is not
// 1 to kMaxSceneObjectId or is another object's, its set is not in the table or takes no scene objects, or what the
// set's reader says.
[[nodiscard]] std::optional<SceneContent> ParseScene(std::string_view text, const std::vector<GameplaySet>& sets, std::string& error);

// The scene's digest for a mode (MatchInfo::sceneDigest): the objects of the sets the mode turns on, each by its id,
// its set's name and the digest of what its reader read. The objects of a set the mode leaves off do not count (they are
// not played); a scene with none on has the empty one's digest.
[[nodiscard]] std::uint64_t SceneDigest(const SceneContent& scene, const SetsOn& on, const std::vector<GameplaySet>& sets);

// Puts the objects of the sets the mode turns on into a World (each reader's put), in the file's order.
void PutScene(const SceneContent& scene, const SetsOn& on, K::World& world);

} // namespace object_fps_pvp::world
