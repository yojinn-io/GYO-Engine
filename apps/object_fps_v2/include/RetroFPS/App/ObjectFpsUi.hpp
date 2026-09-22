#pragma once

#include "RetroFPS/Game/GameSession.hpp"

#include "ui/UiDocument.hpp"
#include "ui/UiRuntime.hpp"

#include <memory>
#include <string>
#include <vector>

namespace fps {

// App-level display policy. It deliberately lives outside the gameplay
// snapshot and is recreated with every process, while surviving screen and
// campaign transitions during one run.
struct ObjectFpsDisplaySettings final {
    static constexpr float kMinimumExposureEv = -2.0F;
    static constexpr float kMaximumExposureEv = 2.0F;
    static constexpr float kExposureStepEv = 0.1F;
    static constexpr float kMinimumGammaAdjustment = 0.75F;
    static constexpr float kMaximumGammaAdjustment = 1.50F;
    static constexpr float kGammaStep = 0.05F;

    float exposureEv{};
    float gammaAdjustment{1.0F};
};

// Object_FPS owns the contents, bindings and consequences of its UI. GYO owns
// document validation, layout, focus, hit testing and typed action emission.
class ObjectFpsUi final {
public:
    ObjectFpsUi();
    ~ObjectFpsUi();
    ObjectFpsUi(ObjectFpsUi&&) noexcept;
    ObjectFpsUi& operator=(ObjectFpsUi&&) noexcept;
    ObjectFpsUi(const ObjectFpsUi&) = delete;
    ObjectFpsUi& operator=(const ObjectFpsUi&) = delete;

    [[nodiscard]] bool Initialize(
        std::shared_ptr<const Engine::Ui::UiDocument> document,
        std::string& error);

    // Converts opaque typed UI actions into Object_FPS semantic commands and
    // applies app-local display values in the same frame.
    [[nodiscard]] bool Update(
        const GameSessionSnapshot& snapshot,
        const Engine::Ui::UiInputFrame& input,
        Engine::Ui::UiViewport viewport,
        ObjectFpsDisplaySettings& displaySettings,
        std::vector<GameSessionCommand>& commands,
        std::string& error);

    // JSON screens and the C++ Playing HUD both leave this boundary as the
    // same ordered GYO draw list.
    [[nodiscard]] bool Compose(
        const GameSessionSnapshot& snapshot,
        const ObjectFpsDisplaySettings& displaySettings,
        Engine::Ui::UiViewport viewport,
        Engine::Ui::UiDrawList& drawList,
        std::string& error);

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] const Engine::Ui::UiInteractionState& InteractionState()
        const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps
