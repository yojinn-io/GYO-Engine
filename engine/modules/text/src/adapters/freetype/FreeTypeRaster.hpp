#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The FreeType adapter's interface, in the text Module's words (Design
// Invariant TX-1: FreeType's headers, types and calls stay in
// src/adapters/freetype/). Sizes are 26.6 fixed point pixels per em.
namespace GYO::Engine::Text::Detail {

enum class FreeTypeOpen : std::uint8_t { Opened, Unreadable, OutOfMemory };

// Whether FreeType opens face faceIndex, with its own temporary library (for
// the loader, on any thread).
[[nodiscard]] FreeTypeOpen FreeTypeCheckFace(std::span<const std::byte> bytes, std::uint32_t faceIndex);

// A glyph's coverage: one byte per pixel, row 0 on top, rows of `width` bytes.
// left and top place the bitmap's top-left corner relative to the glyph origin
// (top is upward).
struct GlyphBitmap {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::vector<std::uint8_t> pixels;
};

// One per TextContext: its FreeType library and the faces opened in it. One
// user at a time (FreeType objects are not for two threads at once).
class FreeTypeLibrary final {
public:
    // Throws std::bad_alloc when FreeType cannot start.
    FreeTypeLibrary();
    ~FreeTypeLibrary();
    FreeTypeLibrary(FreeTypeLibrary&&) noexcept;
    FreeTypeLibrary& operator=(FreeTypeLibrary&&) noexcept;

    // A handle to the opened face; nothing when FreeType cannot open it. The
    // bytes must outlive the library.
    [[nodiscard]] std::optional<std::uint32_t> OpenFace(std::span<const std::byte> bytes, std::uint32_t faceIndex,
                                                        FreeTypeOpen& status);
    [[nodiscard]] std::string FamilyName(std::uint32_t face) const;
    [[nodiscard]] std::string StyleName(std::uint32_t face) const;
    // The advance in font units, unscaled and unhinted (Design Invariant
    // TX-2's check).
    [[nodiscard]] std::int32_t UnscaledAdvance(std::uint32_t face, std::uint32_t glyph);
    // Draws the glyph from its outline (embedded bitmaps are not used) with
    // light hinting and grey anti-aliasing. False if FreeType fails.
    [[nodiscard]] bool Render(std::uint32_t face, std::uint32_t glyph, std::int32_t size26, GlyphBitmap& out);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Engine::Text::Detail
