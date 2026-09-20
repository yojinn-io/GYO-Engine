#include "../TestSupport.hpp"

#include "RetroFPS/App/AnimationSetDefinition.hpp"
#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "engine/asset/loaders/TextureLoader.hpp"
#ifdef OBJECT_FPS_TEST_SDL_IMAGE
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#endif
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "model/Animation.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <unordered_map>

namespace fps::tests {
namespace {

using Engine::Asset::AssetId;
using Engine::Model::ModelAsset;
constexpr const char* kMale = "object_fps.model.character.superhero_male";
constexpr const char* kFemale = "object_fps.model.character.superhero_female";
constexpr const char* kMannequin = "object_fps.model.animation.ual1_mannequin";
constexpr const char* kWorldPistol = "object_fps.model.weapon.ultimate_pistol_1.world";
constexpr const char* kAnimatedPistol = "object_fps.model.weapon.animated_pistol.viewmodel";
constexpr const char* kLocomotion = "object_fps.animset.ual1.locomotion";
constexpr const char* kMalePresentation = "object_fps.character.superhero_male";

// Actual FBX, JSON and PNG bytes use the registered production loaders.
// Builds without SDL_image require existing PNG files but substitute PPM pixels.
class DefinitionAssetSource final : public Engine::Asset::Loading::IAssetSource {
public:
    using ByteBuffer = Engine::Asset::Loading::ByteBuffer;
    std::unordered_map<std::string, std::string> overrides;
    std::unordered_map<std::string, std::size_t> modelReads;

    Engine::Base::Result<ByteBuffer, Engine::Asset::AssetError> ReadAll(std::string_view path) override {
        const std::string key{path};
        if (const auto override = overrides.find(key); override != overrides.end()) {
            return Bytes(override->second);
        }
        Engine::Asset::Loading::NativeFileAssetSource native;
        if (path.ends_with(".fbx")) ++modelReads[key];
#ifndef OBJECT_FPS_TEST_SDL_IMAGE
        if (path.ends_with(".png") && std::filesystem::is_regular_file(std::filesystem::path{path})) {
            return Bytes("P3\n1 1\n255\n255 255 255\n");
        }
#endif
        return native.ReadAll(path);
    }

private:
    static Engine::Base::Result<ByteBuffer, Engine::Asset::AssetError> Bytes(std::string_view text) {
        const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
        return Engine::Base::Result<ByteBuffer, Engine::Asset::AssetError>::Ok(
            ByteBuffer(bytes.begin(), bytes.end()));
    }
};

struct DefinitionFixture final {
    Engine::Asset::AssetCatalog catalog;
    Engine::Asset::Loading::LoaderRegistry registry;
    DefinitionAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, registry};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};

    bool Initialize(std::string& error) {
#ifdef OBJECT_FPS_TEST_SDL_IMAGE
        using TextureDecoder = Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader;
#else
        using TextureDecoder = Engine::Asset::Loaders::TextureLoader;
#endif
        if (!registry.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>()) ||
            !registry.Register(std::make_unique<TextureDecoder>()) ||
            !registry.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>())) {
            error = "asset definition test loaders failed to register";
            return false;
        }
        const std::filesystem::path root{RETROFPS_TEST_RESOURCE_ROOT};
        Engine::Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = root.string();
        Engine::Asset::Resolver::AssetPathResolver resolver(std::move(options));
        Engine::Asset::Catalog::CatalogParser parser;
        const auto loaded = catalog.LoadFromFile((root / "asset_catalog.json").string(), parser, resolver);
        if (!loaded) { error = loaded.error().message + " " + loaded.error().detail; return false; }
#ifdef OBJECT_FPS_TEST_SDL_IMAGE
        // Full object-fps builds decode every selected character texture through
        // SDL_image, without creating a window or GPU device.
        for (const auto* name : {
                "object_fps.texture.character.male.eyes", "object_fps.texture.character.male.hair",
                "object_fps.texture.character.male.body", "object_fps.texture.character.female.eyes",
                "object_fps.texture.character.female.hair", "object_fps.texture.character.female.body"}) {
            const auto texture = assets.Load(AssetId::FromString(name),
                Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::Texture()));
            if (!texture) {
                error = std::string{"character PNG '"} + name + "': " +
                    texture.error().message + " " + texture.error().detail;
                return false;
            }
            const auto pixels = assets.GetSharedConst<Engine::Asset::Loaders::TextureAsset>(texture.value());
            assets.Release(texture.value());
            if (!pixels || pixels->width == 0 || pixels->height == 0 ||
                pixels->rgba.size() != static_cast<std::size_t>(pixels->width) * pixels->height * 4U) {
                error = std::string{"character PNG '"} + name + "' has invalid decoded dimensions or RGBA pixels";
                return false;
            }
        }
