#pragma once

#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "RetroFPS/Pvp/Combat.hpp"
#include "RetroFPS/Pvp/PlayerAnimation.hpp"
#include "engine/render/RenderTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Engine::Asset { class AssetManager; }
namespace Engine::Render { class IRenderDevice; class RenderQueue; }

namespace fps::pvp {

// Already sampled presentation values, never movement commands or authority state.
struct PlayerPresentationFrame final {
    std::uint64_t playerId{}, movementEpoch{};
    Engine::Math::Vec3 position{};
    // Look direction; a positive pitch looks down (the camera and ShotQuery sign).
    float yaw{}, pitch{};
    double presentationSeconds{}, deltaSeconds{};
    // The Match's animation at this sample (protocol-v9 §4.4): the gait phase,
    // the weights and the jump phase come only from it. Absent before the
    // first snapshot that carries one: the rest pose.
    std::optional<PlayerAnimationSample> animation;
    bool continuous{true}, holding{};
    std::uint64_t lifeGeneration{1};
    bool dead{};
    // Authority-time action inputs from the same snapshot interval and life as
    // the position, in the presentationSeconds base (tick / authority rate).
    // The action pose is a pure function of these and presentationSeconds, so
    // resends, ACKs, duplicate snapshots and timeline holds cannot restart it.
    bool grounded{true};
    float verticalVelocity{};
    double lifeStateSeconds{};
    // Zero IDs mean none: the latest accepted shot and the active reload.
    std::uint64_t shotActionId{}, reloadActionId{};
    double shotSeconds{}, reloadStartSeconds{}, reloadEndSeconds{};
    // The latest damage of this life: its count (zero means none) and time.
    std::uint32_t damageCount{};
    double damageSeconds{};
    // The authoritative stance (protocol-v8 section 4). It selects the lower
    // body's crouch clips only; the model keeps its standing scale.
    bool crouched{};
};

// Product-owned CPU binding. Loading needs neither a renderer nor Campaign/Enemy.
struct PlayerPresentationDefinition final {
    std::shared_ptr<const CharacterPresentationDefinition> character, weapon;
    std::size_t idleClip{}, walkClip{}, jogClip{}, upperBodyRoot{}, weaponNode{};
    std::size_t shootClip{}, reloadClip{}, jumpStartClip{}, jumpLoopClip{}, jumpLandClip{}, deathClip{};
    // The upper body aims by blending these poses (their first frames) by
    // pitch; the idle, shot and reload clips move relative to their first frame.
    std::size_t aimUpClip{}, aimNeutralClip{}, aimDownClip{}, hitClip{};
    // The crouched lower body: idle and moving, blended by the move weight.
    std::size_t crouchIdleClip{}, crouchMoveClip{};
    // Pitch magnitudes at which the up and down poses apply in full: the
    // muzzle elevation of each pose relative to the neutral one.
    double aimUpRadians{}, aimDownRadians{};
    std::vector<bool> upperBodyMask;
    Engine::Math::Vec3 anchor{};
    float scale{}, bodyHeight{}, referenceSpeed{};
    // Measured stance foot speed of each clip at authored playback. They are
    // the two points of the speed blend; walk and jog share one gait phase.
    double walkNativeSpeed{}, jogNativeSpeed{};
    double transitionSeconds{}, maxFrameDeltaSeconds{};
    // Contract presentation spans; the authored clips are time-scaled into
    // them. Reload follows the authoritative interval, death its own clip.
    double shotSeconds{}, jumpStartSeconds{}, jumpLandSeconds{}, hitSeconds{};
    Engine::Model::Transform weaponMount{};
    Engine::Model::Pose weaponReferencePose;
    // The Match's animation rules from the same content (protocol-v9 §4.4).
    PlayerAnimationRules animationRules{};
};

[[nodiscard]] std::shared_ptr<const PlayerPresentationDefinition>
LoadPlayerPresentationDefinition(Engine::Asset::AssetManager& assets, float bodyHeight,
                                 std::string& error);
// The Match's definition (v8-product-hitbox-data D2): the same body, clips and
// calibration with no texture, accessory or weapon; it composes the body pose
// only (SamplePlayerBodyPose), the one the Client presents.
[[nodiscard]] std::shared_ptr<const PlayerPresentationDefinition>
LoadPlayerPoseDefinition(Engine::Asset::AssetManager& assets, float bodyHeight, std::string& error);
[[nodiscard]] std::shared_ptr<const PlayerPresentationDefinition>
LoadPlayerPoseDefinition(const std::filesystem::path& contentRoot, float bodyHeight, std::string& error);
// The animation content both ends pose with (protocol-v9 §4.6,
// v8-product-hitbox-digest-scope (a)): the presentation values that move the
// pose, the transferred keys of the 15 clips in their fixed order and the
// skeleton. Never zero: zero is an invalid digest on the wire. The shapes, the
// weapon and the accessories are outside it.
[[nodiscard]] std::uint64_t AnimationContentDigest(const PlayerPresentationDefinition& definition);
// The digest from a SHA-256 in hex: its first 8 bytes read little-endian, with
// zero (the invalid digest) taken as 1.
[[nodiscard]] std::uint64_t AnimationDigestFromSha256(const std::string& hex);

// Jump presentation follows the sampled grounded state. Only a rising liftoff
// plays Start; leaving the ground while falling goes straight to the Loop.
enum class PlayerJumpPhase : std::uint8_t { Grounded, Start, Airborne, Land };

struct PlayerLocomotionState final {
    bool initialized{}, jogging{}, backward{}, holding{}, phaseReset{};
    std::uint64_t playerId{}, movementEpoch{}, resetCount{};
    Engine::Math::Vec3 previousPosition{};
    double previousPresentationSeconds{};
    // Gait phase in cycles, shared by walk and jog; playbackRate is cycles/s.
    double phaseCycles{}, unwrappedPhaseCycles{}, idleSeconds{};
    double signedDistance{}, totalDistance{}, distanceDelta{}, speed{}, playbackRate{};
    double jogWeight{}, cycleDistance{};
    float moveWeight{};
    std::string resetReason;
    std::uint64_t lifeGeneration{1};
    bool dead{};
    bool grounded{true};
    PlayerJumpPhase jumpPhase{PlayerJumpPhase::Grounded};
    double jumpPhaseSeconds{}; // the phase's clip time zero point (the Match's tick / rate)
    bool crouched{}; // the latest frame's stance
    // The sampled q16 phase this state follows (protocol-v9 §4.4), and the one
    // of the previous frame, whose signed difference unwrapPhaseCycles adds.
    double authorityPhaseQ16{};
};

// The speed blend: jog weight from a sampled planar speed, and the distance one
// gait cycle covers at that weight so the blended feet move at that speed.
[[nodiscard]] double PlayerJogWeight(const PlayerPresentationDefinition& definition, double planarSpeed);
[[nodiscard]] double PlayerCycleDistance(const PlayerPresentationDefinition& definition, double jogWeight);

// The authority half of a presentation frame (player-presentation Design
// Invariants 3): one player's state with its own interval's combat state and
// Match animation, at a presentation tick. The Client adds deltaSeconds,
// continuous and holding; the Match's judged pose takes it as is
// (v8-product-hitbox-frame-sharing (a)).
[[nodiscard]] PlayerPresentationFrame PlayerFrameFromAuthority(const PlayerState& player,
    const std::optional<CombatState>& combat, const std::optional<PlayerAnimationSample>& animation,
    double presentationTick);
// Where the renderer draws a point of the composed body (model space): the
// instance offsets it by -anchor, then the renderer applies
// ComposeEulerXYZ(position, {0, yaw, 0}, scale) (PlayerPresentation::Submit).
[[nodiscard]] Engine::Math::Vec3 PlayerBodyPointToWorld(const PlayerPresentationDefinition& definition,
    const Engine::Math::Vec3& position, float yaw, const Engine::Math::Vec3& modelPoint);

[[nodiscard]] bool AdvancePlayerLocomotion(PlayerLocomotionState& state,
    const PlayerPresentationFrame& frame, const PlayerPresentationDefinition& definition,
    std::string& error);

enum class PlayerUpperAction : std::uint8_t { Hold, Shoot, Reload, Hit };
enum class PlayerLowerAction : std::uint8_t { Locomotion, JumpStart, JumpLoop, JumpLand, Death };

// Clip times already mapped from the contract spans; Death is full body.
struct PlayerActionPose final {
    PlayerUpperAction upper{PlayerUpperAction::Hold};
    PlayerLowerAction lower{PlayerLowerAction::Locomotion};
    double upperClipSeconds{}, lowerClipSeconds{};
    // -1 is the full up pose, 0 neutral, 1 the full down pose.
    float aimWeight{};
    std::uint64_t shotActionId{}, reloadActionId{};
};

// Pure: identical inputs always select the identical action pose.
[[nodiscard]] bool ResolvePlayerActions(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, const PlayerPresentationFrame& frame,
    PlayerActionPose& output, std::string& error);

struct PlayerPresentationPose final {
    Engine::Model::Pose body, weapon;
    std::vector<Engine::Model::Pose> accessories;
};

// The composed body skeleton alone: the judged pose. SamplePlayerPresentationPose
// composes the same body and adds the weapon and accessories.
[[nodiscard]] bool SamplePlayerBodyPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, const PlayerActionPose& actions, Engine::Model::Pose& body,
    std::string& error);
