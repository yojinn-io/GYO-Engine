// Included by the product GUI probe inside its anonymous namespace. All input,
// fault injection and assertions belong to acceptance, never the application.
using WeaponJson = nlohmann::json;

void PushMouse(SDL_Window* window, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = down;
    Require(SDL_PushEvent(&event), "Cannot enqueue weapon mouse edge");
}
void PushWindowEvent(SDL_Window* window, Uint32 type) {
    SDL_Event event{};
    event.type = type;
    event.window.windowID = SDL_GetWindowID(window);
    Require(SDL_PushEvent(&event), "Cannot enqueue weapon window event");
}
void PushMotion(SDL_Window* window, float dx, float dy) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.xrel = dx;
    event.motion.yrel = dy;
    Require(SDL_PushEvent(&event), "Cannot enqueue relative mouse motion");
}

WeaponJson WeaponSample(const fps::pvp::WeaponFeedbackObservation& w) {
    return {{"ready", w.ready}, {"active", w.active}, {"captured", w.inputCaptured},
        {"shooting", w.shooting}, {"drawing", w.drawing}, {"hit_marker", w.hitMarkerVisible},
        {"submitted", w.submittedActions}, {"animations", w.animationStarts},
        {"decisions", w.decisionCount}, {"accepted", w.acceptedDecisions},
        {"rejected", w.rejectedDecisions}, {"hits", w.hitDecisions},
        {"last_action", w.lastActionId}, {"last_decision", w.lastDecisionActionId},
        {"last_rejection", static_cast<int>(w.lastRejection)},
        {"last_hit_kind", static_cast<int>(w.lastHitKind)}, {"hp", w.hp}, {"maximum_hp", w.maximumHp},
        {"yaw", w.yaw}, {"pitch", w.pitch}, {"mouse_consumptions", w.mouseDeltaConsumeCount},
        {"animation_revision", w.animationRevision}, {"animation_elapsed", w.actionElapsedSeconds},
        {"animation_duration", w.actionDurationSeconds}, {"cooldown_remaining", w.cooldownRemainingSeconds},
        {"last_submitted_seconds", w.lastSubmittedSeconds}, {"last_decision_seconds", w.lastDecisionSeconds},
        {"last_decision_tick", w.lastDecisionTick}, {"last_target", w.lastTargetId}, {"last_damage", w.lastDamage},
        {"mesh_count", w.meshCount}, {"material_count", w.materialCount}, {"submitted_meshes", w.submittedMeshes},
        {"pose_revision", w.poseRevision}, {"sampled_animation_seconds", w.sampledAnimationSeconds},
        {"recoil_radians", w.recoilRadians}, {"life_generation", w.lifeGeneration}, {"life_state_tick", w.lifeStateTick},
        {"respawn_tick", w.respawnTick}, {"reload_start_tick", w.reloadStartTick}, {"reload_end_tick", w.reloadEndTick},
        {"ammo", w.magazineAmmo}, {"capacity", w.magazineCapacity}, {"dead", w.dead}, {"reloading", w.reloading},
        {"reload_pending", w.reloadPending}, {"grounded", w.grounded}, {"vertical_velocity", w.verticalVelocity},
        {"reload_progress", w.reloadProgress}, {"respawn_remaining_seconds", w.respawnRemainingSeconds},
        {"last_action_kind", static_cast<int>(w.lastActionKind)}, {"last_decision_kind", static_cast<int>(w.lastDecisionKind)}};
}

