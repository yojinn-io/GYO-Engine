// Acceptance-only passive observer. After ordinary initial room setup all
// gameplay and lifecycle operations enter through native desktop input.
// The state file is diagnostic IPC, never an application input/control API.
void RunNativeWindow(const Options& options) {
    using namespace fps::pvp;
    std::filesystem::create_directories(options.output);
    PvpApplication application;
    std::string error;
    Require(application.InitializeContent(options.assetRoot, error), error);
    PvpApplicationOptions graphics;
    graphics.title = "GYO PvP native acceptance / " + options.role;
    graphics.gateway = options.gateway;
    graphics.gpuDriver = options.gpu;
    graphics.width = 800;
    graphics.height = 600;
    graphics.vsync = false;
    Require(application.InitializeGraphics(graphics, error), error);
    auto* window = application.Platform().NativeWindow();
    const auto nativeId = SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    Require(nativeId != 0, "Native acceptance requires an actual X11 window");
    auto& connection = application.Connection();
    bool initialJoin = options.role == "create";
    if (initialJoin) connection.CreateAndJoin(options.gateway);
    else connection.Refresh(options.gateway);
    const auto started = Clock::now();
    auto previous = started, refreshed = started, published = started;
    const auto statePath = options.output / (options.role + "-native-state.json");
    const auto tempPath = options.output / (options.role + "-native-state.tmp");
    const auto stopPath = options.output / (options.role + "-native-stop");
    std::vector<WeaponJson> frames;
    frames.reserve(static_cast<std::size_t>((options.duration + 10) * options.fps));
    std::uint64_t frameId{};
    WeaponJson final;
    bool closed{}, timedOut{};
    for (;;) {
        const auto now = Clock::now();
        if (Seconds(now, started) >= options.duration) { timedOut = true; break; }
        if (std::filesystem::exists(stopPath)) break;
        auto state = connection.State();
        Require(state.error.empty(), "Native connection error: " + state.error);
        if (!initialJoin && state.phase == ConnectionPhase::Lobby) {
            if (!state.rooms.empty()) {
                connection.Join(options.gateway, state.rooms.front().id);
                initialJoin = true;
            } else if (Seconds(now, refreshed) > .3) {
                connection.Refresh(options.gateway);
                refreshed = now;
            }
        }
        const Engine::Runtime::FrameContext frame{frameId++, Seconds(now, previous)};
        previous = now;
        if (application.ProcessEvents(frame) != Control::Continue) { closed = true; break; }
        Require(application.Update(frame) == Control::Continue, application.LastError());
        Require(application.Render(frame) == Control::Continue, application.LastError());
        state = connection.State();
        int x{}, y{}, width{}, height{};
        Require(SDL_GetWindowPosition(window, &x, &y) && SDL_GetWindowSize(window, &width, &height), SDL_GetError());
        const auto& local = application.LocalMovement();
        WeaponJson sample{{"seconds", Seconds(now, started)}, {"frame_seconds", frame.deltaSeconds},
            {"frame_id", frame.frameIndex}, {"window_id", nativeId}, {"title", graphics.title},
            {"phase", static_cast<int>(state.phase)}, {"player_id", state.playerId},
            {"geometry", {x, y, width, height}}, {"focused", bool(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)},
            {"weapon", WeaponSample(application.WeaponFeedback())},
            {"local", {{"active", local.active}, {"predicted", {local.predictedPosition.x, local.predictedPosition.z}},
                {"render", {local.renderPosition.x, local.renderPosition.z}},
                {"correction", {local.correctionOffset.x, local.correctionOffset.z}},
                {"pending", local.pendingCommands}}}, {"players", WeaponJson::array()}, {"rooms", WeaponJson::array()},
            {"presented", false}};
        for (const auto& room : state.rooms)
            sample["rooms"].push_back({{"id", room.id}, {"players", room.players}, {"capacity", room.capacity}});
        if (state.snapshot) {
            sample["authority_tick"] = state.snapshot->tick;
            for (const auto& player : state.snapshot->players) {
                WeaponJson value{{"id", player.playerId}, {"x", player.position.x}, {"z", player.position.z},
                    {"yaw", player.yaw}, {"pitch", player.pitch}, {"epoch", player.movementEpoch}};
                for (const auto& combat : state.snapshot->combat) if (combat.playerId == player.playerId) value["hp"] = combat.hp;
                sample["players"].push_back(std::move(value));
            }
        }
        if (const auto& shown = application.PresentedMovement()) {
            Require(shown->frameId == frame.frameIndex, "Native observer reused a stale Presented frame");
            sample["presented"] = true;
            sample["presented_seconds"] = shown->hostSteadySeconds;
            sample["presented_weapon"] = WeaponSample(shown->weapon);
            sample["skipped_frames"] = shown->skippedFrames;
        }
        Require(frames.size() < frames.capacity(), "Native acceptance bounded frame buffer exhausted");
        frames.push_back(sample);
        final = sample;
        if (frameId == 1 || Seconds(now, published) >= .05) {
            std::ofstream output(tempPath);
            output << sample.dump() << '\n';
            output.close();
            Require(bool(output), "Cannot publish native read-only state");
            std::filesystem::rename(tempPath, statePath);
            published = now;
        }
        const double remaining = 1 / options.fps - Seconds(Clock::now(), now);
        if (remaining > 0) SDL_DelayNS(static_cast<Uint64>(remaining * 1e9));
    }
    connection.Leave();
    std::ofstream trace(options.output / (options.role + "-native-frames.jsonl"));
    for (const auto& frame : frames) trace << frame.dump() << '\n';
    trace.close();
    Require(bool(trace), "Cannot write bounded native frame evidence");
    std::ofstream report(options.output / (options.role + "-native-final.json"));
    report << WeaponJson{{"closed_by_native_event", closed}, {"timed_out", timedOut},
        {"frames", frames.size()}, {"last", final}}.dump(2) << '\n';
    report.close();
    Require(bool(report), "Cannot write native final report");
    Require(!timedOut, "Native acceptance deadline exhausted");
}