[[nodiscard]] bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, PlayerPresentationPose& output, std::string& error);
[[nodiscard]] bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, const PlayerActionPose& actions,
    PlayerPresentationPose& output, std::string& error);

struct PlayerPresentationObservation final {
    bool ready{}, jogging{}, holding{}, backward{}, phaseReset{};
    std::uint64_t playerId{}, movementEpoch{}, poseRevision{}, resetCount{};
    double phaseCycles{}, unwrappedPhaseCycles{}, signedDistance{}, totalDistance{};
    double distanceDelta{}, cycleDistance{}, jogWeight{}, speed{}, playbackRate{};
    // The Match's sampled phase this frame used: phaseCycles is its fraction.
    double authorityPhaseCycles{};
    float moveWeight{}, scale{};
    Engine::Math::Vec3 footAnchor{};
    Engine::Math::Vec3 weaponWorldPosition{};
    std::size_t bodySubmittedMeshes{}, hairSubmittedMeshes{}, weaponSubmittedMeshes{};
    std::size_t declaredAccessories{};
    std::size_t upperBodyMaskCount{}, preparedInstances{};
    std::string resetReason;
    std::uint64_t lifeGeneration{1};
    bool dead{};
    PlayerJumpPhase jumpPhase{PlayerJumpPhase::Grounded};
    PlayerActionPose actions;
};

