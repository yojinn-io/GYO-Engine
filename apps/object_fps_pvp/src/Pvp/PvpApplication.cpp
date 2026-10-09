#include "RetroFPS/Pvp/PvpApplication.hpp"
#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/FireGate.hpp"
#include "RetroFPS/Pvp/HitFeedback.hpp"
#include "RetroFPS/Pvp/ClientSimulationRole.hpp"
#include "RetroFPS/Pvp/PointerCapture.hpp"
#include "RetroFPS/Pvp/SnapshotTimeline.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/App/WeaponViewModel.hpp"

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/runtime/RuntimeLoop.hpp"
#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "engine/render/PrimitiveMesh.hpp"
#include "engine/render/Renderer.hpp"
#include "engine/text/backend/sdl_ttf/SdlTtfTextRasterizer.hpp"
#include "engine/ui/UiDocumentCodec.hpp"
#include "engine/ui/UiRenderer.hpp"
#include "engine/ui/UiRuntime.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

namespace fps::pvp {
namespace {
using Control = Engine::Runtime::RuntimeControl;
using Clock = std::chrono::steady_clock;
constexpr float WorldVerticalFovRadians = Engine::Math::DegreesToRadians(60.0F);
double Milliseconds(Clock::time_point end, Clock::time_point begin) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}
template<class Error> std::string Explain(const Error& error) {
    return error.message + (error.detail.empty() ? "" : ": " + error.detail);
}
std::string ExplainUi(const Engine::Ui::UiError& error) {
    return error.message + (error.jsonPointer.empty() ? "" : " at " + error.jsonPointer);
}
const PlayerState* FindPlayer(const WorldSnapshot& snapshot, PlayerId id) {
    const auto found = std::find_if(snapshot.players.begin(), snapshot.players.end(),
        [id](const PlayerState& player) { return player.playerId == id; });
    return found == snapshot.players.end() ? nullptr : &*found;
}
void AddText(Engine::Ui::UiDrawList& list, std::string text,
    Engine::Math::Rect bounds, float size = 18.0F, Engine::Ui::UiColor color = {0.9F, 0.96F, 1.0F, 1.0F}) {
    Engine::Ui::UiTextDraw draw;
    draw.utf8 = std::move(text);
    draw.boundsPixels = bounds;
    draw.fontAssetId = "object_fps_pvp.font.ui";
    draw.pointSizePixels = size;
    draw.color = color;
    list.commands.emplace_back(std::move(draw));
}
} // namespace

struct PvpApplication::Impl final {
    Engine::Asset::AssetCatalog catalog;
    Engine::Asset::Loading::LoaderRegistry loaders;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy cache{{
        Engine::Asset::Core::AssetCachePolicy::Mode::KeepWhileReferenced, 120, true, 0, 0}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, cache};
    // Every installed arena, and the one shown: the first until a Match selects one.
    std::vector<Arena> arenas;
    std::optional<Arena> arena;
    Engine::Render::ShaderLibrary shaders;
    std::unique_ptr<Engine::Platform::Sdl::SdlPlatform> platform;
    std::unique_ptr<Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice> device;
    Engine::Render::Renderer renderer;
    std::unique_ptr<Engine::Text::Backend::SdlTtf::SdlTtfTextRasterizer> text;
    Engine::Ui::UiRenderer uiRenderer;
    Engine::Ui::UiRuntime ui;
    std::unique_ptr<Engine::Input::Backend::Sdl::SdlInput> input;
    Engine::Render::MeshHandle cube;
    Engine::Render::MeshHandle floor;
    Engine::Render::RenderQueue queue;
    std::unique_ptr<fps::WeaponViewModel> weapon;
    std::unique_ptr<PlayerPresentation> players;
    std::uint64_t characterPhaseReanchors{};
    bool characterPresentationSkipped{};
    double remoteSubmitMilliseconds{};
    WeaponFeedbackObservation weaponFeedback;
    fps::WeaponViewModelAction weaponAction{fps::WeaponViewModelAction::Idle};
    Clock::time_point weaponStartedAt{}, hitMarkerUntil{}, rejectionUntil{};
    LocalFireGate fireGate;
    ClientConnection connection;
    ClientConnectionState state;
    // The simulation role: declared after the connection so it stops first.
    std::unique_ptr<ClientSimulationRole> simulation;
    // Jump presses counted for the intent, and this frame's placed presentation.
    std::uint64_t jumpPresses{};
    ClientPresented presented;
    SnapshotTimeline timeline;
    std::uint64_t connectionGeneration{}, ingressHistoryDrops{};
    std::optional<RemoteMovementObservation> remoteMovement;
    std::optional<PlayerId> observedRemote;
    std::optional<PresentedMovementObservation> presentedMovement;
    std::uint64_t skippedPresentationFrames{};
    std::string address{"127.0.0.1:8080"};
    std::string lastError;
    std::string localStatus;
    float width{1280}, height{720};
    // The controlled view; the hit shake only reaches its camera.
    ClientView view;
    LocalHitFeedback hitFeedback;
    HitFeedbackSettings hitSettings;
    HitFeedbackSample hitSample;
    PlayerId viewPlayer{}, renderedPlayer{};
    std::uint64_t viewLife{};
    LifeState viewLifeState{LifeState::Alive};
    std::optional<ActionId> pendingReload;
    std::uint64_t acceptedReloadEndTick{};
    // The authoritative reload start this animation is anchored to, and the
    // local instant at which its authority progress was first observed.
    std::uint64_t reloadAnchorStartTick{};
    Clock::time_point reloadAnchorAt{};
    double reloadAnchorSeconds{};
    bool pendingReloadEdge{}, lifeBoundaryThisFrame{};
    bool editing{}, suppressUiEnter{}, inputCaptured{}, windowInteraction{}, initialized{}, quit{};
    bool leftButtonDown{}, pointerAcquiredThisFrame{}, pendingShotEdge{};
    // A live frame runs inside the event pump while the OS holds it (window
    // resize or move). Its input frame is incomplete, so it advances time and
    // presents without consuming input.
    bool liveFrame{};
    std::uint64_t liveFramesThisPump{};
    int exitCode{};

    ~Impl() {
        connection.Leave();
        players.reset();
        weapon.reset();
        uiRenderer.Reset();
        if (device) {
            if (cube.IsValid()) static_cast<void>(device->ReleaseMesh(cube));
            if (floor.IsValid()) static_cast<void>(device->ReleaseMesh(floor));
        }
    }

    bool InWorld() const {
        return state.phase == ConnectionPhase::Playing && state.snapshot &&
            FindPlayer(*state.snapshot, state.playerId) != nullptr;
    }
    bool Alive() const {
        const auto* player = state.snapshot ? FindPlayer(*state.snapshot, state.playerId) : nullptr;
        return player && player->lifeState == LifeState::Alive;
    }
    Engine::Ui::UiViewport Viewport() const { return {width, height}; }
    // The presented local movement belongs to this frame's player and life.
    bool LocalPresentationCurrent() const {
        const auto* self = state.snapshot ? FindPlayer(*state.snapshot, state.playerId) : nullptr;
        return presented.observation.active && self && presented.observation.lifeGeneration == self->lifeGeneration;
    }

    void EditAddress(bool value) {
        editing = value;
        if (value) SDL_StartTextInput(platform->NativeWindow());
        else SDL_StopTextInput(platform->NativeWindow());
    }

