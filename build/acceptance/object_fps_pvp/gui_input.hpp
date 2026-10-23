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

// A real window move (sdl-isolation J2, replacing the synthetic MOVED): the OS
// moves the window by 32 pixels. What orders the MOVED before input injected
// afterwards is that the platform reports the move while SetWindowPosition
// runs (AppKit's windowDidMove:, Win32's WM_WINDOWPOSCHANGED) and SDL updates
// the window's position only when it queues that MOVED; so the check that the
// position changed is the guard, not Sync (on macOS it pumps once, on Windows
// it does nothing; on X11 it waits for the new position). A platform that cannot
// place windows (Wayland) fails here: the check that needs the move cannot run,
// which is never a pass. The record is evidence only.
struct WindowMoveRecord final {
    int beforeX{}, requestedX{}, afterX{};
    bool synced{};
    double syncMs{};
    WeaponJson Json() const {
        return {{"before_x", beforeX}, {"requested_x", requestedX}, {"after_x", afterX}, {"synced", synced},
            {"sync_ms", syncMs}};
    }
};
WindowMoveRecord MoveWindow(ProbeInput& input) {
    WindowMoveRecord record;
    const auto before = input.platform.QueryWindow().rect;
    record.beforeX = before.x;
    record.requestedX = before.x + 32;
    if (const auto display = input.platform.QueryDisplay()) {
        const auto& bounds = display.value().usableBounds;
        if (bounds.width > 0 && record.requestedX + before.width > bounds.x + bounds.width) record.requestedX = before.x - 32;
    }
    const auto moved = input.platform.SetWindowPosition(record.requestedX, before.y);
    Require(bool(moved), moved ? std::string() : "Window move unavailable, so the window interaction check cannot run: " +
        moved.error().message + ": " + moved.error().detail);
    const auto syncing = std::chrono::steady_clock::now();
    record.synced = input.platform.Sync();
    record.syncMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - syncing).count();
    Require(record.synced, "Window move did not complete before the scripted input (Sync failed or timed out)");
    record.afterX = input.platform.QueryWindow().rect.x;
    Require(record.afterX != before.x, "Window move left the window in place");
    return record;
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
