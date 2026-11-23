#include "engine/text/GlyphAtlas.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <memory>

#include "TextInternal.hpp"
#include "engine/base/Assert.hpp"

namespace GYO::Engine::Text {

namespace Detail {

struct Shelf {
    std::uint32_t y = 0;
    std::uint32_t height = 0;
    std::uint32_t x = 0;
};

struct AtlasPage {
    std::vector<std::uint8_t> pixels;
    std::vector<Shelf> shelves;
    std::uint32_t nextY = 0;
};

struct LoggedChange {
    std::uint64_t revision = 0;
    AtlasChange change;
};

struct AtlasState {
    AtlasOptions options;
    std::vector<AtlasPage> pages;
    std::map<GlyphKey, GlyphPlacement> glyphs;
    std::size_t placed = 0;
    std::uint64_t revision = 0;
    // Every change with a revision above logBase.
    std::deque<LoggedChange> log;
    std::uint64_t logBase = 0;
    std::weak_ptr<ContextIdentity> owner;
    bool bound = false;

    AtlasRect FullPage() const { return AtlasRect{0, 0, options.pageSize, options.pageSize}; }

    // First fit over the pages and their shelves; a new shelf, then a new
    // page, when nothing fits. Placed glyphs never move.
    void Allocate(const std::uint32_t width, const std::uint32_t height, std::uint32_t& page, std::uint32_t& x,
                  std::uint32_t& y, std::vector<AtlasChange>& changes) {
        const std::uint32_t size = options.pageSize;
        for (std::uint32_t p = 0; p < pages.size(); ++p) {
            for (Shelf& shelf : pages[p].shelves) {
                if (shelf.height >= height && size - shelf.x >= width) {
                    page = p;
                    x = shelf.x;
                    y = shelf.y;
                    shelf.x += width;
                    return;
                }
            }
            if (size - pages[p].nextY >= height) {
                pages[p].shelves.push_back(Shelf{pages[p].nextY, height, width});
                page = p;
                x = 0;
                y = pages[p].nextY;
                pages[p].nextY += height;
                return;
            }
        }
        AtlasPage fresh;
        fresh.pixels.assign(static_cast<std::size_t>(size) * size, 0);
        fresh.shelves.push_back(Shelf{0, height, width});
        fresh.nextY = height;
        pages.push_back(std::move(fresh));
        page = static_cast<std::uint32_t>(pages.size() - 1);
        x = 0;
        y = 0;
        changes.push_back(AtlasChange{page, FullPage()});
    }

