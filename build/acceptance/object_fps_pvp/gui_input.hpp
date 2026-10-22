// Included by the product GUI probe inside its anonymous namespace. Shared SDL
// input injection and first-person weapon sampling for the GUI acceptance
// modes. All input, fault injection and assertions belong to acceptance, never
// the application.
using WeaponJson = nlohmann::json;

using Engine::Input::Key;
// Scripted input enters SDL's event queue through Engine's injector, so the
// product's SdlInput sees it like a user's (docs/architecture/platform-sdl.md).
struct ProbeInput final {
    Engine::Platform::Sdl::SdlPlatform& platform;
    Engine::Input::Backend::Sdl::SdlInputInjector injector;
    explicit ProbeInput(Engine::Platform::Sdl::SdlPlatform& owner) : platform(owner), injector(owner) {}
};
template <class Pushed> void RequirePushed(const Pushed& pushed, const char* what) {
    Require(bool(pushed), pushed ? std::string() : std::string(what) + ": " + pushed.error().message);
}
void PushMouse(ProbeInput& input, bool down) {
    RequirePushed(input.injector.PushMouseButton(Engine::Input::MouseButton::Left, down), "Cannot enqueue mouse edge");
}
void PushFocus(ProbeInput& input, bool gained) {
    RequirePushed(input.injector.PushFocus(gained), "Cannot enqueue window focus event");
}
void PushKey(ProbeInput& input, Key key, bool held) {
    RequirePushed(input.injector.PushKey(key, held), "Could not enqueue key");
}
void PushMovement(ProbeInput& input, bool held, Key direction = Key::W) {
    if (held) {
        PushFocus(input, true);
        if (!input.platform.QueryWindow().flags.relativeMouseMode) {
            PushMouse(input, true);
            PushMouse(input, false);
        }
    }
    PushKey(input, direction, held);
}

// Temporary until probe_window_move: the synthetic MOVED (sdl-isolation J2).
void PushWindowEvent(SDL_Window* window, Uint32 type) {
    SDL_Event event{};
    event.type = type;
    event.window.windowID = SDL_GetWindowID(window);
    Require(SDL_PushEvent(&event), "Cannot enqueue window event");
}
void PushMotion(ProbeInput& input, float dx, float dy) {
    RequirePushed(input.injector.PushMouseMotion(dx, dy), "Cannot enqueue relative mouse motion");
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
        {"local_cooldown_blocks", w.localCooldownBlocks}, {"authority_cooldown_rejections", w.authorityCooldownRejections},
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
