#include "../TestSupport.hpp"
#include "../TestAssets.hpp"

#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/App/ObjectFpsUi.hpp"
#include "RetroFPS/App/CampaignContentLoader.hpp"
#include "RetroFPS/App/WeaponPresentationDefinition.hpp"
#include "RetroFPS/App/WeaponViewModel.hpp"
#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Game/CampaignContent.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextureLoader.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/IAssetSource.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "render/IRenderDevice.hpp"
#include "render/Renderer.hpp"
#include "RenderDeviceStub.hpp"
#include "render/RenderQueue.hpp"
#include "text/ITextRasterizer.hpp"
#include "ui/UiDocumentCodec.hpp"
#include <nlohmann/json.hpp>

#include <cstddef>
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fps::tests {
namespace {

constexpr std::string_view kEnemies =
    "enemy_id,kind,damage,attack_interval_seconds,hp,defense,hitbox_radius,hitbox_height,render_width,render_height,texture_asset_id,frame_width_px,frame_height_px\n"
    "melee_basic,melee,15,0.9,50,5,0.2,0.8,0.973913,0.8,object_fps.texture.enemy.blood_dog,560,460\n"
    "ranged_basic,ranged,10,1.25,40,0,0.2,1.6,1.230769,1.6,object_fps.texture.enemy.spitter,700,910\n";

constexpr std::string_view kAnimations =
    "enemy_id,state,origin_x_px,origin_y_px,frame_count,seconds_per_frame,event_frame_index,muzzle_x_px,muzzle_y_px\n"
    "melee_basic,idle,0,0,3,0.1,,,\n"
    "melee_basic,move,0,460,4,0.1,,,\n"
    "melee_basic,attack,0,920,6,0.05,3,,\n"
    "melee_basic,dead,0,1380,4,0.1,,,\n"
    "ranged_basic,idle,0,0,3,0.1,,,\n"
    "ranged_basic,move,0,910,4,0.1,,,\n"
    "ranged_basic,attack,0,1820,5,0.05,2,350,420\n"
    "ranged_basic,dead,0,2730,4,0.1,,,\n";

constexpr std::string_view kWeapons =
    "weapon_id,damage,magazine_size,reserve_ammo,recoil,automatic,fire_interval_seconds,reload_seconds,draw_seconds,hide_seconds,presentation_asset_id\n"
    "starter_pistol,25,12,48,1.5,false,0.333333333,3.733333333,0.833333333,0.366666667,object_fps.weapon.mark23\n";

constexpr std::string_view kLevels =
    "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,melee_enemy_count,active_enemy_limit,clear_kill_count\n"
    "room_0,ROOM 0,object_fps.map.room_0,,1,1,2,1\n";

constexpr std::string_view kMap =
    "##############\n"
    "#P..........R#\n"
    "#..###..#....#\n"
    "#......##....#\n"
    "#......##....#\n"
    "#......##....#\n"
    "#..#...##....#\n"
    "#..#..####...#\n"
    "#.M.........D#\n"
    "##############\n";

class PpmAssetSource final : public Engine::Asset::Loading::IAssetSource {
public:
    std::optional<std::string> presentationOverride;
    std::optional<std::string> animationSetOverride;
    std::size_t modelReadCount{};

    Engine::Base::Result<std::vector<std::byte>, Engine::Asset::AssetError>
    ReadAll(std::string_view path) override {
        if (path.find("mark23") != std::string_view::npos &&
            path.ends_with("viewmodel.animset.json") && animationSetOverride) {
            const auto bytes = std::as_bytes(std::span(animationSetOverride->data(), animationSetOverride->size()));
            return Engine::Base::Result<std::vector<std::byte>, Engine::Asset::AssetError>::Ok(
                std::vector<std::byte>(bytes.begin(), bytes.end()));
        }
        if (path.ends_with("mark23_viewmodel.json") && presentationOverride) {
            const auto bytes = std::as_bytes(std::span(presentationOverride->data(), presentationOverride->size()));
            return Engine::Base::Result<std::vector<std::byte>, Engine::Asset::AssetError>::Ok(
                std::vector<std::byte>(bytes.begin(), bytes.end()));
        }
        if (path.ends_with(".fbx")) ++modelReadCount;
        if (path.ends_with(".fbx") || path.ends_with(".json") ||
            path.ends_with(".csv") || path.ends_with(".txt")) {
            Engine::Asset::Loading::NativeFileAssetSource native;
            return native.ReadAll(path);
        }
        constexpr std::string_view ppm = "P3\n1 1\n255\n255 255 255\n";
        std::vector<std::byte> bytes;
        bytes.reserve(ppm.size());
        for (const char value : ppm) {
            bytes.push_back(static_cast<std::byte>(
                static_cast<unsigned char>(value)));
        }
        return Engine::Base::Result<
            std::vector<std::byte>, Engine::Asset::AssetError>::Ok(
                std::move(bytes));
    }
};

class FakeTextRasterizer final : public Engine::Text::ITextRasterizer {
public:
    Engine::Base::Result<Engine::Text::TextBitmap, Engine::Text::TextError>
    Rasterize(
        std::span<const std::byte>,
        const Engine::Text::TextRasterRequest&) override {
        Engine::Text::TextBitmap bitmap;
        bitmap.width = 2;
        bitmap.height = 2;
        bitmap.rowPitch = 8;
        bitmap.rgba8.resize(16, std::byte{0xFF});
        return Engine::Base::Result<
            Engine::Text::TextBitmap, Engine::Text::TextError>::Ok(
                std::move(bitmap));
    }
};

class CapturingRenderDevice final : public Gyo::Tests::RenderDeviceStub {
public:
    Engine::Render::FrameDescription frame;
    std::vector<Engine::Render::MeshSubmission> meshes;
    std::vector<Engine::Render::SpriteSubmission> sprites;
    std::optional<Engine::Render::PerspectiveCamera3D> camera, viewmodelCamera;
    std::unordered_map<Engine::Render::MeshHandle, std::vector<Engine::Render::Vertex3D>> vertices;
    std::size_t updateCount{};

    Engine::Base::Result<Engine::Render::MeshHandle, Engine::Render::RenderError>
    CreateMesh(const Engine::Render::MeshView& mesh) override {
        const auto handle = Engine::Render::MeshHandle::FromParts(++meshSerial_, 1);
        vertices[handle].assign(mesh.vertices.begin(), mesh.vertices.end());
        return Engine::Base::Result<
            Engine::Render::MeshHandle, Engine::Render::RenderError>::Ok(
                handle);
    }

    Engine::Base::Result<void, Engine::Render::RenderError> UpdateMeshVertices(
        Engine::Render::MeshHandle handle,
        std::span<const Engine::Render::Vertex3D> data) override {
        vertices[handle].assign(data.begin(), data.end());
        ++updateCount;
        return Engine::Base::Result<void, Engine::Render::RenderError>::Ok();
    }

    Engine::Base::Result<
        Engine::Render::TextureHandle, Engine::Render::RenderError>
    CreateTexture(const Engine::Render::ImageView&) override {
        return Engine::Base::Result<
            Engine::Render::TextureHandle, Engine::Render::RenderError>::Ok(
                Engine::Render::TextureHandle::FromParts(++textureSerial_, 1));
    }

    Engine::Base::Result<void, Engine::Render::RenderError> ReleaseMesh(
        Engine::Render::MeshHandle) override {
        return Engine::Base::Result<void, Engine::Render::RenderError>::Ok();
    }

    Engine::Base::Result<void, Engine::Render::RenderError> ReleaseTexture(
        Engine::Render::TextureHandle) override {
        return Engine::Base::Result<void, Engine::Render::RenderError>::Ok();
    }

    void Capture(const Engine::Render::RenderQueue& queue) {
        frame = queue.Frame();
        camera = queue.Camera();
        viewmodelCamera = queue.ViewModelCamera();
        meshes.assign(queue.Meshes().begin(), queue.Meshes().end());
        sprites.assign(queue.Sprites().begin(), queue.Sprites().end());
    }

private:
    std::uint32_t meshSerial_{};
    std::uint32_t textureSerial_{};
};

bool PreparePresentation(ObjectFpsPresentation& presentation, CapturingRenderDevice& device,
    const GameSessionSnapshot& snapshot, const ObjectFpsDisplaySettings& display,
    const Engine::Ui::UiDrawList& ui, std::string& error) {
    if (!presentation.PrepareFrame(snapshot, display, ui, error)) return false;
    device.Capture(presentation.PreparedQueue());
    return true;
}

struct AssetFixture final {
    Engine::Asset::AssetCatalog catalog;
    Engine::Asset::Loading::LoaderRegistry registry;
    PpmAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, registry};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{
        catalog, pipeline, storage, lifetime, policy};

    [[nodiscard]] bool Initialize(std::string& error) {
        auto texture = registry.Register(
            std::make_unique<Engine::Asset::Loaders::TextureLoader>());
        if (!texture) {
            error = texture.error().message;
            return false;
        }
        auto font = registry.Register(
            std::make_unique<Engine::Asset::Loaders::FontLoader>());
        if (!font) {
            error = font.error().message;
            return false;
        }
        if (!registry.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>()) ||
            !registry.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>())) {
            error = "presentation fixture model/text loaders failed";
            return false;
        }

        try { catalog = LoadTestCatalog(); }
        catch (const std::exception& failure) { error = failure.what(); return false; }
        return true;
    }
};

