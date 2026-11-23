#include "engine/image/PngLoader.hpp"

#include <string>
#include <utility>

#include "adapters/stb_image/StbPngDecode.hpp"
#include "engine/json/Document.hpp"

namespace GYO::Engine::Image {
namespace {

using Failure = Asset::LoaderFailure<PngLoaderCode>;

constexpr unsigned char kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
// Signature, chunk length, chunk type, the 13 bytes of IHDR and its CRC.
constexpr std::size_t kHeaderEnd = 8 + 4 + 4 + 13 + 4;

std::uint32_t U32BigEndian(const std::byte* p) noexcept {
    return (std::to_integer<std::uint32_t>(p[0]) << 24) | (std::to_integer<std::uint32_t>(p[1]) << 16) |
           (std::to_integer<std::uint32_t>(p[2]) << 8) | std::to_integer<std::uint32_t>(p[3]);
}

Base::Err<Failure> Fail(const PngLoaderCode code, const char* message, const Asset::LoadContext& context,
                        const std::string& detail) {
    return Base::Err(Failure::Make(code, message, context.Id().Text() + " (" + context.Path() + "): " + detail));
}

Base::Err<Failure> BadSettings(const std::string& detail) {
    return Base::Err(Failure::Make(PngLoaderCode::BadSettings,
                                   "the settings of a png image are {\"colorSpace\": \"srgb\" | \"linear\"}", detail));
}

} // namespace

Base::Result<PngSettings, Asset::LoaderFailure<PngLoaderCode>> PngLoader::ParseSettings(
    const std::optional<std::string_view> settingsJson) const {
    PngSettings settings;
    if (!settingsJson) return settings;
    const auto document = Json::Parse(*settingsJson);
    if (!document) return BadSettings("not JSON: " + document.error().message);
    const auto object = document->Root().AsObject();
    if (!object) return BadSettings("not an object");
    for (const Json::Member member : *object) {
        if (member.key != "colorSpace") return BadSettings("unknown member \"" + std::string(member.key) + "\"");
        const auto value = member.value.AsString();
        if (value && *value == "srgb") {
            settings.colorSpace = ColorSpace::Srgb;
        } else if (value && *value == "linear") {
            settings.colorSpace = ColorSpace::Linear;
        } else {
            return BadSettings("colorSpace is not \"srgb\" or \"linear\"");
        }
    }
    return settings;
}

Base::Result<ImageData, Asset::LoaderFailure<PngLoaderCode>> PngLoader::Load(const std::span<const std::byte> bytes,
                                                                             const PngSettings& settings,
                                                                             Asset::LoadContext& context) const {
    const std::byte* const p = bytes.data();
    const std::size_t n = bytes.size();
    // 1
    bool signature = n >= sizeof(kSignature);
    for (std::size_t i = 0; signature && i < sizeof(kSignature); ++i) {
        signature = std::to_integer<unsigned char>(p[i]) == kSignature[i];
    }
    if (!signature) {
        return Fail(PngLoaderCode::NotPng, "not a PNG file", context, "bytes=" + std::to_string(n));
    }
    // 2
    if (n < kHeaderEnd || U32BigEndian(p + 8) != 13 || std::to_integer<char>(p[12]) != 'I' ||
        std::to_integer<char>(p[13]) != 'H' || std::to_integer<char>(p[14]) != 'D' ||
        std::to_integer<char>(p[15]) != 'R') {
        return Fail(PngLoaderCode::BadHeader, "a PNG file starts with a whole IHDR chunk of 13 bytes", context,
                    "bytes=" + std::to_string(n));
    }
    const std::uint32_t width = U32BigEndian(p + 16);
    const std::uint32_t height = U32BigEndian(p + 20);
    if (width == 0 || height == 0) {
        return Fail(PngLoaderCode::BadHeader, "a PNG image is at least 1 by 1", context,
                    std::to_string(width) + "x" + std::to_string(height));
    }
    // 3
    if (width > kMaxDimension || height > kMaxDimension) {
        return Fail(PngLoaderCode::TooLarge, "a PNG image is at most 16384 pixels wide and high", context,
                    std::to_string(width) + "x" + std::to_string(height));
    }
    // 4
    auto decoded = Detail::DecodePngToRgba8(bytes);
    if (!decoded) {
        return Fail(PngLoaderCode::Undecodable, "the decoder could not make pixels of the PNG file", context,
                    std::to_string(width) + "x" + std::to_string(height));
    }
    return ImageData(width, height, settings.colorSpace, std::move(decoded->pixels));
}

} // namespace GYO::Engine::Image
