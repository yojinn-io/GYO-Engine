// Included by the product GUI probe inside its anonymous namespace. Bounded v5
// action regression whose input is injected at the SDL layer, so the same
// command runs on every platform: held fire, single shots, reload and what it
// suppresses, a moving reload, an airborne shot, an empty magazine, death
// suppression and respawn, and the remote presentation of each action. The
// actor also covers what the retired weapon short probe owned: local feedback
// on its first successful presentation, an authority cooldown rejection, wall
// and hit decisions, a combined move/turn/shoot frame that consumes the mouse
// delta once, window interactions that release capture without a ghost shot,
// and ESC/rejoin clearing every weapon effect. Native OS input stays the manual
// checklist. Input, timing and assertions belong to acceptance, never the
// application.
using ActionJson = nlohmann::json;

const char* UpperActionName(const fps::pvp::PlayerUpperAction action) {
    switch (action) {
    case fps::pvp::PlayerUpperAction::Hold: return "hold";
    case fps::pvp::PlayerUpperAction::Shoot: return "shoot";
    case fps::pvp::PlayerUpperAction::Reload: return "reload";
    case fps::pvp::PlayerUpperAction::Hit: return "hit";
    }
    return "invalid";
}

const char* LowerActionName(const fps::pvp::PlayerLowerAction action) {
    switch (action) {
    case fps::pvp::PlayerLowerAction::Locomotion: return "locomotion";
    case fps::pvp::PlayerLowerAction::JumpStart: return "jump_start";
    case fps::pvp::PlayerLowerAction::JumpLoop: return "jump_loop";
    case fps::pvp::PlayerLowerAction::JumpLand: return "jump_land";
    case fps::pvp::PlayerLowerAction::Death: return "death";
    }
    return "invalid";
}

ActionJson RemoteActionSample(const fps::pvp::PlayerPresentationObservation& c) {
    return {{"player_id", c.playerId}, {"life", c.lifeGeneration}, {"dead", c.dead},
        {"upper", UpperActionName(c.actions.upper)}, {"lower", LowerActionName(c.actions.lower)},
        {"upper_clip_seconds", c.actions.upperClipSeconds}, {"lower_clip_seconds", c.actions.lowerClipSeconds},
        {"shot_action_id", c.actions.shotActionId}, {"reload_action_id", c.actions.reloadActionId},
        {"body_meshes", c.bodySubmittedMeshes}, {"weapon_meshes", c.weaponSubmittedMeshes},
        {"pose_revision", c.poseRevision}};
}

ActionJson LocalActionSample(const fps::pvp::WeaponFeedbackObservation& w) {
    return {{"active", w.active}, {"captured", w.inputCaptured}, {"dead", w.dead}, {"life", w.lifeGeneration},
        {"hp", w.hp}, {"ammo", w.magazineAmmo}, {"capacity", w.magazineCapacity}, {"shooting", w.shooting},
        {"reloading", w.reloading}, {"reload_pending", w.reloadPending}, {"reload_animating", w.reloadAnimating},
        {"reload_animation_starts", w.reloadAnimationStarts}, {"submitted", w.submittedActions},
        {"animations", w.animationStarts}, {"last_action", w.lastActionId},
        {"last_action_kind", static_cast<int>(w.lastActionKind)}, {"grounded", w.grounded}, {"yaw", w.yaw},
        {"pitch", w.pitch}, {"mouse_consumptions", w.mouseDeltaConsumeCount}, {"decisions", w.decisionCount},
        {"hits", w.hitDecisions}, {"hit_marker", w.hitMarkerVisible},
        {"last_rejection", static_cast<int>(w.lastRejection)}, {"last_hit_kind", static_cast<int>(w.lastHitKind)},
        {"rejected", w.rejectedDecisions}, {"local_cooldown_blocks", w.localCooldownBlocks},
        {"authority_cooldown_rejections", w.authorityCooldownRejections},
        {"hit_flash", w.hitFlashAlpha}, {"hit_direction", w.hitDirectionVisible}};
}

// Every action of the other player must appear in one contiguous run of
// presented frames; a run that ends and later restarts is a replay.
struct RemoteActionTrack final {
    std::set<std::uint64_t> shots, reloads, endedShots, endedReloads;
    std::uint64_t currentShot{}, currentReload{}, replays{}, frames{}, deathLife{};
    bool jumpStart{}, jumpLoop{}, jumpLand{}, death{}, newLife{};
    double deathClipMaximum{};