[[nodiscard]] std::shared_ptr<const CampaignContent> MakeContent(
    TestContext& context) {
    GameDataLoadResult data =
        GameDataLoader::Parse(kEnemies, kAnimations, kWeapons, kLevels);
    context.Expect(data.Succeeded(), "presentation fixture game data parses");
    if (!data.catalog.has_value()) return {};

    MapLoadResult map = GridMapLoader::Parse(kMap);
    context.Expect(map.Succeeded(), "presentation fixture map parses");
    if (!map.map.has_value()) return {};

    std::vector<GridMap> maps;
    maps.push_back(std::move(*map.map));
    CampaignContentBuildResult content =
        CampaignContent::Build(std::move(*data.catalog), std::move(maps),
            {{"starter_pistol", {{0.116820F, -0.090313F, 0.763802F},
                                55.0F * std::numbers::pi_v<float> / 180.0F}}});
    context.Expect(content.Succeeded(), "presentation fixture content builds");
    if (!content.content.has_value()) return {};
    return std::make_shared<CampaignContent>(std::move(*content.content));
}

[[nodiscard]] std::shared_ptr<const Engine::Ui::UiDocument> LoadUiDocument(
    TestContext& context) {
    const auto path = TestAssetPath("object_fps.ui.screens");
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(buffer.str(), path.string());
    context.Expect(static_cast<bool>(parsed), "presentation fixture UI JSON parses");
    if (!parsed) return {};
    return std::make_shared<const Engine::Ui::UiDocument>(
        std::move(parsed).value());
}