    void AppendAddress(const char* value) {
        if (!value) return;
        for (; *value != '\0' && address.size() < 128; ++value) {
            const unsigned char c = static_cast<unsigned char>(*value);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '.' || c == ':' ||
                c == '-' || c == '[' || c == ']') address += static_cast<char>(c);
        }
    }

    // Applies this frame's window events from Engine input, after the pump and
    // before the update: the new window size, then the in-world capture policy.
    void ApplyInputEvents(bool focusedAtStart) {
        const auto& events = input->Snapshot().events;
        for (const auto& event : events) {
            if (event.kind == Engine::Input::InputEventKind::WindowResized) {
                width = static_cast<float>(Engine::Math::Max(1, event.windowWidth));
                height = static_cast<float>(Engine::Math::Max(1, event.windowHeight));
            }
        }
        if (!InWorld()) return;
        PointerCaptureState capture{inputCaptured, leftButtonDown, pendingShotEdge, pendingReloadEdge,
            pointerAcquiredThisFrame, windowInteraction};
        const auto result = ApplyPointerCaptureEvents(capture, events, focusedAtStart);
        inputCaptured = capture.inputCaptured;
        leftButtonDown = capture.leftButtonDown;
        pendingShotEdge = capture.pendingShotEdge;
        pendingReloadEdge = capture.pendingReloadEdge;
        pointerAcquiredThisFrame = capture.pointerAcquiredThisFrame;
        windowInteraction = capture.windowInteraction;
        if (result.releases == 0) return;
        if (result.releasesWhileCaptured > 0) SDL_Log("PvP releasing pointer player=%llu application_ms=%llu",
            static_cast<unsigned long long>(state.playerId), static_cast<unsigned long long>(SDL_GetTicks()));
        const auto released = input->SetRelativeMouseMode(false);
        if (!released) {
            lastError = Explain(released.error());
            exitCode = 1;
            quit = true;
        }
    }

    // Lobby address editing keeps native SDL text input and clipboard, which
    // Engine input does not provide.
    void HandleTextEditing(const SDL_Event& event) {
        if (InWorld() || !editing) return;
        if (event.type == SDL_EVENT_TEXT_INPUT) AppendAddress(event.text.text);
        if (event.type != SDL_EVENT_KEY_DOWN) return;
        if (event.key.key == SDLK_BACKSPACE && !address.empty()) address.pop_back();
        if ((event.key.mod & SDL_KMOD_CTRL) != 0 && event.key.key == SDLK_A) address.clear();
        if ((event.key.mod & SDL_KMOD_CTRL) != 0 && event.key.key == SDLK_V) {
            char* clipboard = SDL_GetClipboardText();
            AppendAddress(clipboard);
            SDL_free(clipboard);
        }
        if (event.key.key == SDLK_RETURN || event.key.key == SDLK_ESCAPE) {
            EditAddress(false);
            suppressUiEnter = true;
        }
    }

    Engine::Ui::UiBindingTable Bindings() const {
        Engine::Ui::UiBindingTable result;
        result.emplace("address", address + (editing ? "_" : ""));
        result.emplace("hint", editing
            ? "Type host:port. Ctrl+A clears, Ctrl+V pastes, Enter confirms."
            : "Click the address to edit. Arrows / Enter also navigate.");
        std::string room = "No room found. Refresh or create one.";
        if (!state.rooms.empty()) {
            const auto& first = state.rooms.front();
            room = "ROOM " + first.id + "  /  " + std::to_string(first.players) +
                " of " + std::to_string(first.capacity) + " slots occupied";
        }
        result.emplace("room", std::move(room));
        std::string status = "Ready. Start a match or join the listed room.";
        if (state.phase == ConnectionPhase::Requesting) status = "Contacting Gateway...";
        else if (state.phase == ConnectionPhase::Connecting) status = "Joining authoritative Match...";
        else if (state.phase == ConnectionPhase::Playing) status = "Joined. Waiting for first world snapshot...";
        else if (!localStatus.empty()) status = localStatus;
        if (!state.error.empty()) status = state.error;
        result.emplace("status", std::move(status));
        return result;
    }

    void Execute(const std::string& action) {
        if (action == "pvp.edit_address") { EditAddress(!editing); return; }
        EditAddress(false);
        localStatus.clear();
        if (action == "pvp.leave") { connection.Leave(); return; }
        if (action == "pvp.quit") { connection.Leave(); quit = true; return; }
        if (state.phase != ConnectionPhase::Lobby) return;
        if (action == "pvp.refresh") connection.Refresh(address);
        else if (action == "pvp.create") connection.CreateAndJoin(address);
        else if (action == "pvp.join") {
            if (state.rooms.empty()) localStatus = "No room selected. Refresh the room list first.";
            else connection.Join(address, state.rooms.front().id);
        }
    }

    void RefreshState() {
        lifeBoundaryThisFrame = false;
        auto received = connection.Drain();
        state = std::move(received.state);
        if (!received.snapshots.empty()) {
            sessionLog.snapshots += received.snapshots.size();
            sessionLog.lastSnapshotAt = received.snapshots.back().receivedAt;
        }
        if (connectionGeneration != received.generation) {
            connectionGeneration = received.generation;
            timeline.Reset();
            viewPlayer = 0;
            ResetWeaponFeedback();
            if (players) players->ResetPlayers();
            characterPhaseReanchors = 0;
        }
        ingressHistoryDrops = received.snapshotHistoryOverflowCount;
        // The joined Match chose the arena: show and predict in that one.
        if (!state.arenaId.empty() && state.arenaId != arena->id) SelectArena(state.arenaId);
        const auto* self = state.snapshot ? FindPlayer(*state.snapshot, state.playerId) : nullptr;
        if (state.phase != ConnectionPhase::Playing || !self) {
            timeline.Reset();
            remoteMovement.reset();
            viewPlayer = 0;
            renderedPlayer = 0;
            inputCaptured = false;
            presented = {};
            ResetWeaponFeedback();
            if (players) players->ResetPlayers();
            characterPhaseReanchors = 0;
            return;
        }
        if (viewPlayer != state.playerId) {
            timeline.Reset();
            view.Reset(self->yaw, self->pitch);
            hitFeedback.Reset();
            viewPlayer = state.playerId;
            inputCaptured = false;
            ResetWeaponFeedback();
            if (players) players->ResetPlayers();
            characterPhaseReanchors = 0;
            weaponFeedback.active = true;
            weaponAction = fps::WeaponViewModelAction::Draw;
            weaponStartedAt = Clock::now();
            ++weaponFeedback.animationRevision;
        }
        if (viewLife != self->lifeGeneration || viewLifeState != self->lifeState) {
            lifeBoundaryThisFrame = true;
            viewLife = self->lifeGeneration;
            viewLifeState = self->lifeState;
            view.Reset(self->yaw, self->pitch);
            pendingShotEdge = pendingReloadEdge = leftButtonDown = false;
            pendingReload.reset();
            acceptedReloadEndTick = 0;
            reloadAnchorStartTick = 0;
            weaponAction = fps::WeaponViewModelAction::Idle;
            weaponStartedAt = hitMarkerUntil = rejectionUntil = {};
            fireGate.Reset();
            ++weaponFeedback.animationRevision;
        }
        weaponFeedback.active = Alive();
        weaponFeedback.lifeGeneration = self->lifeGeneration;
        weaponFeedback.dead = !Alive();
        weaponFeedback.lifeStateTick = self->lifeStateTick;
        weaponFeedback.respawnTick = self->respawnTick;
        weaponFeedback.grounded = self->grounded;
        weaponFeedback.verticalVelocity = self->verticalVelocity;
        weaponFeedback.reloadPending = pendingReload.has_value();
        weaponFeedback.respawnRemainingSeconds = self->respawnTick > state.snapshot->tick ?
            static_cast<double>(self->respawnTick - state.snapshot->tick) / AuthorityTickRate : 0;
        weaponFeedback.maximumHp = state.combatRules ? state.combatRules->maximumHp : 0;
        weaponFeedback.magazineCapacity = state.combatRules ? state.combatRules->magazineCapacity : 0;
        for (const auto& combat : state.snapshot->combat) {
            if (combat.playerId != state.playerId || combat.lifeGeneration != self->lifeGeneration) continue;
            fireGate.ObserveSnapshot(state.snapshot->tick, combat);
            // Both positions come from this snapshot; the attacker may be absent.
            std::optional<Engine::Math::Vec3> attacker;
            for (const auto& other : state.snapshot->players)
                if (combat.lastAttackerId != 0 && other.playerId == combat.lastAttackerId) attacker = other.position;
            hitFeedback.Observe(combat, self->lifeState, self->position, attacker,
                std::chrono::duration<double>(Clock::now().time_since_epoch()).count());
            weaponFeedback.hp = combat.hp;
            weaponFeedback.magazineAmmo = combat.magazineAmmo;
            weaponFeedback.reloadStartTick = combat.reloadStartTick;
            weaponFeedback.reloadEndTick = combat.reloadEndTick;
            weaponFeedback.reloading = combat.reloadActionId != 0;
            weaponFeedback.reloadProgress = combat.reloadEndTick > combat.reloadStartTick ?
                Engine::Math::Clamp(static_cast<double>(state.snapshot->tick - combat.reloadStartTick) /
                    static_cast<double>(combat.reloadEndTick - combat.reloadStartTick), 0.0, 1.0) : 0;
        }
        // Drain owns delivery exactly once, independently of the movement ACK.
        // A decision can display a hit/rejection, but never restart a local shot.
        for (const auto& decision : received.decisions) {
            ++weaponFeedback.decisionCount;
            if (decision.accepted) ++weaponFeedback.acceptedDecisions;
            else ++weaponFeedback.rejectedDecisions;
            if (decision.rejection == ShotRejection::Cooldown) ++weaponFeedback.authorityCooldownRejections;
            // Drain/ACK includes old lives, but their cosmetic result cannot alter this life.
            if (decision.lifeGeneration != self->lifeGeneration) continue;
            if (state.combatRules) fireGate.ObserveDecision(decision, state.combatRules->cooldownTicks);
            if (decision.kind == ActionKind::Reload && pendingReload == decision.actionId) {
                if (decision.accepted && state.combatRules)
                    acceptedReloadEndTick = decision.resolvedTick + state.combatRules->reloadTicks;
                else { pendingReload.reset(); acceptedReloadEndTick = 0; }
            }
            weaponFeedback.lastDecisionKind = decision.kind;
            weaponFeedback.lastDecisionActionId = decision.actionId;
            weaponFeedback.lastDecisionTick = decision.resolvedTick;
            weaponFeedback.lastRejection = decision.rejection;
            weaponFeedback.lastHitKind = decision.hitKind;
            weaponFeedback.lastTargetId = decision.targetId;
            weaponFeedback.lastDamage = decision.damage;
            const auto now = Clock::now();
            weaponFeedback.lastDecisionSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
            if (decision.accepted) {
                if (decision.hitKind == ShotHitKind::Player) {
                    ++weaponFeedback.hitDecisions;
                    hitMarkerUntil = now + std::chrono::milliseconds{180};
                }
            } else {
                rejectionUntil = now + std::chrono::seconds{1};
            }
        }
        if (pendingReload && acceptedReloadEndTick &&
            (weaponFeedback.reloading || state.snapshot->tick >= acceptedReloadEndTick)) {
            pendingReload.reset(); acceptedReloadEndTick = 0;
        }
        weaponFeedback.reloadPending = pendingReload.has_value();
        // Consume the complete receipt-stamped batch before selecting this
        // frame's timeline bracket. Never play a stalled backlog one frame at a time.
        // The simulation role observes the same snapshots through its own drain.
        for (const auto& sample : received.snapshots)
            static_cast<void>(timeline.Push(sample.snapshot, sample.receivedAt));
    }

    void ResetWeaponFeedback() {
        weaponFeedback = {};
        weaponFeedback.ready = weapon != nullptr;
        weaponAction = fps::WeaponViewModelAction::Idle;
        weaponStartedAt = hitMarkerUntil = rejectionUntil = {};
        fireGate.Reset();
        pendingShotEdge = pendingReloadEdge = leftButtonDown = false;
        viewLife = 0;
        pendingReload.reset(); acceptedReloadEndTick = 0;
        reloadAnchorStartTick = 0;
    }

    void UpdateWeaponFeedback(Clock::time_point now, bool consumeInput = true) {
        weaponFeedback.inputCaptured = InWorld() && inputCaptured && input->Snapshot().windowFocused;
        weaponFeedback.yaw = view.Input().yaw;
        weaponFeedback.pitch = view.Input().pitch;
        if (!weaponFeedback.active) {
            pendingShotEdge = pendingReloadEdge = false;
            weaponFeedback.shooting = weaponFeedback.drawing = weaponFeedback.reloadAnimating =
                weaponFeedback.hitMarkerVisible = false;
            return;
        }
        const bool canAct = consumeInput && !pointerAcquiredThisFrame && !windowInteraction && weaponFeedback.inputCaptured;
        if (pendingReloadEdge && canAct && state.combatRules && !pendingReload && !weaponFeedback.reloading) {
            if (const auto id = connection.SubmitAction(ActionKind::Reload, viewLife, state.snapshot->tick)) {
                pendingReload = *id;
                weaponFeedback.lastActionId = *id;
                weaponFeedback.lastActionKind = ActionKind::Reload;
                ++weaponFeedback.submittedActions;
                weaponFeedback.lastSubmittedSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
            }
        }
        // One gate decides a shot: the earliest authority tick that can resolve
        // it must reach every known cooldown (FireGate.hpp). A blocked click is
        // dropped, never queued.
        const auto shotTiming = presented.shotTiming;
        if (consumeInput && pendingShotEdge && !pendingReloadEdge && !pendingReload && !weaponFeedback.reloading &&
            weaponFeedback.magazineAmmo > 0 && !pointerAcquiredThisFrame && !windowInteraction &&
            weaponFeedback.inputCaptured && state.combatRules) {
            const auto combat = std::find_if(state.snapshot->combat.begin(), state.snapshot->combat.end(),
                [this](const CombatState& value) { return value.playerId == state.playerId; });
            if (combat != state.snapshot->combat.end() && !fireGate.Allows(shotTiming)) {
                ++weaponFeedback.localCooldownBlocks;
            } else if (combat != state.snapshot->combat.end()) {
                if (const auto id = connection.SubmitAction(ActionKind::Shot, viewLife, state.snapshot->tick, view.Input().yaw, view.Input().pitch)) {
                    weaponFeedback.lastActionId = *id;
                    weaponFeedback.lastActionKind = ActionKind::Shot;
                    ++weaponFeedback.submittedActions;
                    ++weaponFeedback.animationStarts;
                    ++weaponFeedback.animationRevision;
                    weaponFeedback.lastSubmittedSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
                    weaponAction = fps::WeaponViewModelAction::Shoot;
                    weaponStartedAt = now;
                    fireGate.Submitted(*id, shotTiming, state.combatRules->cooldownTicks);
                }
            }
        }
        // A live frame leaves the pending edges to the regular update.
        if (consumeInput) pendingShotEdge = pendingReloadEdge = false;
        weaponFeedback.reloadPending = pendingReload.has_value();
        double elapsed{}, duration{};
        if (weaponFeedback.reloading && weaponFeedback.reloadEndTick > weaponFeedback.reloadStartTick) {
            // Authority owns the reload clock: the clip starts only once a snapshot
            // shows the reload, and local time merely smooths between snapshots.
            if (weaponAction != fps::WeaponViewModelAction::Reload ||
                reloadAnchorStartTick != weaponFeedback.reloadStartTick) {
                reloadAnchorStartTick = weaponFeedback.reloadStartTick;
                reloadAnchorAt = now;
                reloadAnchorSeconds = state.snapshot->tick > weaponFeedback.reloadStartTick ?
                    static_cast<double>(state.snapshot->tick - weaponFeedback.reloadStartTick) / AuthorityTickRate : 0;
                weaponAction = fps::WeaponViewModelAction::Reload;
                ++weaponFeedback.reloadAnimationStarts;
                ++weaponFeedback.animationRevision;
            }
            duration = static_cast<double>(weaponFeedback.reloadEndTick - weaponFeedback.reloadStartTick) / AuthorityTickRate;
            elapsed = Engine::Math::Min(duration,
                reloadAnchorSeconds + std::chrono::duration<double>(now - reloadAnchorAt).count());
        } else {
            if (weaponAction == fps::WeaponViewModelAction::Reload) {
                // Completed, cancelled by death or replaced by a new life.
                weaponAction = fps::WeaponViewModelAction::Idle;
                reloadAnchorStartTick = 0;
                ++weaponFeedback.animationRevision;
            }
            elapsed = std::chrono::duration<double>(now - weaponStartedAt).count();
            duration = weapon->GetActionDurationSeconds(weaponAction);
            if (weaponAction != fps::WeaponViewModelAction::Idle && elapsed >= duration) {
                weaponAction = fps::WeaponViewModelAction::Idle;
                ++weaponFeedback.animationRevision;
                elapsed = 0;
                duration = 0;
            }
        }
        weaponFeedback.actionElapsedSeconds = weaponAction == fps::WeaponViewModelAction::Idle ? 0 : elapsed;
        weaponFeedback.actionDurationSeconds = duration;
        weaponFeedback.shooting = weaponAction == fps::WeaponViewModelAction::Shoot;
        weaponFeedback.drawing = weaponAction == fps::WeaponViewModelAction::Draw;
        weaponFeedback.reloadAnimating = weaponAction == fps::WeaponViewModelAction::Reload;
        weaponFeedback.hitMarkerVisible = now < hitMarkerUntil;
        weaponFeedback.cooldownRemainingSeconds = state.combatRules ?
            static_cast<double>(fireGate.RemainingTicks(presented.shotTiming)) / AuthorityTickRate : 0;
    }

    bool SubmitBox(Engine::Math::Vec3 center, Engine::Math::Vec3 scale,
        Engine::Render::Color color, float rotation = 0.0F) {
        Engine::Render::MeshSubmission draw;
        draw.mesh = cube;
        draw.transform = {center, {0, rotation, 0}, scale};
        draw.material.tint = color;
        const auto result = queue.Submit(draw);
        if (!result) { lastError = Explain(result.error()); return false; }
        return true;
    }

    bool SubmitWeapon() {
        if (!Alive()) { weaponFeedback.submittedMeshes = 0; return true; }
        fps::WeaponViewModelFrame weaponFrame;
        weaponFrame.action = weaponAction;
        weaponFrame.elapsedSeconds = static_cast<float>(weaponFeedback.actionElapsedSeconds);
        weaponFrame.durationSeconds = static_cast<float>(weaponFeedback.actionDurationSeconds);
        // Cosmetic kick belongs solely to the dedicated viewmodel camera/layer.
        weaponFrame.recoilRadians = weaponFeedback.shooting ?
            .04F * std::exp(-14.0F * weaponFrame.elapsedSeconds) : 0;
        if (!weapon->Submit(weaponFrame, queue, lastError)) return false;
        const auto observation = weapon->GetObservation();
        weaponFeedback.meshCount = observation.meshCount;
        weaponFeedback.materialCount = observation.materialCount;
        weaponFeedback.submittedMeshes = observation.submittedMeshCount;
        weaponFeedback.poseRevision = observation.poseRevision;
        weaponFeedback.sampledAnimationSeconds = observation.sampledTimeSeconds;
        weaponFeedback.recoilRadians = observation.recoilRadians;
        return true;
    }

    bool PrepareWorld(double deltaSeconds) {
        remoteMovement.reset();
        remoteSubmitMilliseconds = 0;
        // Until the simulation role has stepped in this session and life, the
        // view stays where the authority shows the player.
        const auto* self = FindPlayer(*state.snapshot, state.playerId);
        const auto position = LocalPresentationCurrent() || !self ? presented.observation.renderPosition : self->position;
        const auto camera = view.Camera();
        queue.SetCamera({{position.x, position.y + arena->eyeHeight, position.z},
            {camera.pitch, camera.yaw, camera.roll}, WorldVerticalFovRadians, 0.05F, 150.0F});
        // Simple checker floor gives movement depth cues without campaign assets.
        const float tile = arena->cellSize;
        for (float z = 0; z < arena->depth; z += tile) {
            for (float x = 0; x < arena->width; x += tile) {
                const float w = Engine::Math::Min(tile, arena->width - x);
                const float d = Engine::Math::Min(tile, arena->depth - z);
                Engine::Render::MeshSubmission draw;
                draw.mesh = floor;
                draw.transform = {{x + w * .5F, 0, z + d * .5F}, {}, {w, 1, d}};
                const bool even = (static_cast<int>(x / tile) + static_cast<int>(z / tile)) % 2 == 0;
                draw.material.tint = even ? Engine::Render::Color{.11F,.15F,.19F,1} : Engine::Render::Color{.15F,.20F,.24F,1};
                draw.doubleSided = true;
                const auto result = queue.Submit(draw);
                if (!result) { lastError = Explain(result.error()); return false; }
            }
        }
        for (const auto& wall : arena->walls) {
            const auto& a = wall.minimum;
            const auto& b = wall.maximum;
            if (!SubmitBox(Engine::Math::Center(wall), b - a, {.20F,.31F,.39F,1})) return false;
            if (!SubmitBox({(a.x+b.x)*.5F,b.y-.06F,(a.z+b.z)*.5F},
                {b.x-a.x+.01F,.12F,b.z-a.z+.01F}, {.15F,.65F,.70F,1})) return false;
        }
        const auto presentationTime = Clock::now();
        std::vector<PlayerPresentationFrame> characterFrames;
        // The timeline's reanchor count is shared by every remote player: compare
        // each against the previous frame's value, and update it once per frame.
        const auto reanchorsBefore = characterPhaseReanchors;
        std::optional<std::uint64_t> reanchorsNow;
        const auto observed = ObservedRemotePlayer(*state.snapshot, state.playerId, observedRemote);
        for (const auto& player : state.snapshot->players) {
            if (player.playerId == state.playerId) continue;
            const auto sampled = timeline.Sample(player.playerId, presentationTime);
            const auto& presented = sampled ? sampled->player : player;
            const auto position = presented.position;
            // Diagnostics observe one remote player (ObservedRemotePlayer).
            if (player.playerId == observed) {
                remoteMovement.emplace();
                auto& observation = *remoteMovement;
                observation.playerId = player.playerId;
                observation.renderPosition = position;
                observation.movementEpoch = presented.movementEpoch;
                observation.lifeGeneration = presented.lifeGeneration;
                observation.lifeState = presented.lifeState;
                observation.yaw = presented.yaw;
                observation.pitch = presented.pitch;
                observation.ingressHistoryDrops = ingressHistoryDrops;
                if (sampled) {
                    observation.lowerTick = sampled->lowerTick;
                    observation.upperTick = sampled->upperTick;
                    observation.presentationTick = sampled->presentationTick;
                    observation.interpolationAlpha = sampled->alpha;
                    observation.latestReceiveAgeSeconds = sampled->latestReceiveAgeSeconds;
                    observation.missingFutureSnapshot = sampled->missingFutureSnapshot;
                    observation.holdSeconds = sampled->holdSeconds;
                    observation.totalHoldSeconds = sampled->totalHoldSeconds;
                    observation.historySize = sampled->historySize;
                    observation.holdCount = sampled->holdCount;
                    observation.gapCount = sampled->gapCount;
                    observation.historyEvictions = sampled->historyEvictions;
                    observation.phaseReanchors = sampled->phaseReanchors;
                    observation.holding = sampled->holding;
                    observation.lowerResolvedCommand = sampled->lowerResolvedCommand;
                    observation.upperResolvedCommand = sampled->upperResolvedCommand;
                }
            }
            PlayerPresentationFrame characterFrame;
            characterFrame.playerId = player.playerId;
            characterFrame.movementEpoch = presented.movementEpoch;
            characterFrame.lifeGeneration = presented.lifeGeneration;
            characterFrame.dead = presented.lifeState == LifeState::Dead;
            characterFrame.position = position;
            characterFrame.yaw = presented.yaw;
            characterFrame.pitch = presented.pitch;
            characterFrame.presentationSeconds = sampled ? sampled->presentationTick / AuthorityTickRate :
                static_cast<double>(state.snapshot->tick) / AuthorityTickRate;
            characterFrame.deltaSeconds = deltaSeconds;
            characterFrame.planarSpeed = sampled ? sampled->planarSpeed : 0;
            characterFrame.continuous = sampled && !characterPresentationSkipped &&
                sampled->phaseReanchors == reanchorsBefore;
            characterFrame.holding = sampled && sampled->holding;
            characterFrame.grounded = presented.grounded;
            characterFrame.verticalVelocity = presented.verticalVelocity;
            characterFrame.lifeStateSeconds = static_cast<double>(presented.lifeStateTick) / AuthorityTickRate;
            // Actions come only from the sampled interval's own combat state, so
            // newer combat is never paired with an older position or life.
            if (sampled && sampled->combat) {
                const auto& combat = *sampled->combat;
                if (combat.lastShotActionId) {
                    characterFrame.shotActionId = combat.lastShotActionId;
                    characterFrame.shotSeconds = static_cast<double>(combat.lastShotTick) / AuthorityTickRate;
                }
                if (combat.damageCount) {
                    characterFrame.damageCount = combat.damageCount;
                    characterFrame.damageSeconds = static_cast<double>(combat.lastDamageTick) / AuthorityTickRate;
                }
                if (combat.reloadActionId && combat.reloadEndTick > combat.reloadStartTick) {
                    characterFrame.reloadActionId = combat.reloadActionId;
                    characterFrame.reloadStartSeconds = static_cast<double>(combat.reloadStartTick) / AuthorityTickRate;
                    characterFrame.reloadEndSeconds = static_cast<double>(combat.reloadEndTick) / AuthorityTickRate;
                }
            }
            characterFrames.push_back(characterFrame);
            if (sampled) reanchorsNow = sampled->phaseReanchors;
        }
        if (reanchorsNow) characterPhaseReanchors = *reanchorsNow;
        const auto characterStarted = Clock::now();
        if (!players->Submit(characterFrames, queue, lastError)) return false;
        remoteSubmitMilliseconds = Milliseconds(Clock::now(), characterStarted);
        if (remoteMovement) {
            for (const auto& character : players->Observations())
                if (character.playerId == remoteMovement->playerId) remoteMovement->character = character;
        }
        return SubmitWeapon();
    }

    // Diagnostics only (SDL_Log, which the Client log file records): connection
    // transitions, connection-quality warnings and one summary per second in a match.
    struct SessionLog {
        ConnectionPhase phase{ConnectionPhase::Lobby};
        std::string error;
        std::uint32_t qualityFailures{};
        Clock::time_point windowStart{}, lastSnapshotAt{};
        std::uint64_t frames{}, snapshots{};
        double longestFrameSeconds{};
        std::uint64_t submitted{}, accepted{}, rejected{}, hits{}, localBlocks{};
    } sessionLog;

    void LogSession(double frameSeconds) {
        auto& log = sessionLog;
        static constexpr const char* phases[] = {"lobby", "requesting", "connecting", "playing"};
        if (state.phase != log.phase) {
            SDL_Log("PvP connection phase=%s player=%llu arena=%s", phases[static_cast<int>(state.phase)],
                    static_cast<unsigned long long>(state.playerId), state.arenaId.empty() ? "-" : state.arenaId.c_str());
            log.phase = state.phase;
        }
        if (state.error != log.error) {
            if (!state.error.empty()) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PvP connection error=%s", state.error.c_str());
            log.error = state.error;
        }
        const PlayerState* self = state.snapshot ? FindPlayer(*state.snapshot, state.playerId) : nullptr;
        const std::uint32_t failures = self ? self->connectionQualityFailures : 0;
        if (failures != log.qualityFailures) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PvP connection quality failed_windows=%u of %u",
                        failures, ConnectionQualityFailedWindows);
            log.qualityFailures = failures;
        }
        const auto now = Clock::now();
        if (state.phase != ConnectionPhase::Playing || !self) {
            log.windowStart = now;
            log.frames = log.snapshots = 0;
            log.longestFrameSeconds = 0;
            return;
        }
        ++log.frames;
        log.longestFrameSeconds = Engine::Math::Max(log.longestFrameSeconds, frameSeconds);
        const double window = std::chrono::duration<double>(now - log.windowStart).count();
        if (window < 1) return;
        const auto& weapon = weaponFeedback;
        const double snapshotAge = log.lastSnapshotAt == Clock::time_point{} ? -1 :
            std::chrono::duration<double>(now - log.lastSnapshotAt).count() * 1000;
        SDL_Log("PvP summary player=%llu tick=%llu players=%zu fps=%.1f longest_frame_ms=%.1f snapshots_per_s=%.1f "
                "snapshot_age_ms=%.1f pending_commands=%zu hp=%u life=%llu dead=%d actions_submitted=%llu accepted=%llu "
                "rejected=%llu hits=%llu local_cooldown_blocks=%llu actions_pending=%zu",
                static_cast<unsigned long long>(state.playerId), static_cast<unsigned long long>(state.snapshot->tick),
                state.snapshot->players.size(), log.frames / window, log.longestFrameSeconds * 1000, log.snapshots / window,
                snapshotAge, static_cast<std::size_t>(presented.observation.pendingCommands), weapon.hp,
                static_cast<unsigned long long>(self->lifeGeneration), self->lifeState == LifeState::Dead ? 1 : 0,
                static_cast<unsigned long long>(weapon.submittedActions - log.submitted),
                static_cast<unsigned long long>(weapon.acceptedDecisions - log.accepted),
                static_cast<unsigned long long>(weapon.rejectedDecisions - log.rejected),
                static_cast<unsigned long long>(weapon.hitDecisions - log.hits),
                static_cast<unsigned long long>(weapon.localCooldownBlocks - log.localBlocks), state.actionTransport.pending);
        log.submitted = weapon.submittedActions;
        log.accepted = weapon.acceptedDecisions;
        log.rejected = weapon.rejectedDecisions;
        log.hits = weapon.hitDecisions;
        log.localBlocks = weapon.localCooldownBlocks;
        log.windowStart = now;
        log.frames = log.snapshots = 0;
        log.longestFrameSeconds = 0;
    }

    void SelectArena(const std::string& id) {
        for (const auto& installed : arenas) {
            if (installed.id != id) continue;
            // The simulation role selects the same arena from its own drain.
            arena = installed;
            SDL_Log("PvP arena selected id=%s spawns=%zu", arena->id.c_str(), arena->spawns.size());
            return;
        }
    }

    Control Fail(std::string message) {
        lastError = std::move(message);
        exitCode = 1;
        return Control::Stop;
    }
};

