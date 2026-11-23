#include "Utf8.hpp"

namespace GYO::Engine::Text::Detail {

void DecodeUtf8(const std::string_view text, std::vector<char32_t>& codePoints, std::vector<std::uint32_t>& offsets) {
    codePoints.clear();
    offsets.clear();
    const auto byte = [&](const std::size_t i) { return static_cast<unsigned char>(text[i]); };
    std::size_t i = 0;
    while (i < text.size()) {
        offsets.push_back(static_cast<std::uint32_t>(i));
        const unsigned char lead = byte(i);
        if (lead < 0x80) {
            codePoints.push_back(lead);
            ++i;
            continue;
        }
        // Table 3-7 (well-formed UTF-8): the length, the first code point bits
        // and the allowed range of the second byte.
        std::size_t length = 0;
        char32_t cp = 0;
        unsigned char low = 0x80;
        unsigned char high = 0xBF;
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            cp = lead & 0x1Fu;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            cp = lead & 0x0Fu;
            if (lead == 0xE0) low = 0xA0;
            if (lead == 0xED) high = 0x9F;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            cp = lead & 0x07u;
            if (lead == 0xF0) low = 0x90;
            if (lead == 0xF4) high = 0x8F;
        } else {
            // C0, C1, F5..FF or a lone continuation byte.
            codePoints.push_back(U'�');
            ++i;
            continue;
        }
        std::size_t used = 1;
        bool complete = true;
        for (; used < length; ++used) {
            if (i + used >= text.size()) {
                complete = false;
                break;
            }
            const unsigned char next = byte(i + used);
            const unsigned char lo = used == 1 ? low : 0x80;
            const unsigned char hi = used == 1 ? high : 0xBF;
            if (next < lo || next > hi) {
                complete = false;
                break;
            }
            cp = (cp << 6) | (next & 0x3Fu);
        }
        codePoints.push_back(complete ? cp : U'�');
        i += used;
    }
    offsets.push_back(static_cast<std::uint32_t>(text.size()));
}

} // namespace GYO::Engine::Text::Detail