[[nodiscard]] nlohmann::json ReadWeaponPresentationConfig() {
    const auto path = TestAssetPath("object_fps.weapon.mark23");
    std::ifstream stream(path);
    return nlohmann::json::parse(stream);
}

bool SamePoint(Engine::Render::Float3 a, Engine::Render::Float3 b, float tolerance = 0.00001F) {
    return NearlyEqual(a.x, b.x, tolerance) && NearlyEqual(a.y, b.y, tolerance) &&
           NearlyEqual(a.z, b.z, tolerance);
}

void TestModelDerivedMuzzle(TestContext& context) {
    AssetFixture fixture;
    std::string error;
    if (!fixture.Initialize(error)) { context.Fail(error); return; }
    const auto id = Engine::Asset::AssetId::FromString("object_fps.weapon.mark23");
    const auto definition = LoadWeaponPresentationDefinition(fixture.assets, id, error);
    context.Expect(static_cast<bool>(definition), "shared weapon definition loads the actual Mark23 model");
    if (!definition) return;
    const auto reloaded = LoadWeaponPresentationDefinition(fixture.assets, id, error);
    context.Expect(reloaded && definition->model == reloaded->model && fixture.source.modelReadCount == 1,
        "content and presentation reuse the immutable AssetManager model payload");
    context.Expect(definition->model->nodes[definition->muzzleNodeIndex].name == "main_j",
        "muzzle attaches to the rigid barrel bone rather than the recoiling slide");
    const auto& geometry = definition->shotGeometry;
    const Engine::Render::Float3 shot{geometry.muzzleViewCameraPosition.x,
        geometry.muzzleViewCameraPosition.y, geometry.muzzleViewCameraPosition.z};
    context.Expect(SamePoint(shot, {0.116820F, -0.090313F, 0.763802F}),
        "Shoot zero muzzle is derived from the calibrated bore and authored placement");
    context.Expect(NearlyEqual(geometry.viewModelVerticalFovRadians,
        55.0F * std::numbers::pi_v<float> / 180.0F), "shot calibration retains the viewmodel FOV");

    Engine::Model::Pose pose;
    auto sampled = Engine::Model::SamplePose(*definition->model, definition->clips[0],
        0, Engine::Model::PlaybackMode::Clamp, pose);
    context.Expect(static_cast<bool>(sampled), "muzzle fixture samples Idle zero");
    if (!sampled) return;
    context.Expect(SamePoint(EvaluateWeaponMuzzleViewCameraPosition(*definition, pose), shot),
        "Idle zero and Shoot zero agree on the muzzle position");
    const auto muzzleModel = Engine::Model::TransformPoint(
        pose.globalTransforms[definition->muzzleNodeIndex], definition->muzzleLocalPosition);
    const auto mainMesh = std::find_if(definition->model->meshes.begin(), definition->model->meshes.end(),
        [](const auto& mesh) { return mesh.name == "main/Mark23_D"; });
    context.Expect(mainMesh != definition->model->meshes.end(), "actual model contains the rigid barrel mesh");
    if (mainMesh == definition->model->meshes.end()) return;
    std::vector<Engine::Model::SkinnedVertex> skinned;
    const auto meshIndex = static_cast<std::size_t>(mainMesh - definition->model->meshes.begin());
    context.Expect(static_cast<bool>(Engine::Model::SkinMesh(*definition->model, meshIndex, pose, skinned)),
        "actual barrel can be skinned for independent socket calibration");
    std::vector<Engine::Model::Vec3> bore;
    for (const auto& vertex : skinned) {
        const auto p = vertex.position;
        const float dx = p.x - muzzleModel.x, dy = p.y - muzzleModel.y;
        const float radius = std::sqrt(dx * dx + dy * dy);
        if (std::abs(p.z - muzzleModel.z) > 0.00013F || radius < 0.0065F || radius > 0.0071F) continue;
        if (std::none_of(bore.begin(), bore.end(), [&](auto prior) {
            return NearlyEqual(prior.x, p.x, 1e-6F) && NearlyEqual(prior.y, p.y, 1e-6F) &&
                   NearlyEqual(prior.z, p.z, 1e-6F);
        })) bore.push_back(p);
    }
    context.Expect(bore.size() == 18, "socket lies in the authored eighteen-vertex inner muzzle ring");
    Engine::Model::Vec3 center{};
    for (const auto p : bore) {
        const float dx = p.x - muzzleModel.x, dy = p.y - muzzleModel.y, dz = p.z - muzzleModel.z;
        context.Expect(std::abs(std::sqrt(dx * dx + dy * dy + dz * dz) - 0.006883F) < 0.00015F,
            "muzzle ring vertices surround the socket at the measured bore radius");
        center.x += p.x; center.y += p.y; center.z += p.z;
    }
    if (!bore.empty()) {
        const float count = static_cast<float>(bore.size());
        context.Expect(SamePoint({center.x / count, center.y / count, center.z / count},
            {muzzleModel.x, muzzleModel.y, muzzleModel.z}, 0.000005F),
            "socket matches the independent centroid of the actual barrel opening");
    }

    CapturingRenderDevice device;
    WeaponViewModel viewmodel;
    context.Expect(viewmodel.Initialize(device, fixture.assets, id, error),
        "viewmodel initializes from the same shared definition loader");
    Engine::Render::RenderQueue queue;
    WeaponPresentationSnapshot snapshot;
    snapshot.action = WeaponAction::Shoot;
    snapshot.durationSeconds = 10.0F / 30.0F;
    snapshot.elapsedSeconds = 4.0F / 30.0F;
    context.Expect(viewmodel.Submit(snapshot, queue, error), "current-pose muzzle follows the Shoot animation");
    const auto current = viewmodel.GetMuzzleViewCameraPosition();
    context.Expect(SamePoint(current, {0.116820F, -0.072591F, 0.724484F}),
        "diagnostic muzzle follows barrel recoil without inheriting slide translation");
    context.Expect(!SamePoint(current, shot), "animated muzzle is distinct from the immutable shot-start calibration");

    const auto content = CampaignContentLoader::Load(fixture.assets);
    context.Expect(content.Succeeded(), "real campaign loader resolves CSV, maps and model-based shot geometry");
    if (content.content) {
        const auto* loadedGeometry = content.content->FindWeaponShotGeometry("starter_pistol");
        context.Expect(loadedGeometry && NearlyEqual(loadedGeometry->muzzleViewCameraPosition.x, shot.x) &&
            NearlyEqual(loadedGeometry->muzzleViewCameraPosition.y, shot.y) &&
            NearlyEqual(loadedGeometry->muzzleViewCameraPosition.z, shot.z) &&
            NearlyEqual(loadedGeometry->viewModelVerticalFovRadians, geometry.viewModelVerticalFovRadians),
            "campaign numeric geometry is the same calibration used by presentation");
    }
}

