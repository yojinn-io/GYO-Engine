#pragma once

#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/Game/GameSession.hpp"

#include "engine/runtime/IRuntimeClient.hpp"
#include "engine/runtime/IRuntimePort.hpp"

#include <memory>
#include <span>
#include <string>

namespace Engine::Asset {
class AssetManager;
}

namespace Engine::Input::Backend::Sdl {
class SdlInput;
}

namespace Engine::Platform::Sdl {
class SdlPlatform;
}

namespace fps {

struct ObjectFpsRuntimeClientConfig final {
    Engine::Asset::AssetId uiDocument{
        Engine::Asset::AssetId::FromString("object_fps.ui.screens")};
    float viewportWidth{1280.0F};
    float viewportHeight{720.0F};
};

// The executable's GYO client and caller-neutral typed runtime port. It maps
// GYO InputActionFrame values into game policy and projects the resulting
// snapshot through GYO presentation mechanisms; it is not an engine facade.
class ObjectFpsRuntimeClient final
    : public Engine::Runtime::IRuntimeClient,
      public Engine::Runtime::IRuntimePort<
          GameSessionSnapshot,
          GameSessionCommand,
          GameSessionEvent> {
public:
    ObjectFpsRuntimeClient(
        Engine::Platform::Sdl::SdlPlatform& platform,
        Engine::Input::Backend::Sdl::SdlInput& input,
        Engine::Asset::AssetManager& assets,
        ObjectFpsPresentation& presentation,
        ObjectFpsRuntimeClientConfig config = {}) noexcept;
    ~ObjectFpsRuntimeClient() override;

    ObjectFpsRuntimeClient(const ObjectFpsRuntimeClient&) = delete;
    ObjectFpsRuntimeClient& operator=(const ObjectFpsRuntimeClient&) = delete;

    [[nodiscard]] bool Initialize(
        std::shared_ptr<const CampaignContent> content,
        const GameSessionConfig& config,
        std::string& error);

    [[nodiscard]] Engine::Runtime::RuntimeControl ProcessEvents(
        const Engine::Runtime::FrameContext& frame) override;
    [[nodiscard]] Engine::Runtime::RuntimeControl Update(
        const Engine::Runtime::FrameContext& frame) override;
    [[nodiscard]] Engine::Runtime::RuntimeControl Render(
        const Engine::Runtime::FrameContext& frame) override;

    [[nodiscard]] const GameSessionSnapshot& Query() const noexcept override;
    void Submit(GameSessionCommand command) override;
    [[nodiscard]] std::span<const GameSessionEvent> Events() const noexcept override;

    [[nodiscard]] const std::string& LastError() const noexcept;
    [[nodiscard]] int ExitCode() const noexcept;
    [[nodiscard]] const ObjectFpsDisplaySettings& DisplaySettings() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps
