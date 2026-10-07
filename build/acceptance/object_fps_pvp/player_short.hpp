// Product acceptance only. Input and snapshot impairment live outside the app.
// Observation only: never filter, rewrite or recapture an OS input event.
struct PlayerInputWatch final {
    struct Event {
        Uint32 type{}, device{};
        Uint64 sdlTimestamp{};
        double hostSeconds{};
        float dx{}, dy{};
        std::size_t scriptOrdinal{};
    };
    std::array<Event, 4096> events{};
    std::mutex mutex;
    SDL_WindowID window{};
    std::size_t count{}, scriptOrdinal{};
    bool overflow{}, installed{};

    static bool SDLCALL Observe(void* userdata, SDL_Event* event) {
        auto& self = *static_cast<PlayerInputWatch*>(userdata);
        Event value;
        value.type = event->type;
        value.sdlTimestamp = event->common.timestamp;
        value.hostSeconds = std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
        SDL_WindowID id{};
        switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            id = event->motion.windowID; value.device = event->motion.which;
            value.dx = event->motion.xrel; value.dy = event->motion.yrel;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP:
            id = event->button.windowID; value.device = event->button.which;
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED: case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_MOVED: case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_MINIMIZED: case SDL_EVENT_WINDOW_RESTORED:
            id = event->window.windowID;
            break;
        default: return true;
        }
        if (id != self.window) return true;
        std::lock_guard lock(self.mutex);
        if (self.count == self.events.size()) self.overflow = true;
        else {
            value.scriptOrdinal = self.scriptOrdinal;
            self.events[self.count++] = value;
        }
        return true;
    }
    void Install(SDL_Window* value) {
        window = SDL_GetWindowID(value);
        Require(SDL_AddEventWatch(&Observe, this), "Cannot install acceptance-only input event observer");
        installed = true;
    }
    void Remove() {
        if (installed) { SDL_RemoveEventWatch(&Observe, this); installed = false; }
    }
    void Script(std::size_t value) { std::lock_guard lock(mutex); scriptOrdinal = value; }
    bool Overflowed() { std::lock_guard lock(mutex); return overflow; }
    WeaponJson Evidence() {
        std::lock_guard lock(mutex);
        WeaponJson recorded = WeaponJson::array();
        for (std::size_t i = 0; i < count; ++i) {
            const auto& event = events[i];
            recorded.push_back({{"event_type", event.type}, {"which", event.device},
                {"sdl_timestamp_ns", event.sdlTimestamp}, {"host_steady_seconds", event.hostSeconds},
                {"xrel", event.dx}, {"yrel", event.dy}, {"script_ordinal_during_push", event.scriptOrdinal}});
        }
        return {{"capacity", events.size()}, {"overflow", overflow}, {"events", recorded},
            {"semantics", "SDL_AddEventWatch only observes this owned window; no filtering or state changes. Ordinal zero means no script PushEvent call was active; device/source attribution remains observational."}};
    }
    ~PlayerInputWatch() { Remove(); }
};

WeaponJson CharacterSample(const fps::pvp::PlayerPresentationObservation& c) {
    return {{"ready", c.ready}, {"player_id", c.playerId}, {"epoch", c.movementEpoch},
        {"jogging", c.jogging}, {"holding", c.holding}, {"backward", c.backward},
        {"phase_reset", c.phaseReset}, {"reset_count", c.resetCount}, {"reset_reason", c.resetReason},
        {"phase_cycles", c.phaseCycles}, {"unwrapped_phase_cycles", c.unwrappedPhaseCycles},
        {"signed_distance", c.signedDistance}, {"total_distance", c.totalDistance},
        {"distance_delta", c.distanceDelta}, {"cycle_distance", c.cycleDistance},
        {"jog_weight", c.jogWeight},
        {"speed", c.speed}, {"playback_rate", c.playbackRate}, {"move_weight", c.moveWeight},
        {"scale", c.scale}, {"foot_anchor", {c.footAnchor.x, c.footAnchor.y, c.footAnchor.z}},
        {"weapon_world_position", {c.weaponWorldPosition.x, c.weaponWorldPosition.y, c.weaponWorldPosition.z}},
        {"body_meshes", c.bodySubmittedMeshes}, {"hair_meshes", c.hairSubmittedMeshes},
        {"weapon_meshes", c.weaponSubmittedMeshes}, {"upper_body_mask_count", c.upperBodyMaskCount},
        {"prepared_instances", c.preparedInstances}, {"pose_revision", c.poseRevision}};
}

