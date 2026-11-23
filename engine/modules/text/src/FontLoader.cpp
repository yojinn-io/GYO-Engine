#include "engine/text/FontLoader.hpp"

#include <vector>

namespace GYO::Engine::Text {

Base::Result<FontSettings, Asset::LoaderFailure<TextFailureCode>> FontLoader::ParseSettings(
    const std::optional<std::string_view>) const {
    return FontSettings{};
}

Base::Result<FontData, Asset::LoaderFailure<TextFailureCode>> FontLoader::Load(const std::span<const std::byte> bytes,
                                                                               const FontSettings&,
                                                                               Asset::LoadContext& context) const {
    auto font = FontData::FromBytes(std::vector<std::byte>(bytes.begin(), bytes.end()));
    if (!font) {
        TextFailure failure = std::move(font).error();
        std::string detail = context.Id().Text() + " (" + context.Path() + ")";
        if (!failure.detail.empty()) {
            detail += ": " + failure.detail;
        }
        return Base::Err(TextFailure::Make(failure.code, std::move(failure.message), std::move(detail)));
    }
    return std::move(font).value();
}

} // namespace GYO::Engine::Text
