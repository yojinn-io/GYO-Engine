#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace GYO::Engine::Text::Detail {

// Decodes UTF-8 into code points. Each maximal ill-formed subsequence becomes
// one U+FFFD (Unicode's recommended practice, Chapter 3 "U+FFFD Substitution
// of Maximal Subparts"). offsets gets one entry per code point (its first
// byte) and a last entry equal to the text's length.
void DecodeUtf8(std::string_view text, std::vector<char32_t>& codePoints, std::vector<std::uint32_t>& offsets);

} // namespace GYO::Engine::Text::Detail
