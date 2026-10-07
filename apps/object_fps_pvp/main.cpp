#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/LogFile.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "gyo/AppConfig.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#ifndef GYO_DEFAULT_GPU_DRIVER
#define GYO_DEFAULT_GPU_DRIVER "auto"
#endif

namespace {
// SDL's own output stays; every SDL log line is also written to the log file.
struct SdlLogForward {
    fps::pvp::LogFile* file{};
    SDL_LogOutputFunction original{};
    void* originalData{};
};
SdlLogForward sdlLog;

void ForwardSdlLog(void*, int category, SDL_LogPriority priority, const char* message) {
    if (sdlLog.original) sdlLog.original(sdlLog.originalData, category, priority, message);
    static constexpr const char* names[] = {"invalid", "trace", "verbose", "debug", "info", "warn", "error", "critical"};
    const auto index = static_cast<int>(priority);
    if (sdlLog.file) sdlLog.file->Write(std::string("[") + (index >= 0 && index < 8 ? names[index] : "log") + "] " + message);
}

// Without --log the Client writes logs/client-<local time>.log next to its executable.
std::filesystem::path DefaultLogPath(const std::filesystem::path& base) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char name[64]{};
    std::strftime(name, sizeof name, "client-%Y%m%d-%H%M%S.log", &local);
    return base / "logs" / name;
}
} // namespace

int main(int argc, char* argv[]) {
    fps::pvp::PvpApplicationOptions options;
    options.title = Gyo::AppConfig::DisplayName;
    options.gpuDriver = GYO_DEFAULT_GPU_DRIVER;
    std::filesystem::path movementTrace, logPath;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            SDL_Log("%s [--gpu-driver auto|d3d12|vulkan|metal] [--gateway host:port] [--movement-trace path] "
                    "[--log path (default: logs/client-<time>.log next to the executable)]", Gyo::AppConfig::DisplayName);
            return 0;
        }
        if (argument == "--gpu-driver" && index + 1 < argc) {
            options.gpuDriver = argv[++index];
        } else if (argument == "--gateway" && index + 1 < argc) {
            options.gateway = argv[++index];
        } else if (argument == "--movement-trace" && index + 1 < argc) {
            movementTrace = argv[++index];
        } else if (argument == "--log" && index + 1 < argc) {
            logPath = argv[++index];
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
    // A Client started from a desktop has no console: its diagnostics go to the log file.
    std::optional<fps::pvp::LogFile> log;
    // Declared after the log so SDL stops forwarding before the file closes.
    struct RestoreSdlLog {
        ~RestoreSdlLog() {
            if (sdlLog.file) SDL_SetLogOutputFunction(sdlLog.original, sdlLog.originalData);
            sdlLog.file = nullptr;
        }
    } restoreSdlLog;
    try {
        log.emplace(logPath.empty() ? DefaultLogPath(base) : logPath);
        log->Tee(std::clog);
        log->Tee(std::cerr);
        SDL_GetLogOutputFunction(&sdlLog.original, &sdlLog.originalData);
        sdlLog.file = &*log;
        SDL_SetLogOutputFunction(ForwardSdlLog, nullptr);
        std::string executable = "unavailable";
        try { executable = fps::pvp::FileSha256(fps::pvp::RunningExecutablePath()); } catch (const std::exception&) {}
        SDL_Log("PvP client start executable_sha256=%s platform=%s gpu_driver=%s gateway=%s log=%s",
                executable.c_str(), SDL_GetPlatform(), options.gpuDriver.c_str(), options.gateway.c_str(),
                log->Path().string().c_str());
    } catch (const std::exception& error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Client log unavailable: %s", error.what());
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
