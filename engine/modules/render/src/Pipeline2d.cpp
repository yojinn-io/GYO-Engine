#include "engine/render/Pipeline2d.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <utility>

#include "engine/math/linear/Vec4.hpp"

namespace GYO::Engine::Render {

ResolvedImage ResolveImage(const ImageRef& image, const TextureFilter filter, const WorldAsset::AssetView* view) {
    ResolvedImage out;
    out.key.filter = filter;
    if (const auto* handle = std::get_if<Asset::Handle<Image::ImageData>>(&image)) {
        out.key.asset = handle->Id().Text();
        // The life tells a reloaded asset (a new picture under the same id) from the one already made: the same
        // reading AssetView makes (asset's Detail::HandleAccess, the accessor for the World end of asset).
        out.key.life = Asset::Detail::HandleAccess::Life(*handle);
        if (out.key.asset.empty()) {
            out.key.asset = "?";  // a Handle that names nothing: never loaded, so the drawable is skipped
            return out;
        }
        if (view != nullptr) out.data = view->Get(*handle);
    } else if (const auto* held = std::get_if<std::shared_ptr<const Image::ImageData>>(&image)) {
        if (*held == nullptr) {
            out.key.asset = "?";
            return out;
        }
        out.key.held = held->get();
        out.data = held->get();
        out.holder = *held;
    }
    return out;
}

std::optional<TextureId> RenderImages::Use(const ImageKey& key, const Image::ImageData* data,
                                           std::shared_ptr<const Image::ImageData> holder, RenderTextures& textures,
                                           Math::Vec2& size) {
    if (key.asset.empty() && key.held == nullptr) {
        size = Math::Vec2{1.0F, 1.0F};
        return TextureId{};
    }
    auto found = entries_.find(key);
    if (found == entries_.end()) {
        if (data == nullptr || data->Width() == 0 || data->Height() == 0) return std::nullopt;
        // An asset requested again after all its Handles went has a new life: the old life's picture can never be
        // shown again, so its texture goes now (Bevy lets an image's GPU texture go when the asset is removed).
        if (!key.asset.empty()) {
            for (auto it = entries_.begin(); it != entries_.end();) {
                if (it->first.asset == key.asset && it->first.life != key.life) {
                    textures.Release(it->second.texture);
                    it = entries_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        const std::span<const std::uint8_t> pixels = data->Pixels();
        const TextureId id = textures.Create(TextureDesc{data->Width(), data->Height(), key.filter},
                                             std::vector<std::uint8_t>(pixels.begin(), pixels.end()));
        Entry entry{id, Math::Vec2{static_cast<float>(data->Width()), static_cast<float>(data->Height())}, std::move(holder), false};
        found = entries_.emplace(key, std::move(entry)).first;
    }
    found->second.used = true;
    size = found->second.size;
    return found->second.texture;
}

void RenderImages::EndFrame(RenderTextures& textures) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        Entry& entry = it->second;
        if (it->first.held != nullptr && !entry.used) {
            textures.Release(entry.texture);
            it = entries_.erase(it);
            continue;
        }
        entry.used = false;
        ++it;
    }
}

void SpriteGeometry(const Sprite& sprite, const Math::Vec2 pictureSize, std::vector<Vertex2D>& vertices,
                    std::vector<std::uint32_t>& indices) {
    Math::Rect region{0.0F, 0.0F, pictureSize.x, pictureSize.y};
    if (sprite.rect) region = *sprite.rect;
    const Math::Vec2 size = sprite.customSize.value_or(Math::Vec2{region.width, region.height});
    float u0 = region.x / pictureSize.x;
    float u1 = (region.x + region.width) / pictureSize.x;
    float v0 = region.y / pictureSize.y;
    float v1 = (region.y + region.height) / pictureSize.y;
    if (sprite.flipX) std::swap(u0, u1);
    if (sprite.flipY) std::swap(v0, v1);
    const float left = (-0.5F - sprite.anchor.x) * size.x;
    const float bottom = (-0.5F - sprite.anchor.y) * size.y;
    const float right = left + size.x;
    const float top = bottom + size.y;
    const auto base = static_cast<std::uint32_t>(vertices.size());
    // y up: the picture's top row (v0) at the top edge.
    vertices.push_back(Vertex2D{{left, top}, {u0, v0}, sprite.color});
    vertices.push_back(Vertex2D{{right, top}, {u1, v0}, sprite.color});
    vertices.push_back(Vertex2D{{right, bottom}, {u1, v1}, sprite.color});
    vertices.push_back(Vertex2D{{left, bottom}, {u0, v1}, sprite.color});
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) indices.push_back(base + i);
}

std::vector<std::uint32_t> TriangleListOf(const Shape2d& shape) {
    std::vector<std::uint32_t> order = shape.indices;
    if (order.empty()) {
        order.resize(shape.vertices.size());
        std::iota(order.begin(), order.end(), 0U);
    }
    std::vector<std::uint32_t> list;
    if (shape.topology == Topology::TriangleList) {
        list.assign(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(order.size() - order.size() % 3));
        return list;
    }
    if (order.size() < 3) return list;
    list.reserve((order.size() - 2) * 3);
    for (std::size_t i = 2; i < order.size(); ++i) {
        if (i % 2 == 0) {
            list.insert(list.end(), {order[i - 2], order[i - 1], order[i]});
        } else {
            list.insert(list.end(), {order[i - 1], order[i - 2], order[i]});
        }
    }
    return list;
}

std::optional<Pass2D> Queue2d(const ExtractedCamera& camera, const std::span<const Extracted2dItem> items) {
    if (!camera.is2d || !camera.computed.valid) return std::nullopt;
    const std::optional<Math::Matrix4> viewFromWorld = Math::Inverse(camera.view);
    if (!viewFromWorld) return std::nullopt;
    const Math::Matrix4 clipFromWorld = Math::Multiply(camera.computed.clipFromView, *viewFromWorld);
    const Camera::Viewport& vp = camera.computed.viewport;
    const float width = static_cast<float>(vp.size.x);
    const float height = static_cast<float>(vp.size.y);

    Pass2D pass;
    pass.target = camera.target;
    pass.viewport = PixelRect{static_cast<std::uint32_t>(vp.position.x), static_cast<std::uint32_t>(vp.position.y),
                              static_cast<std::uint32_t>(vp.size.x), static_cast<std::uint32_t>(vp.size.y)};
    // Back to front: the z of each item's position, ties by source entity.
    std::vector<std::size_t> order;
    order.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        if ((items[i].layers & camera.layers) == 0 || items[i].indices.empty()) continue;
        order.push_back(i);
    }
    const auto zOf = [&](std::size_t i) { return items[i].world.values[14]; };
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const float za = zOf(a);
        const float zb = zOf(b);
        if (za != zb) return za < zb;
        if (items[a].entity != items[b].entity) return items[a].entity < items[b].entity;
        return a < b;  // a sprite and a shape of one entity: in extract order, the same on every platform
    });

    for (const std::size_t i : order) {
        const Extracted2dItem& item = items[i];
        const Math::Matrix4 clipFromLocal = Math::Multiply(clipFromWorld, item.world);
        // The item's position against the near and far planes (clip depth 0 to 1, math.md).
        const Math::Vec4 origin = Math::Multiply(clipFromLocal, Math::Vec4{0.0F, 0.0F, 0.0F, 1.0F});
        const float depth = origin.z / origin.w;
        if (!(depth >= 0.0F && depth <= 1.0F)) continue;
        const auto base = static_cast<std::uint32_t>(pass.vertices.size());
        for (const Vertex2D& v : item.vertices) {
            const Math::Vec4 clip = Math::Multiply(clipFromLocal, Math::Vec4{v.position.x, v.position.y, 0.0F, 1.0F});
            const float x = clip.x / clip.w;
            const float y = clip.y / clip.w;
            // Clip space (y up, -1 to 1) to pixels of the viewport (y down from its top-left).
            pass.vertices.push_back(Vertex2D{{(x + 1.0F) * 0.5F * width, (1.0F - y) * 0.5F * height}, v.uv, v.color});
        }
        const auto first = static_cast<std::uint32_t>(pass.indices.size());
        for (const std::uint32_t index : item.indices) pass.indices.push_back(base + index);
        const auto count = static_cast<std::uint32_t>(item.indices.size());
        if (!pass.draws.empty()) {
            Draw2D& last = pass.draws.back();
            if (last.texture == item.texture && last.blend == item.blend && last.indexOffset + last.indexCount == first) {
                last.indexCount += count;
                continue;
            }
        }
        pass.draws.push_back(Draw2D{item.texture, item.blend, std::nullopt, first, count, 0});
    }
    return pass;
}

} // namespace GYO::Engine::Render
