#pragma once

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

    [[nodiscard]] bool Initialize(SDL_Renderer& renderer, std::string& error);
    [[nodiscard]] bool Initialize(SDL_Renderer& renderer,
        std::unique_ptr<Engine::Text::ITextRasterizer> rasterizer, std::string& error);
    [[nodiscard]] bool Mount(
        const ReadOnlyAssetCatalog& catalog, std::string& error);
    [[nodiscard]] bool Unmount() noexcept;

    // Textures returned within a frame survive until EndFrame. Call EndFrame
    // only after presentation, or after discarding an unsubmitted ImGui frame.
    // Mount/unmount are rejected while a frame is active. LRU trim occurs at
    // BeginFrame, so even a frame with more than 256 text runs remains valid.
    void BeginFrame();
    void EndFrame() noexcept;

    [[nodiscard]] bool IsMounted() const noexcept;
    [[nodiscard]] SDL_Texture* Texture(
        std::string_view assetId,
        std::string& error);
    [[nodiscard]] TextTextureView Text(
        std::string_view fontAssetId,
        std::string_view utf8,
        float pointSize,
        std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Gyo::Tools::UiEditor