    void Observe(const fps::pvp::PlayerPresentationObservation& c) {
        using namespace fps::pvp;
        ++frames;
        const auto run = [this](const bool active, const std::uint64_t id, std::uint64_t& current,
                                std::set<std::uint64_t>& seen, std::set<std::uint64_t>& ended) {
            const std::uint64_t now = active ? id : 0;
            if (current && current != now) { ended.insert(current); current = 0; }
            if (now && !current) {
                if (ended.contains(now)) ++replays;
                seen.insert(now);
                current = now;
            }
        };
        run(c.actions.upper == PlayerUpperAction::Shoot, c.actions.shotActionId, currentShot, shots, endedShots);
        run(c.actions.upper == PlayerUpperAction::Reload, c.actions.reloadActionId, currentReload, reloads, endedReloads);
        jumpStart = jumpStart || c.actions.lower == PlayerLowerAction::JumpStart;
        jumpLoop = jumpLoop || c.actions.lower == PlayerLowerAction::JumpLoop;
        jumpLand = jumpLand || c.actions.lower == PlayerLowerAction::JumpLand;
        if (c.dead && c.actions.lower == PlayerLowerAction::Death) {
            if (!death) deathLife = c.lifeGeneration;
            death = true;
            deathClipMaximum = std::max(deathClipMaximum, c.actions.lowerClipSeconds);
        }
        if (death && !c.dead && c.lifeGeneration > deathLife && c.actions.lower != PlayerLowerAction::Death)
            newLife = true;
    }
    ActionJson Json() const {
        return {{"presented_frames", frames}, {"shot_ids", shots}, {"reload_ids", reloads}, {"replays", replays},
            {"jump_start", jumpStart}, {"jump_loop", jumpLoop}, {"jump_land", jumpLand}, {"death", death},
            {"death_clip_maximum_seconds", deathClipMaximum}, {"new_life_after_death", newLife}};
    }
};

