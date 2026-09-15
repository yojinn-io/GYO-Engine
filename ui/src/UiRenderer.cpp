#include "ui/UiRenderer.hpp"

#include "engine/asset/AssetHandle.hpp"
#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/loaders/FontAsset.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "render/IRenderDevice.hpp"
#include "render/RenderQueue.hpp"
#include "render/RenderTypes.hpp"
#include "text/ITextRasterizer.hpp"
#include "text/TextTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace Engine::Ui {
namespace {

[[nodiscard]] UiError RendererError(
    UiErrorCode code,
    std::string message) {
    return {code, std::move(message), {}, {}};
}

[[nodiscard]] std::string AssetName(const Asset::AssetId& id) {
    return id.debugName.empty() ? std::to_string(id.value) : id.debugName;
}

[[nodiscard]] bool IsValidBitmap(const Text::TextBitmap& bitmap) noexcept {
    const std::uint64_t tightPitch = static_cast<std::uint64_t>(bitmap.width) * 4U;
    if (bitmap.width == 0 || bitmap.height == 0 || bitmap.rowPitch < tightPitch) {
        return false;
    }
    const std::uint64_t rowsBeforeLast = bitmap.height - 1U;
    if (rowsBeforeLast != 0 && bitmap.rowPitch >
        ((std::numeric_limits<std::uint64_t>::max)() - tightPitch) / rowsBeforeLast) {
        return false;
    }
    const std::uint64_t required = rowsBeforeLast * bitmap.rowPitch + tightPitch;
    return required <= bitmap.rgba8.size();
}

[[nodiscard]] Render::Color ConvertColor(UiColor color) noexcept {
    return {color.red, color.green, color.blue, color.alpha};
}

[[nodiscard]] Render::Rect ConvertRect(UiRect rect) noexcept {
    return {rect.x, rect.y, rect.width, rect.height};
}

[[nodiscard]] bool ClipSprite(
    Render::SpriteSubmission& sprite,
    UiRect clip) noexcept {
    const Render::Rect original = sprite.destinationPixels;
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.width) || !std::isfinite(clip.height) ||
        original.width <= 0.0F || original.height <= 0.0F ||
        clip.width <= 0.0F || clip.height <= 0.0F) {
        return false;
    }
    const float x = std::max(original.x, clip.x);
    const float y = std::max(original.y, clip.y);
    const float right = std::min(
        original.x + original.width,
        clip.x + clip.width);
    const float bottom = std::min(
        original.y + original.height,
        clip.y + clip.height);
    if (right <= x || bottom <= y) return false;

    const float leftFraction = (x - original.x) / original.width;
    const float topFraction = (y - original.y) / original.height;
    const float widthFraction = (right - x) / original.width;
    const float heightFraction = (bottom - y) / original.height;
    sprite.sourceUv = {
        sprite.sourceUv.x + sprite.sourceUv.width * leftFraction,
        sprite.sourceUv.y + sprite.sourceUv.height * topFraction,
        sprite.sourceUv.width * widthFraction,
        sprite.sourceUv.height * heightFraction,
    };
    sprite.destinationPixels = {x, y, right - x, bottom - y};
    return true;
}

[[nodiscard]] std::size_t HashCombine(
    std::size_t seed,
    std::size_t value) noexcept {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

} // namespace

struct UiRenderer::Impl final {
    struct FontResource final {
        Asset::AssetHandle asset;
        std::shared_ptr<const Asset::Loaders::FontAsset> font;
    };

    struct ImageResource final {
        Asset::AssetHandle asset;
        Render::TextureHandle gpu;
    };

    struct TextKey final {
        std::string fontAssetId;
        std::uint32_t fontGeneration{};
        std::string utf8;
        float pointSize{};

        friend bool operator==(const TextKey&, const TextKey&) noexcept = default;
    };

    struct TextKeyHash final {
        [[nodiscard]] std::size_t operator()(const TextKey& key) const noexcept {
            std::size_t result = std::hash<std::string>{}(key.fontAssetId);
            result = HashCombine(result, std::hash<std::uint32_t>{}(key.fontGeneration));
            result = HashCombine(result, std::hash<std::string>{}(key.utf8));
            result = HashCombine(result, std::hash<float>{}(key.pointSize));
            return result;
        }
    };

