// Included by the product GUI probe inside its anonymous namespace. Every probe
// report names the platform it ran on: a pass on one platform never stands for
// another, and the input method separates injected SDL events from native or
// manual operation. The timer baseline records how precisely this host wakes
// the probe's sleeping frame loop; it is interpretation only, never a gate.
struct PlatformFingerprint final {
    std::string os, architecture, videoDriver, gpuDriver, input;
    float refreshHz{};
    std::optional<SDL_Rect> usable;
    TimerBaseline timer;

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
        // The GUI probes sleep the remainder of each frame with SDL_DelayNS.
        result.timer = TimerBaseline::Measure("SDL_DelayNS remainder", TimerBaseline::Schedule::FrameRelative,
            [](std::chrono::steady_clock::time_point deadline) {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining > std::chrono::steady_clock::duration::zero())
                SDL_DelayNS(static_cast<Uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count()));
        });
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
        report << "\nplatform_input=" << input
               << "\nplatform_timer_sleeper=" << timer.sleeper << "\nplatform_timer_schedule=" << timer.schedule << "\nplatform_timer_samples=" << timer.samples
               << "\nplatform_timer_late_p50_ms=" << timer.lateP50Ms << "\nplatform_timer_late_p99_ms=" << timer.lateP99Ms
               << "\nplatform_timer_late_max_ms=" << timer.lateMaxMs
               << "\nplatform_timer_interval_p50_ms=" << timer.intervalP50Ms
               << "\nplatform_timer_interval_p99_ms=" << timer.intervalP99Ms
               << "\nplatform_timer_interval_max_ms=" << timer.intervalMaxMs
               << "\nplatform_timer_interval_over_slow_fraction=" << timer.intervalOverSlowFraction << '\n';
    }
    nlohmann::json Json() const {
        nlohmann::json json{{"os", Known(os)}, {"architecture", architecture}, {"video_driver", Known(videoDriver)},
            {"gpu_driver", Known(gpuDriver)}, {"refresh_hz", refreshHz}, {"input", input}};
        json["usable_bounds"] = usable ? nlohmann::json::array({usable->x, usable->y, usable->w, usable->h}) :
            nlohmann::json(nullptr);
        json["timer"] = timer.Json();
        return json;
    }
};
