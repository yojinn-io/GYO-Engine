#include "ui/UiTypes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace Engine::Ui {
namespace {

[[nodiscard]] int HexDigit(char value) noexcept {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] float SrgbToLinear(float value) noexcept {
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] float LinearToSrgb(float value) noexcept {
    value = std::clamp(value, 0.0F, 1.0F);
    return value <= 0.0031308F
        ? value * 12.92F
        : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
}

[[nodiscard]] UiError ColorError(
    std::string message,
    std::string_view source,
    std::string_view jsonPointer) {
    return {
        UiErrorCode::ValidationFailed,
        std::move(message),
        std::string(source),
        std::string(jsonPointer),
    };
}

} // namespace

UiResult<UiColor> DecodeSrgbHexColor(
    std::string_view value,
    std::string_view source,
    std::string_view jsonPointer) {
    if ((value.size() != 7 && value.size() != 9) || value.front() != '#') {
        return UiResult<UiColor>::Err(ColorError(
            "color must use #RRGGBB or #RRGGBBAA",
            source,
            jsonPointer));
    }

    std::array<unsigned, 4> channels{0U, 0U, 0U, 255U};
    const std::size_t channelCount = value.size() == 9 ? 4U : 3U;
    for (std::size_t index = 0; index < channelCount; ++index) {
        const int high = HexDigit(value[1U + index * 2U]);
        const int low = HexDigit(value[2U + index * 2U]);
        if (high < 0 || low < 0) {
            return UiResult<UiColor>::Err(ColorError(
                "color contains a non-hexadecimal digit",
                source,
                jsonPointer));
        }
        channels[index] = static_cast<unsigned>(high * 16 + low);
    }

    constexpr float kByteScale = 1.0F / 255.0F;
    return UiResult<UiColor>::Ok({
        SrgbToLinear(static_cast<float>(channels[0]) * kByteScale),
        SrgbToLinear(static_cast<float>(channels[1]) * kByteScale),
        SrgbToLinear(static_cast<float>(channels[2]) * kByteScale),
        static_cast<float>(channels[3]) * kByteScale,
    });
}

std::string EncodeSrgbHexColor(const UiColor& color) {
    const auto byte = [](float value) {
        return static_cast<unsigned>(
            std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    };
    const unsigned red = byte(LinearToSrgb(color.red));
    const unsigned green = byte(LinearToSrgb(color.green));
    const unsigned blue = byte(LinearToSrgb(color.blue));
    const unsigned alpha = byte(color.alpha);

    std::array<char, 10> encoded{};
    std::snprintf(
        encoded.data(),
        encoded.size(),
        "#%02X%02X%02X%02X",
        red,
        green,
        blue,
        alpha);
    return encoded.data();
}

} // namespace Engine::Ui
