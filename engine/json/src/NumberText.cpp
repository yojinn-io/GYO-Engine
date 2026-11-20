#include "NumberText.hpp"

#include <charconv>
#include <system_error>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Json::Detail {

bool IsIntegerLiteral(const std::string_view text) noexcept {
    return text.find_first_of(".eE") == std::string_view::npos;
}

bool HasNonZeroSignificand(const std::string_view text) noexcept {
    for (const char c : text) {
        if (c == 'e' || c == 'E') return false;
        if (c >= '1' && c <= '9') return true;
    }
    return false;
}

namespace {

template <class Integer>
IntegerRead ReadInteger(const std::string_view text, Integer& out) noexcept {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    if (result.ec == std::errc::result_out_of_range) return IntegerRead::OutOfRange;
    GYO_ASSERT(result.ec == std::errc{} && result.ptr == text.data() + text.size());
    return IntegerRead::Ok;
}

} // namespace

IntegerRead ReadInt64(const std::string_view text, std::int64_t& out) noexcept {
    GYO_ASSERT(IsIntegerLiteral(text));
    return ReadInteger(text, out);
}

IntegerRead ReadUint64(const std::string_view text, std::uint64_t& out) noexcept {
    GYO_ASSERT(IsIntegerLiteral(text));
    if (!text.empty() && text.front() == '-') {
        // std::from_chars does not take a sign for unsigned types; JSON's
        // grammar allows no leading zeros, so "-0" is the only negative zero.
        if (text == "-0") {
            out = 0;
            return IntegerRead::Ok;
        }
        return IntegerRead::OutOfRange;
    }
    return ReadInteger(text, out);
}

} // namespace GYO::Engine::Json::Detail
