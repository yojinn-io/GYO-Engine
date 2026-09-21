#pragma once

#include <cstddef>
#include <memory>

#include "ui/UiTypes.hpp"

namespace Engine::Asset {
class AssetManager;
}

namespace Engine::Render {
class IRenderDevice;
class RenderQueue;
}

namespace Engine::Text {
class ITextRasterizer;
}

namespace Engine::Ui {

struct UiRendererOptions final {
    std::size_t maximumCachedTextRuns{256};
};

class UiRenderer final {
public:
    UiRenderer();
    ~UiRenderer();
    UiRenderer(UiRenderer&&) noexcept;
    UiRenderer& operator=(UiRenderer&&) noexcept;
    UiRenderer(const UiRenderer&) = delete;
    UiRenderer& operator=(const UiRenderer&) = delete;

    [[nodiscard]] UiResult<void> Initialize(
        Render::IRenderDevice& renderDevice,
        Text::ITextRasterizer& textRasterizer,
        Asset::AssetManager& assets,
        UiRendererOptions options = {});
    void Reset() noexcept;

    // UI submissions are always Overlay. The document has no render-layer knob.
    [[nodiscard]] UiResult<void> Submit(
        const UiDrawList& drawList,
        Render::RenderQueue& queue);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Ui
