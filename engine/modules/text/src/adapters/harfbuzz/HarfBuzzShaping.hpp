#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

// The HarfBuzz adapter's interface, in the text Module's words (Design
// Invariant TX-1: HarfBuzz's headers, types and calls stay in
// src/adapters/harfbuzz/). Sizes and positions are 26.6 fixed point pixels.
namespace GYO::Engine::Text::Detail {

// A script (an ISO 15924 tag as HarfBuzz numbers it).
using ScriptCode = std::uint32_t;

[[nodiscard]] ScriptCode ScriptOf(char32_t cp) noexcept;
// Common and Inherited take the script of the text around them.
[[nodiscard]] bool IsNeutralScript(ScriptCode script) noexcept;
[[nodiscard]] ScriptCode CommonScript() noexcept;

// The number of faces HarfBuzz finds (0: not a font HarfBuzz reads).
[[nodiscard]] std::uint32_t HarfBuzzFaceCount(std::span<const std::byte> bytes);

struct HarfBuzzFaceCheck {
    bool hasGlyphs = false;
    bool hasUnicodeCmap = false;
};

// Line metrics at one size, 26.6, ascender and descender both positive.
struct LineMetrics {
    std::int32_t ascender = 0;
    std::int32_t descender = 0;
    std::int32_t lineGap = 0;
};

// One face. Immutable after Create: its const members may run on many threads.
class HarfBuzzFace final {
public:
    // Null when the face cannot be read. The bytes must outlive the face.
    [[nodiscard]] static std::unique_ptr<HarfBuzzFace> Create(std::span<const std::byte> bytes,
                                                              std::uint32_t faceIndex);
    ~HarfBuzzFace();
    HarfBuzzFace(const HarfBuzzFace&) = delete;
    HarfBuzzFace& operator=(const HarfBuzzFace&) = delete;

    [[nodiscard]] HarfBuzzFaceCheck Check() const;
    [[nodiscard]] bool HasGlyph(char32_t cp) const;
    [[nodiscard]] std::uint32_t UnitsPerEm() const;
    [[nodiscard]] std::uint32_t GlyphCount() const;
    // The advance in font units, from 'hmtx' (Design Invariant TX-2's check).
    [[nodiscard]] std::int32_t UnscaledAdvance(std::uint32_t glyph) const;

private:
    friend class HarfBuzzShaper;
    struct Impl;
    explicit HarfBuzzFace(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

// A glyph HarfBuzz placed. cluster is an index into the code points given.
struct RawGlyph {
    std::uint32_t glyph = 0;
    std::uint32_t cluster = 0;
    std::int32_t xAdvance = 0;
    std::int32_t xOffset = 0;
    std::int32_t yOffset = 0;
    bool unsafeToBreak = false;
};

// The glyph a hidden default-ignorable character becomes (zero advance,
// never drawn).
inline constexpr std::uint32_t kInvisibleGlyph = 0xFFFFFFFFu;

// One per TextContext: its buffer and the sized fonts it made. Not for two
// threads at once.
class HarfBuzzShaper final {
public:
    HarfBuzzShaper();
    ~HarfBuzzShaper();
    HarfBuzzShaper(HarfBuzzShaper&&) noexcept;
    HarfBuzzShaper& operator=(HarfBuzzShaper&&) noexcept;

    // Shapes text[begin, begin + length) left to right, the rest of text as
    // context, in a language (a BCP 47 tag; empty: "und"). Appends to out.
    void Shape(const HarfBuzzFace& face, std::int32_t size26, std::span<const char32_t> text, std::size_t begin,
               std::size_t length, ScriptCode script, std::string_view language, std::vector<RawGlyph>& out);
    // The face's line metrics at a size (hhea, or OS/2 when the font says so).
    [[nodiscard]] LineMetrics Metrics(const HarfBuzzFace& face, std::int32_t size26);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Engine::Text::Detail
