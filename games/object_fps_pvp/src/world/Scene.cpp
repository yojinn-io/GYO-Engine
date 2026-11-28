#include "object_fps_pvp/world/Scene.hpp"

#include "data/JsonFields.hpp"
#include "object_fps_pvp/sim/Arena.hpp"
#include "object_fps_pvp/world/ContentHash.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace object_fps_pvp::world {

std::optional<SceneContent> ParseScene(const std::string_view text, const std::vector<GameplaySet>& sets, std::string& error) {
    error.clear();
    std::string where;
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the scene");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported scene version");
        SceneContent scene;
        scene.id = data::String(data::Member(json, "id"), "id");
        if (scene.id.empty() || scene.id.size() > sim::kMaxContentIdBytes) throw std::runtime_error("id is 1 to 64 characters");
        const auto objects = data::Member(json, "objects").AsArray();
        if (!objects) throw std::runtime_error("objects is not an array");
        std::size_t index = 0;
        for (const auto item : objects.value()) {
            const std::string at = "object " + std::to_string(index++);
            where = at + ": ";
            const auto object = data::AsObject(item, "an object");
            const std::uint64_t id = data::Unsigned(data::Member(object, "id"), "id");
            if (id == 0 || id > kMaxSceneObjectId) throw std::runtime_error("id is not 1 to " + std::to_string(kMaxSceneObjectId));
            where = at + " (id " + std::to_string(id) + "): ";
            if (std::any_of(scene.objects.begin(), scene.objects.end(), [&](const SceneObject& other) { return other.id == id; })) {
                throw std::runtime_error("id is another object's");
            }
            const std::string name = data::String(data::Member(object, "set"), "set");
            const auto found = std::find_if(sets.begin(), sets.end(), [&](const GameplaySet& set) { return set.name == name; });
            if (found == sets.end()) throw std::runtime_error("the game has no gameplay set '" + name + "'");
            if (!found->readScene) throw std::runtime_error("the set '" + name + "' takes no scene objects");
            const auto fields = data::AsObject(data::Member(object, "data"), "data");
            SceneObject read;
            read.id = static_cast<std::uint32_t>(id);
            read.slot = static_cast<std::size_t>(found - sets.begin());
            std::string why;
            auto data = found->readScene(SceneEntity(read.id), fields, why);
            if (!data) throw std::runtime_error("set '" + name + "': " + why);
            read.data = std::move(*data);
            scene.objects.push_back(std::move(read));
        }
        return scene;
    } catch (const std::exception& exception) {
        error = where + exception.what();
        return std::nullopt;
    }
}

std::uint64_t SceneDigest(const SceneContent& scene, const SetsOn& on, const std::vector<GameplaySet>& sets) {
    K::Hasher h;
    HashValue(h, std::string("gyo.object_fps_pvp.scene/1"));
    // By id (the file's order is not the scene's content).
    std::vector<const SceneObject*> played;
    for (const SceneObject& object : scene.objects) {
        if (on.Has(object.slot)) played.push_back(&object);
    }
    std::sort(played.begin(), played.end(), [](const SceneObject* a, const SceneObject* b) { return a->id < b->id; });
    HashValue(h, static_cast<std::uint64_t>(played.size()));
    for (const SceneObject* object : played) {
        HashValue(h, object->id);
        HashValue(h, sets[object->slot].name);
        HashValue(h, object->data.digest);
    }
    return Digest64(h);
}

std::vector<K::EntityId> PutScene(const SceneContent& scene, const SetsOn& on, K::World& world) {
    std::vector<K::EntityId> put;
    for (const SceneObject& object : scene.objects) {
        if (!on.Has(object.slot)) continue;
        if (object.data.put) object.data.put(world);
        put.push_back(SceneEntity(object.id));
    }
    return put;
}

} // namespace object_fps_pvp::world