#endif
        return true;
    }

    std::string PathFor(const char* name) const {
        const auto* entry = catalog.Find(AssetId::FromString(name));
        if (!entry) throw std::runtime_error(std::string{"missing fixture catalog entry: "} + name);
        return entry->resolvedPath;
    }

    nlohmann::json ReadConfig(const char* name) const {
        std::ifstream stream(PathFor(name));
        return nlohmann::json::parse(stream);
    }

    void OverrideConfig(const char* name, const nlohmann::json& config) {
        source.overrides[PathFor(name)] = config.dump();
        auto request = Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::Text());
        request.mode = Engine::Asset::AssetRequest::Mode::ForceReload;
        const auto loaded = assets.Load(AssetId::FromString(name), request);
        if (!loaded) throw std::runtime_error(loaded.error().message);
        assets.Release(loaded.value());
    }

    std::shared_ptr<const ModelAsset> LoadModel(const char* name, TestContext& context) {
        const auto loaded = assets.Load(AssetId::FromString(name),
            Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::FromString("model")));
        context.Expect(static_cast<bool>(loaded), std::string{"actual selected FBX loads: "} + name);
        if (!loaded) { context.Fail(loaded.error().message + " " + loaded.error().detail); return {}; }
        auto model = assets.GetSharedConst<ModelAsset>(loaded.value());
        assets.Release(loaded.value());
        context.Expect(static_cast<bool>(model), "selected FBX owns a typed immutable model payload");
        return model;
    }
};

bool Finite(Engine::Model::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void CheckPoseAndSkin(TestContext& context, const ModelAsset& model, const Engine::Model::Pose& pose) {
    context.Expect(pose.localTransforms.size() == model.nodes.size() &&
                   pose.globalTransforms.size() == model.nodes.size(),
                   "pose contains every imported node, including nodes without animation tracks");
    const bool finiteMatrices = std::all_of(pose.globalTransforms.begin(), pose.globalTransforms.end(),
        [](const auto& matrix) { return std::all_of(matrix.values.begin(), matrix.values.end(),
            [](float value) { return std::isfinite(value); }); });
    context.Expect(finiteMatrices, "all imported reference/animated pose matrices are finite");
    for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
        const auto& mesh = model.meshes[meshIndex];
        context.Expect(mesh.materialIndex < model.materials.size(), "selected mesh material slot is valid");
        std::vector<Engine::Model::SkinnedVertex> vertices;
        const auto skinned = Engine::Model::SkinMesh(model, meshIndex, pose, vertices);
        context.Expect(static_cast<bool>(skinned), "actual selected mesh supports CPU skinning");
        if (!skinned) { context.Fail(skinned.error()); continue; }
        context.Expect(vertices.size() == mesh.vertices.size() &&
            std::all_of(vertices.begin(), vertices.end(), [](const auto& vertex) {
                return Finite(vertex.position) && Finite(vertex.normal) &&
                       std::isfinite(vertex.uv.x) && std::isfinite(vertex.uv.y);
            }), "CPU skinning emits complete finite model-space vertices");
    }
}

void TestSelectedAssets(TestContext& context, DefinitionFixture& fixture) {
    for (const auto* name : {kMale, kFemale, kMannequin, kWorldPistol, kAnimatedPistol}) {
        const auto model = fixture.LoadModel(name, context);
        if (!model) continue;
        context.Expect(!model->meshes.empty() && !model->materials.empty(),
                       "selected model retains its geometry and material slots");
        Engine::Model::Pose pose;
        const auto result = Engine::Model::MakeDefaultPose(*model, pose);
        context.Expect(static_cast<bool>(result), "selected model can evaluate its full reference pose without a clip");
        if (result) CheckPoseAndSkin(context, *model, pose);
        std::vector<std::string> expectedMaterials;
        if (name == kMale || name == kFemale) {
            expectedMaterials = name == kMale
                ? std::vector<std::string>{"MI_Eyes", "MI_Hair_1", "MI_Superhero_Male"}
                : std::vector<std::string>{"MI_Eyes", "MI_Hair_2", "MI_Superhero_Female"};
            context.Expect(model->clips.empty(), "superhero source retains reference pose without invented animation clips");
        } else if (name == kMannequin) {
            expectedMaterials = {"M_Main", "M_Joints"};
            context.Expect(model->clips.size() == 43, "UAL1 source retains all 43 clips in one model payload");
        } else if (name == kWorldPistol) {
            expectedMaterials = {"DarkWood", "DarkMetal", "Metal", "Black", "Wood", "Black2"};
            context.Expect(model->clips.empty(), "world pistol loads successfully without animation clips");
        } else if (name == kAnimatedPistol) {
            expectedMaterials = {"Muzzle", "Magazine", "Metal", "Wood", "Material.003", "DarkerMetal"};
            context.Expect(model->clips.size() == 3, "animated pistol retains its three authored clips");
        }
        std::vector<std::string> actualMaterials;
        for (const auto& material : model->materials) actualMaterials.push_back(material.name);
        std::sort(actualMaterials.begin(), actualMaterials.end());
        std::sort(expectedMaterials.begin(), expectedMaterials.end());
        context.Expect(actualMaterials == expectedMaterials, "selected model retains every exact source material slot name");
        const std::unordered_map<std::string, std::array<float, 4>> sourceColors = name == kMannequin
            ? std::unordered_map<std::string, std::array<float, 4>>{
                {"M_Main", {0.799098313F, 0.401978195F, 0.042311523F, 1}},
                {"M_Joints", {0.401975006F, 0.132868230F, 0.708375990F, 1}}}
            : name == kWorldPistol ? std::unordered_map<std::string, std::array<float, 4>>{
                {"DarkWood", {0.08197512F, 0.05763881F, 0.04415309F, 1}},
                {"DarkMetal", {0.0376F, 0.03756696F, 0.03663054F, 1}},
                {"Metal", {0.05330075F, 0.05478255F, 0.0536F, 1}},
                {"Black", {0.02157299F, 0.02198585F, 0.02327782F, 1}},
                {"Wood", {0.10628763F, 0.07202349F, 0.05472363F, 1}},
                {"Black2", {0.01095676F, 0.01114547F, 0.01173435F, 1}}}
            : std::unordered_map<std::string, std::array<float, 4>>{};
        for (const auto& material : model->materials) {
            if (const auto expected = sourceColors.find(material.name); expected != sourceColors.end()) {
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    context.Expect(NearlyEqual(material.baseColorLinear[channel], expected->second[channel], 1e-5F),
                                   "real source diffuse colors and factors survive importing");
                }
            } else if (name == kAnimatedPistol) {
                context.Expect(material.baseColorLinear == std::array<float, 4>{0.8F, 0.8F, 0.8F, 1},
                               "animated pistol preserves the diffuse color inherited from its FBX property template");
            }
        }
    }
}

