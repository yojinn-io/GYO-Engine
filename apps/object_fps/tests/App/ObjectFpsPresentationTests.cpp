#include "../TestSupport.hpp"

#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/App/ObjectFpsUi.hpp"
#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Game/CampaignContent.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextureLoader.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/IAssetSource.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "render/IRenderDevice.hpp"
#include "render/RenderQueue.hpp"
#include "text/ITextRasterizer.hpp"
#include "ui/UiDocumentCodec.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
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
    "weapon_id,damage,magazine_size,reserve_ammo,recoil,automatic,fire_interval_seconds,reload_seconds,texture_asset_id\n"
    "starter_pistol,25,12,48,1.5,false,0.2,1.5,object_fps.texture.weapon.starter_pistol\n";

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
    Engine::Base::Result<std::vector<std::byte>, Engine::Asset::AssetError>
    ReadAll(std::string_view) override {
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

class CapturingRenderDevice final : public Engine::Render::IRenderDevice {
public:
    Engine::Render::FrameDescription frame;
    std::vector<Engine::Render::MeshSubmission> meshes;
    std::vector<Engine::Render::SpriteSubmission> sprites;

    Engine::Base::Result<Engine::Render::MeshHandle, Engine::Render::RenderError>
    CreateMesh(const Engine::Render::MeshView&) override {
        return Engine::Base::Result<
            Engine::Render::MeshHandle, Engine::Render::RenderError>::Ok(
                Engine::Render::MeshHandle::FromParts(++meshSerial_, 1));
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

    Engine::Base::Result<
        Engine::Render::PresentStatus, Engine::Render::RenderError>
    Render(const Engine::Render::RenderQueue& queue) override {
        frame = queue.Frame();
        meshes.assign(queue.Meshes().begin(), queue.Meshes().end());
        sprites.assign(queue.Sprites().begin(), queue.Sprites().end());
        return Engine::Base::Result<
            Engine::Render::PresentStatus, Engine::Render::RenderError>::Ok(
                Engine::Render::PresentStatus::Presented);
    }

private:
    std::uint32_t meshSerial_{};
    std::uint32_t textureSerial_{};
};

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

        const std::filesystem::path assetRoot{RETROFPS_TEST_RESOURCE_ROOT};
        Engine::Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = assetRoot.string();
        Engine::Asset::Resolver::AssetPathResolver resolver(std::move(options));
        Engine::Asset::Catalog::CatalogParser parser;
        auto loaded = catalog.LoadFromFile(
            (assetRoot / "asset_catalog.json").string(), parser, resolver);
        if (!loaded) {
            error = loaded.error().message;
            return false;
        }
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
        CampaignContent::Build(std::move(*data.catalog), std::move(maps));
    context.Expect(content.Succeeded(), "presentation fixture content builds");
    if (!content.content.has_value()) return {};
    return std::make_shared<CampaignContent>(std::move(*content.content));
}

[[nodiscard]] std::shared_ptr<const Engine::Ui::UiDocument> LoadUiDocument(
    TestContext& context) {
    const std::filesystem::path path =
        std::filesystem::path{RETROFPS_TEST_RESOURCE_ROOT} / "ui" / "screens.json";
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(buffer.str(), path.string());
    context.Expect(static_cast<bool>(parsed), "presentation fixture UI JSON parses");
    if (!parsed) return {};
    return std::make_shared<const Engine::Ui::UiDocument>(
        std::move(parsed).value());
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
    FakeTextRasterizer rasterizer;
    ObjectFpsPresentation presentation;
    context.Expect(
        presentation.Initialize(
            renderDevice, rasterizer, assets.assets, content, {}, error),
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
    snapshot.weapon.weaponId = "starter_pistol";
    snapshot.weapon.magazineAmmo = 7;
    snapshot.weapon.reserveAmmo = 35;

    Engine::Ui::UiDrawList pausedUi;
    const ObjectFpsDisplaySettings display{0.5F, 1.1F};
    context.Expect(
        ui.Compose(snapshot, display, {1280.0F, 720.0F}, pausedUi, error),
        "paused HUD and authored menu compose into one ordered draw list");
    context.Expect(
        presentation.Present(snapshot, display, pausedUi, error),
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
        renderDevice.sprites.size() == pausedUi.commands.size() + 2U,
        "weapon, ordered UI commands, and final fade each produce one sprite");
    if (renderDevice.sprites.size() != pausedUi.commands.size() + 2U) return;

    const Engine::Render::SpriteSubmission& weapon = renderDevice.sprites.front();
    context.Expect(
        weapon.layer == Engine::Render::CompositeLayer::Scene &&
            weapon.texture.IsValid() && weapon.sourceUv.width == 1.0F &&
            weapon.sourceUv.height == 1.0F,
        "weapon sprite is a full-UV Scene submission");
    for (std::size_t index = 1; index < renderDevice.sprites.size(); ++index) {
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
            NearlyEqual(fade.tint.alpha, snapshot.fadeOpacity),
        "fade is the final full-viewport Overlay submission");

    snapshot.screen = GameScreen::MainMenu;
    snapshot.fadeOpacity = 0.0F;
    snapshot.activeStage.reset();
    snapshot.player.reset();
    Engine::Ui::UiDrawList menuUi;
    context.Expect(
        ui.Compose(snapshot, display, {1280.0F, 720.0F}, menuUi, error) &&
            presentation.Present(snapshot, display, menuUi, error),
        "authored MainMenu composes and presents through the same renderer");
    context.Expect(
        renderDevice.meshes.empty() &&
            renderDevice.sprites.size() == menuUi.commands.size(),
        "MainMenu has no Scene submissions and preserves UI draw order");
    for (const Engine::Render::SpriteSubmission& sprite : renderDevice.sprites) {
        context.Expect(
            sprite.layer == Engine::Render::CompositeLayer::Overlay,
            "every authored MainMenu sprite is Overlay");
    }
}

} // namespace

void RunObjectFpsPresentationTests(TestContext& context) {
    TestSceneAndOverlaySubmissionOrder(context);
}

} // namespace fps::tests