PvpApplication::PvpApplication() : impl_(std::make_unique<Impl>()) {}
PvpApplication::~PvpApplication() = default;

bool PvpApplication::InitializeContent(const std::filesystem::path& assetRoot, std::string& error) {
    namespace Asset = Engine::Asset;
    error.clear();
    auto manifest = Asset::ContentManifest::Load(assetRoot);
    if (!manifest) { error = Explain(manifest.error()); return false; }
    auto catalog = manifest.value().LoadCatalogs(assetRoot);
    if (!catalog) { error = Explain(catalog.error()); return false; }
    impl_->catalog = std::move(catalog).value();
    for (const auto& bundle : manifest.value().shaderBundles) {
        Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = (assetRoot / bundle.path).string();
        options.allowAbsolutePath = false;
        options.allowEscapeAssetsRoot = false;
        const auto loaded = impl_->shaders.AppendBundle(impl_->source,
            Asset::Resolver::AssetPathResolver(std::move(options)));
        if (!loaded) { error = Explain(loaded.error()); return false; }
    }
    if (!impl_->shaders.CompleteFormats()) { error = "Shader bundles have no complete common format"; return false; }
    impl_->loaders.Register(std::make_unique<Asset::Loaders::FontLoader>());
    impl_->loaders.Register(std::make_unique<Asset::Loaders::TextLoader>());
    impl_->loaders.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>());
    impl_->loaders.Register(std::make_unique<Asset::Loaders::SdlImage::SdlImageTextureLoader>());
    auto installed = LoadInstalledArenas(assetRoot / "arenas.json", error);
    if (!installed) return false;
    std::vector<ArenaIdentity> identities;
    for (const auto& arena : *installed) {
        const auto digest = ArenaContentDigest(arena);
        if (!digest) { error = "Arena content digest is zero: " + arena.id; return false; }
        identities.push_back({arena.id, arena.version, digest});
    }
    impl_->arenas = std::move(*installed);
    impl_->arena = impl_->arenas.front();
    impl_->connection.SetArenaIdentities(std::move(identities));
    try {
        impl_->simulation = std::make_unique<ClientSimulationRole>(impl_->connection, impl_->arenas);
    } catch (const std::exception& failure) {
        error = failure.what();
        return false;
    }
    const auto loaded = impl_->assets.Load(Asset::AssetId::FromString("object_fps_pvp.ui.pvp_lobby"),
        Asset::AssetRequest::WithTypeHint(Asset::AssetType::Text()));
    if (!loaded) { error = Explain(loaded.error()); return false; }
    const auto text = impl_->assets.GetSharedConst<Asset::Loaders::TextAsset>(loaded.value());
    if (!text) {
        impl_->assets.Release(loaded.value());
        error = "PvP Lobby asset has no text payload";
        return false;
    }
    auto document = Engine::Ui::UiDocumentCodec::Parse(text->text, "object_fps_pvp.ui.pvp_lobby");
    impl_->assets.Release(loaded.value());
    if (!document) { error = ExplainUi(document.error()); return false; }
    const auto ready = impl_->ui.Initialize(std::make_shared<const Engine::Ui::UiDocument>(std::move(document).value()));
    if (!ready) { error = ExplainUi(ready.error()); return false; }
    const auto activated = impl_->ui.ActivateCanvas("lobby");
    if (!activated) { error = ExplainUi(activated.error()); return false; }
    const auto feedback = impl_->assets.Load(Asset::AssetId::FromString("object_fps_pvp.ui.hit_feedback"),
        Asset::AssetRequest::WithTypeHint(Asset::AssetType::Text()));
    if (!feedback) { error = Explain(feedback.error()); return false; }
    const auto feedbackText = impl_->assets.GetSharedConst<Asset::Loaders::TextAsset>(feedback.value());
    const auto settings = feedbackText ? ParseHitFeedbackSettings(feedbackText->text, error) : std::nullopt;
    impl_->assets.Release(feedback.value());
    if (!settings) {
        if (error.empty()) error = "PvP hit feedback asset has no text payload";
        return false;
    }
    impl_->hitSettings = *settings;
    return true;
}