void TestSharedPlacementAndDefinitionErrors(TestContext& context) {
    const auto original = ReadWeaponPresentationConfig();
    auto changed = original;
    changed["rotation_degrees"] = {90, 90, 90};
    changed["offset_meters"] = {0.2, -0.3, 1.2};
    changed["scale"] = 2;
    changed["vertical_fov_degrees"] = 65;
    AssetFixture fixture;
    fixture.source.presentationOverride = changed.dump();
    std::string error;
    const auto id = Engine::Asset::AssetId::FromString("object_fps.weapon.mark23");
    if (!fixture.Initialize(error)) { context.Fail(error); return; }
    const auto definition = LoadWeaponPresentationDefinition(fixture.assets, id, error);
    context.Expect(static_cast<bool>(definition), "modified placement is accepted by the shared loader");
    if (definition) {
        const auto point = definition->shotGeometry.muzzleViewCameraPosition;
        context.Expect(SamePoint({point.x, point.y, point.z}, {-0.227604F, -0.120627F, 1.193640F}),
            "muzzle placement applies scale then all XYZ rotations then translation");
        CapturingRenderDevice device;
        WeaponViewModel viewmodel;
        context.Expect(viewmodel.Initialize(device, fixture.assets, id, error),
            "modified presentation shares placement parsing with shot geometry");
        Engine::Render::RenderQueue queue;
        WeaponPresentationSnapshot snapshot;
        snapshot.action = WeaponAction::Shoot;
        snapshot.durationSeconds = 10.0F / 30.0F;
        context.Expect(viewmodel.Submit(snapshot, queue, error), "modified Shoot zero submits successfully");
        context.Expect(SamePoint(viewmodel.GetMuzzleViewCameraPosition(), {point.x, point.y, point.z}),
            "current rendered pose uses the same modified muzzle placement as gameplay calibration");
        for (const auto& mesh : queue.Meshes()) {
            context.Expect(SamePoint(mesh.transform.translation, definition->placement.translation) &&
                SamePoint(mesh.transform.rotationRadians, definition->placement.rotationRadians) &&
                SamePoint(mesh.transform.scale, definition->placement.scale),
                "every mesh receives exactly the placement used to derive shot geometry");
        }
        context.Expect(queue.ViewModelCamera() && NearlyEqual(
            queue.ViewModelCamera()->verticalFieldOfViewRadians,
            definition->shotGeometry.viewModelVerticalFovRadians),
            "modified FOV reaches both render camera and shot geometry");
    }

    const auto reject = [&](std::string_view label, auto mutate) {
        auto config = original;
        mutate(config);
        AssetFixture invalid;
        invalid.source.presentationOverride = config.dump();
        std::string failure;
        if (!invalid.Initialize(failure)) { context.Fail(failure); return; }
        const auto loaded = LoadWeaponPresentationDefinition(invalid.assets, id, failure);
        context.Expect(!loaded && !failure.empty() && failure.find("object_fps.weapon.mark23") != std::string::npos,
            label);
        const auto campaign = CampaignContentLoader::Load(invalid.assets);
        context.Expect(!campaign && campaign.error.find("starter_pistol") != std::string::npos,
            "campaign loading reports the weapon whose shared presentation is invalid");
    };
    reject("missing muzzle configuration fails without a fallback", [](auto& config) { config.erase("muzzle"); });
    reject("missing model muzzle node fails with an asset diagnostic", [](auto& config) { config["muzzle"]["node"] = "missing_muzzle"; });
    reject("non-finite muzzle coordinates fail validation", [](auto& config) { config["muzzle"]["local_position_meters"] = {1e300, 0, 0}; });
    reject("muzzle behind the viewmodel camera fails validation", [](auto& config) { config["offset_meters"] = {0, 0, -2}; });
    reject("zero placement scale fails validation", [](auto& config) { config["scale"] = 0; });
    reject("invalid viewmodel FOV fails validation", [](auto& config) { config["vertical_fov_degrees"] = 0; });
    reject("missing Shoot clip fails before shot geometry is created", [](auto& config) {
        config.erase("animation_set_asset_id");
        config["clips"] = {{"Idle", "Idle"}, {"Shoot", "missing_shoot"}, {"Reload", "Reload"},
                           {"Draw", "Draw"}, {"Hide", "Hide"}};
    });
    reject("missing material binding fails shared definition loading", [](auto& config) { config["materials"].erase("Mark23_D"); });
    reject("unknown material slot is diagnosed", [](auto& config) { config["materials"]["Typo"] = "common.texture.white"; });
    reject("both animation forms are rejected", [](auto& config) { config["clips"] = {{"Idle", "Idle"}}; });
    reject("missing both animation forms is rejected", [](auto& config) { config.erase("animation_set_asset_id"); });
}

