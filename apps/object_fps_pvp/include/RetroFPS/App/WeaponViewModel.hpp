#pragma once

#include "engine/asset/AssetId.hpp"
#include "render/RenderTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace Engine::Asset { class AssetManager; }
namespace Engine::Render { class IRenderDevice; class RenderQueue; }

namespace fps {

struct WeaponPresentationSnapshot;

// Client presentation does not require Campaign weapon/ammunition state.
enum class WeaponViewModelAction : std::uint8_t { Idle, Draw, Shoot };

struct WeaponViewModelFrame final {
    WeaponViewModelAction action{WeaponViewModelAction::Idle};
    float elapsedSeconds{};
    float durationSeconds{};
    // Rotation of the weapon only, never the world camera or submitted aim.
    float recoilRadians{};
};

struct WeaponViewModelObservation final {
    bool ready{};
    std::size_t meshCount{}, materialCount{}, submittedMeshCount{};
    std::uint64_t submissionCount{}, poseRevision{};
    std::string animationName;
    double sampledTimeSeconds{};
    float recoilRadians{};
};

// Game-owned model/clip/material binding and camera-relative framing. The
// immutable model and animation evaluator are reusable GYO mechanisms.
class WeaponViewModel final {
public:
    WeaponViewModel();
    ~WeaponViewModel();
    WeaponViewModel(const WeaponViewModel&) = delete;
    WeaponViewModel& operator=(const WeaponViewModel&) = delete;

    [[nodiscard]] bool Initialize(
        Engine::Render::IRenderDevice& device,
        Engine::Asset::AssetManager& assets,
        const Engine::Asset::AssetId& presentationId,
        std::string& error);
    [[nodiscard]] bool Submit(
        const WeaponViewModelFrame& frame,
        Engine::Render::RenderQueue& queue,
        std::string& error);
    [[nodiscard]] bool Submit(
        const WeaponPresentationSnapshot& snapshot,
        Engine::Render::RenderQueue& queue,
        std::string& error);
    // Current sampled pose, in the dedicated viewmodel camera's coordinates.
    [[nodiscard]] Engine::Render::Float3 GetMuzzleViewCameraPosition() const noexcept;
    [[nodiscard]] float GetActionDurationSeconds(WeaponViewModelAction action) const noexcept;
    // Observes successful pose/GPU preparation and queue submission; reading
    // this cannot advance or restart an animation.
    [[nodiscard]] WeaponViewModelObservation GetObservation() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps
