#pragma once

#include "RetroFPS/Gameplay/Weapon/WeaponState.hpp"
#include "engine/asset/AssetId.hpp"
#include "render/RenderTypes.hpp"

#include <memory>
#include <string>

namespace Engine::Asset { class AssetManager; }
namespace Engine::Render { class IRenderDevice; class RenderQueue; }

namespace fps {

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
        const WeaponPresentationSnapshot& snapshot,
        Engine::Render::RenderQueue& queue,
        std::string& error);
    // Current sampled pose, in the dedicated viewmodel camera's coordinates.
    [[nodiscard]] Engine::Render::Float3 GetMuzzleViewCameraPosition() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps
