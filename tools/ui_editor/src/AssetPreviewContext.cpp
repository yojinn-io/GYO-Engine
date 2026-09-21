#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"

#include <SDL3/SDL.h>

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loaders/FontAsset.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "text/backend/sdl_ttf/SdlTtfTextRasterizer.hpp"

#include <cmath>
#include <cassert>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Gyo::Tools::UiEditor {
namespace {

template <typename Error>
[[nodiscard]] std::string Describe(const Error& error) {
    std::string result = error.message;
    if constexpr (requires { error.detail; }) {
        if (!error.detail.empty()) result += ": " + error.detail;
    }
    return result;
}

[[nodiscard]] SDL_Texture* Upload(
    SDL_Renderer& renderer,
    const std::uint32_t width,
    const std::uint32_t height,
    const void* pixels,
    const std::uint32_t pitch,
    std::string& error) {
    SDL_Texture* texture = SDL_CreateTexture(
        &renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        static_cast<int>(width),
        static_cast<int>(height));
    if (texture == nullptr) {
        error = std::string{"SDL texture creation failed: "} + SDL_GetError();
        return nullptr;
    }
    if (!SDL_UpdateTexture(texture, nullptr, pixels, static_cast<int>(pitch))) {
        error = std::string{"SDL texture upload failed: "} + SDL_GetError();
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    static_cast<void>(SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND));
    static_cast<void>(SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR));
    return texture;
}

} // namespace

struct AssetPreviewContext::Impl final {
    struct TextureEntry final {
        Engine::Asset::AssetHandle handle;
        SDL_Texture* texture{};
    };

    struct FontEntry final {
        Engine::Asset::AssetHandle handle;
        std::shared_ptr<const Engine::Asset::Loaders::FontAsset> asset;
    };

    struct TextEntry final {
        SDL_Texture* texture{};
        float width{};
        float height{};
        std::uint64_t lastUse{};
    };

    Engine::Asset::AssetCatalog catalog;
    Engine::Asset::Loading::LoaderRegistry loaders;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy cache{{
        Engine::Asset::Core::AssetCachePolicy::Mode::KeepWhileReferenced,
        0,
        true,
        0,
        0,
    }};
    Engine::Asset::AssetManager assets{
        catalog, pipeline, storage, lifetime, cache};
    std::unique_ptr<Engine::Text::ITextRasterizer> rasterizer;
    SDL_Renderer* renderer{};
    bool initialized{};
    bool mounted{};
    bool frameActive{};
    std::unordered_map<std::string, TextureEntry> textures;
    std::unordered_map<std::string, FontEntry> fonts;
    std::unordered_map<std::string, TextEntry> text;
    std::uint64_t textClock{};

    void Clear() noexcept {
        std::vector<Engine::Asset::AssetHandle> handles;
        handles.reserve(textures.size() + fonts.size());
        for (auto& [ignored, entry] : text) {
            static_cast<void>(ignored);
            if (entry.texture != nullptr) SDL_DestroyTexture(entry.texture);
        }
        text.clear();
        textClock = 0U;
        for (auto& [ignored, entry] : textures) {
            static_cast<void>(ignored);
            if (entry.texture != nullptr) SDL_DestroyTexture(entry.texture);
            handles.push_back(entry.handle);
        }
        textures.clear();
        for (auto& [ignored, entry] : fonts) {
            static_cast<void>(ignored);
            handles.push_back(entry.handle);
        }
        fonts.clear();
        for (const Engine::Asset::AssetHandle& handle : handles) {
            assets.Release(handle);
        }
        // Storage::Clear retires generations, so a remount cannot make an old
        // handle valid (ABA) or reuse the prior catalog's resolved path.
        storage.Clear();
        lifetime.Clear();
        catalog.Clear();
        mounted = false;
    }
};

AssetPreviewContext::AssetPreviewContext() : impl_(std::make_unique<Impl>()) {}
AssetPreviewContext::~AssetPreviewContext() {
    if (impl_) impl_->Clear();
}
AssetPreviewContext::AssetPreviewContext(AssetPreviewContext&&) noexcept = default;
AssetPreviewContext& AssetPreviewContext::operator=(AssetPreviewContext&&) noexcept = default;

bool AssetPreviewContext::Initialize(SDL_Renderer& renderer, std::string& error) {
    auto rasterizer = Engine::Text::Backend::SdlTtf::SdlTtfTextRasterizer::Create();
    if (!rasterizer) { error = Describe(rasterizer.error()); return false; }
    return Initialize(renderer, std::move(rasterizer).value(), error);
}

bool AssetPreviewContext::Initialize(SDL_Renderer& renderer,
    std::unique_ptr<Engine::Text::ITextRasterizer> rasterizer, std::string& error) {
    error.clear();
    if (impl_->initialized || !rasterizer) {
        error = "asset preview requires a rasterizer and may be initialized only once";
        return false;
    }
    auto font = impl_->loaders.Register(
        std::make_unique<Engine::Asset::Loaders::FontLoader>());
    if (!font) {
        error = Describe(font.error());
        return false;
    }
    auto image = impl_->loaders.Register(std::make_unique<
        Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader>());
    if (!image) {
        error = Describe(image.error());
        return false;
    }
    impl_->rasterizer = std::move(rasterizer);
    impl_->renderer = &renderer;
    impl_->initialized = true;
    return true;
}

