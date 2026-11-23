#include "DecimalToFloat.hpp"

#include <system_error>

#include <fast_float/fast_float.h>

namespace object_connect::adapters {

std::optional<float> DecimalToFloat(const std::string_view text) noexcept {
    float value = 0.0f;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = fast_float::from_chars(begin, end, value, fast_float::chars_format::general);
    if (text.empty() || result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

} // namespace object_connect::adapters
