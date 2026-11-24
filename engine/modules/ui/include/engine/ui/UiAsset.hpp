#pragma once

// A gyo.ui document loaded as an asset (docs/architecture/target/v2/ui.zh-Hant.md, section 4.2): the document and
// the references its loader declared to the fonts and pictures it names by asset id. So a document whose
// dependencies are Complete has its fonts and pictures loaded (asset declaration 5.17).

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetTypeName.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/text/FontData.hpp"
#include "engine/ui/UiDocument.hpp"

namespace GYO::Engine::Ui {

struct UiAsset final {
    // Shared and immutable: a UiRuntime keeps it alive.
    std::shared_ptr<const UiDocument> document;
    // By font asset id (the values of the document's "fonts").
    std::map<std::string, Asset::AssetRef<Text::FontData>, std::less<>> fonts;
    // By image asset id (the texture_asset of its image elements).
    std::map<std::string, Asset::AssetRef<Image::ImageData>, std::less<>> textures;
};

} // namespace GYO::Engine::Ui

template <>
struct GYO::Engine::Asset::AssetTypeName<GYO::Engine::Ui::UiAsset> {
    static constexpr std::string_view value = "ui";
};
