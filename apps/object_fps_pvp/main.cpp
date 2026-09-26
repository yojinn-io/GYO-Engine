#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "gyo/AppConfig.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <string>
#include <string_view>

#ifndef GYO_DEFAULT_GPU_DRIVER
#define GYO_DEFAULT_GPU_DRIVER "auto"
#endif

int main(int argc, char* argv[]) {
    fps::pvp::PvpApplicationOptions options;
    options.title = Gyo::AppConfig::DisplayName;
    options.gpuDriver = GYO_DEFAULT_GPU_DRIVER;
    std::filesystem::path movementTrace;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            SDL_Log("%s [--gpu-driver auto|d3d12|vulkan|metal] [--gateway host:port] [--movement-trace path]", Gyo::AppConfig::DisplayName);
            return 0;
        }
        if (argument == "--gpu-driver" && index + 1 < argc) {
            options.gpuDriver = argv[++index];
        } else if (argument == "--gateway" && index + 1 < argc) {
            options.gateway = argv[++index];
        } else if (argument == "--movement-trace" && index + 1 < argc) {
            movementTrace = argv[++index];
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown or incomplete option: %s", argv[index]);
            return 2;
        }
    }
    const char* base = SDL_GetBasePath();
    if (!base) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot locate executable directory");
        return 1;
    }
    try {
        fps::pvp::MovementTraceWriter trace(movementTrace);
        int result{};
        {
            fps::pvp::PvpApplication application;
            std::string error;
            if (!application.InitializeContent(std::filesystem::path(base) / Gyo::AppConfig::Assets, error) ||
                !application.InitializeGraphics(options, error)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.c_str());
                return 1;
            }
            result = application.Run();
        }
        // Join the connection worker before closing its optional trace sink.
        trace.Finish();
        return trace.Good() ? result : 1;
    } catch (const std::exception& error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
        return 1;
    }
}
