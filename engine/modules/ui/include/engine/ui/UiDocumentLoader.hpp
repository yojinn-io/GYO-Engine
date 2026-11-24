#pragma once

// The gyo.ui loader: asset type "ui", format json (docs/architecture/target/v2/ui.zh-Hant.md, section 4.2). Load
// reads and validates the document (UiDocumentCodec::Parse) and declares a reference by id for every font of its
// "fonts" and every texture_asset of its image elements. No settings. A product registers it with
// AssetLoaderRegistry::Register<UiDocumentLoader>().

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/ui/UiAsset.hpp"

namespace GYO::Engine::Ui {

struct UiDocumentSettings final {
    [[nodiscard]] friend bool operator==(const UiDocumentSettings&, const UiDocumentSettings&) = default;
};

class UiDocumentLoader final {
public:
    using Output = UiAsset;
    using Settings = UiDocumentSettings;
    using Code = UiFailureCode;

    static constexpr std::string_view kTypeName = "ui";
    static constexpr std::array<std::string_view, 1> kFormats{"json"};
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{16} * 1024u * 1024u;

    // Any settings object is accepted and ignored.
    [[nodiscard]] Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;

    // A failure's detail names the asset, its path and, for a document error, the JSON pointer.
    [[nodiscard]] Base::Result<UiAsset, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                                        const Settings& settings,
                                                                        Asset::LoadContext& context) const;
};

static_assert(Asset::AssetLoader<UiDocumentLoader>);

} // namespace GYO::Engine::Ui
