#pragma once

#include "engine/runtime/IRuntimeClient.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace Engine::Platform::Sdl { class SdlPlatform; }
namespace Engine::Render {
class Renderer;
namespace Backend::SdlGpu { class SdlGpuRenderDevice; }
}

namespace fps::pvp {
class ClientConnection;
struct LocalMovementObservation;
struct RemoteMovementObservation final {
    PlayerId playerId{};
    Float3 renderPosition{};
    std::uint64_t movementEpoch{1}, lowerTick{}, upperTick{};
    double presentationTick{}, interpolationAlpha{};
    float yaw{}, pitch{};
    double latestReceiveAgeSeconds{}, holdSeconds{}, totalHoldSeconds{};
    std::size_t historySize{};
    std::uint64_t holdCount{}, gapCount{}, historyEvictions{}, ingressHistoryDrops{}, phaseReanchors{};
    bool holding{};
    std::uint64_t lowerResolvedCommand{}, upperResolvedCommand{};
    bool missingFutureSnapshot{};
};

// A successful renderer submission, not a monitor scanout timestamp. Cleared
// before each Render call so skipped frames cannot repeat an old observation.
struct PresentedMovementObservation final {
    std::uint64_t frameId{};
    double hostSteadySeconds{};
    PlayerId localPlayerId{};
    LocalMovementObservation local;
    std::optional<RemoteMovementObservation> remote;
    std::uint64_t skippedFrames{};
    std::uint64_t connectionGeneration{};
};

struct PvpApplicationOptions final {
    std::string title{"Object_FPS PVP"};
    std::string gpuDriver{"auto"};
    std::string gateway{"127.0.0.1:8080"};
    int width{1280};
    int height{720};
    bool vsync{true};
};

// Product composition owns input sampling, local movement prediction and
// presentation. The remote Match retains authority over the world.
class PvpApplication final : public Engine::Runtime::IRuntimeClient {
public:
    PvpApplication();
    ~PvpApplication() override;
    PvpApplication(const PvpApplication&) = delete;
    PvpApplication& operator=(const PvpApplication&) = delete;

    [[nodiscard]] bool InitializeContent(const std::filesystem::path& assetRoot, std::string& error);
    [[nodiscard]] bool InitializeGraphics(const PvpApplicationOptions& options, std::string& error);
    [[nodiscard]] int Run();
    [[nodiscard]] Engine::Runtime::RuntimeControl ProcessEvents(const Engine::Runtime::FrameContext& frame) override;
    [[nodiscard]] Engine::Runtime::RuntimeControl Update(const Engine::Runtime::FrameContext& frame) override;
    [[nodiscard]] Engine::Runtime::RuntimeControl Render(const Engine::Runtime::FrameContext& frame) override;

    void SetGatewayAddress(std::string address);
    [[nodiscard]] ClientConnection& Connection();
    [[nodiscard]] Engine::Render::Renderer& Renderer();
    [[nodiscard]] Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice& RenderDevice();
    [[nodiscard]] Engine::Platform::Sdl::SdlPlatform& Platform();
    [[nodiscard]] const LocalMovementObservation& LocalMovement() const noexcept;
    [[nodiscard]] const std::optional<RemoteMovementObservation>& RemoteMovement() const noexcept;
    [[nodiscard]] const std::optional<PresentedMovementObservation>& PresentedMovement() const noexcept;
    [[nodiscard]] std::uint64_t SkippedPresentationFrames() const noexcept;
    [[nodiscard]] const std::string& LastError() const noexcept;
    [[nodiscard]] int ExitCode() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace fps::pvp
