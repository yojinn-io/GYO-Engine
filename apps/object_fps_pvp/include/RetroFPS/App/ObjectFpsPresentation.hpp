#pragma once

#include "RetroFPS/App/ObjectFpsUi.hpp"
#include "RetroFPS/Game/CampaignContent.hpp"
#include "RetroFPS/Game/GameSession.hpp"
#include "RetroFPS/World/WorldSettings.hpp"

#include "engine/asset/AssetId.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace Engine::Asset {
class AssetManager;
}

namespace Engine::Render {
class IRenderDevice;
class Renderer;
class RenderQueue;
}

namespace Engine::Text {
class ITextRasterizer;
}

namespace fps {

struct ObjectFpsPresentationConfig final {
    Engine::Asset::AssetId floorTexture{
        Engine::Asset::AssetId::FromString("object_fps_pvp.texture.world.floor")};
    Engine::Asset::AssetId wallTexture{
        Engine::Asset::AssetId::FromString("object_fps_pvp.texture.world.wall")};
    Engine::Asset::AssetId doorTexture{
        Engine::Asset::AssetId::FromString("common.texture.white")};
    Engine::Asset::AssetId skyTexture{
        Engine::Asset::AssetId::FromString("object_fps_pvp.texture.sky.default")};
    WorldSettings world{};
    float viewportWidth{1280.0F};
    float viewportHeight{720.0F};
};

// App-side projection from an immutable game snapshot to GYO's RenderQueue.
// It owns only the GYO asset handles and GPU handles required by this game;
// simulation and backend implementation remain outside this class.
class ObjectFpsPresentation final {
public:
    ObjectFpsPresentation() noexcept;
    ~ObjectFpsPresentation();

    ObjectFpsPresentation(const ObjectFpsPresentation&) = delete;
    ObjectFpsPresentation& operator=(const ObjectFpsPresentation&) = delete;
    ObjectFpsPresentation(ObjectFpsPresentation&&) noexcept;
    ObjectFpsPresentation& operator=(ObjectFpsPresentation&&) noexcept;

    [[nodiscard]] bool Initialize(
        Engine::Render::IRenderDevice& renderDevice,
        Engine::Render::Renderer& renderer,
        Engine::Text::ITextRasterizer& textRasterizer,
        Engine::Asset::AssetManager& assets,
        std::shared_ptr<const CampaignContent> content,
        const ObjectFpsPresentationConfig& config,
        std::string& error);

    [[nodiscard]] bool Present(
        const GameSessionSnapshot& snapshot,
        const ObjectFpsDisplaySettings& displaySettings,
        const Engine::Ui::UiDrawList& uiDrawList,
        std::string& error);

    // Prepare the CPU submission independently of frame execution, also used
    // by deterministic headless conformance tests and capture diagnostics.
    [[nodiscard]] bool PrepareFrame(
        const GameSessionSnapshot& snapshot,
        const ObjectFpsDisplaySettings& displaySettings,
        const Engine::Ui::UiDrawList& uiDrawList,
        std::string& error);
    [[nodiscard]] const Engine::Render::RenderQueue& PreparedQueue() const noexcept;

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] std::size_t LastVisibleSubmissionCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps
