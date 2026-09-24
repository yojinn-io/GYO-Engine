#pragma once

#include "engine/runtime/IRuntimeClient.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace Engine::Platform::Sdl { class SdlPlatform; }
namespace Engine::Render {
class Renderer;
namespace Backend::SdlGpu { class SdlGpuRenderDevice; }
}

namespace fps::pvp {
class ClientConnection;

struct PvpApplicationOptions final {
    std::string title{"Object_FPS PVP"};
    std::string gpuDriver{"auto"};
    std::string gateway{"127.0.0.1:8080"};
    int width{1280};
    int height{720};
};

// Product composition for the network client. It owns presentation and local
// input sampling, never an authoritative or predicted gameplay world.
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
    [[nodiscard]] const std::string& LastError() const noexcept;
    [[nodiscard]] int ExitCode() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace fps::pvp