std::shared_ptr<const AnimationSetDefinition> TestAnimationSets(TestContext& context, DefinitionFixture& fixture) {
    std::string error;
    const auto set = LoadAnimationSetDefinition(fixture.assets, AssetId::FromString(kLocomotion), error);
    context.Expect(static_cast<bool>(set), error.empty() ? "locomotion animation set loads" : error);
    if (!set) return {};
    const std::array<std::pair<const char*, const char*>, 3> expected{{
        {"idle", "Armature|Idle_Loop"}, {"walk", "Armature|Walk_Loop"}, {"run", "Armature|Jog_Fwd_Loop"}}};
    const auto sourceModel = fixture.LoadModel(kMannequin, context);
    for (const auto& [semantic, clipName] : expected) {
        const auto reference = set->clips.find(semantic);
        context.Expect(reference != set->clips.end(), "locomotion definition exposes the requested semantic");
        if (reference == set->clips.end()) continue;
        context.Expect(reference->second.modelAssetId == AssetId::FromString(kMannequin) &&
            reference->second.model == sourceModel &&
            reference->second.clipIndex == sourceModel->FindClip(clipName),
            "semantic resolves to canonical model ownership and exact authored clip");
    }
    const auto repeated = LoadAnimationSetDefinition(fixture.assets, AssetId::FromString(kLocomotion), error);
    context.Expect(repeated && repeated->clips.at("run").model == sourceModel &&
        fixture.source.modelReads[fixture.PathFor(kMannequin)] == 1,
        "multiple clip references and repeated definition loads parse the source FBX only once");
    context.Expect(ValidateAnimationSetBinding(*set, AssetId::FromString(kMannequin), sourceModel, error),
                   "same canonical model and snapshot can bind for playback");
    auto otherSnapshot = std::make_shared<const ModelAsset>();
    context.Expect(!ValidateAnimationSetBinding(*set, AssetId::FromString(kMannequin), otherSnapshot, error) &&
        error.find("snapshot") != std::string::npos,
        "matching canonical ID alone cannot bind a different model snapshot");
    context.Expect(!ValidateAnimationSetBinding(*set, AssetId::FromString(kMale), sourceModel, error) &&
        error.find("cross-source") != std::string::npos,
        "even the same pointer cannot bypass the cross-source canonical ID guard");

    const auto pistolSet = LoadAnimationSetDefinition(fixture.assets,
        AssetId::FromString("object_fps.animset.weapon.animated_pistol.viewmodel"), error);
    context.Expect(pistolSet && pistolSet->clips.size() == 3, "animated pistol animation set resolves three actions");
    if (pistolSet) {
        const auto pistolModel = fixture.LoadModel(kAnimatedPistol, context);
        context.Expect(ValidateAnimationSetBinding(*pistolSet, AssetId::FromString(kAnimatedPistol), pistolModel, error),
                       "animated pistol actions bind to their own model");
        const std::array<std::pair<const char*, const char*>, 3> pistolClips{{
            {"fire", "PistolArmature|Fire"}, {"reload", "PistolArmature|Reload"}, {"slide", "PistolArmature|Slide"}}};
        for (const auto& [semantic, clipName] : pistolClips) {
            const auto found = pistolSet->clips.find(semantic);
            context.Expect(found != pistolSet->clips.end(), "pistol semantic exists");
            if (found == pistolSet->clips.end()) continue;
            context.Expect(found->second.clipIndex == found->second.model->FindClip(clipName),
                           "pistol semantic selects the exact authored clip name");
            Engine::Model::Pose pose;
            const auto sampled = Engine::Model::SamplePose(*found->second.model, found->second.clipIndex,
                0.1, Engine::Model::PlaybackMode::Clamp, pose);
            context.Expect(static_cast<bool>(sampled), "authored pistol animation samples");
            if (sampled) CheckPoseAndSkin(context, *found->second.model, pose);
        }
    }
    return set;
}

void TestIndependentInstances(TestContext& context, const AnimationSetDefinition& set) {
    const auto& animation = set.clips.at("run");
    const auto& model = *animation.model;
    Engine::Model::Pose first, second;
    const auto firstSample = Engine::Model::SamplePose(model, animation.clipIndex,
        0, Engine::Model::PlaybackMode::Loop, first);
    context.Expect(static_cast<bool>(firstSample), "first instance samples run at zero");
    if (!firstSample) { context.Fail(firstSample.error()); return; }
    const auto firstMatrices = first.globalTransforms;
    std::vector<std::vector<Engine::Model::SkinnedVertex>> firstSkin(model.meshes.size());
    for (std::size_t mesh = 0; mesh < model.meshes.size(); ++mesh) {
        context.Expect(static_cast<bool>(Engine::Model::SkinMesh(model, mesh, first, firstSkin[mesh])),
                       "first instance skins into its own vertex storage");
    }
    const double time = model.clips[animation.clipIndex].durationSeconds * 0.37;
    const auto secondSample = Engine::Model::SamplePose(model, animation.clipIndex,
        time, Engine::Model::PlaybackMode::Loop, second);
    context.Expect(static_cast<bool>(secondSample), "second instance samples run at another time");
    if (!secondSample) { context.Fail(secondSample.error()); return; }
    bool poseUnchanged = first.globalTransforms.size() == firstMatrices.size();
    bool posesDiffer = false, skinDiffers = false, firstSkinUnchanged = true;
    for (std::size_t node = 0; node < firstMatrices.size(); ++node) {
        poseUnchanged &= firstMatrices[node].values == first.globalTransforms[node].values;
        posesDiffer |= firstMatrices[node].values != second.globalTransforms[node].values;
    }
    for (std::size_t mesh = 0; mesh < model.meshes.size(); ++mesh) {
        std::vector<Engine::Model::SkinnedVertex> secondSkin, firstResampled;
        if (!Engine::Model::SkinMesh(model, mesh, second, secondSkin) ||
            !Engine::Model::SkinMesh(model, mesh, first, firstResampled)) { context.Fail("independent instance skinning failed"); continue; }
        for (std::size_t vertex = 0; vertex < firstSkin[mesh].size(); ++vertex) {
            const auto original = firstSkin[mesh][vertex].position;
            const auto other = secondSkin[vertex].position;
            const auto repeated = firstResampled[vertex].position;
            skinDiffers |= !NearlyEqual(original.x, other.x, 1e-6F) ||
                           !NearlyEqual(original.y, other.y, 1e-6F) || !NearlyEqual(original.z, other.z, 1e-6F);
            firstSkinUnchanged &= original.x == repeated.x && original.y == repeated.y && original.z == repeated.z;
        }
    }
    context.Expect(poseUnchanged && posesDiffer && skinDiffers && firstSkinUnchanged,
                   "instances share immutable source but keep different poses and independent CPU skinning output");
}

