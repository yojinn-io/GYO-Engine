#include <string_view>
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "engine/runtime/RuntimeLoop.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/backend/sdl/SdlRenderer.hpp"

namespace {

using Engine::Platform::Sdl::SdlPlatform;
using Engine::Render::Backend::Sdl::Color;
using Engine::Render::Backend::Sdl::SdlRenderer;
using Engine::Runtime::FrameContext;
using Engine::Runtime::IRuntimeClient;
using Engine::Runtime::RuntimeControl;

bool HasSmokeTestArgument(int argc, char* argv[]) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--smoke-test") {
            return true;
        }
    }
    return false;
}

template <typename Error>
void LogError(const Error& error) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "%s%s%s",
        error.message.c_str(),
        error.detail.empty() ? "" : ": ",
        error.detail.c_str());
}

class RuntimeClient final : public IRuntimeClient {
public:
    RuntimeClient(
        SdlPlatform& platform,
        SdlRenderer& renderer,
        bool smokeTest) noexcept
        : platform_(platform), renderer_(renderer), smokeTest_(smokeTest) {}

    RuntimeControl ProcessEvents(const FrameContext&) override {
        return platform_.PumpEvents();
    }

    RuntimeControl Update(const FrameContext&) override {
        return RuntimeControl::Continue;
    }

    RuntimeControl Render(const FrameContext& frame) override {
        auto clearResult = renderer_.Clear(Color{20, 20, 22, 255});
        if (!clearResult) {
            LogError(clearResult.error());
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }

        auto presentResult = renderer_.Present();
        if (!presentResult) {
            LogError(presentResult.error());
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }

        if (smokeTest_ && frame.frameIndex == 0) {
            return RuntimeControl::Stop;
        }

        return RuntimeControl::Continue;
    }

    [[nodiscard]] int ExitCode() const noexcept {
        return exitCode_;
    }

private:
    SdlPlatform& platform_;
    SdlRenderer& renderer_;
    bool smokeTest_{};
    int exitCode_{};
};

} // namespace

int main(int argc, char* argv[]) {
    auto platformResult = SdlPlatform::Create();
    if (!platformResult) {
        LogError(platformResult.error());
        return 1;
    }
    auto platform = std::move(platformResult).value();

    auto rendererResult = SdlRenderer::Create(*platform);
    if (!rendererResult) {
        LogError(rendererResult.error());
        return 1;
    }
    auto renderer = std::move(rendererResult).value();

    RuntimeClient client(*platform, *renderer, HasSmokeTestArgument(argc, argv));
    Engine::Runtime::RuntimeLoop loop(client);
    loop.Run();

    return client.ExitCode();
}
