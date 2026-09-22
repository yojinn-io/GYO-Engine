#include "RetroFPS/App/EnemyPresentationDefinition.hpp"
#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace fps {
std::shared_ptr<const EnemyRig> LoadEnemyRig(Engine::Asset::AssetManager& assets,
                                             const EnemyDefinition& definition,
                                             std::string& error) {
    error.clear();
    try {
        using namespace asset_definition_detail;
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(
            assets, definition.presentationAssetId, Engine::Asset::AssetType::Text());
        auto config = nlohmann::json::parse(text->text);
        RequireVersionOne(config);
        auto rig = std::make_shared<EnemyRig>();
        rig->characterAssetId = ReadAssetId(config.at("character_asset_id"));
        auto character = LoadCharacterPresentationDefinition(assets, rig->characterAssetId, error);
        if (!character || !character->animationSet)
            throw std::runtime_error(error.empty() ? "enemy requires an animation set" : error);
        rig->model = character->model;
        const std::array<const char*, 4> states{"idle", "move", "attack", "dead"};
        for (std::size_t i = 0; i < states.size(); ++i) {
            const auto found = character->animationSet->clips.find(states[i]);
            if (found == character->animationSet->clips.end())
                throw std::runtime_error(std::string("missing enemy animation: ") + states[i]);
            const auto* clip = &found->second;
            rig->clips[i] = clip->clipIndex;
            if (rig->model->clips[clip->clipIndex].durationSeconds <= 0)
                throw std::runtime_error("enemy clips require positive duration");
        }
        Engine::Model::Pose reference;
        auto made = Engine::Model::MakeDefaultPose(*rig->model, reference);
        if (!made)
            throw std::runtime_error(made.error());
        float minimum = std::numeric_limits<float>::max(), maximum = -minimum;
        std::vector<Engine::Model::SkinnedVertex> vertices;
        for (std::size_t mesh = 0; mesh < rig->model->meshes.size(); ++mesh) {
            auto skinned = Engine::Model::SkinMesh(*rig->model, mesh, reference, vertices);
            if (!skinned)
                throw std::runtime_error(skinned.error());
            for (const auto& v : vertices) {
                minimum = (std::min)(minimum, v.position.y);
                maximum = (std::max)(maximum, v.position.y);
            }
        }
        if (!std::isfinite(maximum - minimum) || maximum - minimum < 0.001F)
            throw std::runtime_error("enemy model has no usable reference height");
        rig->anchor = {0, minimum, 0};
        rig->scale = definition.hitboxHeight / (maximum - minimum);
        const auto point = [&](const nlohmann::json& value) {
            EnemyBonePoint result;
            const auto name = value.at("node").get<std::string>();
            auto node = rig->model->FindNode(name);
            if (!node ||
                std::count_if(rig->model->nodes.begin(), rig->model->nodes.end(),
                              [&](const auto& candidate) { return candidate.name == name; }) != 1)
                throw std::runtime_error("missing or ambiguous enemy bone: " + name);
            result.node = *node;
            if (value.contains("offset")) {
                const auto& o = value.at("offset");
                if (!o.is_array() || o.size() != 3)
                    throw std::runtime_error("bone offset requires three coordinates");
                result.offset = {o[0].get<float>(), o[1].get<float>(), o[2].get<float>()};
            }
            if (!std::isfinite(result.offset.x) || !std::isfinite(result.offset.y) ||
                !std::isfinite(result.offset.z))
                throw std::runtime_error("bone offset must be finite");
            return result;
        };
        std::unordered_set<std::string> regions;
        for (const auto& entry : config.at("hurt_regions")) {
            EnemyHurtRegion region{entry.at("id").get<std::string>(), point(entry.at("start")),
                                   point(entry.at("end")), entry.at("radius").get<float>()};
            if (region.id.empty() || !regions.insert(region.id).second ||
                !std::isfinite(region.radius) || region.radius <= 0)
                throw std::runtime_error("invalid or duplicate hurt region");
            rig->hurtRegions.push_back(std::move(region));
        }
        if (regions.empty())
            throw std::runtime_error("enemy requires hurt regions");
        const auto& attack = config.at("attack");
        rig->attackPoint = point(attack.at("point"));
        rig->attackRadius = attack.value("radius", 0.12F);
        rig->attackBeginSeconds = attack.value("begin_seconds", 0.0);
        rig->attackEndSeconds = attack.value("end_seconds", 0.0);
        rig->releaseSeconds = attack.value("release_seconds", 0.0);
        const double duration = rig->model->clips[rig->clips[2]].durationSeconds;
        if (duration > definition.attackIntervalSeconds + 0.00001 ||
            !std::isfinite(rig->attackRadius) || rig->attackRadius <= 0 ||
            !std::isfinite(rig->attackBeginSeconds) || !std::isfinite(rig->attackEndSeconds) ||
            !std::isfinite(rig->releaseSeconds) || rig->attackBeginSeconds < 0 ||
            rig->attackEndSeconds < rig->attackBeginSeconds || rig->attackEndSeconds > duration ||
            rig->releaseSeconds < 0 || rig->releaseSeconds > duration)
            throw std::runtime_error(
                "enemy attack window, release or cooldown is outside its clip");
        if (definition.kind == EnemyKind::Melee && rig->attackBeginSeconds == rig->attackEndSeconds)
            throw std::runtime_error("melee requires a non-empty attack window");
        return rig;
    } catch (const std::exception& exception) {
        error = "enemy presentation '" + definition.presentationAssetId.debugName +
                "': " + exception.what();
        return {};
    }
}
} // namespace fps
