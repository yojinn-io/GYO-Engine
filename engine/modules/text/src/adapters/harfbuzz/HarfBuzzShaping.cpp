#include "adapters/harfbuzz/HarfBuzzShaping.hpp"

#include <hb-ot.h>
#include <hb.h>

#include <map>
#include <new>
#include <utility>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Text::Detail {

namespace {

struct BlobDeleter {
    void operator()(hb_blob_t* blob) const noexcept { hb_blob_destroy(blob); }
};
struct FaceDeleter {
    void operator()(hb_face_t* face) const noexcept { hb_face_destroy(face); }
};
struct FontDeleter {
    void operator()(hb_font_t* font) const noexcept { hb_font_destroy(font); }
};
struct BufferDeleter {
    void operator()(hb_buffer_t* buffer) const noexcept { hb_buffer_destroy(buffer); }
};
using BlobPtr = std::unique_ptr<hb_blob_t, BlobDeleter>;
using FacePtr = std::unique_ptr<hb_face_t, FaceDeleter>;
using FontPtr = std::unique_ptr<hb_font_t, FontDeleter>;
using BufferPtr = std::unique_ptr<hb_buffer_t, BufferDeleter>;

BlobPtr MakeBlob(const std::span<const std::byte> bytes) {
    // Read only, not copied: the caller keeps the bytes alive.
    return BlobPtr(hb_blob_create(reinterpret_cast<const char*>(bytes.data()),
                                  static_cast<unsigned int>(bytes.size()), HB_MEMORY_MODE_READONLY, nullptr,
                                  nullptr));
}

} // namespace

ScriptCode ScriptOf(const char32_t cp) noexcept {
    return static_cast<ScriptCode>(hb_unicode_script(hb_unicode_funcs_get_default(), static_cast<hb_codepoint_t>(cp)));
}

bool IsNeutralScript(const ScriptCode script) noexcept {
    const auto s = static_cast<hb_script_t>(script);
    return s == HB_SCRIPT_COMMON || s == HB_SCRIPT_INHERITED || s == HB_SCRIPT_UNKNOWN;
}

ScriptCode CommonScript() noexcept { return static_cast<ScriptCode>(HB_SCRIPT_COMMON); }

std::uint32_t HarfBuzzFaceCount(const std::span<const std::byte> bytes) {
    const BlobPtr blob = MakeBlob(bytes);
    return hb_face_count(blob.get());
}

struct HarfBuzzFace::Impl {
    FacePtr face;
    // At the face's own units per em: for UnscaledAdvance and HasGlyph only.
    FontPtr unscaled;
};

