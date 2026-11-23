#pragma once

#include <cstdint>
#include <memory>

// A shaped text (TextContext::Shape): fonts chosen, glyphs and their clusters,
// the line break opportunities and the grapheme boundaries. Immutable; copies
// share it. TextContext::Layout breaks it into lines for any width without
// shaping it again (Bevy: shape once, break many times). Only the context that
// made it may lay it out.
namespace GYO::Engine::Text {

namespace Detail {
struct ShapedData;
} // namespace Detail

class ShapedText final {
public:
    ShapedText() = default;

    [[nodiscard]] std::uint32_t TextBytes() const noexcept;
    [[nodiscard]] std::uint32_t GlyphCount() const noexcept;
    [[nodiscard]] bool Empty() const noexcept { return data_ == nullptr; }

private:
    friend class TextContext;
    explicit ShapedText(std::shared_ptr<const Detail::ShapedData> data) noexcept : data_(std::move(data)) {}

    std::shared_ptr<const Detail::ShapedData> data_;
};

} // namespace GYO::Engine::Text
