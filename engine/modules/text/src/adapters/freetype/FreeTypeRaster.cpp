#include "adapters/freetype/FreeTypeRaster.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_DRIVER_H
#include FT_MODULE_H

#include <new>
#include <utility>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Text::Detail {

namespace {

struct LibraryDeleter {
    void operator()(FT_Library library) const noexcept { FT_Done_FreeType(library); }
};
using LibraryPtr = std::unique_ptr<FT_LibraryRec_, LibraryDeleter>;

// The properties that change hinting, set explicitly on every library: the
// environment cannot change them (FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES is
// off), and an upstream default that changes between FreeType versions does
// not change the output silently (Design Invariant TX-8).
void SetProperties(FT_Library library) {
    FT_UInt interpreter = TT_INTERPRETER_VERSION_40;
    FT_Property_Set(library, "truetype", "interpreter-version", &interpreter);
    FT_UInt engine = FT_HINTING_ADOBE;
    FT_Property_Set(library, "cff", "hinting-engine", &engine);
    FT_Bool noDarkening = 1;
    FT_Property_Set(library, "cff", "no-stem-darkening", &noDarkening);
    FT_Property_Set(library, "autofitter", "no-stem-darkening", &noDarkening);
}

LibraryPtr MakeLibrary() {
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        return nullptr;
    }
    SetProperties(library);
    return LibraryPtr(library);
}

FreeTypeOpen StatusOf(const FT_Error error) {
    return error == FT_Err_Out_Of_Memory ? FreeTypeOpen::OutOfMemory : FreeTypeOpen::Unreadable;
}

} // namespace

FreeTypeOpen FreeTypeCheckFace(const std::span<const std::byte> bytes, const std::uint32_t faceIndex) {
    const LibraryPtr library = MakeLibrary();
    if (library == nullptr) {
        return FreeTypeOpen::OutOfMemory;
    }
    FT_Face face = nullptr;
    const FT_Error error = FT_New_Memory_Face(library.get(), reinterpret_cast<const FT_Byte*>(bytes.data()),
                                              static_cast<FT_Long>(bytes.size()), static_cast<FT_Long>(faceIndex),
                                              &face);
    if (error != 0) {
        return StatusOf(error);
    }
    FT_Done_Face(face);
    return FreeTypeOpen::Opened;
}

struct FreeTypeLibrary::Impl {
    LibraryPtr library;
    std::vector<FT_Face> faces;
    // The size each face was last set to, so it is set only when it changes.
    std::vector<FT_F26Dot6> sizes;

    ~Impl() {
        for (FT_Face face : faces) {
            FT_Done_Face(face);
        }
    }
};

FreeTypeLibrary::FreeTypeLibrary() : impl_(std::make_unique<Impl>()) {
    impl_->library = MakeLibrary();
    if (impl_->library == nullptr) {
        throw std::bad_alloc();
    }
}

FreeTypeLibrary::~FreeTypeLibrary() = default;
FreeTypeLibrary::FreeTypeLibrary(FreeTypeLibrary&&) noexcept = default;
FreeTypeLibrary& FreeTypeLibrary::operator=(FreeTypeLibrary&&) noexcept = default;

std::optional<std::uint32_t> FreeTypeLibrary::OpenFace(const std::span<const std::byte> bytes,
                                                       const std::uint32_t faceIndex, FreeTypeOpen& status) {
    FT_Face face = nullptr;
    const FT_Error error = FT_New_Memory_Face(impl_->library.get(), reinterpret_cast<const FT_Byte*>(bytes.data()),
                                              static_cast<FT_Long>(bytes.size()), static_cast<FT_Long>(faceIndex),
                                              &face);
    if (error != 0) {
        status = StatusOf(error);
        return std::nullopt;
    }
    impl_->faces.push_back(face);
    impl_->sizes.push_back(0);
    status = FreeTypeOpen::Opened;
    return static_cast<std::uint32_t>(impl_->faces.size() - 1);
}

std::string FreeTypeLibrary::FamilyName(const std::uint32_t face) const {
    const char* name = impl_->faces.at(face)->family_name;
    return name != nullptr ? std::string(name) : std::string();
}

std::string FreeTypeLibrary::StyleName(const std::uint32_t face) const {
    const char* name = impl_->faces.at(face)->style_name;
    return name != nullptr ? std::string(name) : std::string();
}

std::int32_t FreeTypeLibrary::UnscaledAdvance(const std::uint32_t face, const std::uint32_t glyph) {
    FT_Face f = impl_->faces.at(face);
    if (FT_Load_Glyph(f, glyph, FT_LOAD_NO_SCALE) != 0) {
        return -1;
    }
    return static_cast<std::int32_t>(f->glyph->advance.x);
}

bool FreeTypeLibrary::Render(const std::uint32_t face, const std::uint32_t glyph, const std::int32_t size26,
                             GlyphBitmap& out) {
    FT_Face f = impl_->faces.at(face);
    if (impl_->sizes[face] != size26) {
        // Pixels per em = size at 72 dpi.
        if (FT_Set_Char_Size(f, 0, size26, 72, 72) != 0) {
            return false;
        }
        impl_->sizes[face] = size26;
    }
    if (FT_Load_Glyph(f, glyph, FT_LOAD_TARGET_LIGHT | FT_LOAD_NO_BITMAP | FT_LOAD_RENDER) != 0) {
        return false;
    }
    const FT_GlyphSlot slot = f->glyph;
    const FT_Bitmap& bitmap = slot->bitmap;
    GYO_ASSERT(bitmap.rows == 0 || bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
    out.width = static_cast<std::int32_t>(bitmap.width);
    out.height = static_cast<std::int32_t>(bitmap.rows);
    out.left = slot->bitmap_left;
    out.top = slot->bitmap_top;
    out.pixels.assign(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height), 0);
    for (std::int32_t row = 0; row < out.height; ++row) {
        // A negative pitch means the rows go up; row 0 of the output is the top row.
        const unsigned char* src = bitmap.pitch >= 0
                                       ? bitmap.buffer + static_cast<std::ptrdiff_t>(row) * bitmap.pitch
                                       : bitmap.buffer + static_cast<std::ptrdiff_t>(out.height - 1 - row) * -bitmap.pitch;
        for (std::int32_t col = 0; col < out.width; ++col) {
            out.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(out.width) +
                       static_cast<std::size_t>(col)] = src[col];
        }
    }
    return true;
}

} // namespace GYO::Engine::Text::Detail