void TestExternalAnimationSetAndIndependentInstances(TestContext& context) {
    const auto id = Engine::Asset::AssetId::FromString("object_fps.weapon.mark23");
    AssetFixture fixture;
    std::string error;
    if (!fixture.Initialize(error)) { context.Fail(error); return; }
    const auto external = LoadWeaponPresentationDefinition(fixture.assets, id, error);
    context.Expect(static_cast<bool>(external), error.empty() ? "external Mark23 animation set loads" : error);
    if (!external) return;

    auto legacyConfig = ReadWeaponPresentationConfig();
    legacyConfig.erase("animation_set_asset_id");
    legacyConfig["clips"] = {{"Idle", "Idle"}, {"Shoot", "Shoot"}, {"Reload", "Reload"},
                             {"Draw", "Draw"}, {"Hide", "Hide"}};
    AssetFixture legacyFixture;
    legacyFixture.source.presentationOverride = legacyConfig.dump();
    if (!legacyFixture.Initialize(error)) { context.Fail(error); return; }
    const auto legacy = LoadWeaponPresentationDefinition(legacyFixture.assets, id, error);
    context.Expect(static_cast<bool>(legacy), "legacy inline weapon animations remain supported");
    if (legacy) {
        context.Expect(legacy->clips == external->clips &&
            legacy->materialTextureAssetIds == external->materialTextureAssetIds &&
            SamePoint({legacy->muzzleLocalPosition.x, legacy->muzzleLocalPosition.y, legacy->muzzleLocalPosition.z},
                      {external->muzzleLocalPosition.x, external->muzzleLocalPosition.y, external->muzzleLocalPosition.z}),
            "inline and external animation selectors preserve clips, materials and muzzle");
    }

    const auto animsetPath = TestAssetPath("object_fps.animset.mark23.viewmodel");
    std::ifstream animsetStream(animsetPath);
    auto incompleteSet = nlohmann::json::parse(animsetStream);
    incompleteSet["clips"].erase("Shoot");
    AssetFixture incomplete;
    incomplete.source.animationSetOverride = incompleteSet.dump();
    if (!incomplete.Initialize(error)) { context.Fail(error); return; }
    context.Expect(!LoadWeaponPresentationDefinition(incomplete.assets, id, error) &&
        error.find("Shoot") != std::string::npos && error.find(id.debugName) != std::string::npos,
        "external set must provide every weapon action with a contextual diagnostic");

    CapturingRenderDevice device;
    WeaponViewModel first, second;
    if (!first.Initialize(device, fixture.assets, id, error) ||
        !second.Initialize(device, fixture.assets, id, error)) { context.Fail(error); return; }
    context.Expect(fixture.source.modelReadCount == 1,
        "two viewmodels and their animation references share one imported model");
    Engine::Render::RenderQueue secondQueue;
    WeaponPresentationSnapshot idle;
    idle.action = WeaponAction::Idle;
    context.Expect(second.Submit(idle, secondQueue, error), "second viewmodel submits its own Idle pose");
    const auto secondMuzzle = second.GetMuzzleViewCameraPosition();
    std::unordered_map<Engine::Render::MeshHandle, std::vector<Engine::Render::Vertex3D>> saved;
    for (const auto& mesh : secondQueue.Meshes()) saved.emplace(mesh.mesh, device.vertices.at(mesh.mesh));
    context.Expect(!saved.empty(), "independence test captures real Mark23 meshes");
    Engine::Render::RenderQueue firstQueue;
    WeaponPresentationSnapshot shoot;
    shoot.action = WeaponAction::Shoot;
    shoot.durationSeconds = 10.0F / 30.0F;
    shoot.elapsedSeconds = 4.0F / 30.0F;
    context.Expect(first.Submit(shoot, firstQueue, error), "first viewmodel advances independently to Shoot");
    context.Expect(!SamePoint(first.GetMuzzleViewCameraPosition(), secondMuzzle) &&
        SamePoint(second.GetMuzzleViewCameraPosition(), secondMuzzle),
        "advancing the first viewmodel leaves the second muzzle and playback pose unchanged");
    bool unchanged = true;
    for (const auto& [handle, before] : saved) {
        const auto& after = device.vertices.at(handle);
        unchanged = unchanged && before.size() == after.size();
        for (std::size_t i = 0; i < before.size() && i < after.size(); ++i) {
            unchanged = unchanged && SamePoint(before[i].position, after[i].position) &&
                NearlyEqual(before[i].uv.x, after[i].uv.x) && NearlyEqual(before[i].uv.y, after[i].uv.y);
        }
    }
    context.Expect(unchanged, "second viewmodel GPU vertex buffers are not overwritten by the first");
    for (const auto& mesh : firstQueue.Meshes()) {
        const auto color = mesh.material.tint;
        context.Expect(color.red == 1 && color.green == 1 && color.blue == 1 && color.alpha == 1,
            "Mark23 explicit texture bindings retain white tint");
    }
}

