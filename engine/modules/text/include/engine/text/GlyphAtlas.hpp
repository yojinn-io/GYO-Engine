#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

// The glyph atlas: pages of glyph coverage, plain data that a renderer reads
// (Design Invariant TX-6). Only TextContext::Rasterize writes it; every const
// member may be called from many threads at once. A renderer keeps the
// revision it last uploaded and asks ChangesSince(revision) for what to upload;
// it never clears anything (the render World only reads, DI-KS 13).
namespace GYO::Engine::Text {

namespace Detail {
struct AtlasState;
} // namespace Detail

enum class AtlasFormat : std::uint8_t {
    // One byte of coverage per pixel; the renderer multiplies it by a colour.
    Alpha8 = 1,
    // Rgba8 is kept for colour glyphs (Replacement Criterion RC-TX6).
};

struct AtlasOptions {
    // Width and height of every page, in pixels.
    std::uint32_t pageSize = 1024;
    // Empty pixels around every glyph.
    std::uint32_t padding = 1;
    // Changes kept for ChangesSince; older ones become "the whole page".
    std::size_t maxChangeLog = 4096;
};

struct AtlasRect {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] friend bool operator==(const AtlasRect&, const AtlasRect&) = default;
};

struct AtlasPageView {
    AtlasFormat format = AtlasFormat::Alpha8;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Bytes from one row to the next; row 0 is the top row.
    std::uint32_t rowPitch = 0;
    std::span<const std::uint8_t> pixels;
};

struct AtlasChange {
    std::uint32_t page = 0;
    // A whole page has the page's full rectangle.
    AtlasRect rect;
};

struct AtlasChanges {
    // The atlas's revision now; ask for changes after it next time.
    std::uint64_t revision = 0;
    std::vector<AtlasChange> changes;
};

enum class GlyphRasterStatus : std::uint8_t {
    // In the atlas at rect.
    Placed = 1,
    // Nothing to draw (a space, an invisible glyph).
    Empty = 2,
    // Larger than a page: not drawn. Not an error; the caller decides.
    TooLarge = 3,
};

struct GlyphPlacement {
    std::uint32_t page = 0;
    AtlasRect rect;
    // The bitmap's top-left corner relative to the glyph origin rounded to a
    // pixel: draw at (round(glyph.x) + left, round(glyph.y) - top).
    std::int32_t left = 0;
    std::int32_t top = 0;
    GlyphRasterStatus status = GlyphRasterStatus::Empty;
};

class GlyphAtlas final {
public:
    explicit GlyphAtlas(AtlasOptions options = {});
    ~GlyphAtlas();
    GlyphAtlas(GlyphAtlas&&) noexcept;
    GlyphAtlas& operator=(GlyphAtlas&&) noexcept;
    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    [[nodiscard]] std::size_t PageCount() const noexcept;
    [[nodiscard]] AtlasPageView Page(std::size_t index) const;
    // Grows by one with every Rasterize that changed the atlas.
    [[nodiscard]] std::uint64_t Revision() const noexcept;
    // The rectangles changed after `revision` (pages created after it are
    // whole); every page whole when the log no longer reaches back that far.
    [[nodiscard]] AtlasChanges ChangesSince(std::uint64_t revision) const;
    // Number of glyphs placed in the atlas.
    [[nodiscard]] std::size_t GlyphCount() const noexcept;

private:
    friend class TextContext;
    std::unique_ptr<Detail::AtlasState> state_;
};

} // namespace GYO::Engine::Text
