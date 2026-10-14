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

// Rapid-fire record for the C2 comparison (v7 batch 09, step S5). Clicks are
// injected at the SDL layer, so every click takes PvpApplication's normal path
// and its real local fire gate decides it; nothing is submitted around the
// gate and no authority rejection is injected (unlike the action short above,
// whose create role deliberately submits one cooldown request; its report names
// it in "cooldown_injection_action_id"). The actor captures input, turns to
// the wall used by the action short and clicks in segments of eleven submitted
// shots, each followed by a reload. Scenarios:
//   intervalN  one click every N authority ticks from the segment start (four segments);
//   edge       one click every frame for 5 s of clicking (the gate decides);
//   pause      the runner stops the Gateway for 250 ms, then 6 s of clicks every frame.
// Record only: the probe fails on its own bounded schedule, never on a decision.
// The product exposes per frame only the newest decision and its counters, so
// when one frame drains several decisions only the newest is exact; the others
// are marked ambiguous unless the counters prove them all accepted.
struct RapidClickPlan final {
    std::string scenario;
    unsigned intervalTicks{};   // 0 = every frame
    unsigned segments{};        // interval scenarios stop after this many segments
    double clickingSeconds{};   // every-frame scenarios stop after this much clicking time
    bool clean{true};
};

RapidClickPlan PlanRapidClicks(const std::string& scenario) {
    RapidClickPlan plan{scenario};
    if (scenario.starts_with("interval")) {
        plan.intervalTicks = static_cast<unsigned>(std::stoul(scenario.substr(8)));
        plan.segments = 4;
    } else if (scenario == "edge") plan.clickingSeconds = 5;
    else if (scenario == "pause") { plan.clickingSeconds = 6; plan.clean = false; }
    return plan;
}

