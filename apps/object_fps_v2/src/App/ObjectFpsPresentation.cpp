#include "RetroFPS/App/ObjectFpsPresentation.hpp"

#include "RetroFPS/App/ObjectFpsUi.hpp"
#include "RetroFPS/App/WeaponViewModel.hpp"
#include "RetroFPS/App/EnemyPresentation.hpp"
#include "RetroFPS/Rendering/MapGeometryGenerator.hpp"

#include "engine/asset/AssetHandle.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "render/IRenderDevice.hpp"
#include "render/PrimitiveMesh.hpp"
#include "render/RenderQueue.hpp"
#include "render/Renderer.hpp"
#include "ui/UiRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fps {
namespace {

[[nodiscard]] std::string AssetName(const Engine::Asset::AssetId& id) {
    return id.debugName.empty() ? std::to_string(id.value) : id.debugName;
}

[[nodiscard]] Engine::Render::Transform3D ConvertTransform(
    const SurfaceTransform& source) noexcept {
    return {
        {source.translation.x, source.translation.y, source.translation.z},
        {
            source.rotationRadians.x,
            source.rotationRadians.y,
            source.rotationRadians.z,
        },
        {source.scale.x, source.scale.y, source.scale.z},
    };
}

} // namespace

struct ObjectFpsPresentation::Impl final {
    struct TextureResource final {
        Engine::Asset::AssetHandle asset;
        Engine::Render::TextureHandle gpu;
        std::uint32_t width{};
        std::uint32_t height{};
    };

    Engine::Render::IRenderDevice* renderDevice{};
    Engine::Render::Renderer* renderer{};
    Engine::Asset::AssetManager* assets{};
    std::shared_ptr<const CampaignContent> content;
    ObjectFpsPresentationConfig config;
    Engine::Render::MeshHandle quadXy;
    Engine::Render::MeshHandle quadXz;
    Engine::Render::MeshHandle cube;
    Engine::Render::MeshHandle skySphere;
    std::unordered_map<Engine::Asset::AssetId, TextureResource> textures;
    std::unordered_map<Engine::Asset::AssetId, std::unique_ptr<WeaponViewModel>> weapons;
    EnemyPresentation enemies;
    std::vector<MapGeometry> stageGeometry;
    Engine::Render::RenderQueue queue;
    Engine::Ui::UiRenderer uiRenderer;
    std::size_t lastVisibleSubmissionCount{};
    bool initialized{};

    ~Impl() { Reset(); }

    void Reset() noexcept {
        uiRenderer.Reset();
        queue.Reset();
        weapons.clear();
        enemies.Reset();
        if (renderDevice != nullptr) {
            for (const auto& [id, texture] : textures) {
                static_cast<void>(id);
                if (texture.gpu.IsValid()) {
                    static_cast<void>(renderDevice->ReleaseTexture(texture.gpu));
                }
            }
            if (quadXy.IsValid()) {
                static_cast<void>(renderDevice->ReleaseMesh(quadXy));
            }
            if (quadXz.IsValid()) {
                static_cast<void>(renderDevice->ReleaseMesh(quadXz));
            }
            if (cube.IsValid()) {
                static_cast<void>(renderDevice->ReleaseMesh(cube));
            }
            if (skySphere.IsValid()) {
                static_cast<void>(renderDevice->ReleaseMesh(skySphere));
            }
        }
        if (assets != nullptr) {
            for (const auto& [id, texture] : textures) {
                static_cast<void>(id);
                assets->Release(texture.asset);
            }
        }
        textures.clear();
        stageGeometry.clear();
        quadXy = {};
        quadXz = {};
        cube = {};
        skySphere = {};
        content.reset();
        assets = nullptr;
        renderDevice = nullptr;
        renderer = nullptr;
        lastVisibleSubmissionCount = 0;
        initialized = false;
    }

    [[nodiscard]] bool CreateMesh(
        const Engine::Render::MeshData& data,
        Engine::Render::MeshHandle& destination,
        const char* name,
        std::string& error) {
        auto result = renderDevice->CreateMesh(data.View());
        if (!result) {
            error = std::string("failed to create GYO ") + name + " mesh: " +
                    result.error().message;
            return false;
        }
        destination = result.value();
        return true;
    }

    [[nodiscard]] bool LoadTexture(
        const Engine::Asset::AssetId& id,
        std::string& error) {
        if (!id.IsValid() || textures.contains(id)) {
            return id.IsValid();
        }

        const auto load = assets->Load(
            id,
            Engine::Asset::AssetRequest::WithTypeHint(
                Engine::Asset::AssetType::Texture()));
        if (!load) {
            error = "failed to load texture asset '" + AssetName(id) + "': " +
                    load.error().message;
            return false;
        }

        const Engine::Asset::AssetHandle assetHandle = load.value();
        const auto texture =
            assets->GetSharedConst<Engine::Asset::Loaders::TextureAsset>(assetHandle);
        if (!texture || texture->width == 0 || texture->height == 0 ||
            texture->rgba.size() !=
                static_cast<std::size_t>(texture->width) * texture->height * 4U) {
            assets->Release(assetHandle);
            error = "texture asset '" + AssetName(id) +
                    "' has no valid decoded RGBA payload";
            return false;
        }

        const std::span<const std::uint8_t> rgba(texture->rgba);
        const Engine::Render::ImageView image{
            texture->width,
            texture->height,
            texture->width * 4U,
            std::as_bytes(rgba),
            Engine::Render::TextureColorSpace::SRgb,
        };
        auto create = renderDevice->CreateTexture(image);
        if (!create) {
            assets->Release(assetHandle);
            error = "failed to upload texture asset '" + AssetName(id) + "': " +
                    create.error().message;
            return false;
        }

        textures.emplace(
            id,
            TextureResource{assetHandle, create.value(), texture->width, texture->height});
        return true;
    }

    [[nodiscard]] const TextureResource* FindTexture(
        const Engine::Asset::AssetId& id) const noexcept {
        const auto found = textures.find(id);
        return found == textures.end() ? nullptr : &found->second;
    }

