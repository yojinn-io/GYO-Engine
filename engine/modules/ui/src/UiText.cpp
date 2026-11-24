#include "engine/ui/UiText.hpp"

#include <bit>
#include <span>
#include <utility>

namespace GYO::Engine::Ui {

UiText::UiText(Text::AtlasOptions atlas) : atlas_(atlas) {}

Text::TextResult<void> UiText::AddFont(std::string id, const Text::FontData& data, const std::uint32_t faceIndex) {
    if (fonts_.find(id) != fonts_.end()) return {};
    auto font = context_.AddFont(data, faceIndex);
    if (!font) return Base::Err(std::move(font).error());
    fonts_.emplace(std::move(id), font.value());
    return {};
}

bool UiText::HasFont(const std::string_view id) const {
    return fonts_.find(id) != fonts_.end();
}

const Text::TextLayout* UiText::Layout(const std::string_view fontId, const float sizePx, const std::string_view utf8,
                                       const Text::HorizontalAlign align) {
    const auto font = fonts_.find(fontId);
    if (font == fonts_.end()) return nullptr;
    Key key{std::string(fontId), std::bit_cast<std::uint32_t>(sizePx), align, std::string(utf8)};
    if (const auto found = layouts_.find(key); found != layouts_.end()) {
        found->second.used = true;
        return &found->second.layout;
    }
    const Text::FontId fonts[] = {font->second};
    Text::TextSpan span;
    span.byteBegin = 0;
    span.byteEnd = static_cast<std::uint32_t>(utf8.size());
    span.style.fonts = std::span<const Text::FontId>(fonts);
    span.style.sizePx = sizePx;
    const Text::ShapedText shaped = context_.Shape(utf8, std::span<const Text::TextSpan>(&span, 1));
    Text::LayoutOptions options;
    options.align = align;
    Entry entry{context_.Layout(shaped, options), true};
    const auto [placed, inserted] = layouts_.emplace(std::move(key), std::move(entry));
    (void)inserted;
    return &placed->second.layout;
}

Text::RasterResult UiText::Rasterize(const Text::TextLayout& layout) {
    return context_.Rasterize(layout, atlas_);
}

void UiText::EndFrame() {
    for (auto it = layouts_.begin(); it != layouts_.end();) {
        if (!it->second.used) {
            it = layouts_.erase(it);
        } else {
            it->second.used = false;
            ++it;
        }
    }
}

} // namespace GYO::Engine::Ui
