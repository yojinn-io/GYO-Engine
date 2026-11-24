#pragma once

// The text a UI lays out and draws (docs/architecture/target/v2/ui.zh-Hant.md, section 4.4): one text Module
// TextContext, one glyph atlas and the fonts added by their asset id, with a cache of laid-out texts. world_ui keeps
// one as a Resource of the World the UI lives in (as Bevy keeps its font system and FontAtlasSet in the main world);
// ui_render reads the atlas through Extract. One user at a time (the text Module's TX-7).

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>

#include "engine/text/FontData.hpp"
#include "engine/text/GlyphAtlas.hpp"
#include "engine/text/TextContext.hpp"
#include "engine/text/TextFailure.hpp"
#include "engine/text/TextLayout.hpp"

namespace GYO::Engine::Ui {

class UiText final {
public:
    // Glyphs are drawn at their own size: a page per 1024 x 1024 pixels by default.
    explicit UiText(Text::AtlasOptions atlas = {});

    // Adds face faceIndex of a font under id (a gyo.ui document's font asset id). An id already added keeps its font.
    [[nodiscard]] Text::TextResult<void> AddFont(std::string id, const Text::FontData& data, std::uint32_t faceIndex = 0);
    [[nodiscard]] bool HasFont(std::string_view id) const;

    // The layout of utf8 in the font id at sizePx pixels per em, lines broken only at mandatory breaks, each line
    // aligned within the widest. Null when the font has not been added (the text is not drawn yet). The pointer
    // stays valid until the next EndFrame.
    [[nodiscard]] const Text::TextLayout* Layout(std::string_view fontId, float sizePx, std::string_view utf8,
                                                 Text::HorizontalAlign align = Text::HorizontalAlign::Left);

    // Draws the layout's glyphs that are not in the atlas yet (TextContext::Rasterize).
    [[nodiscard]] Text::RasterResult Rasterize(const Text::TextLayout& layout);

    [[nodiscard]] const Text::GlyphAtlas& Atlas() const noexcept { return atlas_; }

    // Forgets the layouts no Layout call asked for since the previous EndFrame.
    void EndFrame();

    // Which UiText this is: unique per process, kept by a move. A renderer that caches the atlas's pages keys them by
    // it, so a UiText replaced (a source World replaced, O-8) is uploaded anew, not read as the old one's revisions.
    [[nodiscard]] std::uint64_t Id() const noexcept { return id_; }

private:
    struct Key {
        std::string font;
        std::uint32_t sizeBits = 0;
        Text::HorizontalAlign align = Text::HorizontalAlign::Left;
        std::string text;

        friend bool operator<(const Key& a, const Key& b) {
            return std::tie(a.font, a.sizeBits, a.align, a.text) < std::tie(b.font, b.sizeBits, b.align, b.text);
        }
    };
    struct Entry {
        Text::TextLayout layout;
        bool used = true;
    };

    std::uint64_t id_ = 0;
    Text::TextContext context_;
    Text::GlyphAtlas atlas_;
    std::map<std::string, Text::FontId, std::less<>> fonts_;
    std::map<Key, Entry> layouts_;
};

} // namespace GYO::Engine::Ui
