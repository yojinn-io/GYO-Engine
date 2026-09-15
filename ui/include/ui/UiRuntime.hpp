#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ui/UiDocument.hpp"

namespace Engine::Ui {

struct UiEvaluatedElement final {
    std::string id;
    UiElementType type{UiElementType::Container};
    UiRect boundsPixels{};
    UiRect clipPixels{};
    std::size_t drawOrder{};
    bool interactive{};
    std::optional<std::size_t> listItemIndex;
};

// Returns the topmost evaluated element containing point. The caller owns the
// span and therefore the returned pointer's lifetime.
[[nodiscard]] const UiEvaluatedElement* HitTestUiLayout(
    std::span<const UiEvaluatedElement> layout,
    UiFloat2 pointPixels,
    bool interactiveOnly = false) noexcept;

class UiRuntime final {
public:
    UiRuntime();
    ~UiRuntime();
    UiRuntime(UiRuntime&&) noexcept;
    UiRuntime& operator=(UiRuntime&&) noexcept;
    UiRuntime(const UiRuntime&) = delete;
    UiRuntime& operator=(const UiRuntime&) = delete;

    [[nodiscard]] UiResult<void> Initialize(
        std::shared_ptr<const UiDocument> document);
    void Reset() noexcept;

    [[nodiscard]] UiResult<void> ActivateCanvas(std::string_view canvasId);

    [[nodiscard]] UiResult<std::vector<UiActionEvent>> Update(
        const UiInputFrame& input,
        const UiBindingTable& bindings,
        UiViewport viewport);

    [[nodiscard]] UiResult<UiDrawList> Compose(
        const UiBindingTable& bindings,
        UiViewport viewport) const;

    [[nodiscard]] UiResult<UiDrawList> ComposePreview(
        UiViewport viewport) const;

    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluateLayout(
        const UiBindingTable& bindings,
        UiViewport viewport) const;

    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluatePreviewLayout(
        UiViewport viewport) const;

    [[nodiscard]] const UiInteractionState& InteractionState() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Ui