    [[nodiscard]] bool Submit(
        const Engine::Render::MeshSubmission& submission,
        std::string& error) {
        const auto result = queue.Submit(submission);
        if (!result) {
            error = "Object_FPS produced an invalid GYO mesh submission: " +
                    result.error().message;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool Submit(
        const Engine::Render::SpriteSubmission& submission,
        std::string& error) {
        const auto result = queue.Submit(submission);
        if (!result) {
            error = "Object_FPS produced an invalid GYO sprite submission: " +
                    result.error().message;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool SubmitWorld(
        const GameSessionSnapshot& snapshot,
        std::string& error) {
        if (!snapshot.activeStage || !snapshot.player) {
            return true;
        }

        const CampaignStageContent* stage =
            content->FindStage(snapshot.activeStage->levelId);
        if (stage == nullptr || snapshot.activeStage->ordinal >= stageGeometry.size()) {
            error = "snapshot references campaign stage content that is not loaded";
            return false;
        }

        const TextureResource* floor = FindTexture(config.floorTexture);
        const TextureResource* wall = FindTexture(config.wallTexture);
        const TextureResource* door = FindTexture(config.doorTexture);
        if (floor == nullptr || wall == nullptr || door == nullptr) {
            error = "Object_FPS world textures are not initialized";
            return false;
        }

        const PlayerSnapshot& player = *snapshot.player;
        queue.SetCamera({
            {player.position.x, player.feetY + player.eyeHeight, player.position.z},
            {player.pitchRadians, player.yawRadians, 0.0F},
            snapshot.worldVerticalFovRadians,
            0.05F,
            100.0F,
        });

        const TextureResource* sky = FindTexture(config.skyTexture);
        if (sky != nullptr) {
            Engine::Render::MeshSubmission submission;
            submission.mesh = skySphere;
            submission.material.texture = sky->gpu;
            submission.material.sampler = Engine::Render::SamplerMode::LinearWrap;
            submission.transform = {{player.position.x, player.feetY + player.eyeHeight, player.position.z}, {}, {60,60,60}};
            submission.surface = Engine::Render::SurfaceMode::Sky;
            submission.doubleSided = true;
            if (!Submit(submission, error)) return false;
        }

        const MapGeometry& geometry = stageGeometry[snapshot.activeStage->ordinal];
        for (const SurfaceInstance& surface : geometry.surfaces) {
            Engine::Render::MeshSubmission submission;
            submission.transform = ConvertTransform(surface.transform);
            submission.material.sampler = Engine::Render::SamplerMode::LinearWrap;
            submission.doubleSided = true;
            switch (surface.type) {
            case SurfaceType::Floor:
                submission.mesh = quadXz;
                submission.material.texture = floor->gpu;
                break;
            case SurfaceType::Wall:
                submission.mesh = quadXy;
                submission.material.texture = wall->gpu;
                break;
            case SurfaceType::Door:
                if (!snapshot.activeStage->doorVisible) {
                    continue;
                }
                submission.mesh = cube;
                submission.material.texture = door->gpu;
                break;
            }
            if (!Submit(submission, error)) {
                return false;
            }
        }

        for (const ProjectileSnapshot& projectile : snapshot.projectiles) {
            const float diameter = projectile.radius * 2.0F;
            Engine::Render::MeshSubmission submission;
            submission.mesh = cube;
            submission.transform = {
                    {projectile.position.x, projectile.position.y, projectile.position.z},
                    {},
                    {diameter, diameter, diameter},
                };
            submission.material.tint = projectile.kind == ProjectileKind::EnemyBullet
                    ? Engine::Render::Color{1.0F, 0.25F, 0.1F, 1.0F}
                    : Engine::Render::Color{1.0F, 0.9F, 0.2F, 1.0F};
            if (!Submit(submission, error)) return false;
        }

        const WeaponDefinition* weapon =
            content->Data().weapons.FindById(snapshot.weaponPresentation.weaponId);
        if (weapon != nullptr) {
            const auto found = weapons.find(weapon->presentationAssetId);
            if (found == weapons.end()) {
                error = "weapon presentation asset was not initialized";
                return false;
            }
            if (!found->second->Submit(snapshot.weaponPresentation, queue, error)) return false;
        }
        return true;
    }
};

ObjectFpsPresentation::ObjectFpsPresentation() noexcept
    : impl_(std::make_unique<Impl>()) {}

ObjectFpsPresentation::~ObjectFpsPresentation() = default;
ObjectFpsPresentation::ObjectFpsPresentation(ObjectFpsPresentation&&) noexcept = default;
ObjectFpsPresentation& ObjectFpsPresentation::operator=(
    ObjectFpsPresentation&&) noexcept = default;

bool ObjectFpsPresentation::Initialize(
    Engine::Render::IRenderDevice& renderDevice,
    Engine::Render::Renderer& renderer,
    Engine::Text::ITextRasterizer& textRasterizer,
    Engine::Asset::AssetManager& assets,
    std::shared_ptr<const CampaignContent> content,
    const ObjectFpsPresentationConfig& config,
    std::string& error) {
    error.clear();
    impl_->Reset();
    if (!content || content->Stages().empty() || config.viewportWidth <= 0.0F ||
        config.viewportHeight <= 0.0F ||
        !std::isfinite(config.world.cellSize) || config.world.cellSize <= 0.0F ||
        !std::isfinite(config.world.wallHeight) || config.world.wallHeight <= 0.0F) {
        error = "ObjectFpsPresentation requires content, a positive viewport, and valid world scale";
        return false;
    }
    impl_->renderDevice = &renderDevice;
    impl_->renderer = &renderer;
    impl_->assets = &assets;
    impl_->content = std::move(content);
    impl_->config = config;

    auto uiInitialized = impl_->uiRenderer.Initialize(
        renderDevice,
        textRasterizer,
        assets);
    if (!uiInitialized) {
        error = "failed to initialize GYO UI renderer: " +
                uiInitialized.error().message;
        impl_->Reset();
        return false;
    }

    const auto sphere = Engine::Render::MakeUvSphere(16, 32);
    if (!sphere) {
        error = "failed to generate Object_FPS sky sphere: " +
                sphere.error().message;
        impl_->Reset();
        return false;
    }
    if (!impl_->CreateMesh(
            Engine::Render::MakeUnitQuadXY(), impl_->quadXy, "XY quad", error) ||
        !impl_->CreateMesh(
            Engine::Render::MakeUnitQuadXZ(), impl_->quadXz, "XZ quad", error) ||
        !impl_->CreateMesh(
            Engine::Render::MakeUnitCube(), impl_->cube, "unit cube", error) ||
        !impl_->CreateMesh(
            sphere.value(), impl_->skySphere, "sky sphere", error)) {
        impl_->Reset();
        return false;
    }

    std::vector<Engine::Asset::AssetId> textureIds{
        config.floorTexture,
        config.wallTexture,
        config.doorTexture,
        config.skyTexture,
    };
    for (const Engine::Asset::AssetId& id : textureIds) {
        if (!impl_->LoadTexture(id, error)) {
            impl_->Reset();
            return false;
        }
    }

    for (const WeaponDefinition& weapon : impl_->content->Data().weapons.GetDefinitions()) {
        if (impl_->weapons.contains(weapon.presentationAssetId)) continue;
        auto viewmodel = std::make_unique<WeaponViewModel>();
        if (!viewmodel->Initialize(renderDevice, assets, weapon.presentationAssetId, error)) {
            impl_->Reset();
            return false;
        }
        impl_->weapons.emplace(weapon.presentationAssetId, std::move(viewmodel));
    }

    try {
        impl_->stageGeometry.reserve(impl_->content->Stages().size());
        for (const CampaignStageContent& stage : impl_->content->Stages()) {
            impl_->stageGeometry.push_back(
                MapGeometryGenerator::Generate(stage.map, impl_->config.world));
        }
    } catch (const std::exception& exception) {
        error = "failed to generate Object_FPS map presentation: ";
        error += exception.what();
        impl_->Reset();
        return false;
    }
    if (!impl_->enemies.Initialize(renderDevice, assets, impl_->content->Data().enemies, error)) { impl_->Reset(); return false; }
    impl_->initialized = true;
    return true;
}

bool ObjectFpsPresentation::PrepareFrame(
    const GameSessionSnapshot& snapshot,
    const ObjectFpsDisplaySettings& displaySettings,
    const Engine::Ui::UiDrawList& uiDrawList,
    std::string& error) {
    error.clear();
    impl_->lastVisibleSubmissionCount = 0;
    if (!impl_->initialized) {
        error = "ObjectFpsPresentation is not initialized";
        return false;
    }

    Engine::Render::FrameDescription frame;
    switch (snapshot.screen) {
    case GameScreen::MainMenu:
        frame.clearColor = {0.03F, 0.04F, 0.07F, 1.0F};
        break;
    case GameScreen::Controls:
        frame.clearColor = {0.05F, 0.05F, 0.08F, 1.0F};
        break;
    case GameScreen::Paused:
        frame.clearColor = {0.02F, 0.02F, 0.02F, 1.0F};
        break;
    case GameScreen::Results:
        frame.clearColor = snapshot.campaignOutcome == CampaignOutcome::Completed
            ? Engine::Render::Color{0.02F, 0.10F, 0.04F, 1.0F}
            : Engine::Render::Color{0.10F, 0.02F, 0.02F, 1.0F};
        break;
    case GameScreen::Playing:
        frame.clearColor = {0.05F, 0.07F, 0.10F, 1.0F};
        break;
    }
    frame.sceneColorTransform = {
        displaySettings.exposureEv,
        displaySettings.gammaAdjustment,
    };
    impl_->queue.Reset(frame);
    if ((snapshot.screen == GameScreen::Playing ||
         snapshot.screen == GameScreen::Paused) &&
        !impl_->SubmitWorld(snapshot, error)) {
        return false;
    }

    if (!impl_->enemies.Submit(snapshot, impl_->queue, displaySettings.showCollisionVolumes, error)) return false;

    auto uiSubmitted = impl_->uiRenderer.Submit(uiDrawList, impl_->queue);
    if (!uiSubmitted) {
        error = "GYO UI renderer rejected Object_FPS draw data: " +
                uiSubmitted.error().message;
        return false;
    }

    if (snapshot.fadeOpacity > 0.0F) {
        Engine::Render::SpriteSubmission fade;
        fade.destinationPixels = {
            0.0F,
            0.0F,
            impl_->config.viewportWidth,
            impl_->config.viewportHeight,
        };
        fade.material.tint = {
            0.0F,
            0.0F,
            0.0F,
            std::clamp(snapshot.fadeOpacity, 0.0F, 1.0F),
        };
        if (!impl_->Submit(fade, error)) {
            return false;
        }
    }

    impl_->lastVisibleSubmissionCount =
        impl_->queue.Meshes().size() + impl_->queue.Sprites().size();

    return true;
}

const Engine::Render::RenderQueue& ObjectFpsPresentation::PreparedQueue() const noexcept {
    return impl_->queue;
}

bool ObjectFpsPresentation::Present(
    const GameSessionSnapshot& snapshot,
    const ObjectFpsDisplaySettings& displaySettings,
    const Engine::Ui::UiDrawList& uiDrawList,
    std::string& error) {
    if (!PrepareFrame(snapshot, displaySettings, uiDrawList, error)) return false;
    auto result = impl_->renderer->Render(impl_->queue);
    if (!result) {
        error = "GYO render device failed to present Object_FPS: " +
                result.error().message;
        return false;
    }
    return true;
}

bool ObjectFpsPresentation::IsInitialized() const noexcept {
    return impl_->initialized;
}

std::size_t ObjectFpsPresentation::LastVisibleSubmissionCount() const noexcept {
    return impl_->lastVisibleSubmissionCount;
}

} // namespace fps
