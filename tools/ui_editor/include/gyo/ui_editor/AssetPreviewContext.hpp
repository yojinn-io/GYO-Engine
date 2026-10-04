#pragma once

#include "gyo/ui_editor/EditorError.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct SDL_Renderer;
struct SDL_Texture;
namespace Engine::Text { class ITextRasterizer; }

namespace Gyo::Tools::UiEditor {
class ReadOnlyAssetCatalog;

struct TextTextureView final {
    SDL_Texture* texture{};
    float width{};
    float height{};
};

// Owns an authoring-only, read-only GYO asset stack. The context can decode
// catalog assets and upload transient SDL textures, but exposes no catalog or
// file mutation operation.
class AssetPreviewContext final {
public:
    AssetPreviewContext();
    ~AssetPreviewContext();
    AssetPreviewContext(AssetPreviewContext&&) noexcept;
    AssetPreviewContext& operator=(AssetPreviewContext&&) noexcept;
    AssetPreviewContext(const AssetPreviewContext&) = delete;
    AssetPreviewContext& operator=(const AssetPreviewContext&) = delete;

    // Creates the SDL_ttf rasterizer; failing to do so is a Runtime Error.
    [[nodiscard]] Result<void, PreviewError> Initialize(SDL_Renderer& renderer);
    // Initializing twice or with a null rasterizer is API misuse (GYO_ASSERT).
    void Initialize(SDL_Renderer& renderer,
        std::unique_ptr<Engine::Text::ITextRasterizer> rasterizer);
    // Requires an initialized context, a mounted catalog and no active frame.
    void Mount(const ReadOnlyAssetCatalog& catalog);
    // Requires no active frame.
    void Unmount();

    // Textures returned within a frame survive until EndFrame. Call EndFrame
    // only after presentation, or after discarding an unsubmitted ImGui frame.
    // Mount/unmount during a frame is API misuse. LRU trim occurs at
    // BeginFrame, so even a frame with more than 256 text runs remains valid.
    void BeginFrame();
    void EndFrame() noexcept;

    [[nodiscard]] bool IsMounted() const noexcept;
    // Only within a frame. With nothing mounted the result is null or empty,
    // as it is for empty text or a non-positive point size.
    [[nodiscard]] Result<SDL_Texture*, PreviewError> Texture(std::string_view assetId);
    [[nodiscard]] Result<TextTextureView, PreviewError> Text(
        std::string_view fontAssetId,
        std::string_view utf8,
        float pointSize);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Gyo::Tools::UiEditor
