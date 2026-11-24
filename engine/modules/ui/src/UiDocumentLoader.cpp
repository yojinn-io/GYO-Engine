#include "engine/ui/UiDocumentLoader.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/ui/UiDocumentCodec.hpp"

namespace GYO::Engine::Ui {

namespace {

void CollectTextures(const std::vector<UiElement>& elements, UiAsset& out, Asset::LoadContext& context) {
    for (const UiElement& element : elements) {
        if (element.type == UiElementType::Image && !element.textureAsset.empty() &&
            out.textures.find(element.textureAsset) == out.textures.end()) {
            out.textures.emplace(element.textureAsset,
                                 context.Reference<Image::ImageData>(Asset::AssetId(element.textureAsset)));
        }
        CollectTextures(element.children, out, context);
        CollectTextures(element.itemTemplate, out, context);
    }
}

} // namespace

Base::Result<UiDocumentSettings, Asset::LoaderFailure<UiFailureCode>> UiDocumentLoader::ParseSettings(
    const std::optional<std::string_view>) const {
    return UiDocumentSettings{};
}

Base::Result<UiAsset, Asset::LoaderFailure<UiFailureCode>> UiDocumentLoader::Load(const std::span<const std::byte> bytes,
                                                                                const UiDocumentSettings&,
                                                                                Asset::LoadContext& context) const {
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    auto parsed = UiDocumentCodec::Parse(text, context.Id().Text());
    if (!parsed) {
        const UiFailure& failure = parsed.error();
        std::string detail = context.Id().Text() + " (" + context.Path() + ")";
        if (!failure.jsonPointer.empty()) detail += " at " + failure.jsonPointer;
        if (!failure.detail.empty()) detail += ": " + failure.detail;
        return Base::Err(Asset::LoaderFailure<UiFailureCode>::Make(failure.code, failure.message, std::move(detail)));
    }
    UiAsset out;
    out.document = std::make_shared<const UiDocument>(std::move(parsed).value());
    for (const auto& [alias, id] : out.document->fonts) {
        (void)alias;
        if (out.fonts.find(id) == out.fonts.end()) {
            out.fonts.emplace(id, context.Reference<Text::FontData>(Asset::AssetId(id)));
        }
    }
    for (const UiCanvas& canvas : out.document->canvases) CollectTextures(canvas.children, out, context);
    return out;
}

} // namespace GYO::Engine::Ui
