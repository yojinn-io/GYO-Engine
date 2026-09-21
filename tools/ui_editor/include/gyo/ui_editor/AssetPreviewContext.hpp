#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct SDL_Renderer;
struct SDL_Texture;

namespace Gyo::Tools::UiEditor {

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
    [[nodiscard]] bool Mount(
        const std::filesystem::path& catalogPath,
        const std::filesystem::path& assetRoot,
        std::string& error);
    void Unmount() noexcept;

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
