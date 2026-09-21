#include "RetroFPS/App/ObjectFpsApplication.hpp"
#include "ViewmodelPreview.hpp"
#include "gyo/AppConfig.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    fps::ObjectFpsApplicationOptions options;
    options.title = std::string(Gyo::AppConfig::DisplayName) + " viewmodel preview";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            SDL_Log("Viewmodel preview: --gpu-driver DRIVER, --preview-4x3, --preview-21x9; "
                    "1-6 action, arrows time, Space height, Esc close");
            return 0;
        }
        if (argument == "--gpu-driver" && index + 1 < argc) options.gpuDriver = argv[++index];
        else if (argument == "--preview-4x3") options.width = 960;
        else if (argument == "--preview-21x9") options.width = 1680;
        else { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown preview option: %s", argv[index]); return 2; }
    }
    const char* base = SDL_GetBasePath();
    if (!base) return 1;
    fps::ObjectFpsApplication application;
    std::string error;
    if (!application.InitializeContent(std::filesystem::path(base) / Gyo::AppConfig::Assets, error) ||
        !application.InitializeGraphics(options, error)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.c_str()); return 1;
    }
    return RunViewmodelPreview(application.Platform(), application.Renderer(), application.Presentation(),
        application.Assets(), application.Content(), static_cast<float>(options.width), static_cast<float>(options.height));
}