void RunPlayerShort(const Options& options) {
    using namespace fps::pvp;
    const bool mover = options.role == "create", captureMode = options.playerCapture;
    std::filesystem::create_directories(options.output);
    const auto startPath = options.output / "player-start.json";
    const auto finishPath = options.output / "player-finished.txt";
    if (mover) { std::filesystem::remove(startPath); std::filesystem::remove(finishPath); }
    WeaponJson report{{"passed", false}, {"role", options.role}, {"mode", captureMode ? "player-capture" : "player-short"},
        {"nominal_fps", options.fps}, {"duration_seconds", options.duration}, {"wire_gameplay", "v4 unchanged"},
        {"capture", captureMode ? "actual GPU scene before UI; separate from timing" : "none"},
        {"scope", "Same-host real two-GUI successful submissions. Sideways uses forward Jog approximation; backward reverses that Jog, not dedicated direction clips."},
        {"events", WeaponJson::array()}, {"captures", WeaponJson::array()}};
    std::vector<WeaponJson> frames;
    PlayerInputWatch inputWatch;
    frames.reserve(static_cast<std::size_t>((options.duration + 25) * options.fps));
    auto save = [&] {
        inputWatch.Remove();
        report["input_event_watch"] = inputWatch.Evidence();
        std::ofstream output(options.output / (options.role + "-player.json"));
        output << report.dump(2) << '\n';
        std::ofstream trace(options.output / (options.role + "-player-frames.jsonl"));
        for (const auto& frame : frames) trace << frame.dump() << '\n';
        Require(bool(output) && bool(trace), "Cannot save player presentation evidence");
    };
    try {
        PvpApplication application;
        std::string error;
        Require(application.InitializeContent(options.assetRoot, error), error);
        PvpApplicationOptions graphics;
        graphics.title = "GYO PvP player presentation / " + options.role;
        graphics.gateway = options.gateway;
        graphics.gpuDriver = options.gpu;
        graphics.width = 1280; graphics.height = 720; graphics.vsync = false;
        const auto preparing = Clock::now();
        Require(application.InitializeGraphics(graphics, error), error);
        report["initialize_graphics_ms"] = Seconds(Clock::now(), preparing) * 1000;
        auto& connection = application.Connection();
        auto* window = application.Platform().NativeWindow();
        report["platform"] = PlatformFingerprint::Capture(application, window).Json();
        inputWatch.Install(window);
        if (mover) connection.CreateAndJoin(options.gateway);
        else connection.Refresh(options.gateway);
        bool joined = mover, leaving{}, rejoinRequested{}, rejoined{};
        bool expectedCapture{};
        double expectedYaw{}, expectedPitch{};
        std::optional<Clock::time_point> lobbyEntered;
        PlayerId originalPlayer{};
        std::uint64_t frameIndex{}, presentedCount{};
        std::optional<double> startSeconds;
        const auto started = Clock::now();
        auto previous = started, refreshed = started;
        struct Event { double at; const char* name; };
        std::vector<Event> schedule{{.15, "capture"}, {.3, "release"},
            {.75, "forward"}, {1.55, "stop"}, {2.05, "backward"}, {2.85, "stop"},
            {3.15, "strafe"}, {3.65, "stop"}, {3.95, "diagonal"}, {4.45, "stop"},
            {4.7, "turn"}, {5.1, "turn-back"}, {5.5, "approach-wall"}, {7.8, "stop"},
            {8.05, "hold-motion"}, {8.95, "stop"}, {9.2, "escape"}, {9.3, "escape-up"},
            {10.1, "capture"}, {10.25, "release"}, {10.5, "forward"}, {11.2, "stop"}};
        std::vector<Event> captures{{.55, "idle"}, {1.25, "jog"}, {2.55, "backward"},
            {3.45, "strafe"}, {4.85, "turn-upper-body"}, {7.65, "wall-stop"}, {8.45, "hold"}, {10.8, "rejoined"}};
        // Capture mode only, after the timed schedule: the mover looks 60
        // degrees up, 60 down, then level, and the observer captures each pose.
        // The player-short timing cases never change pitch.
        constexpr double AimSeconds = 2.5;
        if (captureMode) {
            schedule.insert(schedule.end(), {{11.5, "look-up"}, {12.2, "look-down"}, {12.9, "look-level"}});
            captures.insert(captures.end(), {{11.95, "pitch-up"}, {12.65, "pitch-down"}, {13.35, "pitch-level"}});
        }
        report["script_schedule"] = WeaponJson::array();
        for (std::size_t i = 0; i < schedule.size(); ++i)
            report["script_schedule"].push_back({{"ordinal", i + 1}, {"seconds", schedule[i].at}, {"name", schedule[i].name}});
        std::size_t nextEvent{}, nextCapture{};
        for (;;) {
            const auto now = Clock::now();
            Require(Seconds(now, started) < options.duration + 25, "Bounded player presentation probe timed out");
            const double stamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            auto state = connection.State();
            Require(state.error.empty(), "Player presentation connection failed: " + state.error);
            if (!joined && state.phase == ConnectionPhase::Lobby) {
                if (!state.rooms.empty()) { connection.Join(options.gateway, state.rooms.front().id); joined = true; }
                else if (Seconds(now, refreshed) >= .3) { connection.Refresh(options.gateway); refreshed = now; }
            }
            if (state.snapshot && state.snapshot->players.size() == 2 && !originalPlayer) {
                originalPlayer = state.playerId;
                report["initial_player_id"] = originalPlayer;
                if (mover) {
                    startSeconds = stamp + .4;
                    std::ofstream start(startPath);
                    start << WeaponJson{{"host_steady_seconds", *startSeconds}, {"snapshot_hold_at_seconds", 8.2},
                        {"snapshot_hold_duration_seconds", .3}}.dump();
                    Require(bool(start), "Cannot publish player presentation start");
                    SDL_RaiseWindow(window);
                }
            }
            if (!mover && !startSeconds && std::filesystem::exists(startPath)) {
                std::ifstream start(startPath);
                WeaponJson value;
                try { start >> value; startSeconds = value.at("host_steady_seconds").get<double>(); }
                catch (const WeaponJson::exception&) { /* A concurrent writer has not closed its file yet. */ }
            }
            const double elapsed = startSeconds ? stamp - *startSeconds : -1;
            if (mover && elapsed >= options.duration + (captureMode ? AimSeconds : 0)) break;
            if (!mover && startSeconds && std::filesystem::exists(finishPath)) break;
            std::string event, captureLabel;
            if (mover && startSeconds && nextEvent < schedule.size() && elapsed >= schedule[nextEvent].at) {
                event = schedule[nextEvent++].name;
                inputWatch.Script(nextEvent);
                if (event == "capture") {
                    PushWindowEvent(window, SDL_EVENT_WINDOW_FOCUS_GAINED);
                    PushMouse(window, true);
                    expectedCapture = true;
                } else if (event == "release") PushMouse(window, false);
                else if (event == "forward") PushKey(window, SDL_SCANCODE_W, true);
                else if (event == "backward") PushKey(window, SDL_SCANCODE_S, true);
                else if (event == "strafe") PushKey(window, SDL_SCANCODE_D, true);
                else if (event == "diagonal") { PushKey(window, SDL_SCANCODE_W, true); PushKey(window, SDL_SCANCODE_A, true); }
                else if (event == "stop") {
                    for (const auto key : {SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D}) PushKey(window, key, false);
                } else if (event == "escape") {
                    PushKey(window, SDL_SCANCODE_ESCAPE, true); leaving = true;
                    expectedCapture = false; expectedYaw = 0;
                }
                else if (event == "escape-up") PushKey(window, SDL_SCANCODE_ESCAPE, false);
                else if (event == "look-up" || event == "look-down" || event == "look-level") {
                    // A positive pitch looks down; one mouse count turns 0.0025 rad.
                    expectedPitch = event == "look-up" ? -1.0471975511965976 : event == "look-down" ? 1.0471975511965976 : 0;
                    PushMotion(window, 0, static_cast<float>((expectedPitch - application.WeaponFeedback().pitch) / .0025));
                } else {
                    const double desired = event == "turn" || event == "hold-motion" ? 1.5707963267948966 :
                        event == "approach-wall" ? -1.5707963267948966 : 0;
                    expectedYaw = desired;
                    PushMotion(window, static_cast<float>(std::remainder(desired - application.WeaponFeedback().yaw,
                        6.283185307179586) / .0025), 0);
                    if (event == "approach-wall" || event == "hold-motion") PushKey(window, SDL_SCANCODE_W, true);
                }
                inputWatch.Script(0);
            }
            // Capture is a separate mode. The observer keeps its existing fixed
            // spawn camera; images honestly show that camera's real field of view.
            if (!mover && captureMode && startSeconds && nextCapture < captures.size() && elapsed >= captures[nextCapture].at)
                captureLabel = captures[nextCapture++].name;
            const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
            previous = now;
            const auto eventStart = Clock::now();
            Require(application.ProcessEvents(frame) == Control::Continue, "Player presentation window closed");
            const auto updateStart = Clock::now();
            Require(application.Update(frame) == Control::Continue, application.LastError());
            const auto renderStart = Clock::now();
            if (!captureLabel.empty()) application.Renderer().RequestSceneCapture();
            Require(application.Render(frame) == Control::Continue, application.LastError());
            const auto renderEnd = Clock::now();
            state = connection.State();
            Require(!inputWatch.Overflowed(), "Bounded acceptance input event observation overflowed");
            if (mover && application.WeaponFeedback().active) {
                const auto& weapon = application.WeaponFeedback();
                const bool expected = weapon.inputCaptured == expectedCapture && std::abs(weapon.pitch - expectedPitch) < .0001 &&
                    std::abs(std::remainder(weapon.yaw - expectedYaw, 6.283185307179586)) < .0001;
                if (!expected) report["unexpected_input"] = {{"seconds", elapsed}, {"host_steady_seconds", stamp},
                    {"frame_id", frame.frameIndex}, {"scheduled_event", event},
                    {"expected_capture", expectedCapture}, {"expected_yaw", expectedYaw}, {"expected_pitch", expectedPitch},
                    {"observed_weapon", WeaponSample(weapon)}, {"source", "undetermined; no corresponding scripted input"}};
                Require(expected, "Unscripted desktop mouse/focus input disturbed the controlled player schedule");
            }
            Require(application.WeaponFeedback().submittedActions == 0,
                "Movement-only player probe accidentally submitted a weapon action");
            if (leaving && !rejoinRequested && state.phase == ConnectionPhase::Lobby && !state.rooms.empty()) {
                Require(!application.LocalMovement().active && !application.WeaponFeedback().active,
                    "ESC retained the local world while testing player lifecycle");
                if (!lobbyEntered) lobbyEntered = now;
                // Give even the 30FPS peer multiple real Presented frames to
                // observe absence, instead of racing Leave/Join in one snapshot.
                if (Seconds(now, *lobbyEntered) >= .25) {
                    connection.Join(options.gateway, state.rooms.front().id);
                    rejoinRequested = true;
                }
            }
            if (rejoinRequested && state.phase == ConnectionPhase::Playing && state.playerId != originalPlayer) {
                rejoined = true;
                report["rejoined_player_id"] = state.playerId;
            }
            WeaponJson sample{{"frame_id", frame.frameIndex}, {"seconds", elapsed}, {"frame_seconds", frame.deltaSeconds},
                {"event", event}, {"presented", false}, {"phase", static_cast<int>(state.phase)}, {"player_id", state.playerId},
                {"events_ms", Seconds(updateStart, eventStart) * 1000}, {"update_ms", Seconds(renderStart, updateStart) * 1000},
                {"render_ms", Seconds(renderEnd, renderStart) * 1000}, {"weapon", WeaponSample(application.WeaponFeedback())},
                {"remote", nullptr}, {"capture", captureLabel}};
            if (const auto& shown = application.PresentedMovement()) {
                Require(shown->frameId == frame.frameIndex, "Player probe reused a stale Presented observation");
                sample["presented"] = true;
                sample["presented_seconds"] = shown->hostSteadySeconds;
                sample["generation"] = shown->connectionGeneration;
                sample["local"] = {shown->local.renderPosition.x, shown->local.renderPosition.y, shown->local.renderPosition.z};
                sample["prepare_world_ms"] = shown->prepareWorldMilliseconds;
                sample["remote_submit_ms"] = shown->remoteSubmitMilliseconds;
                sample["skipped_frames"] = shown->skippedFrames;
                if (const auto& remote = shown->remote) {
                    const auto& character = remote->character;
                    Require(character.ready && character.playerId == remote->playerId && character.preparedInstances > 0,
                        "Remote character was not prepared before its first successful presentation");
                    Require(character.bodySubmittedMeshes && character.hairSubmittedMeshes && character.weaponSubmittedMeshes,
                        "Successful player presentation omitted body, hair or hand weapon meshes");
                    sample["remote"] = {{"player_id", remote->playerId},
                        {"position", {remote->renderPosition.x, remote->renderPosition.y, remote->renderPosition.z}},
                        {"yaw", remote->yaw}, {"pitch", remote->pitch}, {"timeline_holding", remote->holding},
                        {"timeline_tick", remote->presentationTick}, {"character", CharacterSample(character)}};
                }
                ++presentedCount;
            }
            if (auto capture = application.Renderer().TakeSceneCapture()) {
                Require(captureMode && !captureLabel.empty(), "Player timing case unexpectedly performed GPU readback");
                const auto file = options.role + "-player-" + captureLabel + ".bmp";
                SaveCapture(*capture, options.output / file);
                report["captures"].push_back({{"label", captureLabel}, {"file", file}, {"frame_id", frame.frameIndex},
                    {"width", capture->width}, {"height", capture->height}, {"remote", sample["remote"]}});
            }
            if (!event.empty()) report["events"].push_back({{"name", event}, {"seconds", elapsed}, {"frame_id", frame.frameIndex}});
            Require(frames.size() < frames.capacity(), "Player presentation frame buffer exhausted");
            frames.push_back(std::move(sample));
            const double remaining = 1 / options.fps - Seconds(Clock::now(), now);
            if (remaining > 0) SDL_DelayNS(static_cast<Uint64>(remaining * 1e9));
        }
        Require(presentedCount > 20, "Insufficient actual player presentations");
        if (mover) Require(rejoined && nextEvent == schedule.size(), "Player lifecycle/movement schedule incomplete");
        if (!mover && captureMode) Require(report["captures"].size() == captures.size(), "Representative GPU captures incomplete");
        report["presentation_frames"] = presentedCount;
        report["skipped_frames"] = application.SkippedPresentationFrames();
        report["passed"] = true;
        save();
        if (mover) { std::ofstream done(finishPath); done << "Player presentation short schedule complete\n"; }
        connection.Leave();
        std::cout << options.role << ": player presentation short probe completed\n";
    } catch (const std::exception& exception) {
        report["error"] = exception.what(); save(); throw;
    }
}