void RunActionShort(const Options& options) {
    using namespace fps::pvp;
    const bool actor = options.role == "create";
    const bool captureMode = options.actionCapture;
    std::filesystem::create_directories(options.output);
    const auto startPath = options.output / "action-start.txt";
    const auto finishedPath = options.output / "action-finished.txt";
    if (actor) { std::filesystem::remove(startPath); std::filesystem::remove(finishedPath); }
    ActionJson evidence{{"passed", false}, {"role", options.role}, {"protocol", AcceptanceProtocolVersion},
        {"mode", captureMode ? "action-capture" : "action-short"}, {"nominal_fps", options.fps},
        {"duration_seconds", options.duration},
        {"scope", "Two real GUI processes with SDL-injected input, real Match and Gateway on localhost. "
                  "Presented is successful renderer submission, not scanout; native OS input is the manual checklist."},
        {"capture", captureMode ? "actual scene GPU readback before UI; functional only, not timing" : "none"},
        {"events", ActionJson::array()}, {"checks", ActionJson::object()}, {"captures", ActionJson::array()},
        {"event_waits", ActionJson::array()}};
    std::vector<ActionJson> frames;
    frames.reserve(static_cast<std::size_t>(options.fps * (options.duration + 25)));
    RemoteActionTrack remote;
    std::uint64_t unscheduledYawFramesSeen{};
    auto save = [&] {
        evidence["remote"] = remote.Json();
        std::ofstream report(options.output / (options.role + "-action.json"));
        report << evidence.dump(2) << '\n';
        std::ofstream trace(options.output / (options.role + "-action-frames.jsonl"));
        for (const auto& frame : frames) trace << frame.dump() << '\n';
        Require(bool(report) && bool(trace), "Cannot write action evidence");
    };
    try {
        PvpApplication application;
        GuiRoom guiRoom(options.output, options.role, options.roomPlayers);
        std::string error;
        Require(application.InitializeContent(options.assetRoot, error), error);
        PvpApplicationOptions graphics;
        graphics.title = "Object FPS PVP action acceptance / " + options.role;
        graphics.gateway = options.gateway;
        graphics.gpuDriver = options.gpu;
        graphics.width = 960;
        graphics.height = 540;
        graphics.vsync = false;
        Require(application.InitializeGraphics(graphics, error), error);
        auto& connection = application.Connection();
        auto* window = application.Platform().NativeWindow();
        evidence["platform"] = PlatformFingerprint::Capture(application, window).Json();
        if (actor) connection.CreateAndJoin(options.gateway);
        else connection.Refresh(options.gateway);

        struct Event { double at; std::string name; };
        // Repeated clicks are scheduled 0.25 s (15 ticks) apart, but a shot is not
        // pushed until the product's local gate is open (its wall-clock cooldown
        // and the newest snapshot tick reaching nextAllowedShotTick), and a reload
        // check waits until the reload has ended. A late-waking host therefore
        // shifts an event instead of failing it; a gate that never opens still
        // fails after one second. The exact 10-tick boundary is a domain test.
        std::deque<Event> schedule;
        if (actor) {
            for (const auto& [at, name] : std::vector<std::pair<double, const char*>>{
                    {.05, "capture-local-draw"}, {.10, "capture"}, {.20, "release"}, {.30, "capture-local-idle"},
                    {.40, "held-shot"}, {.48, "capture-local-shoot"}, {.90, "held-release"},
                    {1.10, "reload"}, {1.15, "key-up"},
                    {1.40, "reload-shot"}, {1.45, "release"}, {1.50, "reload-again"}, {1.55, "key-up"},
                    {1.60, "back-start"}, {2.10, "capture-local-reload"}, {2.40, "back-stop"},
                    {3.00, "reload-done"}, {3.20, "jump"}, {3.25, "key-up"}, {3.40, "air-shot"}, {3.45, "release"},
                    {3.80, "turn-wall"}})
                schedule.push_back({at, name});
            // The airborne shot leaves eleven rounds: fire them all at a wall.
            // The first one also moves and turns in the same frame.
            schedule.push_back({4.0, "combined-wall-shot"});
            schedule.push_back({4.05, "combined-release"});
            for (int shot = 1; shot < 11; ++shot) {
                schedule.push_back({4.0 + .25 * shot, "wall-shot"});
                schedule.push_back({4.05 + .25 * shot, "release"});
            }
            for (const auto& [at, name] : std::vector<std::pair<double, const char*>>{
                    {6.80, "empty-shot"}, {6.85, "release"}, {7.00, "reload-empty"}, {7.05, "key-up"},
                    {8.70, "reload-empty-done"}, {8.80, "turn-back"},
                    {9.00, "kill-shot"}, {9.05, "release"}, {9.25, "kill-shot"}, {9.30, "release"},
                    {9.50, "kill-shot"}, {9.55, "release"}, {9.75, "kill-shot"}, {9.80, "release"},
                    // Window interactions release capture and never become a shot.
                    {10.20, "focus-loss"}, {10.30, "focus-gain-held"}, {10.40, "release"},
                    {10.55, "capture"}, {10.65, "release"}, {10.80, "tab-release"}, {10.90, "tab-key-up"},
                    {11.05, "capture"}, {11.15, "release"}, {11.30, "drag"}, {11.40, "release"},
                    {11.60, "resize"}, {12.20, "capture"}, {12.30, "release"}, {12.35, "capture-local-resize"},
                    // Two seconds of walking reach the wall, as the retired weapon probe did.
                    {12.40, "approach-wall"}, {14.40, "stop"}, {14.50, "capture-local-wall-depth"},
                    // After the target's respawn has been presented: leave and rejoin.
                    {14.80, "escape"}, {14.85, "escape-up"}})
                schedule.push_back({at, name});
        } else {
            schedule = {{.10, "capture"}, {.20, "release"}, {.30, "capture-remote-idle"}};
        }

        bool joined = actor;
        PlayerId playerId{};
        std::string room;
        std::uint64_t frameIndex{}, presentedCount{}, heldBase{}, deathLife{}, allocatedAtDeath{};
        std::uint64_t injectedShots{}, injectedActions{}, beforeCombinedMouse{}, allocatedBeforeEscape{};
        bool cooldownDecision{}, wallDecision{}, hitMarkerSeen{}, localFeedbackBeforeDecision{};
        bool combinedChecked{}, combinedNextChecked{}, rejoining{}, rejoined{};
        float beforeCombinedYaw{}, beforeCombinedPitch{}, combinedYaw{}, combinedPitch{};
        std::optional<ActionId> awaitingPresentedAction;
        std::optional<Clock::time_point> firstRejoined;
        std::vector<double> feedbackDelays;
        std::optional<double> waitingSince;
        static constexpr double MaximumEventWaitSeconds = 1;
        double maximumFrameGap{}, savedYaw{}, backDisplacement{}, unscheduledYaw{};
        std::uint64_t unscheduledYawFrames{};
        bool reloadAnimatingSeen{}, died{}, deadChecked{}, respawned{}, respawnShot{};
        bool remoteDeathScheduled{}, remoteReloadCaptured{}, remoteJumpCaptured{}, remoteShotCaptured{};
        bool remoteHitCaptured{}, localHitCaptured{};
        std::optional<double> measurementStart;
        Engine::Math::Vec3 backStart{}, deathPosition{};
        std::optional<std::string> pendingCapture;
        const auto started = Clock::now();
        auto previous = started, refreshed = started;
        const auto schedulePush = [&](const double at, std::string name) {
            const auto position = std::find_if(schedule.begin(), schedule.end(),
                [&](const Event& event) { return event.at > at; });
            schedule.insert(position, Event{at, std::move(name)});
        };
        for (;;) {
            const auto now = Clock::now();
            Require(Seconds(now, started) < options.duration + 25, "Action short probe exceeded bounded deadline");
            const double stamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            auto state = connection.State();
            Require(state.error.empty(), "Action connection failed: " + state.error);
            if (!joined && state.phase == ConnectionPhase::Lobby) {
                if (!state.rooms.empty()) {
                    room = state.rooms.front().id;
                    connection.Join(options.gateway, room);
                    joined = true;
                } else if (Seconds(now, refreshed) >= .3) { connection.Refresh(options.gateway); refreshed = now; }
            }
            if (const auto peer = guiRoom.Observe(state)) application.ObserveRemote(peer);
            if (guiRoom.Full(state) && !playerId) {
                playerId = state.playerId;
                evidence["player_id"] = playerId;
                if (actor) {
                    measurementStart = stamp + .25;
                    std::ofstream start(startPath);
                    start << std::setprecision(17) << *measurementStart;
                    Require(bool(start), "Cannot signal action start");
                    // The actor holds real OS focus; the target never raises,
                    // so no real focus loss interrupts the actor's capture.
                    SDL_RaiseWindow(window);
                }
            }
            if (!actor && !measurementStart && std::filesystem::exists(startPath)) {
                std::ifstream start(startPath);
                double value{};
                if (start >> value) measurementStart = value;
            }
            const double elapsed = measurementStart ? stamp - *measurementStart : -1;
            if (actor && elapsed >= options.duration) break;
            if (!actor && measurementStart && std::filesystem::exists(finishedPath) &&
                (respawnShot || elapsed >= options.duration + 6)) break;

            const auto before = application.WeaponFeedback();
            const auto beforeAllocated = state.actionTransport.allocatedThrough;
            const auto beforePosition = application.LocalMovement().predictedPosition;
            std::string event;
            bool shotExpected{}, reloadExpected{}, silenceExpected{}, captureExpected{}, releaseExpected{};
            std::string waitReason;
            if (measurementStart && !schedule.empty() && elapsed >= schedule.front().at) {
                const auto& due = schedule.front().name;
                const bool shot = due == "held-shot" || due == "air-shot" || due == "combined-wall-shot" ||
                    due == "wall-shot" || due == "respawn-shot" || (due == "kill-shot" && !remote.death);
                if (shot && !state.snapshot) waitReason = "no snapshot";
                else if (shot) {
                    const auto own = std::find_if(state.snapshot->combat.begin(), state.snapshot->combat.end(),
                        [&](const CombatState& combat) { return combat.playerId == state.playerId; });
                    if (before.cooldownRemainingSeconds > 0) waitReason = "local cooldown";
                    else if (own == state.snapshot->combat.end() || state.snapshot->tick < own->nextAllowedShotTick)
                        waitReason = "snapshot tick before nextAllowedShotTick";
                } else if ((due == "reload-done" || due == "reload-empty-done") && (before.reloading || before.reloadPending))
                    waitReason = "reload still running";
            }
            if (!waitReason.empty()) {
                if (!waitingSince) waitingSince = elapsed;
                Require(elapsed - *waitingSince <= MaximumEventWaitSeconds, "Event '" + schedule.front().name +
                    "' waited more than one second for: " + waitReason);
            } else if (measurementStart && !schedule.empty() && elapsed >= schedule.front().at) {
                event = schedule.front().name;
                if (waitingSince || elapsed - schedule.front().at > .05)
                    evidence["event_waits"].push_back({{"name", event}, {"scheduled_seconds", schedule.front().at},
                        {"fired_seconds", elapsed}, {"waited_for_gate", waitingSince.has_value()}});
                waitingSince.reset();
                schedule.pop_front();
                if (event == "capture") {
                    PushWindowEvent(window, SDL_EVENT_WINDOW_FOCUS_GAINED);
                    PushMouse(window, true);
                    silenceExpected = captureExpected = true;
                } else if (event == "release") PushMouse(window, false);
                else if (event == "key-up") {
                    for (const auto key : {SDL_SCANCODE_R, SDL_SCANCODE_SPACE, SDL_SCANCODE_W, SDL_SCANCODE_S})
                        PushKey(window, key, false);
                } else if (event == "held-shot") {
                    heldBase = before.submittedActions;
                    PushMouse(window, true);
                    shotExpected = true;
                } else if (event == "held-release") {
                    Require(before.submittedActions == heldBase + 1 && before.animationStarts == heldBase + 1,
                        "Holding fire repeated a shot or animation");
                    evidence["checks"]["held_fire_one_shot"] = true;
                    PushMouse(window, false);
                } else if (event == "reload" || event == "reload-empty") {
                    PushKey(window, SDL_SCANCODE_R, true);
                    reloadExpected = true;
                } else if (event == "reload-shot" || event == "reload-again") {
                    Require(before.reloading || before.reloadPending, "Reload was not active for its suppression check");
                    if (event == "reload-shot") PushMouse(window, true);
                    else PushKey(window, SDL_SCANCODE_R, true);
                    silenceExpected = true;
                } else if (event == "back-start") {
                    Require(before.reloading, "Authority reload had not started before the moving reload");
                    backStart = beforePosition;
                    PushKey(window, SDL_SCANCODE_S, true);
                } else if (event == "back-stop") {
                    PushKey(window, SDL_SCANCODE_S, false);
                    backDisplacement = std::hypot(double(beforePosition.x - backStart.x), double(beforePosition.z - backStart.z));
                    Require(backDisplacement > .2 && before.reloading && reloadAnimatingSeen,
                        "Moving during the reload did not move, or the reload stopped or was never animated");
                    evidence["checks"]["move_while_reloading"] = true;
                } else if (event == "reload-done") {
                    Require(!before.reloading && before.magazineAmmo == before.magazineCapacity &&
                        before.reloadAnimationStarts == 1 && !before.reloadAnimating,
                        "First reload did not complete with one authority-anchored animation");
                    evidence["checks"]["reload_completes_and_refills"] = true;
                } else if (event == "jump") PushKey(window, SDL_SCANCODE_SPACE, true);
                else if (event == "air-shot") {
                    Require(!before.grounded, "Authority still grounded at the airborne shot");
                    PushMouse(window, true);
                    shotExpected = true;
                } else if (event == "turn-wall") {
                    savedYaw = before.yaw;
                    PushMotion(window, static_cast<float>((std::atan2(5., 5.) - before.yaw) / .0025), 0);
                } else if (event == "turn-back") {
                    PushMotion(window, static_cast<float>((savedYaw - before.yaw) / .0025), 0);
                } else if (event == "combined-wall-shot") {
                    // Move, turn and shoot in one frame: the mouse delta is consumed exactly once.
                    beforeCombinedYaw = before.yaw;
                    beforeCombinedPitch = before.pitch;
                    beforeCombinedMouse = before.mouseDeltaConsumeCount;
                    PushMotion(window, 4, 4);
                    PushKey(window, SDL_SCANCODE_W, true);
                    PushMouse(window, true);
                    shotExpected = true;
                } else if (event == "combined-release") {
                    PushMouse(window, false);
                    PushKey(window, SDL_SCANCODE_W, false);
                } else if (event == "focus-loss") {
                    PushWindowEvent(window, SDL_EVENT_WINDOW_FOCUS_LOST);
                    PushMouse(window, true);
                    silenceExpected = releaseExpected = true;
                } else if (event == "focus-gain-held") {
                    PushWindowEvent(window, SDL_EVENT_WINDOW_FOCUS_GAINED);
                    silenceExpected = releaseExpected = true;
                } else if (event == "tab-release") {
                    PushKey(window, SDL_SCANCODE_TAB, true);
                    PushMouse(window, true);
                    silenceExpected = releaseExpected = true;
                } else if (event == "tab-key-up") {
                    PushKey(window, SDL_SCANCODE_TAB, false);
                    PushMouse(window, false);
                } else if (event == "drag") {
                    PushWindowEvent(window, SDL_EVENT_WINDOW_MOVED);
                    PushMouse(window, true);
                    silenceExpected = releaseExpected = true;
                } else if (event == "resize") {
                    Require(SDL_SetWindowSize(window, 1120, 630), SDL_GetError());
                } else if (event == "approach-wall") {
                    PushMotion(window, static_cast<float>((std::atan2(5., 5.) - before.yaw) / .0025), 0);
                    PushKey(window, SDL_SCANCODE_W, true);
                } else if (event == "stop") {
                    PushKey(window, SDL_SCANCODE_W, false);
                } else if (event == "escape") {
                    Require(before.hitDecisions >= 1 && hitMarkerSeen && wallDecision && cooldownDecision,
                        "Hit, hit marker, wall or authority cooldown outcome was not observed before leaving");
                    allocatedBeforeEscape = beforeAllocated;
                    Require(allocatedBeforeEscape == before.submittedActions + injectedActions,
                        "Action IDs do not equal SDL submissions plus the explicit authority injection");
                    evidence["before_escape"] = WeaponSample(before);
                    PushKey(window, SDL_SCANCODE_ESCAPE, true);
                    rejoining = true;
                } else if (event == "escape-up") {
                    PushKey(window, SDL_SCANCODE_ESCAPE, false);
                } else if (event == "wall-shot") {
                    PushMouse(window, true);
                    shotExpected = true;
                } else if (event == "empty-shot") {
                    Require(before.magazineAmmo == 0, "Magazine was not empty for the empty-magazine check");
                    PushMouse(window, true);
                    silenceExpected = true;
                } else if (event == "reload-empty-done") {
                    Require(!before.reloading && before.magazineAmmo == before.magazineCapacity &&
                        before.reloadAnimationStarts == 2, "Empty-magazine reload did not complete");
                    evidence["checks"]["empty_magazine_reload"] = true;
                } else if (event == "kill-shot") {
                    if (remote.death) evidence["checks"]["kill_shot_skipped_target_dead"] = true;
                    else { PushMouse(window, true); shotExpected = true; }
                } else if (event == "dead-click") {
                    PushMouse(window, true);
                    silenceExpected = true;
                } else if (event == "dead-move") {
                    for (const auto key : {SDL_SCANCODE_W, SDL_SCANCODE_SPACE, SDL_SCANCODE_R}) PushKey(window, key, true);
                    silenceExpected = true;
                } else if (event == "dead-stop") {
                    for (const auto key : {SDL_SCANCODE_W, SDL_SCANCODE_SPACE, SDL_SCANCODE_R}) PushKey(window, key, false);
                    const double moved = std::hypot(double(beforePosition.x - deathPosition.x),
                                                    double(beforePosition.z - deathPosition.z));
                    Require(before.dead && moved < .05 && beforeAllocated == allocatedAtDeath,
                        "Dead player moved or allocated an action");
                    evidence["checks"]["dead_suppresses_move_jump_fire_reload"] = true;
                    evidence["dead_horizontal_displacement"] = moved;
                    deadChecked = true;
                } else if (event == "respawn-capture") {
                    if (!before.inputCaptured) {
                        PushWindowEvent(window, SDL_EVENT_WINDOW_FOCUS_GAINED);
                        PushMouse(window, true);
                        silenceExpected = captureExpected = true;
                    }
                } else if (event == "respawn-shot") {
                    PushMouse(window, true);
                    shotExpected = true;
                } else if (event.starts_with("capture-")) {
                    if (captureMode) pendingCapture = event;
                }
            }
            const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
            previous = now;
            if (elapsed >= 0) maximumFrameGap = std::max(maximumFrameGap, frame.deltaSeconds);
            Require(application.ProcessEvents(frame) == Control::Continue, "Action acceptance window closed");
            Require(application.Update(frame) == Control::Continue, application.LastError());
            const auto after = application.WeaponFeedback();
            const auto afterAllocated = connection.State().actionTransport.allocatedThrough;
            if (shotExpected) {
                Require(after.submittedActions == before.submittedActions + 1 &&
                    after.animationStarts == before.animationStarts + 1 && after.shooting &&
                    after.lastActionKind == ActionKind::Shot,
                    "Shot edge '" + event + "' did not submit exactly one locally animated shot (local cooldown " +
                    std::to_string(before.cooldownRemainingSeconds) + " s, ammo " + std::to_string(before.magazineAmmo) + ")");
                if (event == "respawn-shot") { respawnShot = true; evidence["checks"]["respawned_player_can_shoot"] = true; }
                ++injectedShots;
                awaitingPresentedAction = after.lastActionId;
                if (event == "held-shot") {
                    // A second request inside the authority cooldown is an explicit
                    // transport injection, not another local click.
                    const auto current = connection.State();
                    Require(current.snapshot.has_value(), "No reference for the cooldown injection");
                    const auto id = connection.SubmitShot(current.snapshot->tick, after.yaw, after.pitch);
                    Require(id && *id == after.lastActionId + 1, "Cooldown injection was not allocated contiguously");
                    evidence["cooldown_injection_action_id"] = *id;
                    ++injectedActions;
                }
            }
            if (event == "combined-wall-shot") {
                Require(std::abs(after.yaw - (beforeCombinedYaw + .01F)) < .0001F &&
                    std::abs(after.pitch - (beforeCombinedPitch + .01F)) < .0001F,
                    "Movement/turn/shot frame applied the raw mouse delta other than once");
                Require(after.mouseDeltaConsumeCount == beforeCombinedMouse + 1,
                    "Combined frame did not consume exactly one nonzero mouse delta");
                combinedYaw = after.yaw;
                combinedPitch = after.pitch;
                combinedChecked = true;
                evidence["checks"]["movement_turn_shoot_mouse_once"] = true;
            } else if (combinedChecked && !combinedNextChecked && event.empty()) {
                Require(std::abs(after.yaw - combinedYaw) < .0001F && std::abs(after.pitch - combinedPitch) < .0001F,
                    "Shot animation or recoil replayed the mouse delta on the next frame");
                combinedNextChecked = true;
                evidence["checks"]["mouse_delta_not_replayed"] = true;
            }
            if (reloadExpected) {
                Require(after.submittedActions == before.submittedActions + 1 && after.lastActionKind == ActionKind::Reload &&
                    after.reloadPending, "Reload key did not submit exactly one reload");
            }
            if (silenceExpected) {
                Require(after.submittedActions == before.submittedActions && afterAllocated == beforeAllocated,
                    "Suppressed input '" + event + "' allocated or submitted an action");
                if (captureExpected) Require(after.inputCaptured, "Content click did not capture input");
                if (releaseExpected) Require(!after.inputCaptured, "Window, Tab or focus interaction retained capture");
                evidence["checks"][event + "_suppressed"] = true;
            }
            cooldownDecision = cooldownDecision || after.lastRejection == ShotRejection::Cooldown;
            wallDecision = wallDecision || (after.lastHitKind == ShotHitKind::World && after.lastRejection == ShotRejection::None);
            hitMarkerSeen = hitMarkerSeen || after.hitMarkerVisible;
            if (rejoining && !rejoined && connection.State().phase == ConnectionPhase::Lobby) {
                Require(!after.active && !after.shooting && after.submittedActions == 0 && after.animationStarts == 0 &&
                    after.decisionCount == 0 && after.lastActionId == 0, "ESC retained weapon actions, animation or decisions");
                const auto lobby = connection.State();
                if (!lobby.rooms.empty()) {
                    room = lobby.rooms.front().id;
                    connection.Join(options.gateway, room);
                    rejoined = true;
                    evidence["checks"]["escape_clears_weapon"] = true;
                }
            }
            if (rejoined && after.active && connection.State().playerId != playerId) {
                if (!firstRejoined) firstRejoined = now;
                Require(after.hp == after.maximumHp && after.magazineAmmo == after.magazineCapacity &&
                    after.submittedActions == 0 && after.animationStarts == 0 && after.decisionCount == 0 &&
                    !after.shooting && !after.hitMarkerVisible,
                    "Rejoin retained old damage, ammo, shot animation or decision effects");
                evidence["rejoined_player_id"] = connection.State().playerId;
                evidence["checks"]["rejoin_full_hp_no_stale_effects"] = true;
            }
            reloadAnimatingSeen = reloadAnimatingSeen || after.reloadAnimating;
            // Only scripted turns may change the aim. Any other change is real
            // mouse motion reaching the captured window: external input, not
            // product behaviour, and it invalidates the aimed shots.
            if (measurementStart && event != "turn-wall" && event != "turn-back" && event != "combined-wall-shot" &&
                event != "approach-wall" && !rejoining && before.active && after.active &&
                std::abs(after.yaw - before.yaw) > 1e-6F) {
                ++unscheduledYawFrames;
                unscheduledYawFramesSeen = unscheduledYawFrames;
                unscheduledYaw += std::abs(after.yaw - before.yaw);
            }
            // The simulation role reconciles the death on its own step: wait until
            // the local movement shows it before taking the dead position.
            if (!actor && !died && after.dead && application.LocalMovement().lifeState == LifeState::Dead) {
                died = true;
                deathLife = after.lifeGeneration;
                allocatedAtDeath = afterAllocated;
                deathPosition = application.LocalMovement().predictedPosition;
                evidence["death_elapsed_seconds"] = elapsed;
                schedulePush(elapsed + .3, "dead-click");
                schedulePush(elapsed + .35, "release");
                schedulePush(elapsed + .4, "dead-move");
                schedulePush(elapsed + .9, "dead-stop");
            }
            if (!actor && died && !respawned && after.active && after.lifeGeneration > deathLife) {
                respawned = true;
                Require(after.hp == after.maximumHp && after.magazineAmmo == after.magazineCapacity && !after.dead,
                    "Respawned life did not start with full HP and magazine");
                evidence["checks"]["respawn_full_hp_and_magazine"] = true;
                evidence["respawn_elapsed_seconds"] = elapsed;
                schedulePush(elapsed + .3, "respawn-capture");
                schedulePush(elapsed + .4, "release");
                schedulePush(elapsed + .7, "respawn-shot");
                schedulePush(elapsed + .75, "release");
            }
            if (pendingCapture) application.Renderer().RequestSceneCapture();
            Require(application.Render(frame) == Control::Continue, application.LastError());
            ActionJson sample{{"frame_id", frame.frameIndex}, {"seconds", elapsed}, {"frame_seconds", frame.deltaSeconds},
                {"event", event}, {"local", LocalActionSample(after)}, {"presented", false}};
            if (const auto& presented = application.PresentedMovement()) {
                Require(presented->frameId == frame.frameIndex, "Stale successful Presented observation");
                sample["presented"] = true;
                const auto& weapon = presented->weapon;
                sample["presented_weapon"] = {{"active", weapon.active}, {"dead", weapon.dead},
                    {"life", weapon.lifeGeneration}, {"submitted_meshes", weapon.submittedMeshes},
                    {"shooting", weapon.shooting}, {"last_action", weapon.lastActionId}};
                if (weapon.active && !weapon.dead)
                    Require(weapon.meshCount > 0 && weapon.materialCount > 0 && weapon.submittedMeshes > 0,
                        "Successful world presentation omitted the prepared first-person weapon");
                if (awaitingPresentedAction) {
                    Require(weapon.shooting && weapon.lastActionId == *awaitingPresentedAction,
                        "Local shot feedback was absent from its first successful presentation");
                    localFeedbackBeforeDecision = localFeedbackBeforeDecision ||
                        weapon.lastDecisionActionId < *awaitingPresentedAction;
                    const double delay = presented->hostSteadySeconds - weapon.lastSubmittedSeconds;
                    sample["submission_to_presented_seconds"] = delay;
                    feedbackDelays.push_back(delay);
                    awaitingPresentedAction.reset();
                }
                if (presented->remote && presented->remote->character.ready) {
                    const auto& character = presented->remote->character;
                    Require(character.bodySubmittedMeshes > 0 && character.weaponSubmittedMeshes > 0,
                        "Remote character or its weapon was not submitted");
                    remote.Observe(character);
                    sample["remote"] = RemoteActionSample(character);
                    if (captureMode && measurementStart && !pendingCapture) {
                        using fps::pvp::PlayerLowerAction;
                        using fps::pvp::PlayerUpperAction;
                        if (!actor && !remoteReloadCaptured && character.actions.upper == PlayerUpperAction::Reload &&
                            character.actions.upperClipSeconds >= .8) {
                            remoteReloadCaptured = true; pendingCapture = "capture-remote-reload";
                        } else if (!actor && !remoteJumpCaptured && character.actions.lower == PlayerLowerAction::JumpLoop) {
                            remoteJumpCaptured = true; pendingCapture = "capture-remote-jump";
                        } else if (!actor && !remoteShotCaptured && character.actions.upper == PlayerUpperAction::Shoot &&
                                   character.actions.upperClipSeconds >= .15) {
                            remoteShotCaptured = true; pendingCapture = "capture-remote-shot";
                        } else if (actor && !remoteHitCaptured && character.actions.upper == PlayerUpperAction::Hit &&
                                   character.actions.upperClipSeconds >= .1) {
                            // The actor's shot struck the target: its upper body reacts.
                            remoteHitCaptured = true; pendingCapture = "capture-remote-hit";
                        }
                    }
                    if (actor && remote.death && !remoteDeathScheduled) {
                        remoteDeathScheduled = true;
                        schedulePush(elapsed + 1.0, "capture-remote-death");
                        schedulePush(elapsed + 2.6, "capture-remote-death-hold");
                    }
                }
                ++presentedCount;
            }
            // The struck target's own first-person feedback: red flash and direction arc.
            if (captureMode && measurementStart && !actor && !localHitCaptured && !pendingCapture &&
                after.hitFlashAlpha > 0) {
                localHitCaptured = true; pendingCapture = "capture-local-hit";
            }
            if (!event.empty()) evidence["events"].push_back({{"name", event}, {"seconds", elapsed},
                {"before", LocalActionSample(before)}, {"after", LocalActionSample(after)}});
            if (auto capture = application.Renderer().TakeSceneCapture()) {
                Require(captureMode && pendingCapture, "Unexpected GPU readback");
                const auto file = options.role + "-" + *pendingCapture + ".bmp";
                SaveCapture(*capture, options.output / file);
                ActionJson record{{"file", file}, {"width", capture->width}, {"height", capture->height},
                    {"frame_id", frame.frameIndex}, {"local", LocalActionSample(application.WeaponFeedback())}};
                if (sample.contains("remote")) record["remote"] = sample["remote"];
                evidence["captures"].push_back(std::move(record));
                pendingCapture.reset();
            }
            Require(frames.size() < frames.capacity(), "Bounded action frame buffer exhausted");
            frames.push_back(std::move(sample));
            const double remaining = 1 / options.fps - Seconds(Clock::now(), now);
            if (remaining > 0) SDL_DelayNS(static_cast<Uint64>(remaining * 1e9));
        }
        evidence["presentation_frames"] = presentedCount;
        evidence["unscheduled_yaw_frames"] = unscheduledYawFrames;
        evidence["unscheduled_yaw_radians"] = unscheduledYaw;
        evidence["disturbed"] = unscheduledYawFrames > 0;
        evidence["maximum_frame_seconds"] = maximumFrameGap;
        evidence["back_displacement"] = backDisplacement;
        evidence["injected_sdl_shots"] = injectedShots;
        evidence["shot_to_first_presented_seconds"] = feedbackDelays;
        evidence["skipped_frames"] = application.SkippedPresentationFrames();
        evidence["final_local"] = LocalActionSample(application.WeaponFeedback());
        Require(presentedCount >= 10, "Insufficient successful real GUI presentations");
        Require(remote.replays == 0, "A remote action replayed after its presentation run ended");
        evidence["checks"]["remote_actions_never_replay"] = true;
        if (actor) {
            Require(schedule.empty(), "Actor schedule did not complete; inspect raw events");
            Require(cooldownDecision && wallDecision && hitMarkerSeen && localFeedbackBeforeDecision &&
                combinedChecked && combinedNextChecked, "Weapon coverage incomplete; inspect raw events");
            evidence["checks"]["authority_cooldown_wall_and_hit"] = true;
            evidence["checks"]["local_feedback_before_decision"] = true;
            Require(rejoined && firstRejoined && Seconds(Clock::now(), *firstRejoined) >= .3,
                "ESC and rejoin were not observed for at least 0.3 seconds");
            Require(remote.death && remote.deathClipMaximum > 2.3 && remote.newLife,
                "Remote target death, held death pose and new life were not all presented");
            evidence["checks"]["remote_death_held_then_new_life"] = true;
        } else {
            Require(remote.shots.size() >= 10 && remote.reloads.size() == 2,
                "Remote actor shots and both reloads were not presented");
            Require(remote.jumpStart && remote.jumpLoop && remote.jumpLand, "Remote jump phases were not all presented");
            Require(died && deadChecked && respawned && respawnShot, "Target death, suppression or respawn coverage incomplete");
            evidence["checks"]["remote_shot_reload_and_jump_presented"] = true;
        }
        if (captureMode) Require(evidence["captures"].size() == (actor ? 9U : 5U), "Action GPU captures were not all produced");
        evidence["passed"] = true;
        save();
        if (actor) { std::ofstream finished(finishedPath); finished << "Actor action probe complete\n"; }
        connection.Leave();
        std::cout << options.role << ": action short evidence passed; " << presentedCount << " successful presentations\n";
    } catch (const std::exception& exception) {
        evidence["error"] = exception.what();
        evidence["unscheduled_yaw_frames"] = unscheduledYawFramesSeen;
        evidence["disturbed"] = unscheduledYawFramesSeen > 0;
        save();
        // Let the target finish its own bounded run and save its report too.
        if (actor) { std::ofstream finished(finishedPath); finished << "Actor action probe failed\n"; }
        throw;
    }
}