bool PvpApplication::InitializeGraphics(const PvpApplicationOptions& options, std::string& error) {
    const auto started = Clock::now();
    error.clear();
    if (!impl_->arena) { error = "PvP content must be initialized before graphics"; return false; }
    Engine::Platform::Sdl::SdlPlatformOptions platformOptions;
    platformOptions.title = options.title;
    platformOptions.width = options.width;
    platformOptions.height = options.height;
    platformOptions.resizable = true;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(platformOptions);
    if (!platform) { error = Explain(platform.error()); return false; }
    impl_->platform = std::move(platform).value();
    Engine::Render::Backend::SdlGpu::SdlGpuOptions gpu;
    gpu.availableShaderFormats = impl_->shaders.CompleteFormats();
    gpu.driver = options.gpuDriver;
    gpu.vsync = options.vsync;
    auto device = Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice::Create(*impl_->platform, gpu);
    if (!device) { error = Explain(device.error()); return false; }
    impl_->device = std::move(device).value();
    const auto renderer = impl_->renderer.Initialize(*impl_->device, impl_->shaders);
    if (!renderer) { error = Explain(renderer.error()); return false; }
    auto text = Engine::Text::Backend::SdlTtf::SdlTtfTextRasterizer::Create();
    if (!text) { error = Explain(text.error()); return false; }
    impl_->text = std::move(text).value();
    const auto ui = impl_->uiRenderer.Initialize(*impl_->device, *impl_->text, impl_->assets);
    if (!ui) { error = ExplainUi(ui.error()); return false; }
    auto cube = impl_->device->CreateMesh(Engine::Render::MakeUnitCube().View());
    if (!cube) { error = Explain(cube.error()); return false; }
    impl_->cube = cube.value();
    auto floor = impl_->device->CreateMesh(Engine::Render::MakeUnitQuadXZ().View());
    if (!floor) { error = Explain(floor.error()); return false; }
    impl_->floor = floor.value();
    impl_->weapon = std::make_unique<fps::WeaponViewModel>();
    if (!impl_->weapon->Initialize(*impl_->device, impl_->assets,
        Engine::Asset::AssetId::FromString("object_fps_pvp.weapon.mark23"), error)) return false;
    impl_->players = std::make_unique<PlayerPresentation>();
    // The current two-player product needs one remote slot. Preallocate it so
    // joining/rejoining never creates a skinned GPU instance in a live frame.
    if (!impl_->players->Initialize(*impl_->device, impl_->assets, impl_->arena->bodyHeight, MaxPlayers - 1, error)) return false;
    // Prepare the actual viewmodel pass before joining. Model, images and GPU
    // resources are loaded above; rendering Idle also warms the lazy pipelines
    // shared by Draw and Shoot, so the first click performs no asset loading.
    bool weaponPresented = false;
    for (unsigned attempt = 0; attempt < 8 && !weaponPresented; ++attempt) {
        impl_->queue.Reset({});
        impl_->queue.SetCamera({{0, impl_->arena->eyeHeight, 0}, {}, WorldVerticalFovRadians, 0.05F, 150.0F});
        if (!impl_->players->SubmitWarmup(impl_->queue, error)) return false;
        if (!impl_->weapon->Submit(fps::WeaponViewModelFrame{}, impl_->queue, error)) return false;
        const auto warmup = impl_->renderer.Render(impl_->queue);
        if (!warmup) { error = Explain(warmup.error()); return false; }
        weaponPresented = warmup.value() == Engine::Render::PresentStatus::Presented;
        if (!weaponPresented) SDL_Delay(10);
    }
    if (!weaponPresented) { error = "Cannot prepare the player and weapon presentation surface"; return false; }
    impl_->players->ResetPlayers();
    impl_->ResetWeaponFeedback();
    impl_->input = std::make_unique<Engine::Input::Backend::Sdl::SdlInput>(*impl_->platform);
    impl_->width = static_cast<float>(options.width);
    impl_->height = static_cast<float>(options.height);
    impl_->address = options.gateway;
    impl_->initialized = true;
    SDL_Log("PvP graphics ready driver=%s initialization_ms=%.1f",
        impl_->device->GetInfo().driver.c_str(), Milliseconds(Clock::now(), started));
    return true;
}

Control PvpApplication::ProcessEvents(const Engine::Runtime::FrameContext&) {
    if (!impl_->initialized) return impl_->Fail("PvP application is not initialized");
    const auto started = Clock::now();
    impl_->suppressUiEnter = false;
    impl_->windowInteraction = false;
    impl_->pointerAcquiredThisFrame = false;
    impl_->pendingShotEdge = false;
    impl_->liveFramesThisPump = 0;
    impl_->input->BeginFrame();
    const bool focusedAtStart = impl_->input->Snapshot().windowFocused;
    double observerMilliseconds = 0;
    const auto control = impl_->platform->PumpEvents([this, &observerMilliseconds](const SDL_Event& event) {
        const auto observed = Clock::now();
        impl_->input->HandleEvent(event);
        impl_->HandleTextEditing(event);
        observerMilliseconds += Milliseconds(Clock::now(), observed);
    });
    impl_->input->EndFrame();
    impl_->ApplyInputEvents(focusedAtStart);
    const auto elapsed = Milliseconds(Clock::now(), started);
    // observer_ms is this product's event handling; the rest is SDL's polling,
    // including any OS modal loop and the live frames run inside it.
    if (elapsed >= 250) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
        "PvP slow event processing elapsed_ms=%.1f observer_ms=%.1f live_frames=%llu",
        elapsed, observerMilliseconds, static_cast<unsigned long long>(impl_->liveFramesThisPump));
    return control;
}

