#include "RetroFPS/App/ObjectFpsApplication.hpp"
#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/App/ObjectFpsRuntimeClient.hpp"
#include "diagnostics/ObjectFpsDiagnostics.hpp"
#include "diagnostics/VisualProbe.hpp"
#include "gyo/AppConfig.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "render/ShaderLibrary.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <string>
#include <string_view>

namespace {
int Fail(const std::string& error, int code = 1) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.c_str());
    return code;
}

int RunPresentationSmoke(fps::ObjectFpsApplication& application, const bool menu) {
    using Engine::Runtime::RuntimeControl;
    auto& client = application.Client();
    if (!menu) client.Submit(fps::StartCampaignCommand{});
    for (std::uint64_t index = 0; index < 180; ++index) {
        const Engine::Runtime::FrameContext frame{index, 1.0 / 60.0};
        if (client.ProcessEvents(frame) == RuntimeControl::Stop ||
            client.Update(frame) == RuntimeControl::Stop ||
            client.Render(frame) == RuntimeControl::Stop) {
            return Fail(client.LastError().empty() ? "Presentation stopped before acceptance completed" : client.LastError());
        }
        const auto expected = menu ? fps::GameScreen::MainMenu : fps::GameScreen::Playing;
        if (client.Query().screen == expected) {
            if (application.Presentation().LastVisibleSubmissionCount() == 0)
                return Fail("Presentation produced no visible submissions");
            SDL_Log("%s presentation smoke passed", menu ? "MainMenu" : "Gameplay");
            return 0;
        }
    }
    return Fail("Presentation did not reach the required screen within 180 frames");
}
}

int main(int argc, char* argv[]) {
    fps::ObjectFpsApplicationOptions options;
    options.title = Gyo::AppConfig::DisplayName;
    std::filesystem::path capture;
    std::string mode;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            SDL_Log("Acceptance: --validate-package, --startup-smoke-test, --headless-smoke-test, "
                    "--shader-smoke-test, --smoke-test, --menu-smoke-test, --viewmodel-smoke-test, "
                    "--reload-smoke-test, --muzzle-smoke-test; --gpu-driver DRIVER, --capture-dir DIR, "
                    "--preview-4x3, --preview-21x9");
            return 0;
        }
        if (argument == "--gpu-driver" && index + 1 < argc) options.gpuDriver = argv[++index];
        else if (argument == "--capture-dir" && index + 1 < argc) capture = argv[++index];
        else if (argument == "--preview-4x3") options.width = 960;
        else if (argument == "--preview-21x9") options.width = 1680;
        else if (argument == "--validate-package" || argument == "--startup-smoke-test" ||
            argument == "--headless-smoke-test" || argument == "--shader-smoke-test" ||
            argument == "--smoke-test" || argument == "--menu-smoke-test" ||
            argument == "--viewmodel-smoke-test" || argument == "--reload-smoke-test" ||
            argument == "--muzzle-smoke-test") {
            if (!mode.empty()) return Fail("Select exactly one acceptance mode", 2);
            mode = argument;
        } else return Fail("Unknown or incomplete acceptance option: " + std::string(argument), 2);
    }
    if (mode.empty()) return Fail("An acceptance mode is required; use --help", 2);
    const char* base = SDL_GetBasePath();
    if (!base) return Fail("Cannot locate acceptance executable directory");
    fps::ObjectFpsApplication application;
    std::string error;
    if (!application.InitializeContent(std::filesystem::path(base) / Gyo::AppConfig::Assets, error)) return Fail(error);
    if (mode == "--validate-package") return ValidatePackage(application.Catalog(), application.Shaders());
    if (mode == "--startup-smoke-test") {
        SDL_Log("Startup smoke passed: stages=%zu, weapons=%zu, shader_programs=%zu, bundle=%s, window=none, gpu=none",
            application.Content()->Stages().size(), application.Content()->Data().weapons.GetDefinitions().size(),
            application.Shaders().ProgramIds().size(), application.Shaders().Version().c_str());
        return 0;
    }
    if (mode == "--headless-smoke-test") {
        if (ValidatePackage(application.Catalog(), application.Shaders()) != 0) return 1;
        std::string report;
        if (!RunHeadlessSmoke(application.Content(), report, error)) return Fail(error);
        SDL_Log("%s", report.c_str());
        return 0;
    }
    if (mode == "--smoke-test") {
        options.session.fadeOutSeconds = 0.0001F;
        options.session.fadeInSeconds = 0.0001F;
    }
    if (!application.InitializeGraphics(options, error)) return Fail(error);
    if (mode == "--shader-smoke-test") return RunShaderProbe(application.Platform(), application.Renderer());
    if (mode == "--muzzle-smoke-test") return RunMuzzleProbe(application.Platform(), application.RenderDevice(),
        application.Renderer(), application.Assets(), *application.Content(), capture);
    if (mode == "--viewmodel-smoke-test" || mode == "--reload-smoke-test") {
        return RunVisualProbe(application.Platform(), application.Renderer(), application.Presentation(),
            application.Assets(), application.Content(), mode == "--viewmodel-smoke-test",
            mode == "--reload-smoke-test", capture, static_cast<float>(options.width), static_cast<float>(options.height));
    }
    return RunPresentationSmoke(application, mode == "--menu-smoke-test");
}
