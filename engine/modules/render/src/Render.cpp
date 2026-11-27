#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "engine/base/Assert.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderMeshes.hpp"
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

namespace {

// A pass's viewport cut to the target (Pass2D and Pass3D alike).
template <class Pass>
PixelRect CutViewport(const Pass& pass, const TargetSize target) {
    if (!pass.viewport) return PixelRect{0, 0, target.width, target.height};
    const PixelRect& v = *pass.viewport;
    const std::uint32_t x = std::min(v.x, target.width);
    const std::uint32_t y = std::min(v.y, target.height);
    const std::uint32_t width = std::min(v.width, target.width - x);
    const std::uint32_t height = std::min(v.height, target.height - y);
    return PixelRect{x, y, width, height};
}

} // namespace

PixelRect ViewportOf(const Pass2D& pass, const TargetSize target) { return CutViewport(pass, target); }

PixelRect ViewportOf(const Pass3D& pass, const TargetSize target) { return CutViewport(pass, target); }

Math::Rect ViewOf(const Pass2D& pass, const TargetSize target) {
    if (pass.view) return *pass.view;
    const PixelRect viewport = ViewportOf(pass, target);
    return Math::Rect{0.0F, 0.0F, static_cast<float>(viewport.width), static_cast<float>(viewport.height)};
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

PixelRect ClipToPixels(const std::optional<Math::Rect>& clip, const Math::Rect view, const PixelRect viewport) {
    if (!clip) return viewport;
    GYO_ASSERT(view.width != 0.0F && view.height != 0.0F && "a view of zero size");
    // View units to pixels of the viewport (the view covers the whole viewport), then to the target.
    const double sx = static_cast<double>(viewport.width) / view.width;
    const double sy = static_cast<double>(viewport.height) / view.height;
    const double left = (static_cast<double>(clip->x) - view.x) * sx;
    const double top = (static_cast<double>(clip->y) - view.y) * sy;
    const double right = (static_cast<double>(clip->x) + clip->width - view.x) * sx;
    const double bottom = (static_cast<double>(clip->y) + clip->height - view.y) * sy;
    const std::uint32_t x0 = PixelFrom(std::min(left, right), viewport.width);
    const std::uint32_t x1 = PixelFrom(std::max(left, right), viewport.width);
    const std::uint32_t y0 = PixelFrom(std::min(top, bottom), viewport.height);
    const std::uint32_t y1 = PixelFrom(std::max(top, bottom), viewport.height);
    return PixelRect{viewport.x + x0, viewport.y + y0, x1 - x0, y1 - y0};
}

PixelRect ClipToPixels(const std::optional<Math::Rect>& clip, const Math::Rect view, const TargetSize target) {
    return ClipToPixels(clip, view, PixelRect{0, 0, target.width, target.height});
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
    const auto same = [&](const auto& entry) { return entry.first == id.value; };
    if (const auto it = std::find_if(live_.begin(), live_.end(), same); it != live_.end()) {
        live_.erase(it);
    } else {
        const auto target = std::find_if(targets_.begin(), targets_.end(), same);
        GYO_ASSERT(target != targets_.end() && "a release of a texture that is not live");
        targets_.erase(target);
    }
    pending_.emplace_back(TextureRelease{id});
}

TextureId RenderTextures::CreateTarget(const RenderTargetDesc desc) {
    GYO_ASSERT((!desc.size || (desc.size->width > 0 && desc.size->height > 0)) && "a target of zero size");
    // One counter for textures and targets: a draw names either by the same kind of id (RD-3).
    GYO_ASSERT(next_ != 0 && "every texture id has been given");
    const TextureId id{next_++};
    targets_.emplace_back(id.value, desc);
    pending_.emplace_back(TextureTargetCreate{id, desc});
    return id;
}

const TextureDesc* RenderTextures::Find(const TextureId id) const noexcept {
    for (const auto& [value, desc] : live_) {
        if (value == id.value) return &desc;
    }
    return nullptr;
}

const RenderTargetDesc* RenderTextures::FindTarget(const TextureId id) const noexcept {
    for (const auto& [value, desc] : targets_) {
        if (value == id.value) return &desc;
    }
    return nullptr;
}

MeshId RenderMeshes::Create(std::vector<Vertex3D> vertices, std::vector<std::uint32_t> indices) {
    GYO_ASSERT(!vertices.empty() && !indices.empty() && "a mesh without vertices or indices");
    GYO_ASSERT(indices.size() % 3 == 0 && "a mesh's indices are not whole triangles");
    GYO_ASSERT(next_ != 0 && "every mesh id has been given");
    const MeshId id{next_++};
    live_.push_back(id.value);
    pending_.emplace_back(MeshCreate{id, std::move(vertices), std::move(indices)});
    return id;
}

void RenderMeshes::Release(const MeshId id) {
    const auto it = std::find(live_.begin(), live_.end(), id.value);
    GYO_ASSERT(it != live_.end() && "a release of a mesh that is not live");
    live_.erase(it);
    pending_.emplace_back(MeshRelease{id});
}

bool RenderMeshes::Live(const MeshId id) const noexcept {
    return std::find(live_.begin(), live_.end(), id.value) != live_.end();
}

} // namespace GYO::Engine::Render