Control PvpApplication::Update(const Engine::Runtime::FrameContext& frame) {
    if (!impl_->initialized) return impl_->Fail("PvP application is not initialized");
    const auto started = Clock::now();
    // The simulation role reports its own wake gaps as runtime gaps; a slow
    // frame here no longer delays commands.
    if (auto failure = impl_->simulation->Error()) return impl_->Fail("PvP client simulation stopped: " + *failure);
    using Engine::Input::Key;
    // A live frame sees no input: no keys, no pointer motion, no focus.
    static const Engine::Input::PhysicalInputFrame noInput{};
    const bool live = impl_->liveFrame;
    const auto& physical = live ? noInput : impl_->input->Snapshot();
    impl_->assets.BeginFrame(frame.frameIndex);
    impl_->assets.Update();
    impl_->RefreshState();
    if (impl_->InWorld()) {
        impl_->EditAddress(false);
        if (physical.Get(Key::Escape).pressed) {
            impl_->connection.Leave();
            impl_->RefreshState();
        } else {
            // Consume a presentation frame's raw mouse delta exactly once.
            if (impl_->Alive() && !impl_->lifeBoundaryThisFrame && impl_->inputCaptured && physical.windowFocused && physical.pointer.relativeMode) {
                if (physical.pointer.deltaX != 0 || physical.pointer.deltaY != 0)
                    ++impl_->weaponFeedback.mouseDeltaConsumeCount;
                impl_->view.Turn(physical.pointer.deltaX, physical.pointer.deltaY);
            }
            const float forward = impl_->Alive() && !impl_->lifeBoundaryThisFrame && impl_->inputCaptured && physical.windowFocused ?
                static_cast<float>(physical.Get(Key::W).held) - static_cast<float>(physical.Get(Key::S).held) : 0;
            const float right = impl_->Alive() && !impl_->lifeBoundaryThisFrame && impl_->inputCaptured && physical.windowFocused ?
                static_cast<float>(physical.Get(Key::D).held) - static_cast<float>(physical.Get(Key::A).held) : 0;
            // Sampled after refresh, so the intent and the placed presentation
            // describe this frame's input and state.
            const auto movementSampledAt = Clock::now();
            const bool controls = impl_->Alive() && !impl_->lifeBoundaryThisFrame && impl_->inputCaptured && physical.windowFocused &&
                !impl_->pointerAcquiredThisFrame && !impl_->windowInteraction;
            impl_->pendingReloadEdge = controls && physical.Get(Key::R).pressed;
            // A live frame sees no input, so it publishes a neutral intent without
            // controls and the player stops at once; it never advances the prediction.
            if (controls && physical.Get(Key::Space).pressed) ++impl_->jumpPresses;
            impl_->simulation->PublishIntent({forward, right, impl_->view.Input().yaw, impl_->view.Input().pitch,
                controls, impl_->jumpPresses, movementSampledAt, IntentOwner(impl_->state)});
            impl_->presented = impl_->simulation->PresentAt(movementSampledAt);
            // Until the role has stepped in this session, it has no local player to show.
            if (impl_->presented.generation != impl_->connectionGeneration) impl_->presented = {};
            impl_->UpdateWeaponFeedback(Clock::now(), !live);
        }
    } else {
        Engine::Ui::UiInputFrame uiInput;
        uiInput.focusPreviousPressed = !impl_->editing && physical.Get(Key::Up).pressed;
        uiInput.focusNextPressed = !impl_->editing && physical.Get(Key::Down).pressed;
        uiInput.activatePressed = !impl_->editing && !impl_->suppressUiEnter && physical.Get(Key::Enter).pressed;
        uiInput.pointerAvailable = physical.windowFocused && !physical.pointer.relativeMode;
        uiInput.pointerPixels = {physical.pointer.x, physical.pointer.y};
        const auto& click = physical.Get(Engine::Input::MouseButton::Left);
        uiInput.pointerPrimaryPressed = click.pressed;
        uiInput.pointerPrimaryHeld = click.held;
        uiInput.pointerPrimaryReleased = click.released;
        const auto updated = impl_->ui.Update(uiInput, impl_->Bindings(), impl_->Viewport());
        if (!updated) return impl_->Fail(ExplainUi(updated.error()));
        for (const auto& event : updated.value()) impl_->Execute(event.action);
    }
    const auto mouseStarted = Clock::now();
    if (!live) {
        const auto relative = impl_->input->SetRelativeMouseMode(impl_->InWorld() && impl_->inputCaptured && physical.windowFocused);
        if (!relative) return impl_->Fail(Explain(relative.error()));
    }
    impl_->LogSession(frame.deltaSeconds);
    const auto finished = Clock::now();
    if (Milliseconds(finished, started) >= 250)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PvP slow update player=%llu elapsed_ms=%.1f mouse_mode_ms=%.1f frame_gap_ms=%.1f",
            static_cast<unsigned long long>(impl_->state.playerId), Milliseconds(finished, started),
            Milliseconds(finished, mouseStarted), frame.deltaSeconds * 1000);
    return impl_->quit ? Control::Stop : Control::Continue;
}