void TestSceneAndOverlaySubmissionOrder(TestContext& context) {
    const std::shared_ptr<const CampaignContent> content = MakeContent(context);
    const std::shared_ptr<const Engine::Ui::UiDocument> document =
        LoadUiDocument(context);
    if (!content || !document) return;

    std::string error;
    AssetFixture assets;
    const bool assetsInitialized = assets.Initialize(error);
    context.Expect(
        assetsInitialized,
        "presentation fixture asset catalog and loaders initialize");
    if (!assetsInitialized) return;

    CapturingRenderDevice renderDevice;
    Engine::Render::Renderer renderer;
    FakeTextRasterizer rasterizer;
    ObjectFpsPresentation presentation;
    context.Expect(
        presentation.Initialize(
            renderDevice, renderer, rasterizer, assets.assets, content, {}, error),
        "ObjectFpsPresentation initializes against a capturing render device");
    if (!presentation.IsInitialized()) return;

    ObjectFpsUi ui;
    context.Expect(
        ui.Initialize(document, error),
        "ObjectFpsUi initializes for presentation ordering");
    if (!ui.IsInitialized()) return;

    GameSessionSnapshot snapshot;
    snapshot.screen = GameScreen::Paused;
    snapshot.fadeOpacity = 0.65F;
    snapshot.activeStage = ActiveStageSnapshot{
        "room_0", "ROOM 0", 0, 1, true};
    snapshot.player = PlayerSnapshot{
        {1.5F, 1.5F}, 0.55F, 0.0F, 0.0F, 75.0F, 100.0F};
    snapshot.worldVerticalFovRadians = 75.0F * std::numbers::pi_v<float> / 180.0F;
    snapshot.weapon.weaponId = "starter_pistol";
    snapshot.weaponPresentation.weaponId = "starter_pistol";
    snapshot.weaponPresentation.action = WeaponAction::Idle;
    snapshot.player->feetY = 0.6F;
    snapshot.weapon.magazineAmmo = 7;
    snapshot.weapon.reserveAmmo = 35;

    Engine::Ui::UiDrawList pausedUi;
    const ObjectFpsDisplaySettings display{0.5F, 1.1F};
    context.Expect(
        ui.Compose(snapshot, display, {1280.0F, 720.0F}, pausedUi, error),
        "paused HUD and authored menu compose into one ordered draw list");
    context.Expect(
        PreparePresentation(presentation, renderDevice, snapshot, display, pausedUi, error),
        "capturing render device accepts the paused presentation");

    context.Expect(
        renderDevice.frame.sceneColorTransform.exposureEv == display.exposureEv &&
            renderDevice.frame.sceneColorTransform.gammaAdjustment ==
                display.gammaAdjustment,
        "app display settings reach the scene frame description");
    context.Expect(
        !renderDevice.meshes.empty(),
        "paused world is submitted as scene meshes before compositing");
    context.Expect(
        renderDevice.sprites.size() == pausedUi.commands.size() + 1U,
        "ordered UI commands and final fade produce sprites; weapon uses meshes");
    if (renderDevice.sprites.size() != pausedUi.commands.size() + 1U) return;
    context.Expect(renderDevice.camera && NearlyEqual(renderDevice.camera->position.y, 1.15F),
        "camera adds the player's world feet height to its eye offset");
    context.Expect(renderDevice.camera && NearlyEqual(renderDevice.camera->verticalFieldOfViewRadians,
        snapshot.worldVerticalFovRadians), "rendered world camera uses the snapshot FOV rather than a fixed default");
    context.Expect(renderDevice.viewmodelCamera.has_value(), "weapon has its own camera");
    std::vector<Engine::Render::MeshHandle> weaponMeshes;
    for (const auto& mesh : renderDevice.meshes) {
        if (mesh.layer == Engine::Render::MeshLayer::ViewModel) {
            weaponMeshes.push_back(mesh.mesh);
            context.Expect(mesh.material.sampler == Engine::Render::SamplerMode::LinearWrap,
                "authored Mark23 UV tiles repeat instead of clamping to texture edges");
            context.Expect(!mesh.doubleSided,
                "weapon uses correct exterior winding rather than disabling face culling");
        }
    }
    context.Expect(weaponMeshes.size() == 5, "Mark23 produces five material batches");
    for (std::size_t index = 0; index < renderDevice.sprites.size(); ++index) {
        context.Expect(
            renderDevice.sprites[index].layer ==
                Engine::Render::CompositeLayer::Overlay,
            "HUD, pause menu, and fade remain Overlay submissions");
    }

    const Engine::Render::SpriteSubmission& fade = renderDevice.sprites.back();
    context.Expect(
        fade.destinationPixels.x == 0.0F &&
            fade.destinationPixels.y == 0.0F &&
            fade.destinationPixels.width == 1280.0F &&
            fade.destinationPixels.height == 720.0F &&
            NearlyEqual(fade.material.tint.alpha, snapshot.fadeOpacity),
        "fade is the final full-viewport Overlay submission");

    const auto idleVertices = renderDevice.vertices;
    snapshot.weaponPresentation.action = WeaponAction::Reload;
    snapshot.weaponPresentation.elapsedSeconds = 1.5F;
    snapshot.weaponPresentation.durationSeconds = 112.0F / 30.0F;
    context.Expect(PreparePresentation(presentation, renderDevice, snapshot, display, pausedUi, error),
        "native reload snapshot evaluates and uploads the model");
    const auto updates = renderDevice.updateCount;
    context.Expect(updates == 5, "each stable mesh receives one vertex update");
    bool moved = false;
    for (auto handle : weaponMeshes) {
        const auto& before = idleVertices.at(handle);
        const auto& after = renderDevice.vertices.at(handle);
        context.Expect(before.size() == after.size(), "skinning preserves mesh topology");
        for (std::size_t i = 0; i < before.size(); ++i) {
            if (!NearlyEqual(before[i].position.x, after[i].position.x) ||
                !NearlyEqual(before[i].position.y, after[i].position.y) ||
                !NearlyEqual(before[i].position.z, after[i].position.z)) moved = true;
        }
    }
    context.Expect(moved, "reload moves the actual mesh vertices");
    context.Expect(PreparePresentation(presentation, renderDevice, snapshot, display, pausedUi, error) &&
        renderDevice.updateCount == updates,
        "presenting the same snapshot never advances animation or uploads again");
    for (const auto& mesh : renderDevice.meshes) {
        if (mesh.layer == Engine::Render::MeshLayer::ViewModel) {
            context.Expect(std::find(weaponMeshes.begin(), weaponMeshes.end(), mesh.mesh) != weaponMeshes.end(),
                "animation preserves GPU mesh handles");
        }
    }

    snapshot.weaponPresentation.action = WeaponAction::Holstered;
    context.Expect(PreparePresentation(presentation, renderDevice, snapshot, display, pausedUi, error), "holstered presentation succeeds");
    context.Expect(std::none_of(renderDevice.meshes.begin(), renderDevice.meshes.end(), [](const auto& mesh) {
        return mesh.layer == Engine::Render::MeshLayer::ViewModel;
    }), "holstered weapon submits no meshes");
    const auto withDoor = renderDevice.meshes;
    snapshot.activeStage->doorVisible = false;
    context.Expect(PreparePresentation(presentation, renderDevice, snapshot, display, pausedUi, error), "closed exit presentation succeeds");
    context.Expect(withDoor.size() == renderDevice.meshes.size() + 1, "door visibility removes one cube");
    const auto missing = std::find_if(withDoor.begin(), withDoor.end(), [&](const auto& candidate) {
        return std::none_of(renderDevice.meshes.begin(), renderDevice.meshes.end(), [&](const auto& current) {
            return candidate.mesh == current.mesh && candidate.material.texture == current.material.texture;
        });
    });
    context.Expect(missing != withDoor.end() && missing->material.texture.IsValid(),
        "door uses a distinct loaded texture from every wall submission");

    for (const auto action : {WeaponAction::Shoot, WeaponAction::Draw, WeaponAction::Hide}) {
        snapshot.weaponPresentation.action = action;
        snapshot.weaponPresentation.durationSeconds = 1.0F;
        snapshot.weaponPresentation.elapsedSeconds = 0.5F;
        context.Expect(PreparePresentation(presentation, renderDevice, snapshot, display, pausedUi, error),
            "every authored action can be sampled from a gameplay snapshot");
        context.Expect(std::count_if(renderDevice.meshes.begin(), renderDevice.meshes.end(), [](const auto& mesh) {
            return mesh.layer == Engine::Render::MeshLayer::ViewModel;
        }) == 5, "each equipped pose submits all five model batches");
    }

    snapshot.screen = GameScreen::MainMenu;
    snapshot.fadeOpacity = 0.0F;
    snapshot.activeStage.reset();
    snapshot.player.reset();
    Engine::Ui::UiDrawList menuUi;
    context.Expect(
        ui.Compose(snapshot, display, {1280.0F, 720.0F}, menuUi, error) &&
            PreparePresentation(presentation, renderDevice, snapshot, display, menuUi, error),
        "authored MainMenu composes and presents through the same renderer");
    context.Expect(renderDevice.meshes.empty() && !renderDevice.camera && !renderDevice.viewmodelCamera,
        "MainMenu clears all world/viewmodel meshes and cameras");
    std::size_t drawableIndex = 0, emptyTextCount = 0;
    for (const auto& command : menuUi.commands) {
        std::visit([&](const auto& draw) {
            if constexpr (requires { draw.utf8; }) {
                if (draw.utf8.empty()) { ++emptyTextCount; return; }
            }
            context.Expect(drawableIndex < renderDevice.sprites.size(),
                "each drawable MainMenu command has its corresponding sprite");
            if (drawableIndex >= renderDevice.sprites.size()) return;
            const auto& sprite = renderDevice.sprites[drawableIndex++];
            if constexpr (requires { draw.utf8; }) {
                // The fake rasterizer returns a 2x2 bitmap. Every authored menu
                // caption has separate bounds, so this also detects reordering.
                context.Expect(sprite.material.texture.IsValid() &&
                    NearlyEqual(sprite.destinationPixels.width, 2) && NearlyEqual(sprite.destinationPixels.height, 2) &&
                    sprite.destinationPixels.x >= draw.boundsPixels.x &&
                    sprite.destinationPixels.y >= draw.boundsPixels.y &&
                    sprite.destinationPixels.x + 2 <= draw.boundsPixels.x + draw.boundsPixels.width &&
                    sprite.destinationPixels.y + 2 <= draw.boundsPixels.y + draw.boundsPixels.height,
                    "MainMenu captions remain in their corresponding ordered command bounds");
            } else {
                context.Expect(NearlyEqual(sprite.destinationPixels.x, draw.destinationPixels.x) &&
                    NearlyEqual(sprite.destinationPixels.y, draw.destinationPixels.y) &&
                    NearlyEqual(sprite.destinationPixels.width, draw.destinationPixels.width) &&
                    NearlyEqual(sprite.destinationPixels.height, draw.destinationPixels.height),
                    "MainMenu rectangles retain drawable command order");
            }
        }, command);
    }
    context.Expect(renderDevice.sprites.size() == drawableIndex &&
        drawableIndex + emptyTextCount == menuUi.commands.size(),
        "MainMenu submits exactly its drawable commands and omits empty text sprites");
    for (const Engine::Render::SpriteSubmission& sprite : renderDevice.sprites) {
        context.Expect(
            sprite.layer == Engine::Render::CompositeLayer::Overlay,
            "every authored MainMenu sprite is Overlay");
    }

    ObjectFpsPresentationConfig missingDoor;
    missingDoor.doorTexture = Engine::Asset::AssetId::FromString("missing.door.texture");
    context.Expect(!presentation.Initialize(renderDevice, renderer, rasterizer, assets.assets,
        content, missingDoor, error) && !error.empty(),
        "missing configured door asset fails initialization with a diagnostic");
}

} // namespace

void RunObjectFpsPresentationTests(TestContext& context) {
    TestModelDerivedMuzzle(context);
    TestSharedPlacementAndDefinitionErrors(context);
    TestExternalAnimationSetAndIndependentInstances(context);
    TestSceneAndOverlaySubmissionOrder(context);
}

} // namespace fps::tests