void RunWeaponShort(const Options& options) {
    using namespace fps::pvp;
    const bool actor = options.role == "create";
    const bool captureMode = options.weaponCapture;
    std::filesystem::create_directories(options.output);
    const auto startPath = options.output / "weapon-start.txt";
    const auto finishedPath = options.output / "weapon-finished.txt";
    if (actor) { std::filesystem::remove(startPath); std::filesystem::remove(finishedPath); }
    WeaponJson evidence{{"passed", false}, {"role", options.role},
        {"mode", captureMode ? "weapon-capture" : "weapon-short"}, {"nominal_fps", options.fps},
        {"scope", "Two real GUI processes, SDL input, real Match and Gateway on localhost. Presented is successful renderer submission, not scanout."},
        {"capture", captureMode ? "actual scene GPU readback before UI" : "none"},
        {"events", WeaponJson::array()}, {"checks", WeaponJson::object()}};
    std::vector<WeaponJson> frames;
    frames.reserve(static_cast<std::size_t>(options.fps * (options.duration + 15)));
    auto save = [&] {
        std::ofstream report(options.output / (options.role + "-weapon.json"));
        report << evidence.dump(2) << '\n';
        std::ofstream trace(options.output / (options.role + "-weapon-frames.jsonl"));
        for (const auto& frame : frames) trace << frame.dump() << '\n';
        Require(bool(report) && bool(trace), "Cannot write weapon evidence");
    };
    try {
        PvpApplication application;
        std::string error;
        Require(application.InitializeContent(options.assetRoot, error), error);
        PvpApplicationOptions graphics;
        graphics.title = "Object FPS PVP weapon acceptance / " + options.role;
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
        bool joined = actor, rejoining{}, rejoined{}, cooldownInjected{}, hpZeroShot{}, hpZeroMovement{};
        bool localFeedbackBeforeDecision{}, wallDecision{}, hitMarkerSeen{}, cooldownDecision{};
        bool combinedChecked{}, combinedNextChecked{}, heldChecked{};
        PlayerId originalPlayer{};
        std::string room;
        std::uint64_t frameIndex{}, presentedCount{}, expectedSubmissions{}, expectedAnimations{};
        std::uint64_t originalAllocated{}, beforeCombinedMouse{};
        float beforeCombinedPitch{}, combinedYaw{}, combinedPitch{};
        Engine::Math::Vec3 zeroMovementStart{};
        double zeroMovementMaximum{}, maximumFrameGap{}, firstFeedbackDelay{};
        std::optional<double> measurementStart;
        std::optional<Clock::time_point> firstRejoined;
        std::optional<std::uint32_t> priorHp;
        std::optional<ActionId> awaitingPresentedAction;
        const auto started = Clock::now();
        auto previous = started, refreshed = started;
        struct Event { double at; const char* name; };
        const std::vector<Event> actions = captureMode ? std::vector<Event>{
            {.05, "capture-draw"}, {.2, "capture"}, {.3, "release"}, {1.0, "capture-idle"},
            {1.4, "shoot"}, {1.48, "capture-shoot"}, {1.6, "release"}, {2.2, "resize"},
            {2.5, "capture-resize"}, {2.8, "capture"}, {2.9, "release"},
            {3.1, "approach-wall"}, {5.1, "stop"}, {5.5, "capture-wall-depth"}
        } : std::vector<Event>{
            {.1, "capture"}, {.2, "release"}, {.4, "held-shot"}, {1.0, "held-release"},
            {1.2, "hit-2"}, {1.3, "release"}, {1.7, "hit-3"}, {1.8, "release"},
            {2.2, "hit-4"}, {2.3, "release"}, {2.7, "wall-shot"}, {2.8, "release"},
            {3.2, "combined"}, {3.4, "combined-release"}, {3.6, "focus-loss"},
            {3.7, "focus-gain-held"}, {3.8, "release"}, {3.95, "capture"},
            {4.05, "release"}, {4.2, "tab-release"}, {4.3, "tab-key-up"},
            {4.45, "capture"}, {4.55, "release"}, {4.7, "drag"}, {4.8, "release"},
            {5.2, "escape"}, {5.3, "escape-up"}
        };
        const std::vector<Event> observerActions{{4.9, "zero-capture"}, {5.0, "release"},
            {5.1, "zero-shot-move"}, {5.2, "release"}, {5.4, "stop"}};
        std::size_t actionIndex{};
        const auto& schedule = actor ? actions : observerActions;
        for (;;) {
            const auto now = Clock::now();
            Require(Seconds(now, started) < options.duration + 20, "Weapon short probe exceeded bounded deadline");
            const double stamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            auto state = connection.State();
            Require(state.error.empty(), "Weapon connection failed: " + state.error);
            if (!joined && state.phase == ConnectionPhase::Lobby) {
                if (!state.rooms.empty()) {
                    room = state.rooms.front().id;
                    connection.Join(options.gateway, room);
                    joined = true;
                } else if (Seconds(now, refreshed) >= .3) { connection.Refresh(options.gateway); refreshed = now; }
            }
            if (state.snapshot && state.snapshot->players.size() == 2 && !originalPlayer) {
                originalPlayer = state.playerId;
                evidence["initial_player_id"] = originalPlayer;
                if (actor) {
                    measurementStart = stamp + .25;
                    std::ofstream start(startPath);
                    start << std::setprecision(17) << *measurementStart;
                    Require(bool(start), "Cannot signal weapon start");
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
            if (!actor && measurementStart && std::filesystem::exists(finishedPath)) break;
            const auto before = application.WeaponFeedback();
            const auto priorState = state;
            std::string event;
            bool shotExpected{}, silenceExpected{}, captureExpected{}, releaseExpected{}, doCapture{};
            if (measurementStart && (!captureMode || actor) && actionIndex < schedule.size() &&
                elapsed >= schedule[actionIndex].at) {
                event = schedule[actionIndex++].name;
                if (event == "capture" || event == "zero-capture") {
                    if (event == "zero-capture") SDL_RaiseWindow(window);
                    PushWindowEvent(window, SDL_EVENT_WINDOW_FOCUS_GAINED);
                    PushMouse(window, true);
                    silenceExpected = captureExpected = true;
                } else if (event == "release" || event == "held-release") {
                    PushMouse(window, false);
                    if (event == "held-release") {
                        Require(before.submittedActions == 1 && before.animationStarts == 1,
                            "Holding left repeated a shot or animation");
                        heldChecked = true;
                    }
                } else if (event == "held-shot" || event == "hit-2" || event == "hit-3" ||
                    event == "hit-4" || event == "shoot") {
                    PushMouse(window, true);
                    shotExpected = true;
                } else if (event == "wall-shot" || event == "combined") {
                    if (event == "wall-shot") PushMotion(window, static_cast<float>((std::atan2(5., 5.) - before.yaw) / .0025), 0);
                    else {
                        beforeCombinedPitch = before.pitch;
                        beforeCombinedMouse = before.mouseDeltaConsumeCount;
                        PushMotion(window, static_cast<float>((.02 - before.yaw) / .0025), 4);
                        PushKey(window, SDL_SCANCODE_W, true);
                    }
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
                } else if (event == "escape") {
                    Require(before.submittedActions == 6 && before.animationStarts == 6,
                        "SDL edges did not produce exactly six actions and animations");
                    Require(before.hitDecisions >= 4 && wallDecision && cooldownDecision,
                        "Hit, wall or authority cooldown outcome was not observed before leaving");
                    originalAllocated = state.actionTransport.allocatedThrough;
                    Require(originalAllocated == 7, "Unexpected action IDs; six edges and one explicit authority injection required");
                    evidence["before_escape"] = WeaponSample(before);
                    PushKey(window, SDL_SCANCODE_ESCAPE, true);
                    rejoining = true;
                } else if (event == "escape-up") PushKey(window, SDL_SCANCODE_ESCAPE, false);
                else if (event == "zero-shot-move") {
                    Require(before.hp == 0, "Second GUI did not reach authority HP=0");
                    zeroMovementStart = application.LocalMovement().predictedPosition;
                    PushKey(window, SDL_SCANCODE_W, true);
                    PushMouse(window, true);
                    shotExpected = hpZeroShot = true;
                } else if (event == "stop") PushKey(window, SDL_SCANCODE_W, false);
                else if (event == "resize") {
                    Require(SDL_SetWindowSize(window, 1120, 630), SDL_GetError());
                } else if (event == "approach-wall") {
                    PushMotion(window, static_cast<float>((std::atan2(5., 5.) - before.yaw) / .0025), 0);
                    PushKey(window, SDL_SCANCODE_W, true);
                } else if (event.starts_with("capture-")) doCapture = true;
            }
            const Engine::Runtime::FrameContext frame{frameIndex++, Seconds(now, previous)};
            previous = now;
            if (elapsed >= 0) maximumFrameGap = std::max(maximumFrameGap, frame.deltaSeconds);
            Require(application.ProcessEvents(frame) == Control::Continue, "Weapon acceptance window closed");
            Require(application.Update(frame) == Control::Continue, application.LastError());
            const auto after = application.WeaponFeedback();
            const auto afterState = connection.State();
            if (after.active) Require(after.ready, "Gameplay began before weapon GPU preparation completed");
            if (shotExpected) {
                Require(after.submittedActions == before.submittedActions + 1 &&
                    after.animationStarts == before.animationStarts + 1 && after.shooting,
                    "One SDL edge failed to start exactly one submitted local animation");
                Require(after.lastActionId == priorState.actionTransport.allocatedThrough + 1,
                    "SDL edge left an action ID gap");
                ++expectedSubmissions; ++expectedAnimations;
                awaitingPresentedAction = after.lastActionId;
                if (event == "held-shot") {
                    // A second request in the same authority cooldown is an
                    // explicit transport injection, not another local click.
                    const auto current = connection.State();
                    Require(current.snapshot.has_value(), "No reference for cooldown injection");
                    const auto id = connection.SubmitShot(current.snapshot->tick, after.yaw, after.pitch);
                    Require(id && *id == after.lastActionId + 1, "Cooldown injection was not allocated contiguously");
                    evidence["cooldown_injection_action_id"] = *id;
                    cooldownInjected = true;
                }
            }
            if (silenceExpected) {
                Require(after.submittedActions == before.submittedActions && after.animationStarts == before.animationStarts,
                    "Capture/window interaction created a ghost shot");
                Require(connection.State().actionTransport.allocatedThrough == priorState.actionTransport.allocatedThrough,
                    "Suppressed input allocated an unsent action ID");
                if (captureExpected) Require(after.inputCaptured, "Content click did not capture input");
                if (releaseExpected) Require(!after.inputCaptured, "Window/Tab/focus interaction retained capture");
                evidence["checks"][event] = true;
            }
            if (event == "combined") {
                Require(std::abs(after.yaw - .02F) < .0001F &&
                    std::abs(after.pitch - (beforeCombinedPitch + .01F)) < .0001F,
                    "Movement/turn/shot frame applied raw mouse delta other than once");
                Require(after.mouseDeltaConsumeCount == beforeCombinedMouse + 1,
                    "Combined frame did not consume exactly one nonzero mouse delta");
                combinedYaw = after.yaw; combinedPitch = after.pitch;
                combinedChecked = true;
            } else if (combinedChecked && !combinedNextChecked && event.empty()) {
                Require(std::abs(after.yaw - combinedYaw) < .0001F && std::abs(after.pitch - combinedPitch) < .0001F,
                    "Shot animation/recoil replayed mouse delta on next frame");
                combinedNextChecked = true;
            }
            wallDecision = wallDecision || (after.lastHitKind == ShotHitKind::World && after.lastRejection == ShotRejection::None);
            cooldownDecision = cooldownDecision || after.lastRejection == ShotRejection::Cooldown;
            hitMarkerSeen = hitMarkerSeen || after.hitMarkerVisible;
            if (rejoining && !rejoined && afterState.phase == ConnectionPhase::Lobby) {
                Require(!after.active && !after.shooting && after.submittedActions == 0 &&
                    after.animationStarts == 0 && after.decisionCount == 0 && after.lastActionId == 0,
                    "ESC retained weapon actions, animation or decisions");
                if (!afterState.rooms.empty()) {
                    room = afterState.rooms.front().id;
                    connection.Join(options.gateway, room);
                    rejoined = true;
                    evidence["checks"]["escape_clears_weapon"] = true;
                    priorHp.reset();
                }
            }
            if (rejoined && after.active && afterState.playerId != originalPlayer) {
                if (!firstRejoined) firstRejoined = now;
                Require(after.hp == after.maximumHp && after.submittedActions == 0 &&
                    after.animationStarts == 0 && after.decisionCount == 0 && !after.shooting && !after.hitMarkerVisible,
                    "Rejoin retained old damage, shot animation or decision effects");
                evidence["rejoined_player_id"] = afterState.playerId;
                evidence["checks"]["rejoin_full_hp_no_stale_effects"] = true;
            }
            if (after.active && afterState.playerId == originalPlayer) {
                if (priorHp) Require(after.hp <= *priorHp, "HUD HP moved backwards within the same identity");
                priorHp = after.hp;
            }
            if (hpZeroShot && application.LocalMovement().active) {
                const auto p = application.LocalMovement().predictedPosition;
                zeroMovementMaximum = std::max(zeroMovementMaximum,
                    std::hypot(double(p.x - zeroMovementStart.x), double(p.z - zeroMovementStart.z)));
                hpZeroMovement = zeroMovementMaximum > .2;
            }
            if (doCapture) application.Renderer().RequestSceneCapture();
            Require(application.Render(frame) == Control::Continue, application.LastError());
            WeaponJson sample{{"frame_id", frame.frameIndex}, {"seconds", elapsed},
                {"frame_seconds", frame.deltaSeconds}, {"presented", false}, {"event", event},
                {"update", WeaponSample(after)}, {"allocated", connection.State().actionTransport.allocatedThrough}};
            if (const auto& presented = application.PresentedMovement()) {
                Require(presented->frameId == frame.frameIndex, "Stale successful Presented observation");
                sample["presented"] = true;
                sample["presented_seconds"] = presented->hostSteadySeconds;
                sample["weapon"] = WeaponSample(presented->weapon);
                sample["player_id"] = presented->localPlayerId;
                sample["local_x"] = presented->local.renderPosition.x;
                sample["local_z"] = presented->local.renderPosition.z;
                sample["authority_tick"] = presented->local.authorityTick;
                sample["pending_commands"] = presented->local.pendingCommands;
                Require(presented->weapon.meshCount > 0 && presented->weapon.materialCount > 0 &&
                    presented->weapon.submittedMeshes > 0,
                    "Successful world presentation omitted prepared weapon meshes/materials");
                if (awaitingPresentedAction) {
                    Require(presented->weapon.shooting && presented->weapon.lastActionId == *awaitingPresentedAction,
                        "Local shot feedback was absent from successful presentation");
                    localFeedbackBeforeDecision = localFeedbackBeforeDecision ||
                        presented->weapon.lastDecisionActionId < *awaitingPresentedAction;
                    const double delay = presented->hostSteadySeconds - presented->weapon.lastSubmittedSeconds;
                    sample["submission_to_presented_seconds"] = delay;
                    if (firstFeedbackDelay == 0) firstFeedbackDelay = delay;
                    awaitingPresentedAction.reset();
                }
                ++presentedCount;
            }
            if (!event.empty()) evidence["events"].push_back({{"name", event}, {"seconds", elapsed},
                {"before", WeaponSample(before)}, {"after", WeaponSample(after)}, {"presented", sample["presented"]}});
            if (auto capture = application.Renderer().TakeSceneCapture()) {
                Require(captureMode && doCapture, "Unexpected GPU readback in timing mode");
                const auto file = options.role + "-" + event + ".bmp";
                SaveCapture(*capture, options.output / file);
                evidence["captures"].push_back({{"file", file}, {"width", capture->width},
                    {"height", capture->height}, {"frame_id", frame.frameIndex},
                    {"weapon", WeaponSample(application.WeaponFeedback())}});
            }
            Require(frames.size() < frames.capacity(), "Bounded weapon frame buffer exhausted");
            frames.push_back(std::move(sample));
            const double remaining = 1 / options.fps - Seconds(Clock::now(), now);
            if (remaining > 0) SDL_DelayNS(static_cast<Uint64>(remaining * 1e9));
        }
        evidence["presentation_frames"] = presentedCount;
        evidence["maximum_frame_seconds"] = maximumFrameGap;
        evidence["skipped_frames"] = application.SkippedPresentationFrames();
        evidence["first_feedback_seconds"] = firstFeedbackDelay;
        evidence["injected_sdl_shots"] = expectedSubmissions;
        evidence["expected_animations"] = expectedAnimations;
        evidence["final_weapon"] = WeaponSample(application.WeaponFeedback());
        Require(presentedCount >= 10, "Insufficient successful real GUI presentations");
        if (captureMode && actor) {
            Require(evidence.contains("captures") && evidence["captures"].size() == 5,
                "Draw/idle/shoot/resize/wall-depth GPU captures were not all produced");
        } else if (!captureMode && actor) {
            Require(actionIndex == actions.size() && heldChecked && cooldownInjected && localFeedbackBeforeDecision &&
                wallDecision && cooldownDecision && hitMarkerSeen && combinedChecked && combinedNextChecked &&
                firstRejoined && Seconds(Clock::now(), *firstRejoined) >= .3,
                "Weapon short coverage incomplete; inspect raw event evidence");
            evidence["checks"]["hold_one_shot_one_animation"] = true;
            evidence["checks"]["local_feedback_before_decision"] = true;
            evidence["checks"]["authority_wall_and_cooldown"] = true;
            evidence["checks"]["authority_hit_marker"] = true;
            evidence["checks"]["movement_turn_shoot_mouse_once"] = true;
            evidence["allocated_before_escape"] = originalAllocated;
        } else if (!captureMode) {
            Require(hpZeroShot && hpZeroMovement && application.WeaponFeedback().animationStarts == 1,
                "HP=0 did not retain real GUI movement and shooting");
            evidence["checks"]["hp_zero_move_and_shoot"] = true;
            evidence["hp_zero_displacement"] = zeroMovementMaximum;
        }
        evidence["passed"] = true;
        save();
        if (actor) { std::ofstream finished(finishedPath); finished << "Actor short probe complete\n"; }
        connection.Leave();
        std::cout << options.role << ": weapon short evidence passed; " << presentedCount << " successful presentations\n";
    } catch (const std::exception& exception) {
        evidence["error"] = exception.what();
        save();
        throw;
    }
}