Control PvpApplication::Render(const Engine::Runtime::FrameContext& context) {
    impl_->presentedMovement.reset();
    const auto started = Clock::now();
    const bool world = impl_->InWorld();
    const bool firstWorldFrame = world && impl_->renderedPlayer != impl_->state.playerId;
    Engine::Render::FrameDescription frame;
    frame.clearColor = {.04F,.065F,.10F,1};
    impl_->queue.Reset(frame);
    Engine::Ui::UiDrawList draws;
    double prepareWorldMilliseconds = 0;
    // First-person hit feedback: sampled once per frame; the shake reaches the camera only.
    impl_->hitSample = impl_->InWorld() ? impl_->hitFeedback.Sample(
        std::chrono::duration<double>(Clock::now().time_since_epoch()).count(), impl_->view.Input().yaw,
        impl_->hitSettings) : HitFeedbackSample{};
    impl_->view.Present(impl_->hitSample);
    impl_->weaponFeedback.hitFlashAlpha = impl_->hitSample.flashAlpha;
    impl_->weaponFeedback.hitDirectionVisible = impl_->hitSample.directionRadians.has_value();
    if (impl_->InWorld()) {
        const auto worldStarted = Clock::now();
        if (!impl_->PrepareWorld(context.deltaSeconds)) return impl_->Fail(impl_->lastError);
        prepareWorldMilliseconds = Milliseconds(Clock::now(), worldStarted);
        const float scale = Engine::Math::Min(impl_->width/1280.0F, impl_->height/720.0F);
        const auto& hit = impl_->hitSample;
        const auto& feedback = impl_->hitSettings;
        if (hit.flashAlpha > 0)
            draws.commands.emplace_back(Engine::Ui::UiQuadDraw{{0, 0, impl_->width, impl_->height},
                {feedback.flash.color[0], feedback.flash.color[1], feedback.flash.color[2], hit.flashAlpha}});
        draws.commands.emplace_back(Engine::Ui::UiQuadDraw{{16,16,680*scale,114*scale},{.015F,.025F,.04F,.88F}});
        AddText(draws, "PLAYERS ONLINE: " + std::to_string(impl_->state.snapshot->players.size()),
            {16+14*scale,16+8*scale,490*scale,25*scale},18*scale);
        AddText(draws, "WASD move | Space jump | Click shoot | R reload | Tab cursor | Esc leave",
            {16+14*scale,16+34*scale,660*scale,22*scale},14*scale);
        AddText(draws, "HP " + std::to_string(impl_->weaponFeedback.hp) + " / " +
            std::to_string(impl_->weaponFeedback.maximumHp) + "    MARK23  " +
            std::to_string(impl_->weaponFeedback.magazineAmmo) + " / " +
            std::to_string(impl_->weaponFeedback.magazineCapacity) + "   reserve: unlimited",
            {16+14*scale,16+58*scale,660*scale,22*scale},14*scale, hit.hpHighlighted ?
                Engine::Ui::UiColor{feedback.hpHighlight.color[0], feedback.hpHighlight.color[1],
                                    feedback.hpHighlight.color[2], 1} : Engine::Ui::UiColor{0.9F, 0.96F, 1.0F, 1.0F});
        if (impl_->weaponFeedback.dead) {
            const auto remaining = impl_->weaponFeedback.respawnRemainingSeconds;
            AddText(draws, remaining > 0 ? "DEAD - respawn in " + std::to_string(static_cast<int>(std::ceil(remaining))) + "s" :
                "DEAD - waiting for a clear spawn", {16+14*scale,16+84*scale,620*scale,22*scale},16*scale);
        } else if (impl_->weaponFeedback.reloading || impl_->weaponFeedback.reloadPending) {
            AddText(draws, impl_->weaponFeedback.reloading ? "RELOADING " +
                std::to_string(static_cast<int>(impl_->weaponFeedback.reloadProgress * 100)) + "%" :
                "Reload requested", {16+14*scale,16+84*scale,620*scale,22*scale},16*scale);
        }
        draws.commands.emplace_back(Engine::Ui::UiQuadDraw{{impl_->width*.5F-5,impl_->height*.5F-1,10,2},{.9F,.96F,1,1}});
        draws.commands.emplace_back(Engine::Ui::UiQuadDraw{{impl_->width*.5F-1,impl_->height*.5F-5,2,10},{.9F,.96F,1,1}});
        if (hit.directionRadians) {
            // A dotted arc around the crosshair toward the attacker; 0 is ahead, positive to the right.
            const auto& direction = feedback.direction;
            const float radius = direction.radiusPixels * scale, dot = direction.dotPixels * scale;
            for (std::uint32_t i = 0; i < direction.dots; ++i) {
                const float angle = *hit.directionRadians +
                    direction.arcRadians * (static_cast<float>(i) / static_cast<float>(direction.dots - 1) - .5F);
                draws.commands.emplace_back(Engine::Ui::UiQuadDraw{
                    {impl_->width*.5F + radius*std::sin(angle) - dot*.5F, impl_->height*.5F - radius*std::cos(angle) - dot*.5F, dot, dot},
                    {direction.color[0], direction.color[1], direction.color[2], hit.directionAlpha}});
            }
        }
        if (impl_->weaponFeedback.hitMarkerVisible) {
            for (const float x : {-9.0F, 6.0F}) for (const float y : {-9.0F, 6.0F})
                draws.commands.emplace_back(Engine::Ui::UiQuadDraw{
                    {impl_->width*.5F+x,impl_->height*.5F+y,3,3},{1,.35F,.12F,1}});
        }
        if (Clock::now() < impl_->rejectionUntil) {
            std::string reason;
            switch (impl_->weaponFeedback.lastRejection) {
            case ShotRejection::Dead: reason = "Action rejected: dead"; break;
            case ShotRejection::StaleLife: reason = "Action rejected: previous life"; break;
            case ShotRejection::InvalidLife: reason = "Action rejected: invalid life"; break;
            case ShotRejection::Reloading: reason = "Action rejected: reloading"; break;
            case ShotRejection::EmptyMagazine: reason = "Action rejected: empty magazine"; break;
            case ShotRejection::MagazineFull: reason = "Action rejected: magazine full"; break;
            case ShotRejection::Cooldown: reason = "Shot rejected: cooling down"; break;
            case ShotRejection::Expired: reason = "Shot rejected: request expired"; break;
            case ShotRejection::InvalidReference: reason = "Shot rejected: world state unavailable"; break;
            default: break;
            }
            if (!reason.empty()) AddText(draws, reason, {30,136,620*scale,22*scale},14*scale);
        } else if (impl_->state.actionTransport.pending > 0)
            AddText(draws, "Action pending confirmation", {30,136,620*scale,22*scale},14*scale);
        if (!impl_->weaponFeedback.dead && (!impl_->inputCaptured || !impl_->input->Snapshot().windowFocused))
            AddText(draws, "Click inside the window to resume input", {impl_->width*.5F-190,110,480,30});
        // The Match evicts after ConnectionQualityFailedWindows failed windows in a row.
        const auto& players = impl_->state.snapshot->players;
        if (const auto self = std::find_if(players.begin(), players.end(),
                [&](const auto& player) { return player.playerId == impl_->state.playerId; });
            self != players.end() && self->connectionQualityFailures > 0 &&
            self->connectionQualityFailures < ConnectionQualityFailedWindows) {
            const auto seconds = (ConnectionQualityFailedWindows - self->connectionQualityFailures) *
                ConnectionQualityWindowTicks / AuthorityTickRate;
            AddText(draws, "CONNECTION POOR - improve within " + std::to_string(seconds) + "s or you will be removed",
                // Bottom centre: the top-left HUD panel and the centred focus hint own the top.
                {impl_->width*.5F-310,impl_->height-70,620,30});
        }
    } else {
        const auto composed = impl_->ui.Compose(impl_->Bindings(), impl_->Viewport());
        if (!composed) return impl_->Fail(ExplainUi(composed.error()));
        draws = composed.value();
    }
    const auto prepared = Clock::now();
    const auto ui = impl_->uiRenderer.Submit(draws, impl_->queue);
    if (!ui) return impl_->Fail(ExplainUi(ui.error()));
    const auto uiFinished = Clock::now();
    const auto rendered = impl_->renderer.Render(impl_->queue);
    if (!rendered) return impl_->Fail(Explain(rendered.error()));
    const auto finished = Clock::now();
    if (firstWorldFrame || Milliseconds(finished, started) >= 250 || context.deltaSeconds >= .25)
        SDL_Log("PvP render player=%llu first_world_frame=%d frame_gap_ms=%.1f prepare_ms=%.1f ui_ms=%.1f render_ms=%.1f presented=%d",
            static_cast<unsigned long long>(impl_->state.playerId), firstWorldFrame, context.deltaSeconds * 1000,
            Milliseconds(prepared, started), Milliseconds(uiFinished, prepared), Milliseconds(finished, uiFinished),
            rendered.value() == Engine::Render::PresentStatus::Presented);
    if (rendered.value() == Engine::Render::PresentStatus::Presented) {
        impl_->characterPresentationSkipped = false;
        impl_->renderedPlayer = world ? impl_->state.playerId : 0;
        if (world) {
            if (impl_->remoteMovement) {
                const auto& remote = *impl_->remoteMovement;
                PlayerState pose{remote.playerId, remote.renderPosition, remote.yaw, remote.pitch};
                pose.movementEpoch = remote.movementEpoch;
                pose.lifeGeneration = remote.lifeGeneration;
                pose.lifeState = remote.lifeState;
                impl_->remoteMovement->holding = impl_->timeline.CommitPresented(finished, &pose, remote.holding);
                impl_->remoteMovement->holdCount = impl_->timeline.HoldCount();
                impl_->remoteMovement->holdSeconds = impl_->timeline.HoldSeconds();
                impl_->remoteMovement->totalHoldSeconds = impl_->timeline.TotalHoldSeconds();
            } else (void)impl_->timeline.CommitPresented(finished, nullptr, false);
        }
        // Until the simulation role has stepped in this session and life the frame
        // shows the authority position and there is no local movement to present.
        if (world && impl_->LocalPresentationCurrent()) impl_->presentedMovement = PresentedMovementObservation{
            context.frameIndex, std::chrono::duration<double>(finished.time_since_epoch()).count(),
            impl_->state.playerId, impl_->presented.observation, impl_->remoteMovement,
            impl_->skippedPresentationFrames, impl_->connectionGeneration, impl_->weaponFeedback,
            prepareWorldMilliseconds, impl_->remoteSubmitMilliseconds};
        if (world) {
            MovementTraceEvent event;
            event.kind = MovementTraceKind::Presentation;
            event.lifeGeneration = impl_->weaponFeedback.lifeGeneration;
            event.timeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(finished.time_since_epoch()).count();
            event.playerId = impl_->state.playerId;
            event.epoch = impl_->presented.observation.movementEpoch;
            event.sequence = impl_->presented.observation.latestCommand;
            event.authorityTick = impl_->presented.observation.authorityTick;
            event.frameSeconds = context.deltaSeconds;
            TraceMovement(event);
        }
    } else {
        // Discard travel selected for a frame that never became visible. The
        // next successful frame starts a fresh animation displacement segment.
        impl_->characterPresentationSkipped = true;
        ++impl_->skippedPresentationFrames;
        // A minimized window has no swapchain wait to pace the runtime loop.
        SDL_Delay(10);
    }
    return Control::Continue;
}

