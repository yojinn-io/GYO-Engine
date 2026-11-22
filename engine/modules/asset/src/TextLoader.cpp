#include "engine/asset/TextLoader.hpp"

#include <string>

namespace GYO::Engine::Asset {
namespace {

// The offset of the first byte that does not start or continue a well-formed
// UTF-8 sequence (RFC 3629: no overlong forms, no surrogates, nothing above
// U+10FFFF), or the size when every byte does.
std::size_t FirstNonUtf8(const std::span<const std::byte> bytes) {
    const std::size_t size = bytes.size();
    std::size_t index = 0;
    while (index < size) {
        const auto lead = static_cast<unsigned char>(bytes[index]);
        if (lead < 0x80) {
            ++index;
            continue;
        }
        std::size_t length = 0;
        unsigned char low = 0x80;  // range of the second byte
        unsigned char high = 0xBF;
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            if (lead == 0xE0) low = 0xA0;   // overlong
            if (lead == 0xED) high = 0x9F;  // surrogates
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            if (lead == 0xF0) low = 0x90;   // overlong
            if (lead == 0xF4) high = 0x8F;  // above U+10FFFF
        } else {
            return index;
        }
        if (size - index < length) return index;
        const auto second = static_cast<unsigned char>(bytes[index + 1]);
        if (second < low || second > high) return index;
        for (std::size_t next = 2; next < length; ++next) {
            const auto continuation = static_cast<unsigned char>(bytes[index + next]);
            if (continuation < 0x80 || continuation > 0xBF) return index;
        }
        index += length;
    }
    return size;
}

} // namespace

Base::Result<TextSettings, LoaderFailure<TextLoaderCode>> TextLoader::ParseSettings(
    const std::optional<std::string_view>) const {
    return TextSettings{};
}

Base::Result<Text, LoaderFailure<TextLoaderCode>> TextLoader::Load(const std::span<const std::byte> bytes,
                                                                  const TextSettings&, LoadContext& context) const {
    const std::size_t bad = FirstNonUtf8(bytes);
    if (bad != bytes.size()) {
        return Base::Err(LoaderFailure<TextLoaderCode>::Make(
            TextLoaderCode::NotUtf8, "the text is not UTF-8",
            context.Id().Text() + " (" + context.Path() + "), byte " + std::to_string(bad)));
    }
    if (bytes.empty()) return Text(std::string());
    return Text(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

} // namespace GYO::Engine::Asset
