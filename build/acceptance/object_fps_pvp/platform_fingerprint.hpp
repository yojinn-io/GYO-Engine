// Included by the product GUI probe inside its anonymous namespace. Every probe
// report names the platform it ran on: a pass on one platform never stands for
// another, and the input method separates injected SDL events from native or
// manual operation.
struct PlatformFingerprint final {
    std::string os, architecture, videoDriver, gpuDriver, input;
    float refreshHz{};
    std::optional<SDL_Rect> usable;

    static PlatformFingerprint Capture(fps::pvp::PvpApplication& application, SDL_Window* window,
                                       std::string input = "sdl_injected") {
        PlatformFingerprint result;
        result.os = SDL_GetPlatform();
#if defined(__aarch64__) || defined(_M_ARM64)
        result.architecture = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
        result.architecture = "x64";
#else
        result.architecture = "other";
#endif
        if (const char* driver = SDL_GetCurrentVideoDriver()) result.videoDriver = driver;
        result.gpuDriver = application.RenderDevice().GetInfo().driver;
        if (const auto display = SDL_GetDisplayForWindow(window)) {
            if (const auto* mode = SDL_GetCurrentDisplayMode(display)) result.refreshHz = mode->refresh_rate;
            SDL_Rect bounds{};
            if (SDL_GetDisplayUsableBounds(display, &bounds)) result.usable = bounds;
        }
        result.input = std::move(input);
        return result;
    }
    static const std::string& Known(const std::string& value) {
        static const std::string unknown{"unknown"};
        return value.empty() ? unknown : value;
    }
    void Report(std::ostream& report) const {
        report << "platform_os=" << Known(os) << "\nplatform_architecture=" << architecture
               << "\nplatform_video_driver=" << Known(videoDriver) << "\nplatform_gpu_driver=" << Known(gpuDriver)
               << "\nplatform_refresh_hz=" << refreshHz << "\nplatform_usable_bounds=";
        if (usable) report << usable->x << ',' << usable->y << ',' << usable->w << ',' << usable->h;
        else report << "unavailable";
        report << "\nplatform_input=" << input << '\n';
    }
    nlohmann::json Json() const {
        nlohmann::json json{{"os", Known(os)}, {"architecture", architecture}, {"video_driver", Known(videoDriver)},
            {"gpu_driver", Known(gpuDriver)}, {"refresh_hz", refreshHz}, {"input", input}};
        json["usable_bounds"] = usable ? nlohmann::json::array({usable->x, usable->y, usable->w, usable->h}) :
            nlohmann::json(nullptr);
        return json;
    }
};
