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
};

// Product-owned CPU binding. Loading needs neither a renderer nor Campaign/Enemy.
struct PlayerPresentationDefinition final {
    std::shared_ptr<const CharacterPresentationDefinition> character, weapon;
    std::size_t idleClip{}, jogClip{}, upperBodyRoot{}, weaponNode{};
    std::vector<bool> upperBodyMask;
    Engine::Model::Vec3 anchor{};
    float scale{}, bodyHeight{}, referenceSpeed{};
    // Calibrated full-clip travel / (referenceSpeed * authored clip seconds).
    double strideScale{};
    double transitionSeconds{}, maxFrameDeltaSeconds{};
    Engine::Model::Transform weaponMount{};
    Engine::Model::Pose weaponReferencePose;
};

[[nodiscard]] std::shared_ptr<const PlayerPresentationDefinition>
LoadPlayerPresentationDefinition(Engine::Asset::AssetManager& assets, float bodyHeight,
                                 std::string& error);

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
};

[[nodiscard]] bool AdvancePlayerLocomotion(PlayerLocomotionState& state,
    const PlayerPresentationFrame& frame, const PlayerPresentationDefinition& definition,
    std::string& error);

struct PlayerPresentationPose final {
    Engine::Model::Pose body, weapon;
    std::vector<Engine::Model::Pose> accessories;
};

[[nodiscard]] bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, PlayerPresentationPose& output, std::string& error);

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