int PvpApplication::Run() {
    Engine::Runtime::RuntimeLoop loop(*this);
    // The handler refers to this loop: it is removed however Run ends.
    struct HandlerScope {
        Engine::Platform::Sdl::SdlPlatform& platform;
        ~HandlerScope() { platform.SetLiveFrameHandler({}); }
    } handlerScope{*impl_->platform};
    impl_->platform->SetLiveFrameHandler([this, &loop] {
        // SDL applies a resize to the window before its queued event reaches
        // the pump, so live frames take the size from the window itself.
        int windowWidth{}, windowHeight{};
        if (SDL_GetWindowSize(impl_->platform->NativeWindow(), &windowWidth, &windowHeight)) {
            impl_->width = static_cast<float>(Engine::Math::Max(1, windowWidth));
            impl_->height = static_cast<float>(Engine::Math::Max(1, windowHeight));
        }
        struct LiveScope {
            bool& live;
            explicit LiveScope(bool& value) : live(value) { live = true; }
            ~LiveScope() { live = false; }
        } liveScope{impl_->liveFrame};
        if (loop.RunLiveFrame()) ++impl_->liveFramesThisPump;
    });
    loop.Run();
    impl_->connection.Leave();
    impl_->RefreshState();
    if (!impl_->lastError.empty()) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", impl_->lastError.c_str());
    return impl_->exitCode;
}
void PvpApplication::SetGatewayAddress(std::string address) { impl_->address = std::move(address); }
void PvpApplication::ObserveRemote(std::optional<PlayerId> playerId) { impl_->observedRemote = playerId; }

std::optional<PlayerId> ObservedRemotePlayer(const WorldSnapshot& snapshot, const PlayerId local,
                                             const std::optional<PlayerId> chosen) noexcept {
    std::optional<PlayerId> last;
    for (const auto& player : snapshot.players) {
        if (player.playerId == local) continue;
        if (chosen && player.playerId == *chosen) return chosen;
        last = player.playerId;
    }
    return chosen ? std::nullopt : last;
}
ClientConnection& PvpApplication::Connection() { return impl_->connection; }
Engine::Render::Renderer& PvpApplication::Renderer() { return impl_->renderer; }
Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice& PvpApplication::RenderDevice() { return *impl_->device; }
Engine::Platform::Sdl::SdlPlatform& PvpApplication::Platform() { return *impl_->platform; }
const LocalMovementObservation& PvpApplication::LocalMovement() const noexcept {
    return impl_->presented.observation;
}
Engine::Time::TimePoint PvpApplication::LocalMovementIntentSampledAt() const noexcept {
    return impl_->presented.intentSampledAt;
}
Engine::Time::TimePoint PvpApplication::LocalMovementSteppedAt() const noexcept {
    return impl_->presented.steppedAt;
}
const std::optional<RemoteMovementObservation>& PvpApplication::RemoteMovement() const noexcept {
    return impl_->remoteMovement;
}
const std::optional<PresentedMovementObservation>& PvpApplication::PresentedMovement() const noexcept {
    return impl_->presentedMovement;
}
const WeaponFeedbackObservation& PvpApplication::WeaponFeedback() const noexcept {
    return impl_->weaponFeedback;
}
std::uint64_t PvpApplication::SkippedPresentationFrames() const noexcept {
    return impl_->skippedPresentationFrames;
}
const std::string& PvpApplication::LastError() const noexcept { return impl_->lastError; }
int PvpApplication::ExitCode() const noexcept { return impl_->exitCode; }
} // namespace fps::pvp