    struct TextResource final {
        Render::TextureHandle gpu;
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint64_t lastUse{};
    };

    Render::IRenderDevice* renderDevice{};
    Text::ITextRasterizer* textRasterizer{};
    Asset::AssetManager* assets{};
    UiRendererOptions options{};
    std::unordered_map<std::string, FontResource> fonts;
    std::unordered_map<std::string, ImageResource> images;
    std::unordered_map<TextKey, TextResource, TextKeyHash> texts;
    std::uint64_t useSerial{};

    ~Impl() { Reset(); }

    void Reset() noexcept {
        if (renderDevice != nullptr) {
            for (const auto& [key, text] : texts) {
                static_cast<void>(key);
                if (text.gpu.IsValid()) {
                    static_cast<void>(renderDevice->ReleaseTexture(text.gpu));
                }
            }
            for (const auto& [key, image] : images) {
                static_cast<void>(key);
                if (image.gpu.IsValid()) {
                    static_cast<void>(renderDevice->ReleaseTexture(image.gpu));
                }
            }
        }
        if (assets != nullptr) {
            for (const auto& [key, image] : images) {
                static_cast<void>(key);
                if (image.asset.valid()) assets->Release(image.asset);
            }
            for (const auto& [key, font] : fonts) {
                static_cast<void>(key);
                if (font.asset.valid()) assets->Release(font.asset);
            }
        }
        texts.clear();
        images.clear();
        fonts.clear();
        renderDevice = nullptr;
        textRasterizer = nullptr;
        assets = nullptr;
        options = {};
        useSerial = 0;
    }

    void TrimTextCache() noexcept {
        // Runs before a new frame is submitted, when textures from the previous
        // queue are no longer in flight. A single frame's working set may exceed
        // the retention limit; it is trimmed safely at the next Submit().
        while (texts.size() > options.maximumCachedTextRuns) {
            const auto oldest = std::min_element(
                texts.begin(),
                texts.end(),
                [](const auto& left, const auto& right) {
                    return left.second.lastUse < right.second.lastUse;
                });
            if (oldest == texts.end()) break;
            if (oldest->second.gpu.IsValid()) {
                static_cast<void>(renderDevice->ReleaseTexture(oldest->second.gpu));
            }
            texts.erase(oldest);
        }
    }