void TestCharacterMaterials(TestContext& context, DefinitionFixture& fixture) {
    std::string error;
    for (const auto* id : {kMalePresentation, "object_fps.character.superhero_female", "object_fps.character.ual1_mannequin"}) {
        const auto character = LoadCharacterPresentationDefinition(fixture.assets, AssetId::FromString(id), error);
        context.Expect(static_cast<bool>(character), error.empty() ? "selected character definition loads" : error);
        if (!character) continue;
        if (character->modelAssetId == AssetId::FromString(kMannequin)) {
            context.Expect(character->animationSet && character->animationSet->clips.size() == 3,
                           "mannequin character references its own locomotion set");
        } else {
            context.Expect(!character->animationSet && !character->animationSetAssetId,
                           "superheroes do not claim unsupported cross-file animation bindings");
            context.Expect(character->materials.size() == 3 &&
                std::all_of(character->materials.begin(), character->materials.end(),
                    [](const auto& material) { return material.textureAssetId.has_value(); }),
                "all three superhero material slots have validated texture asset IDs");
        }
    }
    const auto original = fixture.ReadConfig(kMalePresentation);
    auto config = original;
    config.erase("material_overrides");
    fixture.OverrideConfig(kMalePresentation, config);
    const auto defaults = LoadCharacterPresentationDefinition(fixture.assets, AssetId::FromString(kMalePresentation), error);
    context.Expect(static_cast<bool>(defaults), "character without material overrides uses imported material constants");
    if (!defaults || defaults->model->materials.empty()) return;
    const auto slot = defaults->model->materials[0].name;
    const auto imported = defaults->model->materials[0].baseColorLinear;
    context.Expect(defaults->materials[0].baseColorLinear == imported && !defaults->materials[0].textureAssetId,
                   "missing overrides preserve imported color and absence of a texture binding");
    config["material_overrides"][slot] = {{"base_color_linear", {0.2F, 0.4F, 0.6F, 0.8F}}, {"sampler", "linear_wrap"}};
    fixture.OverrideConfig(kMalePresentation, config);
    const auto overridden = LoadCharacterPresentationDefinition(fixture.assets, AssetId::FromString(kMalePresentation), error);
    context.Expect(overridden && overridden->model == defaults->model &&
        overridden->materials[0].baseColorLinear == std::array<float, 4>{0.2F, 0.4F, 0.6F, 0.8F} &&
        overridden->materials[0].sampler == Engine::Render::SamplerMode::LinearWrap &&
        defaults->materials[0].baseColorLinear == imported && defaults->model->materials[0].baseColorLinear == imported,
        "per-assembly material color/sampler overrides do not mutate shared models or prior definitions");
    fixture.OverrideConfig(kMalePresentation, original);
}