HarfBuzzFace::HarfBuzzFace(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
HarfBuzzFace::~HarfBuzzFace() = default;

std::unique_ptr<HarfBuzzFace> HarfBuzzFace::Create(const std::span<const std::byte> bytes,
                                                   const std::uint32_t faceIndex) {
    const BlobPtr blob = MakeBlob(bytes);
    if (faceIndex >= hb_face_count(blob.get())) {
        return nullptr;
    }
    FacePtr face(hb_face_create(blob.get(), faceIndex));
    if (face == nullptr || face.get() == hb_face_get_empty()) {
        return nullptr;
    }
    hb_face_make_immutable(face.get());
    FontPtr unscaled(hb_font_create(face.get()));
    hb_font_make_immutable(unscaled.get());
    auto impl = std::make_unique<Impl>();
    impl->face = std::move(face);
    impl->unscaled = std::move(unscaled);
    return std::unique_ptr<HarfBuzzFace>(new HarfBuzzFace(std::move(impl)));
}

HarfBuzzFaceCheck HarfBuzzFace::Check() const {
    HarfBuzzFaceCheck check;
    check.hasGlyphs = hb_face_get_glyph_count(impl_->face.get()) > 0;
    hb_set_t* unicodes = hb_set_create();
    hb_face_collect_unicodes(impl_->face.get(), unicodes);
    check.hasUnicodeCmap = hb_set_get_population(unicodes) > 0;
    hb_set_destroy(unicodes);
    return check;
}

bool HarfBuzzFace::HasGlyph(const char32_t cp) const {
    hb_codepoint_t glyph = 0;
    return hb_font_get_nominal_glyph(impl_->unscaled.get(), static_cast<hb_codepoint_t>(cp), &glyph) != 0;
}

std::uint32_t HarfBuzzFace::UnitsPerEm() const { return hb_face_get_upem(impl_->face.get()); }

std::uint32_t HarfBuzzFace::GlyphCount() const { return hb_face_get_glyph_count(impl_->face.get()); }

std::int32_t HarfBuzzFace::UnscaledAdvance(const std::uint32_t glyph) const {
    return hb_font_get_glyph_h_advance(impl_->unscaled.get(), glyph);
}

struct HarfBuzzShaper::Impl {
    BufferPtr buffer{hb_buffer_create()};
    // The sized fonts made so far, by face and size: made once, kept for the
    // context's life (a face is never removed from a context).
    std::map<std::pair<const HarfBuzzFace*, std::int32_t>, FontPtr> fonts;
    hb_language_t language = hb_language_from_string("und", -1);

    hb_font_t* FontFor(const HarfBuzzFace& face, const hb_face_t* hbFace, const std::int32_t size26) {
        auto& slot = fonts[{&face, size26}];
        if (slot == nullptr) {
            slot.reset(hb_font_create(const_cast<hb_face_t*>(hbFace)));
            hb_font_set_scale(slot.get(), size26, size26);
            hb_font_make_immutable(slot.get());
        }
        return slot.get();
    }
};

HarfBuzzShaper::HarfBuzzShaper() : impl_(std::make_unique<Impl>()) {
    if (!hb_buffer_allocation_successful(impl_->buffer.get())) {
        throw std::bad_alloc();
    }
}
HarfBuzzShaper::~HarfBuzzShaper() = default;
HarfBuzzShaper::HarfBuzzShaper(HarfBuzzShaper&&) noexcept = default;
HarfBuzzShaper& HarfBuzzShaper::operator=(HarfBuzzShaper&&) noexcept = default;

void HarfBuzzShaper::Shape(const HarfBuzzFace& face, const std::int32_t size26, const std::span<const char32_t> text,
                           const std::size_t begin, const std::size_t length, const ScriptCode script,
                           const std::string_view language, std::vector<RawGlyph>& out) {
    GYO_ASSERT(begin + length <= text.size());
    hb_font_t* const font = impl_->FontFor(face, face.impl_->face.get(), size26);
    hb_buffer_t* const buffer = impl_->buffer.get();
    hb_buffer_clear_contents(buffer);
    static_assert(sizeof(char32_t) == sizeof(std::uint32_t));
    hb_buffer_add_codepoints(buffer, reinterpret_cast<const hb_codepoint_t*>(text.data()),
                             static_cast<int>(text.size()), static_cast<unsigned int>(begin),
                             static_cast<int>(length));
    hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, static_cast<hb_script_t>(script));
    // Set explicitly: HarfBuzz would otherwise take the process locale (TX-8).
    hb_buffer_set_language(buffer, language.empty() ? impl_->language
                                                    : hb_language_from_string(language.data(),
                                                                              static_cast<int>(language.size())));
    hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
    hb_buffer_set_invisible_glyph(buffer, kInvisibleGlyph);
    hb_shape(font, buffer, nullptr, 0);
    if (!hb_buffer_allocation_successful(buffer)) {
        throw std::bad_alloc();
    }
    unsigned int count = 0;
    const hb_glyph_info_t* const infos = hb_buffer_get_glyph_infos(buffer, &count);
    const hb_glyph_position_t* const positions = hb_buffer_get_glyph_positions(buffer, nullptr);
    out.reserve(out.size() + count);
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(RawGlyph{infos[i].codepoint, infos[i].cluster, positions[i].x_advance, positions[i].x_offset,
                               positions[i].y_offset,
                               (hb_glyph_info_get_glyph_flags(&infos[i]) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) != 0});
    }
}

LineMetrics HarfBuzzShaper::Metrics(const HarfBuzzFace& face, const std::int32_t size26) {
    hb_font_t* const font = impl_->FontFor(face, face.impl_->face.get(), size26);
    hb_font_extents_t extents{};
    hb_font_get_h_extents(font, &extents);
    return LineMetrics{extents.ascender, -extents.descender, extents.line_gap};
}

} // namespace GYO::Engine::Text::Detail
