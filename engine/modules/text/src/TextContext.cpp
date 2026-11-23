#include "engine/text/TextContext.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <string>

#include "TextInternal.hpp"
#include "Utf8.hpp"
#include "engine/base/Assert.hpp"
#include "unicode/DefaultIgnorable.hpp"

namespace GYO::Engine::Text {

namespace Detail {

bool IsMandatoryBreak(const char32_t cp) noexcept {
    return (cp >= 0x0A && cp <= 0x0D) || cp == 0x85 || cp == 0x2028 || cp == 0x2029;
}

bool IsLineBreakOrTab(const char32_t cp) noexcept { return cp == 0x09 || IsMandatoryBreak(cp); }

bool IsHangingSpace(const char32_t cp) noexcept { return cp == 0x20 || cp == 0x3000 || IsLineBreakOrTab(cp); }

std::int32_t ToFixed26(const float value) {
    GYO_ASSERT(std::isfinite(value));
    const double scaled = std::nearbyint(static_cast<double>(value) * 64.0);
    GYO_ASSERT(std::fabs(scaled) <= static_cast<double>(std::numeric_limits<std::int32_t>::max() / 4));
    return static_cast<std::int32_t>(scaled);
}

namespace {

constexpr float kMaxSizePx = 16384.0f;

std::int32_t FloorDiv2(const std::int32_t value) noexcept {
    return value >= 0 ? value / 2 : -((-value + 1) / 2);
}

bool SameOwner(const std::weak_ptr<ContextIdentity>& owner, const std::shared_ptr<ContextIdentity>& identity) noexcept {
    return !owner.owner_before(identity) && !identity.owner_before(owner);
}

std::uint32_t CodePointAt(const std::vector<std::uint32_t>& offsets, const std::uint32_t byte) {
    const auto it = std::lower_bound(offsets.begin(), offsets.end(), byte);
    // A span boundary falls on a code point (or on the start of an ill-formed
    // subsequence): anything else is a Programmer Error.
    GYO_ASSERT(it != offsets.end() && *it == byte);
    return static_cast<std::uint32_t>(it - offsets.begin());
}

// Turns HarfBuzz's glyphs for the code points [cpBegin, cpEnd) of run `runIndex`
// into Glyphs whose clusters cover [cpBegin, cpEnd) exactly (Design Invariant TX-3).
void AppendGlyphs(const ShapedData& data, const std::uint32_t runIndex, const std::uint32_t cpBegin,
                  const std::uint32_t cpEnd, const std::vector<RawGlyph>& raw, std::vector<Glyph>& out) {
    const auto visibleCluster = [&](const std::uint32_t b, const std::uint32_t e) {
        for (std::uint32_t k = b; k < e; ++k) {
            if (!IsLineBreakOrTab(data.codePoints[k])) {
                return true;
            }
        }
        return false;
    };
    if (raw.empty()) {
        if (cpBegin < cpEnd) {
            out.push_back(Glyph{kInvisibleGlyph, cpBegin, cpEnd, 0, 0, 0, runIndex, false, false});
        }
        return;
    }
    for (std::size_t t = 0; t < raw.size(); ++t) {
        // Clusters are monotone left to right; the first glyph also takes
        // anything HarfBuzz merged away before it.
        const std::uint32_t begin = t == 0 ? cpBegin : std::max(cpBegin, raw[t].cluster);
        std::uint32_t end = cpEnd;
        for (std::size_t u = t + 1; u < raw.size(); ++u) {
            if (raw[u].cluster > raw[t].cluster) {
                end = std::min(cpEnd, raw[u].cluster);
                break;
            }
        }
        std::uint32_t clusterBegin = begin;
        if (t > 0 && raw[t].cluster == raw[t - 1].cluster) {
            clusterBegin = out.back().cpBegin;
        }
        const bool visible = raw[t].glyph != kInvisibleGlyph && visibleCluster(clusterBegin, end);
        out.push_back(Glyph{raw[t].glyph, clusterBegin, end, raw[t].xAdvance, raw[t].xOffset, raw[t].yOffset,
                            runIndex, raw[t].unsafeToBreak, visible});
    }
}

struct Piece {
    std::uint32_t span = 0;
    FontId font;
    ScriptCode script = 0;
    bool neutral = true;
    std::uint32_t cpBegin = 0;
    std::uint32_t cpEnd = 0;
};

} // namespace

} // namespace Detail

using namespace Detail;

std::uint32_t ShapedText::TextBytes() const noexcept { return data_ == nullptr ? 0 : data_->offsets.back(); }

std::uint32_t ShapedText::GlyphCount() const noexcept {
    return data_ == nullptr ? 0 : static_cast<std::uint32_t>(data_->glyphs.size());
}

TextContext::TextContext() : state_(std::make_unique<ContextState>()) {}
TextContext::~TextContext() = default;
TextContext::TextContext(TextContext&&) noexcept = default;
TextContext& TextContext::operator=(TextContext&&) noexcept = default;

TextResult<FontId> TextContext::AddFont(const FontData& data, const std::uint32_t faceIndex) {
    if (faceIndex >= data.FaceCount()) {
        return Base::Err(TextFailure::Make(TextFailureCode::FaceIndexOutOfRange, "no face with this index",
                                           "face " + std::to_string(faceIndex) + " of " +
                                               std::to_string(data.FaceCount())));
    }
    try {
        FontEntry entry{data, HarfBuzzFace::Create(data.Bytes(), faceIndex), 0, {}};
        if (entry.face == nullptr) {
            return Base::Err(TextFailure::Make(TextFailureCode::FaceUnreadable, "HarfBuzz cannot read the face"));
        }
        FreeTypeOpen status = FreeTypeOpen::Unreadable;
        const auto handle = state_->freeType.OpenFace(data.Bytes(), faceIndex, status);
        if (!handle) {
            return Base::Err(TextFailure::Make(status == FreeTypeOpen::OutOfMemory ? TextFailureCode::OutOfMemory
                                                                                   : TextFailureCode::FaceUnreadable,
                                               "FreeType cannot open the face"));
        }
        entry.freeTypeFace = *handle;
        entry.info = FontInfo{state_->freeType.FamilyName(*handle), state_->freeType.StyleName(*handle),
                              entry.face->UnitsPerEm(), entry.face->GlyphCount()};
        state_->fonts.push_back(std::move(entry));
        return FontId{static_cast<std::uint32_t>(state_->fonts.size() - 1)};
    } catch (const std::bad_alloc&) {
        return Base::Err(TextFailure::Make(TextFailureCode::OutOfMemory, "out of memory adding the font"));
    }
}

FontInfo TextContext::Info(const FontId font) const {
    GYO_ASSERT(font.value < state_->fonts.size());
    return state_->fonts[font.value].info;
}

std::uint32_t TextContext::FontCount() const noexcept { return static_cast<std::uint32_t>(state_->fonts.size()); }

ShapedText TextContext::Shape(const std::string_view utf8, const std::span<const TextSpan> spans,
                              const ShapeOptions& options) {
    GYO_ASSERT(utf8.size() < std::numeric_limits<std::uint32_t>::max());
    GYO_ASSERT(!spans.empty());
    auto data = std::make_shared<ShapedData>();
    data->owner = state_->identity;
    data->language = std::string(options.language);
    DecodeUtf8(utf8, data->codePoints, data->offsets);
    const auto count = static_cast<std::uint32_t>(data->codePoints.size());

    // The spans, copied: the caller's font lists are not read again.
    std::uint32_t expected = 0;
    for (const TextSpan& span : spans) {
        GYO_ASSERT(span.byteBegin == expected && span.byteBegin <= span.byteEnd && span.byteEnd <= utf8.size());
        expected = span.byteEnd;
        const TextStyle& style = span.style;
        GYO_ASSERT(!style.fonts.empty());
        for (const FontId font : style.fonts) {
            GYO_ASSERT(font.value < state_->fonts.size());
        }
        GYO_ASSERT(std::isfinite(style.sizePx) && style.sizePx > 0.0f && style.sizePx <= kMaxSizePx);
        GYO_ASSERT(std::isfinite(style.lineHeight.value) && style.lineHeight.value >= 0.0f);
        StoredSpan stored;
        stored.fonts.assign(style.fonts.begin(), style.fonts.end());
        stored.size26 = ToFixed26(style.sizePx);
        stored.lineHeight26 =
            style.lineHeight.kind == LineHeightKind::RelativeToFont
                ? static_cast<std::int32_t>(std::nearbyint(static_cast<double>(style.lineHeight.value) * stored.size26))
                : ToFixed26(style.lineHeight.value);
        stored.cpBegin = CodePointAt(data->offsets, span.byteBegin);
        stored.cpEnd = CodePointAt(data->offsets, span.byteEnd);
        stored.metrics = state_->shaper.Metrics(*state_->fonts[stored.fonts.front().value].face, stored.size26);
        data->spans.push_back(std::move(stored));
    }
    GYO_ASSERT(expected == utf8.size());

    FindLineBreaks(data->codePoints, data->language, options.strictness == LineBreakStrictness::Strict,
                   data->lineBreaks);
    FindGraphemeBoundaries(data->codePoints, data->graphemeEnds);
    data->shapingCodePoints = data->codePoints;
    for (char32_t& cp : data->shapingCodePoints) {
        if (IsLineBreakOrTab(cp)) {
            cp = U' ';
        }
    }

    // Itemize: per span, per grapheme, the first font in the list that has a
    // glyph for every character of the grapheme (default-ignorable characters
    // skipped), else the primary font (TX-5); then the script.
    std::vector<Piece> pieces;
    for (std::uint32_t s = 0; s < data->spans.size(); ++s) {
        const StoredSpan& span = data->spans[s];
        std::uint32_t i = span.cpBegin;
        while (i < span.cpEnd) {
            std::uint32_t j = i + 1;
            while (j < span.cpEnd && data->graphemeEnds[j - 1] == 0) {
                ++j;
            }
            FontId chosen = span.fonts.front();
            for (const FontId font : span.fonts) {
                const HarfBuzzFace& face = *state_->fonts[font.value].face;
                bool covers = true;
                for (std::uint32_t k = i; k < j && covers; ++k) {
                    if (!IsDefaultIgnorable(data->codePoints[k])) {
                        covers = face.HasGlyph(data->shapingCodePoints[k]);
                    }
                }
                if (covers) {
                    chosen = font;
                    break;
                }
            }
            Piece piece{s, chosen, CommonScript(), true, i, j};
            for (std::uint32_t k = i; k < j; ++k) {
                const ScriptCode script = ScriptOf(data->codePoints[k]);
                if (!IsNeutralScript(script)) {
                    piece.script = script;
                    piece.neutral = false;
                    break;
                }
            }
            pieces.push_back(piece);
            i = j;
        }
    }
    // Common and Inherited take the script before them, or after them at the
    // start of the text.
    std::optional<ScriptCode> last;
    for (Piece& piece : pieces) {
        if (!piece.neutral) {
            last = piece.script;
        } else if (last) {
            piece.script = *last;
        }
    }
    const auto firstStrong =
        std::find_if(pieces.begin(), pieces.end(), [](const Piece& piece) { return !piece.neutral; });
    if (firstStrong != pieces.end()) {
        for (auto it = pieces.begin(); it != firstStrong; ++it) {
            it->script = firstStrong->script;
        }
    }
    for (const Piece& piece : pieces) {
        if (!data->runs.empty()) {
            Run& back = data->runs.back();
            if (back.span == piece.span && back.font == piece.font && back.script == piece.script &&
                back.cpEnd == piece.cpBegin) {
                back.cpEnd = piece.cpEnd;
                continue;
            }
        }
        data->runs.push_back(Run{piece.span, piece.font, piece.script, piece.cpBegin, piece.cpEnd, {}});
    }

    std::vector<RawGlyph> raw;
    for (std::uint32_t r = 0; r < data->runs.size(); ++r) {
        Run& run = data->runs[r];
        const StoredSpan& span = data->spans[run.span];
        const HarfBuzzFace& face = *state_->fonts[run.font.value].face;
        run.metrics = state_->shaper.Metrics(face, span.size26);
        raw.clear();
        state_->shaper.Shape(face, span.size26, data->shapingCodePoints, run.cpBegin, run.cpEnd - run.cpBegin,
                             run.script, data->language, raw);
        AppendGlyphs(*data, r, run.cpBegin, run.cpEnd, raw, data->glyphs);
    }
    GYO_ASSERT(count == 0 || !data->glyphs.empty());
    return ShapedText(std::move(data));
}

TextLayout TextContext::Layout(const ShapedText& shaped, const LayoutOptions& options) {
    GYO_ASSERT(shaped.data_ != nullptr);
    const ShapedData& data = *shaped.data_;
    GYO_ASSERT(SameOwner(data.owner, state_->identity));
    std::optional<std::int32_t> maxWidth;
    if (options.maxWidth) {
        GYO_ASSERT(*options.maxWidth >= 0.0f);
        maxWidth = ToFixed26(*options.maxWidth);
    }
    const std::vector<Glyph>& glyphs = data.glyphs;
    const auto glyphCount = static_cast<std::uint32_t>(glyphs.size());
    const auto cpCount = static_cast<std::uint32_t>(data.codePoints.size());

    const auto firstOfCluster = [&](const std::uint32_t g) {
        return g == 0 || glyphs[g - 1].cpBegin != glyphs[g].cpBegin;
    };
    const auto breakBefore = [&](const std::uint32_t g) {
        if (!firstOfCluster(g) || glyphs[g].cpBegin == 0) {
            return LineBreakAfter::None;
        }
        return data.lineBreaks[glyphs[g].cpBegin - 1];
    };
    const auto hangs = [&](const std::uint32_t g) { return IsHangingSpace(data.codePoints[glyphs[g].cpBegin]); };

    // 1. Where the lines end, in the shaped glyphs.
    struct LineRange {
        std::uint32_t glyphBegin;
        std::uint32_t glyphEnd;
        bool hard;
    };
    std::vector<LineRange> ranges;
    std::uint32_t lineStart = 0;
    std::int64_t width = 0;
    std::optional<std::uint32_t> lastBreak;
    for (std::uint32_t g = 0; g < glyphCount; ++g) {
        if (g > lineStart) {
            const LineBreakAfter kind = breakBefore(g);
            if (kind == LineBreakAfter::Mandatory) {
                ranges.push_back({lineStart, g, true});
                lineStart = g;
                width = 0;
                lastBreak.reset();
            } else if (kind == LineBreakAfter::Allowed) {
                lastBreak = g;
            }
        }
        width += glyphs[g].advance;
        if (maxWidth && !hangs(g) && width > *maxWidth && lastBreak && *lastBreak > lineStart) {
            ranges.push_back({lineStart, *lastBreak, false});
            lineStart = *lastBreak;
            width = 0;
            for (std::uint32_t k = lineStart; k <= g; ++k) {
                width += glyphs[k].advance;
            }
            lastBreak.reset();
        }
    }
    ranges.push_back({lineStart, glyphCount, false});
    if (cpCount > 0 && IsMandatoryBreak(data.codePoints.back())) {
        ranges.back().hard = true;
        ranges.push_back({glyphCount, glyphCount, false});
    }

    // 2. The glyphs of each line; a run cut where HarfBuzz says breaking is
    // unsafe is shaped again on each side (HarfBuzz's recommended practice).
    const auto lineCpBegin = [&](const LineRange& range) {
        return range.glyphBegin < glyphCount ? glyphs[range.glyphBegin].cpBegin : cpCount;
    };
    const auto lineCpEnd = [&](const LineRange& range) {
        return range.glyphEnd < glyphCount ? glyphs[range.glyphEnd].cpBegin : cpCount;
    };
    // The first glyph of the cluster starting at cp (glyphs are in text order).
    const auto unsafeAt = [&](const std::uint32_t cp) {
        const auto it = std::lower_bound(glyphs.begin(), glyphs.end(), cp,
                                         [](const Glyph& glyph, const std::uint32_t value) { return glyph.cpBegin < value; });
        return it != glyphs.end() && it->cpBegin == cp && it->unsafeToBreak;
    };
    std::vector<std::vector<Glyph>> lineGlyphs(ranges.size());
    std::vector<RawGlyph> raw;
    for (std::size_t l = 0; l < ranges.size(); ++l) {
        const std::uint32_t lb = lineCpBegin(ranges[l]);
        const std::uint32_t le = lineCpEnd(ranges[l]);
        for (std::uint32_t r = 0; r < data.runs.size(); ++r) {
            const Run& run = data.runs[r];
            const std::uint32_t sb = std::max(run.cpBegin, lb);
            const std::uint32_t se = std::min(run.cpEnd, le);
            if (sb >= se) {
                continue;
            }
            const bool cutUnsafe = (sb > run.cpBegin && unsafeAt(sb)) || (se < run.cpEnd && unsafeAt(se));
            if (!cutUnsafe) {
                for (std::uint32_t g = ranges[l].glyphBegin; g < ranges[l].glyphEnd; ++g) {
                    if (glyphs[g].run == r) {
                        lineGlyphs[l].push_back(glyphs[g]);
                    }
                }
                continue;
            }
            raw.clear();
            state_->shaper.Shape(*state_->fonts[run.font.value].face, data.spans[run.span].size26,
                                 data.shapingCodePoints, sb, se - sb, run.script, data.language, raw);
            AppendGlyphs(data, r, sb, se, raw, lineGlyphs[l]);
        }
    }

    // 3. Line metrics, alignment and positions, in 26.6.
    struct LineBox {
        std::int64_t width = 0;
        std::int32_t ascent = 0;
        std::int32_t descent = 0;
    };
    std::vector<LineBox> boxes(ranges.size());
    std::int64_t widest = 0;
    for (std::size_t l = 0; l < ranges.size(); ++l) {
        const std::vector<Glyph>& line = lineGlyphs[l];
        std::size_t end = line.size();
        while (end > 0 && IsHangingSpace(data.codePoints[line[end - 1].cpBegin])) {
            --end;
        }
        for (std::size_t k = 0; k < end; ++k) {
            boxes[l].width += line[k].advance;
        }
        widest = std::max(widest, boxes[l].width);
        const auto add = [&](const LineMetrics& m, const std::int32_t lineHeight) {
            const std::int32_t leading = lineHeight - (m.ascender + m.descender);
            const std::int32_t halfTop = FloorDiv2(leading);
            boxes[l].ascent = std::max(boxes[l].ascent, m.ascender + halfTop);
            boxes[l].descent = std::max(boxes[l].descent, m.descender + (leading - halfTop));
        };
        if (line.empty()) {
            const StoredSpan& span = cpCount == 0 ? data.spans.front() : data.spans.back();
            add(span.metrics, span.lineHeight26);
        } else {
            std::uint32_t lastRun = std::numeric_limits<std::uint32_t>::max();
            for (const Glyph& glyph : line) {
                if (glyph.run != lastRun) {
                    lastRun = glyph.run;
                    const Run& run = data.runs[glyph.run];
                    add(run.metrics, data.spans[run.span].lineHeight26);
                }
            }
        }
    }
    const std::int64_t basis = maxWidth ? *maxWidth : widest;

    TextLayout layout;
    layout.textBytes = data.offsets.back();
    std::int64_t top = 0;
    for (std::size_t l = 0; l < ranges.size(); ++l) {
        const LineBox& box = boxes[l];
        std::int64_t x = 0;
        if (options.align == HorizontalAlign::Center) {
            const std::int64_t room = basis - box.width;
            x = room >= 0 ? room / 2 : -((-room + 1) / 2);
        } else if (options.align == HorizontalAlign::Right) {
            x = basis - box.width;
        }
        const std::int64_t baseline = top + box.ascent;
        LayoutLine line;
        line.top = FromFixed26(top);
        line.baseline = FromFixed26(baseline);
        line.height = FromFixed26(static_cast<std::int64_t>(box.ascent) + box.descent);
        line.width = FromFixed26(box.width);
        line.x = FromFixed26(x);
        line.glyphBegin = static_cast<std::uint32_t>(layout.glyphs.size());
        line.byteBegin = data.offsets[lineCpBegin(ranges[l])];
        line.byteEnd = data.offsets[lineCpEnd(ranges[l])];
        line.hardBreak = ranges[l].hard;
        std::int64_t pen = x;
        for (const Glyph& glyph : lineGlyphs[l]) {
            const Run& run = data.runs[glyph.run];
            PositionedGlyph out;
            out.font = run.font;
            out.glyph = glyph.glyph;
            out.x = FromFixed26(pen + glyph.xOffset);
            out.y = FromFixed26(baseline - glyph.yOffset);
            out.penX = FromFixed26(pen);
            out.advance = FromFixed26(glyph.advance);
            out.sizePx = FromFixed26(data.spans[run.span].size26);
            out.byteBegin = data.offsets[glyph.cpBegin];
            out.byteEnd = data.offsets[glyph.cpEnd];
            out.span = run.span;
            out.line = static_cast<std::uint32_t>(l);
            out.visible = glyph.visible;
            layout.glyphs.push_back(out);
            pen += glyph.advance;
        }
        line.glyphEnd = static_cast<std::uint32_t>(layout.glyphs.size());
        layout.lines.push_back(line);
        top += static_cast<std::int64_t>(box.ascent) + box.descent;
    }
    layout.width = FromFixed26(widest);
    layout.height = FromFixed26(top);
    layout.firstBaseline = layout.lines.front().baseline;
    layout.graphemeBoundaries.push_back(0);
    for (std::uint32_t i = 0; i < cpCount; ++i) {
        if (data.graphemeEnds[i] != 0) {
            layout.graphemeBoundaries.push_back(data.offsets[i + 1]);
        }
    }
    return layout;
}

} // namespace GYO::Engine::Text
