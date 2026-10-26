#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/LogFile.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "RetroFPS/Pvp/TracePaths.hpp"
#include "gyo/AppConfig.hpp"

#include "RetroFPS/Pvp/ClientLog.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/os/Files.hpp"
#include "engine/platform/sdl/EntryPoint.hpp"
#include "engine/platform/sdl/SdlLogBridge.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#ifndef GYO_DEFAULT_GPU_DRIVER
#define GYO_DEFAULT_GPU_DRIVER "auto"
#endif

namespace {
using Engine::Base::LogLevel;
using fps::pvp::ClientLog;

// Every diagnostics line of the Client goes through the Engine log facade:
// the Client's own lines (category "app"), Engine lines, and SDL's lines (the
// bridge, categories "sdl.*"). In the log file each line reads as it did when
// they all went through SDL's log: "[level] text", Engine lines with their
// category before the text.
std::atomic<fps::pvp::LogFile*> logFile{nullptr};

std::string_view LevelName(const LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warning: return "warn";
    case LogLevel::Error: return "error";
    }
    return "log";
}

void WriteClientLog(const Engine::Base::LogMessage& message) noexcept {
    try {
        const bool fromSdl = message.category.starts_with("sdl.");
        std::string text;
        if (message.category != "app" && !fromSdl) text.append(message.category).append(" ");
        text.append(message.text);
        if (auto* file = logFile.load()) file->Write(std::string("[") + std::string(LevelName(message.level)) + "] " + text);
        // SDL's lines keep SDL's console output (the bridge tees); the others
        // go to the console here (C stdio: the file does not copy it again).
        if (!fromSdl) std::fprintf(stderr, "%s\n", text.c_str());
    } catch (...) {
        // A log line is never worth a failure.
    }
}

// Without --log the Client writes logs/client-<local time>.log next to its executable.
std::filesystem::path DefaultLogPath(const std::filesystem::path& base) {
    return base / "logs" / fps::pvp::LogFileName("client", fps::pvp::LocalWallClock(std::chrono::system_clock::now()));
}
} // namespace

int GyoMain(int argc, char* argv[]) {
    Engine::Base::SetLogHandler(&WriteClientLog);
    fps::pvp::PvpApplicationOptions options;
    options.title = Gyo::AppConfig::DisplayName;
    options.gpuDriver = GYO_DEFAULT_GPU_DRIVER;
    std::filesystem::path movementTrace, logPath;
    bool gatewayFromArgument = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            ClientLog(LogLevel::Info, "%s [--gpu-driver auto|d3d12|vulkan|metal] [--gateway host:port] [--movement-trace path] "
                    "[--log path (default: logs/client-<time>.log next to the executable)]", Gyo::AppConfig::DisplayName);
            return 0;
        }
        if (argument == "--gpu-driver" && index + 1 < argc) {
            options.gpuDriver = argv[++index];
        } else if (argument == "--gateway" && index + 1 < argc) {
            options.gateway = argv[++index];
            gatewayFromArgument = true;
        } else if ((argument == "--movement-trace" || argument == "--log") && index + 1 < argc) {
            // SDL hands main UTF-8 arguments; text that is not UTF-8 is refused, not guessed.
            auto path = Engine::Os::Files::PathFromUtf8(argv[++index]);
            if (!path) {
                ClientLog(LogLevel::Error, "Invalid path for %s: %s", argv[index - 1],
                          Engine::Base::Describe(path.error()).c_str());
                return 2;
            }
            if (argument == "--log") logPath = std::move(*path);
            else movementTrace = std::move(*path);
        } else {
            ClientLog(LogLevel::Error, "Unknown or incomplete option: %s", argv[index]);
            return 2;
        }
    }
    // A restart never truncates an earlier trace (D191): the next free name.
    movementTrace = fps::pvp::FreshTracePath(movementTrace);
    const auto baseDirectory = Engine::Platform::Sdl::BaseDirectory();
    if (!baseDirectory) {
        ClientLog(LogLevel::Error, "Cannot locate executable directory");
        return 1;
    }
    const std::filesystem::path& base = baseDirectory.value();
    // A Client started from a desktop has no console: its diagnostics go to the log file.
    std::optional<fps::pvp::LogFile> log;
    // Declared after the log: the handler stops writing to the file, and the
    // bridge stops forwarding SDL's lines, before the file closes.
    struct DetachLog {
        ~DetachLog() { logFile.store(nullptr); }
    } detachLog;
    try {
        log.emplace(logPath.empty() ? DefaultLogPath(base) : logPath);
        log->FlushOnCrash();
        log->Tee(std::clog);
        log->Tee(std::cerr);
        logFile.store(&*log);
    } catch (const std::exception& error) {
        ClientLog(LogLevel::Warning, "Client log unavailable: %s", error.what());
    }
    // SDL's own lines join the log (and keep SDL's console output). The
    // handler above never calls SDL's log, as the bridge requires.
    const Engine::Platform::Sdl::SdlLogBridge sdlLogBridge;
    if (log) {
        std::string executable = "unavailable";
        try { executable = fps::pvp::FileSha256(fps::pvp::RunningExecutablePath()); } catch (const std::exception&) {}
        const std::string platform(Engine::Platform::Sdl::PlatformName());
        ClientLog(LogLevel::Info, "PvP client start executable_sha256=%s platform=%s gpu_driver=%s gateway=%s log=%s",
                  executable.c_str(), platform.c_str(), options.gpuDriver.c_str(), options.gateway.c_str(),
                  Engine::Os::Files::PathForMessage(log->Path()).c_str());
        // log-contract (original batch 10): whether the Gateway address came from --gateway or the default.
        ClientLog(LogLevel::Info, "PvP client gateway source=%s address=%s", gatewayFromArgument ? "argument" : "default",
                  options.gateway.c_str());
        if (!movementTrace.empty())
            ClientLog(LogLevel::Info, "PvP client trace file movement=%s",
                      Engine::Os::Files::PathForMessage(movementTrace).c_str());
    }
    try {
        fps::pvp::MovementTraceWriter trace(movementTrace);
        int result{};
        {
            fps::pvp::PvpApplication application;
            std::string error;
            if (!application.InitializeContent(base / Gyo::AppConfig::Assets, error) ||
                !application.InitializeGraphics(options, error)) {
                ClientLog(LogLevel::Error, "%s", error.c_str());
                return 1;
            }
            result = application.Run();
        }
        // Join the connection worker before closing its optional trace sink.
        trace.Finish();
        return trace.Good() ? result : 1;
    } catch (const std::exception& error) {
        ClientLog(LogLevel::Error, "%s", error.what());
        return 1;
    }
}
