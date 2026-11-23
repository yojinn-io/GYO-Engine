#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/text/FontData.hpp"

// The font loader: asset type "font", formats ttf, otf, ttc and otc (version 1
// of the Data Contract in docs/architecture/target/v2/text.zh-Hant.md). Load
// copies the bytes and hands them to FontData::FromBytes, which checks every
// face with its own HarfBuzz and FreeType objects: no global state, so loads
// may run on several threads at once. No references. A product registers it
// with AssetLoaderRegistry::Register<FontLoader>().
namespace GYO::Engine::Text {

struct FontSettings final {
    [[nodiscard]] friend bool operator==(const FontSettings&, const FontSettings&) = default;
};

class FontLoader final {
public:
    using Output = FontData;
    using Settings = FontSettings;
    using Code = TextFailureCode;

    static constexpr std::string_view kTypeName = "font";
    static constexpr std::array<std::string_view, 4> kFormats{"ttf", "otf", "ttc", "otc"};
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{64} * 1024u * 1024u;

    // Any settings object is accepted and ignored (the face is chosen when the
    // font is added to a TextContext).
    [[nodiscard]] Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;

    [[nodiscard]] Base::Result<FontData, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                                         const Settings& settings,
                                                                         Asset::LoadContext& context) const;
};

static_assert(Asset::AssetLoader<FontLoader>);

} // namespace GYO::Engine::Text