// All GPU instances are allocated before Join. Submit reuses these slots and
// samples only the supplied presentation values; it cannot alter gameplay.
class PlayerPresentation final {
public:
    PlayerPresentation();
    ~PlayerPresentation();
    PlayerPresentation(const PlayerPresentation&) = delete;
    PlayerPresentation& operator=(const PlayerPresentation&) = delete;
    [[nodiscard]] bool Initialize(Engine::Render::IRenderDevice& device,
        Engine::Asset::AssetManager& assets, float bodyHeight, std::size_t capacity,
        std::string& error);
    // Caller supplies a world camera and renders the returned queue before Join.
    [[nodiscard]] bool SubmitWarmup(Engine::Render::RenderQueue& queue, std::string& error);
    [[nodiscard]] bool Submit(std::span<const PlayerPresentationFrame> players,
        Engine::Render::RenderQueue& queue, std::string& error);
    void ResetPlayers() noexcept;
    void Reset() noexcept;
    [[nodiscard]] bool Ready() const noexcept;
    [[nodiscard]] std::size_t Capacity() const noexcept;
    [[nodiscard]] std::span<const PlayerPresentationObservation> Observations() const noexcept;
    // AnimationContentDigest of the loaded definition; zero before Initialize.
    [[nodiscard]] std::uint64_t AnimationDigest() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps::pvp