void RunActionRapid(const Options& options) {
    using namespace fps::pvp;
    const bool actor = options.role == "create";
    const auto plan = PlanRapidClicks(options.actionRapid);
    static constexpr unsigned ShotsPerSegment = 11;
    static constexpr double MaximumWaitSeconds = 3;
    std::filesystem::create_directories(options.output);
    const auto startPath = options.output / "rapid-start.txt";
    const auto finishedPath = options.output / "rapid-finished.txt";
    const auto pauseReadyPath = options.output / "rapid-pause-ready.txt";
    const auto pausePath = options.output / "rapid-pause.json";
    if (actor) for (const auto& path : {startPath, finishedPath, pauseReadyPath}) std::filesystem::remove(path);
    ActionJson evidence{{"passed", false}, {"role", options.role}, {"protocol", AcceptanceProtocolVersion},
        {"mode", "action-rapid"}, {"scenario", plan.scenario}, {"nominal_fps", options.fps},
        {"interval_ticks", plan.intervalTicks}, {"segments_planned", plan.segments},
        {"clicking_seconds_planned", plan.clickingSeconds}, {"shots_per_segment", ShotsPerSegment},
        {"clean", plan.clean}, {"deliberate_cooldown_injections", ActionJson::array()},
        {"scope", "Two real GUI processes, SDL-injected clicks through PvpApplication's own fire gate, real Match and "
                  "Gateway on localhost. Record only: no verdict on decisions."}};
    std::vector<ActionJson> frames, clicks;
    frames.reserve(static_cast<std::size_t>(options.fps * (options.duration + 25)));
    std::uint64_t unscheduledYawFrames{};
    double unscheduledYaw{};
    auto save = [&] {
        std::ofstream report(options.output / (options.role + "-rapid.json"));
        report << evidence.dump(2) << '\n';
        std::ofstream trace(options.output / (options.role + "-rapid-frames.jsonl"));
        for (const auto& frame : frames) trace << frame.dump() << '\n';
        std::ofstream clickTrace(options.output / (options.role + "-rapid-clicks.jsonl"));
        for (const auto& click : clicks) clickTrace << click.dump() << '\n';
        Require(bool(report) && bool(trace) && bool(clickTrace), "Cannot write rapid evidence");
    };
    try {
        PvpApplication application;
        GuiRoom guiRoom(options.output, options.role, options.roomPlayers);
        std::string error;
        Require(application.InitializeContent(options.assetRoot, error), error);
        PvpApplicationOptions graphics;
        graphics.title = "Object FPS PVP rapid-fire acceptance / " + options.role;
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

        enum class Phase { Setup, AwaitPause, Clicking, AwaitLastDecision, ReloadKey, Reloading, Settle, Draining, Done };
        const auto phaseName = [](const Phase phase) {
            switch (phase) {
            case Phase::Setup: return "setup";
            case Phase::AwaitPause: return "await-pause";
            case Phase::Clicking: return "clicking";
            case Phase::AwaitLastDecision: return "await-last-decision";
            case Phase::ReloadKey: return "reload-key";
            case Phase::Reloading: return "reloading";
            case Phase::Settle: return "settle";
            case Phase::Draining: return "draining";
            case Phase::Done: return "done";
            }
            return "invalid";
        };
        Phase phase = Phase::Setup;
        double phaseSince{};
        const auto enter = [&](const Phase next, const double at) { phase = next; phaseSince = at; };
        bool joined = actor, setupCaptured{}, setupReleased{}, setupTurned{};
        PlayerId playerId{};
        std::string room;
        std::optional<double> measurementStart;
        std::uint64_t frameIndex{}, presentedCount{};
        double maximumFrameGap{}, clickingSeconds{}, segmentStart{};
        unsigned segment{}, segmentClicks{}, segmentSent{};
        // Decisions arrive in action-ID order; decidedThrough is the newest drained ID.
        ActionId decidedThrough{}, previousSent{}, lastSent{};
        std::map<ActionId, std::size_t> clickOfAction;
        std::set<ActionId> reloadActions;
        std::uint64_t attributionMismatches{}, decisionOrderViolations{}, reloadsSubmitted{}, lostCapture{};
        std::optional<WeaponFeedbackObservation> baseline;
        const auto started = Clock::now();
        auto previous = started, refreshed = started;
        for (;;) {
            const auto now = Clock::now();
            Require(Seconds(now, started) < options.duration + 25, "Rapid probe exceeded bounded deadline");
            const double stamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            auto state = connection.State();
            Require(state.error.empty(), "Rapid connection failed: " + state.error);
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
                    Require(bool(start), "Cannot signal rapid start");
                    SDL_RaiseWindow(window);
                }
            }
            if (!actor && !measurementStart && std::filesystem::exists(startPath)) {
                std::ifstream start(startPath);
                double value{};
                if (start >> value) measurementStart = value;
            }
            const double elapsed = measurementStart ? stamp - *measurementStart : -1;
            if (actor && phase == Phase::Done) break;
            if (!actor && measurementStart && std::filesystem::exists(finishedPath)) break;

            const auto before = application.WeaponFeedback();
            std::string event;
            bool click{};
            ActionJson clickRecord;
            if (actor && measurementStart && elapsed >= 0) {
                if (!baseline && before.active) baseline = before;
                const auto waited = elapsed - phaseSince;
                switch (phase) {
                case Phase::Setup:
                    if (!setupCaptured && elapsed >= .10) {
                        PushWindowEvent(window, SDL_EVENT_WINDOW_FOCUS_GAINED);
                        PushMouse(window, true);
                        setupCaptured = true; event = "capture";
                    } else if (setupCaptured && !setupReleased && elapsed >= .20) {
                        PushMouse(window, false);
                        setupReleased = true; event = "release";
                    } else if (setupReleased && !setupTurned && elapsed >= .30) {
                        // The wall the action short's eleven wall shots hit.
                        PushMotion(window, static_cast<float>((std::atan2(5., 5.) - before.yaw) / .0025), 0);
                        setupTurned = true; event = "turn-wall";
                    } else if (setupTurned && elapsed >= .60) {
                        Require(before.active && before.inputCaptured && before.magazineAmmo == before.magazineCapacity,
                            "Rapid setup did not capture input with a full magazine");
                        if (plan.clean) { enter(Phase::Clicking, elapsed); segmentStart = elapsed; }
                        else {
                            std::ofstream ready(pauseReadyPath);
                            ready << std::setprecision(17) << stamp;
                            Require(bool(ready), "Cannot signal pause readiness");
                            enter(Phase::AwaitPause, elapsed);
                        }
                        event = "setup-done";
                    }
                    break;
                case Phase::AwaitPause:
                    Require(waited <= 5, "Runner did not release the Gateway pause within 5 s");
                    if (std::filesystem::exists(pausePath)) {
                        std::ifstream pause(pausePath);
                        ActionJson fault = ActionJson::parse(pause, nullptr, false);
                        if (!fault.is_discarded()) {
                            evidence["gateway_pause"] = fault;
                            evidence["gateway_pause_observed_seconds"] = elapsed;
                            enter(Phase::Clicking, elapsed);
                            segmentStart = elapsed;
                            event = "pause-released";
                        }
                    }
                    break;
                case Phase::Clicking: {
                    const double due = plan.intervalTicks ?
                        segmentStart + segmentClicks * double(plan.intervalTicks) / AuthorityTickRate : elapsed;
                    if (elapsed >= due) {
                        click = true;
                        const CombatState* own{};
                        if (state.snapshot)
                            for (const auto& combat : state.snapshot->combat)
                                if (combat.playerId == state.playerId) own = &combat;
                        clickRecord = {{"index", clicks.size()}, {"segment", segment}, {"segment_click", segmentClicks},
                            {"scheduled_seconds", plan.intervalTicks ? ActionJson(due) : ActionJson(nullptr)},
                            {"seconds", elapsed}, {"host_steady_seconds", stamp}, {"frame_id", frameIndex},
                            {"ammo_before", before.magazineAmmo}, {"captured_before", before.inputCaptured},
                            {"snapshot_tick", state.snapshot ? ActionJson(state.snapshot->tick) : ActionJson(nullptr)},
                            {"next_allowed_shot_tick", own ? ActionJson(own->nextAllowedShotTick) : ActionJson(nullptr)},
                            {"local_cooldown_remaining_seconds", before.cooldownRemainingSeconds},
                            {"previous_sent_action", previousSent ? ActionJson(previousSent) : ActionJson(nullptr)}};
                        PushMouse(window, true);
                        PushMouse(window, false);
                        ++segmentClicks;
                        event = "click";
                    }
                    break;
                }
                case Phase::AwaitLastDecision:
                    Require(waited <= MaximumWaitSeconds, "The segment's last shot decision did not arrive within 3 s");
                    if (decidedThrough >= lastSent && before.cooldownRemainingSeconds <= 0) {
                        PushKey(window, SDL_SCANCODE_R, true);
                        enter(Phase::ReloadKey, elapsed);
                        event = "reload";
                    }
                    break;
                case Phase::ReloadKey:
                    PushKey(window, SDL_SCANCODE_R, false);
                    enter(Phase::Reloading, elapsed);
                    event = "reload-key-up";
                    break;
                case Phase::Reloading:
                    Require(waited <= MaximumWaitSeconds, "The segment reload did not complete within 3 s");
                    if (!before.reloading && !before.reloadPending && before.magazineAmmo == before.magazineCapacity &&
                        decidedThrough >= before.lastActionId) {
                        enter(Phase::Settle, elapsed);
                        event = "reload-done";
                    }
                    break;
                case Phase::Settle:
                    if (waited >= .15) {
                        ++segment;
                        segmentClicks = segmentSent = 0;
                        segmentStart = elapsed;
                        enter(Phase::Clicking, elapsed);
                        event = "segment-start";
                    }
                    break;
                case Phase::Draining:
                    Require(waited <= MaximumWaitSeconds, "Rapid shot decisions did not all arrive within 3 s");
                    if (decidedThrough >= lastSent) { enter(Phase::Done, elapsed); event = "done"; }
                    break;
                case Phase::Done:
                    break;
                }
            }
            const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
            previous = now;
            if (elapsed >= 0) maximumFrameGap = std::max(maximumFrameGap, frame.deltaSeconds);
            if (phase == Phase::Clicking) clickingSeconds += frame.deltaSeconds;
            Require(application.ProcessEvents(frame) == Control::Continue, "Rapid acceptance window closed");
            Require(application.Update(frame) == Control::Continue, application.LastError());
            const auto after = application.WeaponFeedback();

            // Decisions drained by this Update (before the gate saw this frame's click).
            const auto drained = after.decisionCount - before.decisionCount;
            if (drained > 0) {
                const ActionId newest = after.lastDecisionActionId;
                if (newest < decidedThrough) ++decisionOrderViolations;
                const auto first = decidedThrough + 1;
                if (newest + 1 != first + drained) ++attributionMismatches;
                const auto rejected = after.rejectedDecisions - before.rejectedDecisions;
                const auto cooldown = after.authorityCooldownRejections - before.authorityCooldownRejections;
                const bool newestRejected = after.lastRejection != ShotRejection::None;
                const bool othersAccepted = rejected == (newestRejected ? 1U : 0U);
                for (ActionId id = first; id <= newest; ++id) {
                    const auto found = clickOfAction.find(id);
                    if (found == clickOfAction.end()) continue;  // a reload, or not a rapid click
                    auto& verdict = clicks[found->second]["verdict"];
                    verdict = {{"arrived_seconds", elapsed}, {"arrived_frame_id", frame.frameIndex},
                        {"drained_with", drained}};
                    if (id == newest) {
                        verdict["state"] = newestRejected ? "rejected" : "accepted";
                        verdict["rejection"] = static_cast<int>(after.lastRejection);
                        verdict["hit_kind"] = static_cast<int>(after.lastHitKind);
                        verdict["resolved_tick"] = after.lastDecisionTick;
                        verdict["host_steady_seconds"] = after.lastDecisionSeconds;
                        verdict["exact"] = true;
                    } else if (othersAccepted) {
                        verdict["state"] = "accepted";
                        verdict["exact"] = true;
                    } else {
                        verdict["state"] = "ambiguous";
                        verdict["frame_rejected"] = rejected;
                        verdict["frame_cooldown_rejections"] = cooldown;
                        verdict["exact"] = false;
                    }
                }
                decidedThrough = std::max(decidedThrough, newest);
            }
            const bool sent = after.submittedActions > before.submittedActions && after.lastActionKind == ActionKind::Shot;
            if (after.submittedActions > before.submittedActions && after.lastActionKind == ActionKind::Reload) {
                reloadActions.insert(after.lastActionId);
                ++reloadsSubmitted;
            }
            if (click) {
                const bool blocked = after.localCooldownBlocks > before.localCooldownBlocks;
                Require(!(sent && blocked), "One click was both submitted and locally blocked");
                clickRecord["outcome"] = sent ? "sent" : blocked ? "local_block" : "none";
                clickRecord["previous_verdict_arrived"] = previousSent ? ActionJson(decidedThrough >= previousSent) :
                    ActionJson(nullptr);
                clickRecord["verdict"] = nullptr;
                if (!after.inputCaptured) ++lostCapture;
                if (sent) {
                    clickRecord["action_id"] = after.lastActionId;
                    clickRecord["submitted_host_steady_seconds"] = after.lastSubmittedSeconds;
                    clickOfAction[after.lastActionId] = clicks.size();
                    previousSent = lastSent = after.lastActionId;
                    ++segmentSent;
                } else clickRecord["action_id"] = nullptr;
                clicks.push_back(std::move(clickRecord));
                const bool budgetSpent = plan.clickingSeconds > 0 && clickingSeconds >= plan.clickingSeconds;
                if (budgetSpent) enter(Phase::Draining, elapsed);
                else if (segmentSent == ShotsPerSegment) {
                    if (plan.segments && segment + 1 >= plan.segments) enter(Phase::Draining, elapsed);
                    else enter(Phase::AwaitLastDecision, elapsed);
                }
            } else Require(!sent, "A shot was submitted without a rapid click");
            // Only the setup turn may change the aim; anything else is external input.
            if (measurementStart && event != "turn-wall" && before.active && after.active &&
                std::abs(after.yaw - before.yaw) > 1e-6F) {
                ++unscheduledYawFrames;
                unscheduledYaw += std::abs(after.yaw - before.yaw);
            }
            Require(application.Render(frame) == Control::Continue, application.LastError());
            const bool presented = application.PresentedMovement().has_value();
            if (presented) ++presentedCount;
            Require(frames.size() < frames.capacity(), "Bounded rapid frame buffer exhausted");
            frames.push_back({{"frame_id", frame.frameIndex}, {"seconds", elapsed}, {"frame_seconds", frame.deltaSeconds},
                {"phase", phaseName(phase)}, {"event", event}, {"presented", presented},
                {"decisions", after.decisionCount}, {"submitted", after.submittedActions},
                {"local_cooldown_blocks", after.localCooldownBlocks},
                {"authority_cooldown_rejections", after.authorityCooldownRejections}, {"ammo", after.magazineAmmo},
                {"captured", after.inputCaptured}, {"hp", after.hp}, {"dead", after.dead}});
            const double remaining = 1 / options.fps - Seconds(Clock::now(), now);
            if (remaining > 0) SDL_DelayNS(static_cast<Uint64>(remaining * 1e9));
        }
        const auto last = application.WeaponFeedback();
        evidence["presentation_frames"] = presentedCount;
        evidence["maximum_frame_seconds"] = maximumFrameGap;
        evidence["unscheduled_yaw_frames"] = unscheduledYawFrames;
        evidence["unscheduled_yaw_radians"] = unscheduledYaw;
        evidence["disturbed"] = unscheduledYawFrames > 0;
        evidence["final_local"] = LocalActionSample(last);
        Require(presentedCount >= 10, "Insufficient successful real GUI presentations");
        if (actor) {
            Require(baseline.has_value(), "Rapid actor never became active");
            ActionJson counts{{"clicks", clicks.size()}, {"segments_started", segment + 1},
                {"clicking_seconds", clickingSeconds}, {"reloads_submitted", reloadsSubmitted},
                {"clicks_without_capture", lostCapture},
                {"local_cooldown_blocks", last.localCooldownBlocks - baseline->localCooldownBlocks},
                {"decisions", last.decisionCount - baseline->decisionCount},
                {"accepted", last.acceptedDecisions - baseline->acceptedDecisions},
                {"rejected", last.rejectedDecisions - baseline->rejectedDecisions},
                {"authority_cooldown_rejections", last.authorityCooldownRejections - baseline->authorityCooldownRejections},
                {"player_hits", last.hitDecisions - baseline->hitDecisions},
                {"attribution_mismatches", attributionMismatches}, {"decision_order_violations", decisionOrderViolations}};
            evidence["counts"] = counts;
            Require(lostCapture == 0, "A rapid click arrived without input capture");
        }
        evidence["passed"] = true;
        save();
        if (actor) { std::ofstream finished(finishedPath); finished << "Actor rapid probe complete\n"; }
        connection.Leave();
        std::cout << options.role << ": rapid evidence recorded; " << clicks.size() << " clicks\n";
    } catch (const std::exception& exception) {
        evidence["error"] = exception.what();
        evidence["unscheduled_yaw_frames"] = unscheduledYawFrames;
        evidence["disturbed"] = unscheduledYawFrames > 0;
        save();
        if (actor) { std::ofstream finished(finishedPath); finished << "Actor rapid probe failed\n"; }
        throw;
    }
}
