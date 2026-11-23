#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/image/ImageData.hpp"

// The image loader: type "image", format "png" (version 1), the Data Contract
// in docs/architecture/target/v2/image.zh-Hant.md section 3. Every PNG colour
// type and bit depth comes out as the one layout of ImageData (RGBA8, top row
// first). The settings choose the colour space. No references. A product
// registers it with AssetLoaderRegistry::Register<PngLoader>().

namespace GYO::Engine::Image {

// The code of the first rule broken, in the order of the Data Contract's rules
// (image.zh-Hant.md section 3: 1 to 4); BadSettings is the settings' rule.
enum class PngLoaderCode : std::uint8_t {
    // Fewer than 8 bytes, or not the PNG signature.
    NotPng = 1,
    // The first chunk is not a whole IHDR of 13 bytes, or the width or the
    // height is 0.
    BadHeader = 2,
    // The width or the height is above PngLoader::kMaxDimension.
    TooLarge = 3,
    // The decoder could not make pixels of the rest (a bad colour type or
    // bit depth, a broken or missing image data stream, a missing palette).
    Undecodable = 4,
    // The entry's settings are not the settings of section 3.
    BadSettings = 5,
};

[[nodiscard]] constexpr const char* ToString(const PngLoaderCode code) noexcept {
    switch (code) {
    case PngLoaderCode::NotPng: return "NotPng";
    case PngLoaderCode::BadHeader: return "BadHeader";
    case PngLoaderCode::TooLarge: return "TooLarge";
    case PngLoaderCode::Undecodable: return "Undecodable";
    case PngLoaderCode::BadSettings: return "BadSettings";
    }
    return "Unknown";
}

struct PngSettings final {
    ColorSpace colorSpace = ColorSpace::Srgb;

    [[nodiscard]] friend bool operator==(const PngSettings&, const PngSettings&) = default;
};

class PngLoader final {
public:
    using Output = ImageData;
    using Settings = PngSettings;
    using Code = PngLoaderCode;

    static constexpr std::string_view kTypeName = "image";
    static constexpr std::array<std::string_view, 1> kFormats{"png"};
    // The file's size; a product may override it.
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{64} * 1024u * 1024u;
    // The largest width and height: the largest 2D texture every graphics API
    // the engine targets must support (Direct3D 12, Vulkan and Metal).
    static constexpr std::uint32_t kMaxDimension = 16384;

    // No settings: sRGB. {"colorSpace": "srgb" | "linear"}; any other member
    // or value is BadSettings.
    [[nodiscard]] Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;

    [[nodiscard]] Base::Result<ImageData, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                                          const Settings& settings,
                                                                          Asset::LoadContext& context) const;
};

static_assert(Asset::AssetLoader<PngLoader>);

} // namespace GYO::Engine::Image
