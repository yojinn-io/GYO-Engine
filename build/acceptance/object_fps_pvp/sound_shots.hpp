// Included by the product GUI probe inside its anonymous namespace. The L2 of
// v8-product-sfx (declared in docs/dev_logs/2026_10_30_pvp_v8_product_sfx.zh-Hant.md):
// local shots whose clicks are injected from a thread of their own, at times
// that do not line up with the 30 FPS frames, so each press carries its own
// SDL timestamp. The actor records its frames, the presses' cues, and the
// sound output's anchors; sound_l2.py finds the onsets in the disk driver's
// file and judges them. The joiner only fills the room and stays still.
// Input, timing and assertions belong to acceptance, never the application.
using SoundJson = nlohmann::json;

std::int64_t EngineNs(const Engine::Time::TimePoint point) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(point.time_since_epoch()).count();
}

void RunSoundShots(const Options& options) {
    using namespace fps::pvp;
    const bool actor = options.role == "create";
    static constexpr unsigned Shots = 60;
    static constexpr unsigned ShotsPerMagazine = 12;
    // Gaps between presses, cycled; each is above the 10-tick cooldown and
    // none is a multiple of a 30 FPS frame.
    static constexpr int GapsMs[] = {350, 410, 470};
    static constexpr int ReloadWaitMs = 2500;
    std::filesystem::create_directories(options.output);
    const auto startPath = options.output / "sound-start.txt";
    const auto finishedPath = options.output / "sound-finished.txt";
    if (actor) for (const auto& path : {startPath, finishedPath}) std::filesystem::remove(path);

    PvpApplication application;
    GuiRoom guiRoom(options.output, options.role, options.roomPlayers);
    // Anchors drained every frame; a 30 FPS frame sees about three callbacks.
    application.EnableSoundDiagnostics(4096);
    std::string error;
    Require(application.InitializeContent(options.assetRoot, error), error);
    PvpApplicationOptions graphics;
    graphics.title = "Object FPS PVP sound L2 / " + options.role;
    graphics.gateway = options.gateway;
    graphics.gpuDriver = options.gpu;
    graphics.width = 960;
    graphics.height = 540;
    graphics.vsync = false;
    Require(application.InitializeGraphics(graphics, error), error);
    auto& connection = application.Connection();
    ProbeInput input(application.Platform());
    if (actor) connection.CreateAndJoin(options.gateway);
    else connection.Refresh(options.gateway);

    std::vector<SoundDiagnostics::Anchor> anchors;
    std::vector<SoundDiagnostics::Cue> cues;
    std::vector<std::int64_t> frameStarts;
    frameStarts.reserve(static_cast<std::size_t>(options.fps * (options.duration + 60)));
    struct Press final { unsigned index{}; std::int64_t pushedNs{}; bool reloadKey{}; };
    std::vector<Press> presses;
    std::mutex pressesLock;
    std::atomic<bool> injectorDone{false};
    std::optional<std::thread> injector;

    bool joined = actor, setupCaptured{}, setupReleased{}, setupTurned{}, injecting{};
    std::optional<double> measurementStart, finishedAt;
    std::string room;
    std::uint64_t frameIndex{};
    std::uint64_t shotsAtStart{}, shotsAtEnd{};
    const auto started = Clock::now();
    auto previous = started, refreshed = started;
    const auto drain = [&] {
        auto taken = application.TakeSoundDiagnostics();
        anchors.insert(anchors.end(), taken.anchors.begin(), taken.anchors.end());
        cues.insert(cues.end(), taken.cues.begin(), taken.cues.end());
        return taken;
    };
    for (;;) {
        const auto now = Clock::now();
        Require(Seconds(now, started) < options.duration + 60, "Sound probe exceeded bounded deadline");
        frameStarts.push_back(EngineNs(now));
        const double stamp = std::chrono::duration<double>(now.time_since_epoch()).count();
        auto state = connection.State();
        Require(state.error.empty(), "Sound probe connection failed: " + state.error);
        if (!joined && state.phase == ConnectionPhase::Lobby) {
            if (!state.rooms.empty()) {
                room = state.rooms.front().id;
                connection.Join(options.gateway, room);
                joined = true;
            } else if (Seconds(now, refreshed) >= .3) { connection.Refresh(options.gateway); refreshed = now; }
        }
        if (const auto peer = guiRoom.Observe(state)) application.ObserveRemote(peer);
        if (actor && guiRoom.Full(state) && !measurementStart) {
            measurementStart = stamp + .25;
            std::ofstream start(startPath);
            start << std::setprecision(17) << *measurementStart;
            Require(bool(start), "Cannot signal sound start");
            application.Platform().Raise();
        }
        if (!actor && std::filesystem::exists(finishedPath)) break;
        const double elapsed = measurementStart ? stamp - *measurementStart : -1;
        const auto before = application.WeaponFeedback();
        if (actor && elapsed >= 0 && !injecting) {
            if (!setupCaptured && elapsed >= .10) {
                PushFocus(input, true);
                PushMouse(input, true);
                setupCaptured = true;
            } else if (setupCaptured && !setupReleased && elapsed >= .20) {
                PushMouse(input, false);
                setupReleased = true;
            } else if (setupReleased && !setupTurned && elapsed >= .30) {
                // Face the wall the action probes use, away from the other player.
                PushMotion(input, static_cast<float>((std::atan2(5., 5.) - before.yaw) / .0025), 0);
                setupTurned = true;
            } else if (setupTurned && elapsed >= .60) {
                Require(before.active && before.inputCaptured && before.magazineAmmo == before.magazineCapacity,
                    "Sound setup did not capture input with a full magazine");
                shotsAtStart = before.animationStarts;
                injecting = true;
                // The presses come from this thread, at wall-clock times of their
                // own: SDL stamps each pushed event when it is pushed.
                injector.emplace([&] {
                    auto due = Clock::now() + std::chrono::milliseconds(200);
                    for (unsigned shot = 0; shot < Shots; ++shot) {
                        std::this_thread::sleep_until(due);
                        const auto pushed = Engine::Time::Now();
                        PushMouse(input, true);
                        PushMouse(input, false);
                        { std::lock_guard lock(pressesLock); presses.push_back({shot, EngineNs(pushed), false}); }
                        if ((shot + 1) % ShotsPerMagazine == 0 && shot + 1 < Shots) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(400));
                            const auto reload = Engine::Time::Now();
                            // Held across frames: a press and release in one frame is no key press to the product.
                            PushKey(input, Key::R, true);
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            PushKey(input, Key::R, false);
                            { std::lock_guard lock(pressesLock); presses.push_back({shot, EngineNs(reload), true}); }
                            due = Clock::now() + std::chrono::milliseconds(ReloadWaitMs);
                        } else {
                            due += std::chrono::milliseconds(GapsMs[shot % std::size(GapsMs)]);
                        }
                    }
                    injectorDone = true;
                });
            }
        }
        if (actor && injecting && injectorDone && !finishedAt) finishedAt = elapsed;
        if (actor && finishedAt && elapsed - *finishedAt >= 2.0) break;

        const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
        previous = now;
        Require(application.ProcessEvents(frame) == Control::Continue, "Sound acceptance window closed");
        Require(application.Update(frame) == Control::Continue, application.LastError());
        Require(application.Render(frame) == Control::Continue, application.LastError());
        static_cast<void>(drain());
        const double remaining = 1 / options.fps - Seconds(Clock::now(), now);
        if (remaining > 0) SleepFor(remaining);
    }
    if (injector) injector->join();
    const auto last = drain();
    shotsAtEnd = application.WeaponFeedback().animationStarts;
    if (!actor) return;
    {
        std::ofstream finished(finishedPath);
        finished << "done\n";
    }
    std::ofstream anchorFile(options.output / "sound-anchors.csv");
    anchorFile << "anchor_ns,first_frame\n";
    for (const auto& anchor : anchors) anchorFile << EngineNs(anchor.anchor) << ',' << anchor.firstFrame << '\n';
    std::ofstream cueFile(options.output / "sound-cues.csv");
    cueFile << "cue,at_ns,played_ns\n";
    for (const auto& cue : cues)
        cueFile << SoundCueName(cue.cue) << ',' << EngineNs(cue.at) << ',' << EngineNs(cue.played) << '\n';
    std::ofstream pressFile(options.output / "sound-presses.csv");
    pressFile << "index,pushed_ns,reload_key\n";
    for (const auto& press : presses) pressFile << press.index << ',' << press.pushedNs << ',' << (press.reloadKey ? 1 : 0) << '\n';
    std::ofstream frameFile(options.output / "sound-frames.csv");
    frameFile << "frame_start_ns\n";
    for (const auto start : frameStarts) frameFile << start << '\n';
    SoundJson summary{{"role", options.role}, {"nominal_fps", options.fps}, {"driver", last.driver},
        {"schedule_delay_ns", last.scheduleDelayNs}, {"anchors_dropped", last.anchorsDropped},
        {"submitted", last.submitted}, {"on_time", last.onTime}, {"late", last.late},
        {"queue_full", last.queueFull}, {"no_voice", last.noVoice},
        {"shots_submitted", shotsAtEnd - shotsAtStart}, {"presses", Shots}};
    std::ofstream summaryFile(options.output / "sound-summary.json");
    summaryFile << summary.dump(2) << '\n';
    Require(bool(anchorFile) && bool(cueFile) && bool(pressFile) && bool(frameFile) && bool(summaryFile),
        "Cannot write sound evidence");
}