    [[nodiscard]] UiResult<FontResource*> ResolveFont(std::string_view assetId) {
        const auto cached = fonts.find(std::string(assetId));
        if (cached != fonts.end()) {
            return UiResult<FontResource*>::Ok(&cached->second);
        }
        const Asset::AssetId id = Asset::AssetId::FromString(assetId);
        auto loaded = assets->Load(
            id,
            Asset::AssetRequest::WithTypeHint(Asset::AssetType::Font()));
        if (!loaded) {
            return UiResult<FontResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "failed to load UI font asset '" + AssetName(id) + "': " +
                    loaded.error().message));
        }
        const Asset::AssetHandle handle = loaded.value();
        auto font = assets->GetSharedConst<Asset::Loaders::FontAsset>(handle);
        if (!font || font->bytes.empty()) {
            assets->Release(handle);
            return UiResult<FontResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "UI font asset '" + AssetName(id) + "' has no encoded payload"));
        }
        auto [inserted, wasInserted] = fonts.emplace(
            std::string(assetId), FontResource{handle, std::move(font)});
        if (!wasInserted) assets->Release(handle);
        return UiResult<FontResource*>::Ok(&inserted->second);
    }

    [[nodiscard]] UiResult<ImageResource*> ResolveImage(std::string_view assetId) {
        const auto cached = images.find(std::string(assetId));
        if (cached != images.end()) {
            return UiResult<ImageResource*>::Ok(&cached->second);
        }
        const Asset::AssetId id = Asset::AssetId::FromString(assetId);
        auto loaded = assets->Load(
            id,
            Asset::AssetRequest::WithTypeHint(Asset::AssetType::Texture()));
        if (!loaded) {
            return UiResult<ImageResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "failed to load UI image asset '" + AssetName(id) + "': " +
                    loaded.error().message));
        }
        const Asset::AssetHandle handle = loaded.value();
        const auto texture = assets->GetSharedConst<Asset::Loaders::TextureAsset>(handle);
        const std::uint64_t expected = static_cast<std::uint64_t>(texture ? texture->width : 0U) *
                                       static_cast<std::uint64_t>(texture ? texture->height : 0U) * 4U;
        if (!texture || texture->width == 0 || texture->height == 0 ||
            expected != texture->rgba.size()) {
            assets->Release(handle);
            return UiResult<ImageResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "UI image asset '" + AssetName(id) + "' has no valid RGBA8 payload"));
        }
        const Render::ImageView image{
            texture->width,
            texture->height,
            texture->width * 4U,
            std::as_bytes(std::span<const std::uint8_t>{texture->rgba}),
            Render::TextureColorSpace::SRgb,
        };
        auto uploaded = renderDevice->CreateTexture(image);
        if (!uploaded) {
            assets->Release(handle);
            return UiResult<ImageResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "failed to upload UI image asset '" + AssetName(id) + "': " +
                    uploaded.error().message));
        }
        auto [inserted, wasInserted] = images.emplace(
            std::string(assetId), ImageResource{handle, uploaded.value()});
        if (!wasInserted) {
            static_cast<void>(renderDevice->ReleaseTexture(uploaded.value()));
            assets->Release(handle);
        }
        return UiResult<ImageResource*>::Ok(&inserted->second);
    }

    [[nodiscard]] UiResult<TextResource*> ResolveText(const UiTextDraw& draw) {
        auto font = ResolveFont(draw.fontAssetId);
        if (!font) return UiResult<TextResource*>::Err(std::move(font).error());
        TextKey key{
            draw.fontAssetId,
            font.value()->asset.generation(),
            draw.utf8,
            draw.pointSizePixels,
        };
        const auto cached = texts.find(key);
        if (cached != texts.end()) {
            cached->second.lastUse = ++useSerial;
            return UiResult<TextResource*>::Ok(&cached->second);
        }

        const auto& bytes = font.value()->font->bytes;
        auto rasterized = textRasterizer->Rasterize(
            std::span<const std::byte>{bytes.data(), bytes.size()},
            Text::TextRasterRequest{draw.utf8, draw.pointSizePixels});
        if (!rasterized) {
            return UiResult<TextResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "failed to rasterize UI text: " + rasterized.error().message));
        }
        Text::TextBitmap& bitmap = rasterized.value();
        if (!IsValidBitmap(bitmap)) {
            return UiResult<TextResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "text rasterizer returned an invalid UI RGBA8 bitmap"));
        }
        const Render::ImageView image{
            bitmap.width,
            bitmap.height,
            bitmap.rowPitch,
            std::span<const std::byte>{bitmap.rgba8.data(), bitmap.rgba8.size()},
            Render::TextureColorSpace::Linear,
        };
        auto uploaded = renderDevice->CreateTexture(image);
        if (!uploaded) {
            return UiResult<TextResource*>::Err(RendererError(
                UiErrorCode::ResourceFailure,
                "failed to upload UI text: " + uploaded.error().message));
        }
        auto [inserted, wasInserted] = texts.emplace(
            std::move(key),
            TextResource{uploaded.value(), bitmap.width, bitmap.height, ++useSerial});
        if (!wasInserted) {
            static_cast<void>(renderDevice->ReleaseTexture(uploaded.value()));
        }
        return UiResult<TextResource*>::Ok(&inserted->second);
    }

    [[nodiscard]] UiResult<void> SubmitSprite(
        const Render::SpriteSubmission& submission,
        Render::RenderQueue& queue) {
        auto submitted = queue.Submit(submission);
        if (!submitted) {
            return UiResult<void>::Err(RendererError(
                UiErrorCode::RenderSubmissionFailed,
                "failed to submit UI sprite: " + submitted.error().message));
        }
        return UiResult<void>::Ok();
    }
};

UiRenderer::UiRenderer() : impl_(std::make_unique<Impl>()) {}
UiRenderer::~UiRenderer() = default;
UiRenderer::UiRenderer(UiRenderer&&) noexcept = default;
UiRenderer& UiRenderer::operator=(UiRenderer&&) noexcept = default;

