#pragma once

#include "RetroFPS/Sim/PlayerPose.hpp"
#include "engine/render/RenderTypes.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Engine::Asset { class AssetManager; }
namespace Engine::Render { class IRenderDevice; class RenderQueue; }

namespace fps::pvp {

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
