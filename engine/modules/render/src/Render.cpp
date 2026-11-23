#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "engine/base/Assert.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::Render {

ViewTransform ViewToClip(const Math::Rect view) {
    GYO_ASSERT(view.width != 0.0F && view.height != 0.0F && "a view of zero size");
    // x: view.x -> -1, view.x + width -> 1. y: view.y -> 1, view.y + height -> -1 (y down in the view, up in clip
    // space: the one place up and down turn round).
    const float sx = 2.0F / view.width;
    const float sy = -2.0F / view.height;
    return ViewTransform{{sx, sy}, {-1.0F - view.x * sx, 1.0F - view.y * sy}};
}

Math::Rect ViewOf(const Pass2D& pass, const TargetSize target) {
    if (pass.view) return *pass.view;
    return Math::Rect{0.0F, 0.0F, static_cast<float>(target.width), static_cast<float>(target.height)};
}

namespace {

// The first whole pixel whose centre lies at or past an edge given in pixels, inside 0..limit.
std::uint32_t PixelFrom(const double edge, const std::uint32_t limit) {
    const double first = std::ceil(edge - 0.5);
    if (!(first > 0.0)) return 0;
    if (first >= static_cast<double>(limit)) return limit;
    return static_cast<std::uint32_t>(first);
}

} // namespace

PixelRect ClipToPixels(const std::optional<Math::Rect>& clip, const Math::Rect view, const TargetSize target) {
    if (!clip) return PixelRect{0, 0, target.width, target.height};
    GYO_ASSERT(view.width != 0.0F && view.height != 0.0F && "a view of zero size");
    // View units to target pixels (the view covers the whole target).
    const double sx = static_cast<double>(target.width) / view.width;
    const double sy = static_cast<double>(target.height) / view.height;
    const double left = (static_cast<double>(clip->x) - view.x) * sx;
    const double top = (static_cast<double>(clip->y) - view.y) * sy;
    const double right = (static_cast<double>(clip->x) + clip->width - view.x) * sx;
    const double bottom = (static_cast<double>(clip->y) + clip->height - view.y) * sy;
    const std::uint32_t x0 = PixelFrom(std::min(left, right), target.width);
    const std::uint32_t x1 = PixelFrom(std::max(left, right), target.width);
    const std::uint32_t y0 = PixelFrom(std::min(top, bottom), target.height);
    const std::uint32_t y1 = PixelFrom(std::max(top, bottom), target.height);
    return PixelRect{x0, y0, x1 - x0, y1 - y0};
}

TextureId RenderTextures::Create(const TextureDesc desc, std::vector<std::uint8_t> pixels) {
    GYO_ASSERT(desc.width > 0 && desc.height > 0 && "a texture of zero size");
    GYO_ASSERT(pixels.size() == static_cast<std::size_t>(desc.width) * desc.height * 4 && "texture pixels of another size");
    GYO_ASSERT(next_ != 0 && "every texture id has been given");
    const TextureId id{next_++};
    live_.emplace_back(id.value, desc);
    pending_.emplace_back(TextureCreate{id, desc, std::move(pixels)});
    return id;
}

void RenderTextures::Update(const TextureId id, const TextureRegion region, std::vector<std::uint8_t> pixels) {
    const TextureDesc* desc = Find(id);
    GYO_ASSERT(desc != nullptr && "an update of a texture that is not live");
    GYO_ASSERT(region.width > 0 && region.height > 0 && "an update of zero size");
    GYO_ASSERT(region.x <= desc->width && region.width <= desc->width - region.x && "an update outside the texture");
    GYO_ASSERT(region.y <= desc->height && region.height <= desc->height - region.y && "an update outside the texture");
    GYO_ASSERT(pixels.size() == static_cast<std::size_t>(region.width) * region.height * 4 && "update pixels of another size");
    pending_.emplace_back(TextureUpdate{id, region, std::move(pixels)});
}

void RenderTextures::Release(const TextureId id) {
    const auto it = std::find_if(live_.begin(), live_.end(), [&](const auto& entry) { return entry.first == id.value; });
    GYO_ASSERT(it != live_.end() && "a release of a texture that is not live");
    live_.erase(it);
    pending_.emplace_back(TextureRelease{id});
}

const TextureDesc* RenderTextures::Find(const TextureId id) const noexcept {
    for (const auto& [value, desc] : live_) {
        if (value == id.value) return &desc;
    }
    return nullptr;
}

} // namespace GYO::Engine::Render