UiResult<void> UiRenderer::Initialize(
    Render::IRenderDevice& renderDevice,
    Text::ITextRasterizer& textRasterizer,
    Asset::AssetManager& assets,
    UiRendererOptions options) {
    if (options.maximumCachedTextRuns == 0) {
        return UiResult<void>::Err(RendererError(
            UiErrorCode::RuntimeState,
            "UiRenderer maximumCachedTextRuns must be positive"));
    }
    impl_->Reset();
    impl_->renderDevice = &renderDevice;
    impl_->textRasterizer = &textRasterizer;
    impl_->assets = &assets;
    impl_->options = options;
    return UiResult<void>::Ok();
}

void UiRenderer::Reset() noexcept {
    impl_->Reset();
}

UiResult<void> UiRenderer::Submit(
    const UiDrawList& drawList,
    Render::RenderQueue& queue) {
    if (impl_->renderDevice == nullptr || impl_->textRasterizer == nullptr ||
        impl_->assets == nullptr) {
        return UiResult<void>::Err(RendererError(
            UiErrorCode::RuntimeState,
            "UiRenderer is not initialized"));
    }
    impl_->TrimTextCache();
    for (const UiDrawCommand& command : drawList.commands) {
        auto submitted = std::visit(
            [this, &queue](const auto& draw) -> UiResult<void> {
                using T = std::decay_t<decltype(draw)>;
                Render::SpriteSubmission sprite{};
                sprite.layer = Render::CompositeLayer::Overlay;
                if constexpr (std::is_same_v<T, UiQuadDraw>) {
                    sprite.destinationPixels = ConvertRect(draw.destinationPixels);
                    sprite.tint = ConvertColor(draw.color);
                } else if constexpr (std::is_same_v<T, UiImageDraw>) {
                    auto image = impl_->ResolveImage(draw.textureAssetId);
                    if (!image) return UiResult<void>::Err(std::move(image).error());
                    sprite.texture = image.value()->gpu;
                    sprite.destinationPixels = ConvertRect(draw.destinationPixels);
                    sprite.sourceUv = ConvertRect(draw.sourceUv);
                    sprite.tint = ConvertColor(draw.tint);
                } else {
                    if (draw.utf8.empty()) return UiResult<void>::Ok();
                    if (!std::isfinite(draw.pointSizePixels) || draw.pointSizePixels <= 0.0F) {
                        return UiResult<void>::Err(RendererError(
                            UiErrorCode::RuntimeState,
                            "UI text point size must be finite and positive"));
                    }
                    auto text = impl_->ResolveText(draw);
                    if (!text) return UiResult<void>::Err(std::move(text).error());
                    float x = draw.boundsPixels.x;
                    float y = draw.boundsPixels.y;
                    if (draw.horizontalAlign == UiHorizontalAlign::Center) {
                        x += (draw.boundsPixels.width - static_cast<float>(text.value()->width)) * 0.5F;
                    } else if (draw.horizontalAlign == UiHorizontalAlign::Right) {
                        x += draw.boundsPixels.width - static_cast<float>(text.value()->width);
                    }
                    if (draw.verticalAlign == UiVerticalAlign::Center) {
                        y += (draw.boundsPixels.height - static_cast<float>(text.value()->height)) * 0.5F;
                    } else if (draw.verticalAlign == UiVerticalAlign::Bottom) {
                        y += draw.boundsPixels.height - static_cast<float>(text.value()->height);
                    }
                    sprite.texture = text.value()->gpu;
                    sprite.destinationPixels = {
                        x,
                        y,
                        static_cast<float>(text.value()->width),
                        static_cast<float>(text.value()->height),
                    };
                    sprite.tint = ConvertColor(draw.color);
                }
                if (!ClipSprite(sprite, draw.clipPixels)) {
                    return UiResult<void>::Ok();
                }
                return impl_->SubmitSprite(sprite, queue);
            },
            command);
        if (!submitted) return submitted;
    }
    return UiResult<void>::Ok();
}

} // namespace Engine::Ui