    void Commit(const std::vector<AtlasChange>& changes) {
        if (changes.empty()) {
            return;
        }
        ++revision;
        for (const AtlasChange& change : changes) {
            log.push_back(LoggedChange{revision, change});
        }
        // Older changes become "the whole page" for a reader that far behind.
        while (log.size() > options.maxChangeLog) {
            const std::uint64_t oldest = log.front().revision;
            while (!log.empty() && log.front().revision == oldest) {
                log.pop_front();
            }
            logBase = oldest;
        }
    }
};

} // namespace Detail

using namespace Detail;

GlyphAtlas::GlyphAtlas(const AtlasOptions options) : state_(std::make_unique<AtlasState>()) {
    GYO_ASSERT(options.pageSize > 0 && options.pageSize <= 16384);
    GYO_ASSERT(options.padding < options.pageSize / 2);
    state_->options = options;
}

GlyphAtlas::~GlyphAtlas() = default;
GlyphAtlas::GlyphAtlas(GlyphAtlas&&) noexcept = default;
GlyphAtlas& GlyphAtlas::operator=(GlyphAtlas&&) noexcept = default;

std::size_t GlyphAtlas::PageCount() const noexcept { return state_->pages.size(); }

AtlasPageView GlyphAtlas::Page(const std::size_t index) const {
    GYO_ASSERT(index < state_->pages.size());
    const std::uint32_t size = state_->options.pageSize;
    return AtlasPageView{AtlasFormat::Alpha8, size, size, size, state_->pages[index].pixels};
}

std::uint64_t GlyphAtlas::Revision() const noexcept { return state_->revision; }

std::size_t GlyphAtlas::GlyphCount() const noexcept { return state_->placed; }

AtlasChanges GlyphAtlas::ChangesSince(const std::uint64_t revision) const {
    AtlasChanges result;
    result.revision = state_->revision;
    if (revision >= state_->revision) {
        return result;
    }
    if (revision < state_->logBase) {
        for (std::uint32_t p = 0; p < state_->pages.size(); ++p) {
            result.changes.push_back(AtlasChange{p, state_->FullPage()});
        }
        return result;
    }
    for (const LoggedChange& logged : state_->log) {
        if (logged.revision > revision) {
            result.changes.push_back(logged.change);
        }
    }
    return result;
}

RasterResult TextContext::Rasterize(const TextLayout& layout, GlyphAtlas& atlas) {
    AtlasState& atlasState = *atlas.state_;
    if (!atlasState.bound) {
        atlasState.owner = state_->identity;
        atlasState.bound = true;
    }
    // An atlas serves one context: the FontIds in its keys mean nothing to another.
    GYO_ASSERT(!atlasState.owner.owner_before(state_->identity) && !state_->identity.owner_before(atlasState.owner));

    RasterResult result;
    result.placements.reserve(layout.glyphs.size());
    std::vector<AtlasChange> changes;
    GlyphBitmap bitmap;
    const std::uint32_t padding = atlasState.options.padding;
    const std::uint32_t pageSize = atlasState.options.pageSize;
    for (const PositionedGlyph& glyph : layout.glyphs) {
        if (!glyph.visible) {
            result.placements.push_back(GlyphPlacement{});
            continue;
        }
        GYO_ASSERT(glyph.font.value < state_->fonts.size());
        const GlyphKey key{glyph.font.value, glyph.glyph, ToFixed26(glyph.sizePx), 0};
        auto found = atlasState.glyphs.find(key);
        if (found == atlasState.glyphs.end()) {
            GlyphPlacement placement;
            const FontEntry& font = state_->fonts[glyph.font.value];
            if (!state_->freeType.Render(font.freeTypeFace, glyph.glyph, key.size26, bitmap) || bitmap.width == 0 ||
                bitmap.height == 0) {
                placement.status = GlyphRasterStatus::Empty;
            } else if (static_cast<std::uint64_t>(bitmap.width) + 2u * padding > pageSize ||
                       static_cast<std::uint64_t>(bitmap.height) + 2u * padding > pageSize) {
                placement.status = GlyphRasterStatus::TooLarge;
            } else {
                const auto width = static_cast<std::uint32_t>(bitmap.width);
                const auto height = static_cast<std::uint32_t>(bitmap.height);
                std::uint32_t page = 0;
                std::uint32_t x = 0;
                std::uint32_t y = 0;
                atlasState.Allocate(width + 2 * padding, height + 2 * padding, page, x, y, changes);
                placement.page = page;
                placement.rect = AtlasRect{x + padding, y + padding, width, height};
                placement.left = bitmap.left;
                placement.top = bitmap.top;
                placement.status = GlyphRasterStatus::Placed;
                std::vector<std::uint8_t>& pixels = atlasState.pages[page].pixels;
                for (std::uint32_t row = 0; row < height; ++row) {
                    const std::size_t dst = static_cast<std::size_t>(placement.rect.y + row) * pageSize + placement.rect.x;
                    const std::size_t src = static_cast<std::size_t>(row) * width;
                    std::copy_n(bitmap.pixels.begin() + static_cast<std::ptrdiff_t>(src), width,
                                pixels.begin() + static_cast<std::ptrdiff_t>(dst));
                }
                changes.push_back(AtlasChange{page, placement.rect});
                ++atlasState.placed;
                ++result.newGlyphs;
            }
            found = atlasState.glyphs.emplace(key, placement).first;
        }
        if (found->second.status == GlyphRasterStatus::TooLarge) {
            ++result.tooLarge;
        }
        result.placements.push_back(found->second);
    }
    atlasState.Commit(changes);
    return result;
}

} // namespace GYO::Engine::Text
