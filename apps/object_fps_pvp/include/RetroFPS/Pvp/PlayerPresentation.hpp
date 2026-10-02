#pragma once

#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "render/RenderTypes.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Engine::Asset { class AssetManager; }
namespace Engine::Render { class IRenderDevice; class RenderQueue; }

namespace fps::pvp {

// Already sampled presentation values, never movement commands or authority state.
struct PlayerPresentationFrame final {
    std::uint64_t playerId{}, movementEpoch{};
    Engine::Render::Float3 position{};
    float yaw{};
    double presentationSeconds{}, deltaSeconds{};
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
};

// Product-owned CPU binding. Loading needs neither a renderer nor Campaign/Enemy.
struct PlayerPresentationDefinition final {
    std::shared_ptr<const CharacterPresentationDefinition> character, weapon;
    std::size_t idleClip{}, jogClip{}, upperBodyRoot{}, weaponNode{};
    std::size_t shootClip{}, reloadClip{}, jumpStartClip{}, jumpLoopClip{}, jumpLandClip{}, deathClip{};
    std::vector<bool> upperBodyMask;
    Engine::Model::Vec3 anchor{};
    float scale{}, bodyHeight{}, referenceSpeed{};
    // Calibrated full-clip travel / (referenceSpeed * authored clip seconds).
    double strideScale{};
    double transitionSeconds{}, maxFrameDeltaSeconds{};
    // Contract presentation spans; the authored clips are time-scaled into
    // them. Reload follows the authoritative interval, death its own clip.
    double shotSeconds{}, jumpStartSeconds{}, jumpLandSeconds{};
    Engine::Model::Transform weaponMount{};
    Engine::Model::Pose weaponReferencePose;
};

[[nodiscard]] std::shared_ptr<const PlayerPresentationDefinition>
LoadPlayerPresentationDefinition(Engine::Asset::AssetManager& assets, float bodyHeight,
                                 std::string& error);

// Jump presentation follows the sampled grounded state. Only a rising liftoff
// plays Start; leaving the ground while falling goes straight to the Loop.
enum class PlayerJumpPhase : std::uint8_t { Grounded, Start, Airborne, Land };

struct PlayerLocomotionState final {
    bool initialized{}, jogging{}, backward{}, holding{}, phaseReset{};
    std::uint64_t playerId{}, movementEpoch{}, resetCount{};
    Engine::Render::Float3 previousPosition{};
    double previousPresentationSeconds{};
    double phaseSeconds{}, unwrappedPhaseSeconds{}, idleSeconds{};
    double signedDistance{}, totalDistance{}, distanceDelta{}, speed{}, playbackRate{};
    float moveWeight{};
    std::string resetReason;
    std::uint64_t lifeGeneration{1};
    bool dead{};
    bool grounded{true};
    PlayerJumpPhase jumpPhase{PlayerJumpPhase::Grounded};
    double jumpPhaseSeconds{}; // presentationSeconds at which jumpPhase began
};

[[nodiscard]] bool AdvancePlayerLocomotion(PlayerLocomotionState& state,
    const PlayerPresentationFrame& frame, const PlayerPresentationDefinition& definition,
    std::string& error);

enum class PlayerUpperAction : std::uint8_t { Hold, Shoot, Reload };
enum class PlayerLowerAction : std::uint8_t { Locomotion, JumpStart, JumpLoop, JumpLand, Death };

// Clip times already mapped from the contract spans; Death is full body.
struct PlayerActionPose final {
    PlayerUpperAction upper{PlayerUpperAction::Hold};
    PlayerLowerAction lower{PlayerLowerAction::Locomotion};
    double upperClipSeconds{}, lowerClipSeconds{};
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

[[nodiscard]] bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, PlayerPresentationPose& output, std::string& error);
[[nodiscard]] bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, const PlayerActionPose& actions,
    PlayerPresentationPose& output, std::string& error);

struct PlayerPresentationObservation final {
    bool ready{}, jogging{}, holding{}, backward{}, phaseReset{};
    std::uint64_t playerId{}, movementEpoch{}, poseRevision{}, resetCount{};
    double phaseSeconds{}, unwrappedPhaseSeconds{}, signedDistance{}, totalDistance{};
    double distanceDelta{}, strideDistance{}, jogDurationSeconds{}, speed{}, playbackRate{};
    float moveWeight{}, scale{};
    Engine::Model::Vec3 footAnchor{};
    Engine::Render::Float3 weaponWorldPosition{};
    std::size_t bodySubmittedMeshes{}, hairSubmittedMeshes{}, weaponSubmittedMeshes{};
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
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps::pvp
