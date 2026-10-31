#include "RetroFPS/Pvp/PlayerHitbox.hpp"

#include "RetroFPS/Pvp/PlayerAnimationContent.hpp"
#include "../App/AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace fps::pvp {
namespace {

void RequireKeys(const nlohmann::json& object, const std::set<std::string>& keys, const std::string& what) {
    if (!object.is_object()) throw std::runtime_error(what + " must be an object");
    for (const auto& key : keys)
        if (!object.contains(key)) throw std::runtime_error(what + " lacks '" + key + "'");
    for (const auto& [key, value] : object.items()) {
        static_cast<void>(value);
        if (!keys.contains(key)) throw std::runtime_error(what + " has an unknown key '" + key + "'");
    }
}

std::size_t Bone(const Engine::Model::ModelAsset& model, const nlohmann::json& name) {
    const auto text = name.get<std::string>();
    const auto count = std::count_if(model.nodes.begin(), model.nodes.end(),
        [&](const auto& node) { return node.name == text; });
    const auto found = model.FindNode(text);
    if (count != 1 || !found) throw std::runtime_error("bone '" + text + "' is not exactly once in the skeleton");
    return *found;
}

float Radius(const nlohmann::json& value) {
    const auto radius = value.get<float>();
    if (!std::isfinite(radius) || radius < 0.01F || radius > 0.5F)
        throw std::runtime_error("radius must be finite within [0.01, 0.5] m");
    return radius;
}

} // namespace

std::optional<PlayerHitbox> ReadPlayerHitbox(std::string_view text, const CharacterPresentationDefinition& character,
                                             std::string& error) {
    error.clear();
    try {
        if (!character.model) throw std::runtime_error("the character has no model");
        const auto& model = *character.model;
        const auto config = nlohmann::json::parse(text);
        RequireKeys(config, {"version", "character_asset_id", "shapes"}, "hitbox");
        if (config.at("version") != 1) throw std::runtime_error("hitbox version must be 1");
        const auto& shapes = config.at("shapes");
        if (!shapes.is_array()) throw std::runtime_error("hitbox shapes must be an array");
        PlayerHitbox hitbox;
        std::set<HitPart> parts;
        for (const auto& entry : shapes) {
            if (!entry.is_object() || !entry.contains("kind") || !entry.contains("part"))
                throw std::runtime_error("each shape needs a part and a kind");
            PlayerHitShape shape;
            const auto part = entry.at("part").get<std::string>();
            if (part == "head") shape.part = HitPart::Head;
            else if (part == "body") shape.part = HitPart::Body;
            else if (part == "leg") shape.part = HitPart::Leg;
            else throw std::runtime_error("unknown part '" + part + "'");
            const auto kind = entry.at("kind").get<std::string>();
            if (kind == "sphere") {
                RequireKeys(entry, {"part", "kind", "bone", "offset", "radius"}, "sphere");
                shape.kind = PlayerHitShape::Kind::Sphere;
                shape.node = shape.nodeTo = Bone(model, entry.at("bone"));
                const auto& offset = entry.at("offset");
                if (!offset.is_array() || offset.size() != 3) throw std::runtime_error("offset needs three numbers");
                shape.offset = {offset[0].get<float>(), offset[1].get<float>(), offset[2].get<float>()};
                if (!Engine::Math::IsFinite(shape.offset)) throw std::runtime_error("offset must be finite");
            } else if (kind == "capsule") {
                RequireKeys(entry, {"part", "kind", "from", "to", "radius"}, "capsule");
                shape.kind = PlayerHitShape::Kind::Capsule;
                shape.node = Bone(model, entry.at("from"));
                shape.nodeTo = Bone(model, entry.at("to"));
                if (shape.node == shape.nodeTo) throw std::runtime_error("a capsule needs two bones");
            } else {
                throw std::runtime_error("unknown kind '" + kind + "'");
            }
            shape.radius = Radius(entry.at("radius"));
            parts.insert(shape.part);
            hitbox.shapes.push_back(shape);
        }
        if (parts.size() != 3) throw std::runtime_error("the hitbox needs head, body and leg shapes");
        return hitbox;
    } catch (const std::exception& exception) {
        error = "player hitbox: " + std::string(exception.what());
        return std::nullopt;
    }
}

std::optional<PlayerHitbox> LoadPlayerHitbox(Engine::Asset::AssetManager& assets, std::string& error) {
    error.clear();
    try {
        using namespace asset_definition_detail;
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(assets,
            Engine::Asset::AssetId::FromString("object_fps_pvp.player.hitbox"), Engine::Asset::AssetType::Text());
        const auto config = nlohmann::json::parse(text->text);
        const auto character = LoadCharacterAnimationDefinition(assets,
            ReadAssetId(config.at("character_asset_id")), error);
        if (!character) throw std::runtime_error(error);
        auto hitbox = ReadPlayerHitbox(text->text, *character, error);
        if (!hitbox) throw std::runtime_error(error);
        return hitbox;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

std::optional<PlayerHitbox> LoadPlayerHitbox(const std::filesystem::path& contentRoot, std::string& error) {
    std::optional<PlayerHitbox> hitbox;
    if (!WithContentAssets(contentRoot, [&](Engine::Asset::AssetManager& assets, std::string& failure) {
            hitbox = LoadPlayerHitbox(assets, failure);
            return hitbox.has_value();
        }, error))
        return std::nullopt;
    return hitbox;
}

} // namespace fps::pvp
