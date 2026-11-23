#include "engine/text/FontData.hpp"

#include <array>
#include <cstring>
#include <new>
#include <string>

#include "TextInternal.hpp"

namespace GYO::Engine::Text {

namespace {

// Rule 1 of the "font" Data Contract: an sfnt (TrueType 0x00010000 or 'true',
// CFF 'OTTO') or an sfnt collection ('ttcf').
bool IsSfnt(const std::span<const std::byte> bytes) {
    if (bytes.size() < 12) {
        return false;
    }
    constexpr std::array<std::array<unsigned char, 4>, 4> kTags{{
        {0x00, 0x01, 0x00, 0x00}, {'t', 'r', 'u', 'e'}, {'O', 'T', 'T', 'O'}, {'t', 't', 'c', 'f'}}};
    for (const auto& tag : kTags) {
        if (std::memcmp(bytes.data(), tag.data(), tag.size()) == 0) {
            return true;
        }
    }
    return false;
}

bool IsCollection(const std::span<const std::byte> bytes) {
    return bytes.size() >= 4 && std::memcmp(bytes.data(), "ttcf", 4) == 0;
}

TextFailure Fail(const TextFailureCode code, const char* message, const std::uint32_t face) {
    return TextFailure::Make(code, message, "face " + std::to_string(face));
}

} // namespace

TextResult<FontData> FontData::FromBytes(std::vector<std::byte>&& bytes) {
    try {
        if (!IsSfnt(bytes)) {
            return Base::Err(TextFailure::Make(TextFailureCode::NotSfnt,
                                               "not an sfnt font or collection (TrueType, OpenType, .ttc, .otc)"));
        }
        const std::uint32_t faceCount = Detail::HarfBuzzFaceCount(bytes);
        if (faceCount == 0) {
            if (IsCollection(bytes)) {
                return Base::Err(TextFailure::Make(TextFailureCode::EmptyCollection, "a collection with no face"));
            }
            return Base::Err(TextFailure::Make(TextFailureCode::FaceUnreadable, "HarfBuzz reads no face", "face 0"));
        }
        // Rule 2: every face opens with both adapters, has glyphs and a Unicode
        // character map; each check uses its own temporary objects.
        for (std::uint32_t face = 0; face < faceCount; ++face) {
            const auto harfBuzz = Detail::HarfBuzzFace::Create(bytes, face);
            if (harfBuzz == nullptr) {
                return Base::Err(Fail(TextFailureCode::FaceUnreadable, "HarfBuzz cannot read the face", face));
            }
            const Detail::HarfBuzzFaceCheck check = harfBuzz->Check();
            if (!check.hasGlyphs) {
                return Base::Err(Fail(TextFailureCode::NoGlyphs, "the face has no glyph", face));
            }
            if (!check.hasUnicodeCmap) {
                return Base::Err(Fail(TextFailureCode::NoUnicodeCmap, "the face has no Unicode character map", face));
            }
            switch (Detail::FreeTypeCheckFace(bytes, face)) {
            case Detail::FreeTypeOpen::Opened: break;
            case Detail::FreeTypeOpen::Unreadable:
                return Base::Err(Fail(TextFailureCode::FaceUnreadable, "FreeType cannot open the face", face));
            case Detail::FreeTypeOpen::OutOfMemory:
                return Base::Err(Fail(TextFailureCode::OutOfMemory, "out of memory opening the face", face));
            }
        }
        auto data = std::make_shared<Detail::FontBytes>();
        data->bytes = std::move(bytes);
        data->faceCount = faceCount;
        return FontData(std::move(data));
    } catch (const std::bad_alloc&) {
        return Base::Err(TextFailure::Make(TextFailureCode::OutOfMemory, "out of memory reading the font"));
    }
}

std::uint32_t FontData::FaceCount() const noexcept { return data_->faceCount; }

std::span<const std::byte> FontData::Bytes() const noexcept { return data_->bytes; }

} // namespace GYO::Engine::Text
