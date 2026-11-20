#include "DecimalToDouble.hpp"

#include <cmath>

#include <fast_float/fast_float.h>

#include "engine/base/Assert.hpp"
#include "../../NumberText.hpp"

namespace GYO::Engine::Json::Detail {

DecimalRead DecimalToDouble(const std::string_view text, double& out) noexcept {
    double value = 0.0;
    const auto result = fast_float::from_chars(text.data(), text.data() + text.size(), value);
    // The text has passed JSON's number grammar, which fast_float accepts in full.
    GYO_ASSERT(result.ptr == text.data() + text.size());
    if (std::isinf(value)) return DecimalRead::OutOfRange;
    if (value == 0.0 && HasNonZeroSignificand(text)) return DecimalRead::OutOfRange;
    out = value;
    return DecimalRead::Ok;
}

} // namespace GYO::Engine::Json::Detail
