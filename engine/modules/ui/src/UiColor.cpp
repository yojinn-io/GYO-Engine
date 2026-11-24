// sRGB hex colours of the gyo.ui Data Contract (ported from master's engine/ui/src/UiColor.cpp): the document writes
// #RRGGBB or #RRGGBBAA in sRGB, UiColor holds linear RGB and linear alpha.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "engine/math/scalar/ColorSpace.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include "engine/ui/UiTypes.hpp"

namespace GYO::Engine::Ui {
namespace {

[[nodiscard]] int HexDigit(const char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

[[nodiscard]] UiFailure ColorFailure(std::string message, const std::string_view source,
                                     const std::string_view jsonPointer) {
    return UiFailure(UiFailureCode::ValidationFailed, std::move(message), std::string(source),
                     std::string(jsonPointer));
}

} // namespace

UiResult<UiColor> DecodeSrgbHexColor(const std::string_view value, const std::string_view source,
                                     const std::string_view jsonPointer) {
    if ((value.size() != 7 && value.size() != 9) || value.front() != '#') {
        return Base::Err(ColorFailure("color must use #RRGGBB or #RRGGBBAA", source, jsonPointer));
    }

    std::array<unsigned, 4> channels{0U, 0U, 0U, 255U};
    const std::size_t channelCount = value.size() == 9 ? 4U : 3U;
    for (std::size_t index = 0; index < channelCount; ++index) {
        const int high = HexDigit(value[1U + index * 2U]);
        const int low = HexDigit(value[2U + index * 2U]);
        if (high < 0 || low < 0) {
            return Base::Err(ColorFailure("color contains a non-hexadecimal digit", source, jsonPointer));
        }
        channels[index] = static_cast<unsigned>(high * 16 + low);
    }

    constexpr float kByteScale = 1.0F / 255.0F;
    return UiColor{
        Math::DecodeSrgb(static_cast<float>(channels[0]) * kByteScale),
        Math::DecodeSrgb(static_cast<float>(channels[1]) * kByteScale),
        Math::DecodeSrgb(static_cast<float>(channels[2]) * kByteScale),
        static_cast<float>(channels[3]) * kByteScale,
    };
}

std::string EncodeSrgbHexColor(const UiColor& color) {
    const auto byte = [](const float value) {
        return static_cast<unsigned>(std::lround(Math::Clamp(value, 0.0F, 1.0F) * 255.0F));
    };
    std::array<char, 10> encoded{};
    std::snprintf(encoded.data(), encoded.size(), "#%02X%02X%02X%02X", byte(Math::EncodeSrgb(color.red)),
                  byte(Math::EncodeSrgb(color.green)), byte(Math::EncodeSrgb(color.blue)), byte(color.alpha));
    return encoded.data();
}

} // namespace GYO::Engine::Ui