bool AssetPreviewContext::Mount(
    const ReadOnlyAssetCatalog& catalog, std::string& error) {
    error.clear();
    if (!impl_->initialized || impl_->renderer == nullptr) {
        error = "asset preview context is not initialized";
        return false;
    }
    if (impl_->frameActive || !catalog.IsMounted()) {
        error = "preview mount requires a parsed catalog outside an active frame";
        return false;
    }
    // Copy the validated snapshot before retiring the old state. No file is
    // parsed twice, and allocation failure cannot clear a working mount.
    auto candidate = catalog.ParsedCatalog();
    impl_->Clear();
    impl_->catalog = std::move(candidate);
    impl_->mounted = true;
    return true;
}

bool AssetPreviewContext::Unmount() noexcept {
    if (impl_->frameActive) return false;
    impl_->Clear();
    return true;
}
void AssetPreviewContext::BeginFrame() {
    assert(!impl_->frameActive);
    if (impl_->frameActive) return;
    constexpr std::size_t maximumTextEntries = 256U;
    while (impl_->text.size() > maximumTextEntries) {
        const auto oldest = std::min_element(impl_->text.begin(), impl_->text.end(),
            [](const auto& left, const auto& right) { return left.second.lastUse < right.second.lastUse; });
        SDL_DestroyTexture(oldest->second.texture);
        impl_->text.erase(oldest);
    }
    impl_->frameActive = true;
}
void AssetPreviewContext::EndFrame() noexcept { impl_->frameActive = false; }
bool AssetPreviewContext::IsMounted() const noexcept { return impl_->mounted; }

SDL_Texture* AssetPreviewContext::Texture(
    const std::string_view assetId,
    std::string& error) {
    error.clear();
    if (!impl_->frameActive || !impl_->mounted || impl_->renderer == nullptr) return nullptr;
    const std::string key{assetId};
    if (const auto found = impl_->textures.find(key);
        found != impl_->textures.end()) {
        return found->second.texture;
    }
    auto loaded = impl_->assets.Load(
        Engine::Asset::AssetId{assetId},
        Engine::Asset::AssetRequest::WithTypeHint(
            Engine::Asset::AssetType::Texture()));
    if (!loaded) {
        error = Describe(loaded.error());
        return nullptr;
    }
    const Engine::Asset::AssetHandle handle = loaded.value();
    auto asset = impl_->assets.GetSharedConst<
        Engine::Asset::Loaders::TextureAsset>(handle);
    if (!asset || asset->rgba.size() !=
            static_cast<std::size_t>(asset->width) * asset->height * 4U) {
        impl_->assets.Release(handle);
        error = "texture asset has an invalid RGBA payload";
        return nullptr;
    }
    SDL_Texture* texture = Upload(
        *impl_->renderer,
        asset->width,
        asset->height,
        asset->rgba.data(),
        asset->width * 4U,
        error);
    if (texture == nullptr) {
        impl_->assets.Release(handle);
        return nullptr;
    }
    impl_->textures.emplace(key, Impl::TextureEntry{handle, texture});
    return texture;
}

TextTextureView AssetPreviewContext::Text(
    const std::string_view fontAssetId,
    const std::string_view utf8,
    const float pointSize,
    std::string& error) {
    error.clear();
    if (!impl_->frameActive || !impl_->mounted || impl_->renderer == nullptr ||
        impl_->rasterizer == nullptr || utf8.empty() ||
        !std::isfinite(pointSize) || pointSize <= 0.0F) {
        return {};
    }
    const std::string fontKey{fontAssetId};
    auto font = impl_->fonts.find(fontKey);
    if (font == impl_->fonts.end()) {
        auto loaded = impl_->assets.Load(
            Engine::Asset::AssetId{fontAssetId},
            Engine::Asset::AssetRequest::WithTypeHint(
                Engine::Asset::AssetType::Font()));
        if (!loaded) {
            error = Describe(loaded.error());
            return {};
        }
        const Engine::Asset::AssetHandle handle = loaded.value();
        auto asset = impl_->assets.GetSharedConst<
            Engine::Asset::Loaders::FontAsset>(handle);
        if (!asset) {
            impl_->assets.Release(handle);
            error = "font asset has no encoded data";
            return {};
        }
        font = impl_->fonts.emplace(
            fontKey, Impl::FontEntry{handle, std::move(asset)}).first;
    }

    const std::string textKey = fontKey + "\n" +
        std::to_string(pointSize) + "\n" + std::string{utf8};
    if (const auto found = impl_->text.find(textKey);
        found != impl_->text.end()) {
        found->second.lastUse = ++impl_->textClock;
        return {found->second.texture, found->second.width, found->second.height};
    }
    auto bitmap = impl_->rasterizer->Rasterize(
        font->second.asset->bytes,
        {utf8, pointSize});
    if (!bitmap) {
        error = Describe(bitmap.error());
        return {};
    }
    const Engine::Text::TextBitmap& pixels = bitmap.value();
    if (pixels.width == 0U || pixels.height == 0U || pixels.rgba8.empty()) {
        return {};
    }
    SDL_Texture* texture = Upload(
        *impl_->renderer,
        pixels.width,
        pixels.height,
        pixels.rgba8.data(),
        pixels.rowPitch,
        error);
    if (texture == nullptr) return {};
    const Impl::TextEntry entry{
        texture,
        static_cast<float>(pixels.width),
        static_cast<float>(pixels.height),
        ++impl_->textClock,
    };
    impl_->text.emplace(textKey, entry);
    return {entry.texture, entry.width, entry.height};
}

} // namespace Gyo::Tools::UiEditor