void TestDefinitionFailures(TestContext& context, DefinitionFixture& fixture) {
    std::string error;
    const auto originalSet = fixture.ReadConfig(kLocomotion);
    auto invalidSet = [&](nlohmann::json config, const char* expected) {
        fixture.OverrideConfig(kLocomotion, config);
        const auto set = LoadAnimationSetDefinition(fixture.assets, AssetId::FromString(kLocomotion), error);
        context.Expect(!set && error.find(kLocomotion) != std::string::npos && error.find(expected) != std::string::npos,
                       std::string{"invalid animation definition reports its ID and cause: "} + expected);
    };
    auto config = originalSet;
    config["clips"]["run"]["clip"] = "missing-authored-animation";
    invalidSet(config, "missing-authored-animation");
    config = originalSet; config["clips"]["run"]["model_asset_id"] = "missing.model.asset";
    invalidSet(config, "missing.model.asset");
    config = originalSet; config["clips"]["run"]["model_asset_id"] = "";
    invalidSet(config, "must not be empty");
    config = originalSet; config["version"] = 1.5;
    invalidSet(config, "version");
    config = originalSet; config["clips"] = nlohmann::json::object();
    invalidSet(config, "nonempty");
    fixture.OverrideConfig(kLocomotion, originalSet);

    const auto originalCharacter = fixture.ReadConfig(kMalePresentation);
    const auto model = fixture.LoadModel(kMale, context);
    if (!model || model->materials.empty()) return;
    const auto slot = model->materials[0].name;
    auto invalidCharacter = [&](nlohmann::json characterConfig, const std::string& expected) {
        fixture.OverrideConfig(kMalePresentation, characterConfig);
        const auto character = LoadCharacterPresentationDefinition(fixture.assets, AssetId::FromString(kMalePresentation), error);
        context.Expect(!character && error.find(kMalePresentation) != std::string::npos && error.find(expected) != std::string::npos,
                       "invalid character definition reports its ID and cause: " + expected);
    };
    config = originalCharacter; config["animation_set_asset_id"] = kLocomotion;
    invalidCharacter(config, "cross-source");
    config = originalCharacter; config["model_asset_id"] = "missing.character.model";
    invalidCharacter(config, "missing.character.model");
    config = originalCharacter; config["material_overrides"]["missing-material-slot"] = nlohmann::json::object();
    invalidCharacter(config, "missing-material-slot");
    config = originalCharacter; config["material_overrides"][slot]["base_color_linear"] = {1, 1, 1};
    invalidCharacter(config, "four finite numbers");
    config = originalCharacter; config["material_overrides"][slot]["base_color_linear"] = {1e100, 1, 1, 1};
    invalidCharacter(config, "four finite numbers");
    config = originalCharacter; config["material_overrides"][slot]["texture_asset_id"] = "missing.texture.asset";
    invalidCharacter(config, "missing.texture.asset");
    config = originalCharacter; config["material_overrides"][slot]["texture_asset_id"] = kMale;
    invalidCharacter(config, kMale);
    config = originalCharacter; config["material_overrides"][slot]["sampler"] = "nearest_guess";
    invalidCharacter(config, "nearest_guess");
    config = originalCharacter; config.erase("version");
    invalidCharacter(config, "version");
    fixture.OverrideConfig(kMalePresentation, originalCharacter);

    context.Expect(!LoadAnimationSetDefinition(fixture.assets, AssetId::FromString("missing.animation.set"), error) &&
        error.find("missing.animation.set") != std::string::npos, "missing definition asset reports the requested ID");
}

} // namespace

void RunAssetPresentationDefinitionTests(TestContext& context) {
    std::shared_ptr<const AnimationSetDefinition> survivingSet;
    {
        DefinitionFixture fixture;
        std::string error;
        if (!fixture.Initialize(error)) { context.Fail(error); return; }
        TestSelectedAssets(context, fixture);
        survivingSet = TestAnimationSets(context, fixture);
        if (survivingSet) TestIndependentInstances(context, *survivingSet);
        TestCharacterMaterials(context, fixture);
        TestDefinitionFailures(context, fixture);
        for (const auto* name : {kMale, kFemale, kMannequin, kWorldPistol, kAnimatedPistol}) {
            context.Expect(fixture.source.modelReads[fixture.PathFor(name)] == 1,
                           std::string{"definition/character lookups retain a single cached FBX parse: "} + name);
        }
    }
    if (survivingSet) {
        const auto& clip = survivingSet->clips.at("idle");
        Engine::Model::Pose pose;
        const auto sampled = Engine::Model::SamplePose(*clip.model, clip.clipIndex,
            0.1, Engine::Model::PlaybackMode::Loop, pose);
        context.Expect(static_cast<bool>(sampled), "animation set owns model storage after AssetManager and cache destruction");
        if (sampled) CheckPoseAndSkin(context, *clip.model, pose);
    }
}

} // namespace fps::tests
