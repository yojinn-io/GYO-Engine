#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

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

PixelRect ViewportOf(const MeshGeometry& pass, const TargetSize target) { return CutViewport(pass, target); }

PixelRect ViewportOf(const PassFullscreen& pass, const TargetSize target) { return CutViewport(pass, target); }

namespace {

template <class... F>
struct Overloaded : F... {
    using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

template <class Draws>
void AppendTextures(const Draws& draws, std::vector<TextureId>& out) {
    for (const auto& draw : draws) {
        if (draw.texture != TextureId{}) out.push_back(draw.texture);
    }
}

} // namespace

PassWrites WritesOf(const RenderPass& pass) noexcept {
    return std::visit(Overloaded{
                          [](const Pass2D& p) { return PassWrites{true, p.target, std::nullopt}; },
                          [](const Pass3D& p) { return PassWrites{true, p.target, p.depth}; },
                          [](const PassFullscreen& p) { return PassWrites{true, p.target, std::nullopt}; },
                          [](const PassDepth& p) { return PassWrites{false, std::nullopt, p.target}; },
                      },
                      pass);
}

std::optional<TextureId> TargetOf(const RenderPass& pass) noexcept {
    const PassWrites writes = WritesOf(pass);
    return writes.colour ? writes.colourTarget : writes.depthTarget;
}

std::vector<TextureId> ReadsOf(const RenderPass& pass) {
    std::vector<TextureId> reads;
    std::visit(Overloaded{
                   [&](const Pass2D& p) { AppendTextures(p.draws, reads); },
                   [&](const Pass3D& p) { AppendTextures(p.draws, reads); },
                   [&](const PassFullscreen& p) { reads = p.inputs; },
                   // A depth-only pass's draws have no texture.
                   [](const PassDepth&) {},
               },
               pass);
    return reads;
}

std::optional<UnwrittenRead> FirstUnwrittenRead(const RenderCommands& commands, const RenderTextures& textures) {
    std::vector<TextureId> written;
    for (const TextureId id : commands.retained) {
        if (textures.WasDrawn(id)) written.push_back(id);
    }
    for (std::size_t i = 0; i < commands.passes.size(); ++i) {
        const PassWrites writes = WritesOf(commands.passes[i]);
        for (const TextureId read : ReadsOf(commands.passes[i])) {
            if (writes.Writes(read) || textures.FindTarget(read) == nullptr) continue;
            if (std::find(written.begin(), written.end(), read) == written.end()) return UnwrittenRead{i, read};
        }
        if (writes.colour && writes.colourTarget) written.push_back(*writes.colourTarget);
        if (writes.depthTarget) written.push_back(*writes.depthTarget);
    }
    return std::nullopt;
}

void RenderFrameReceipt::RecordDrawn(const TextureId target) {
    if (std::find(drawn_.begin(), drawn_.end(), target) == drawn_.end()) drawn_.push_back(target);
}

void RenderFrameReceipt::RecordApplied(const TextureId target, const TargetSize size) {
    const auto it = std::find_if(applied_.begin(), applied_.end(), [&](const TextureTargetSize& a) { return a.id == target; });
    if (it != applied_.end()) {
        it->size = size;
    } else {
        applied_.push_back(TextureTargetSize{target, size});
    }
}

void RenderFrameReceipt::Clear() noexcept {
    drawn_.clear();
    applied_.clear();
}

std::optional<std::size_t> FirstDepthOfAnotherSize(const RenderCommands& commands, const RenderTextures& textures) {
    for (std::size_t i = 0; i < commands.passes.size(); ++i) {
        const auto* pass = std::get_if<Pass3D>(&commands.passes[i]);
        if (pass == nullptr || !pass->depth) continue;
        const std::optional<TargetSize> depth = textures.SizeOf(*pass->depth);
        const std::optional<TargetSize> colour = pass->target ? textures.SizeOf(*pass->target) : textures.MainSize();
        if (depth && colour && depth != colour) return i;
    }
    return std::nullopt;
}

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
    GYO_ASSERT(desc != nullptr && "an update of a texture that is not live or is a target (it has no pixels)");
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
    std::erase_if(targets_, [&](const TargetState& t) { return t.id == id.value; });
}

TextureId RenderTextures::CreateTarget(const RenderTargetDesc desc) {
    if (const auto* fixed = std::get_if<TargetSize>(&desc.size)) {
        GYO_ASSERT(fixed->width > 0 && fixed->height > 0 && "a target of zero size");
    } else if (const std::optional<TextureId>& followed = std::get<FollowTarget>(desc.size).target) {
        GYO_ASSERT(FindTarget(*followed) != nullptr && "a target following an id that is not a live target");
    }
    GYO_ASSERT(next_ != 0 && "every texture id has been given");
    const TextureId id{next_++};
    live_.emplace_back(id.value, desc);
    pending_.emplace_back(TextureTargetCreate{id, desc});
    targets_.push_back(TargetState{id.value});
    ResolveSizes();
    return id;
}

void RenderTextures::SetMainSize(const std::optional<TargetSize> size) {
    main_ = size;
    ResolveSizes();
}

void RenderTextures::ResolveSizes() {
    for (TargetState& t : targets_) {
        const RenderTargetDesc* desc = FindTarget(TextureId{t.id});
        std::optional<TargetSize> wanted;
        if (const auto* fixed = std::get_if<TargetSize>(&desc->size)) {
            wanted = *fixed;
        } else if (const std::optional<TextureId>& followed = std::get<FollowTarget>(desc->size).target) {
            wanted = SizeOf(*followed);
        } else {
            wanted = main_;
        }
        if (wanted && t.size != wanted) {
            t.size = wanted;
            t.applied.reset();
            t.drawn = false;
            t.picture = false;
            t.asked = false;
        }
        if (!t.size || t.applied == t.size || t.asked) continue;
        t.asked = true;
        if (t.unanswered) {
            // Not answered (no command buffer took the frame's asks): its making may be lost too. Making a target the
            // backend has is nothing to do there (ids are never given twice).
            pending_.emplace_back(TextureTargetCreate{TextureId{t.id}, *desc});
            t.unanswered = false;
        }
        pending_.emplace_back(TextureTargetSize{TextureId{t.id}, *t.size});
    }
}

void RenderTextures::Apply(const RenderFrameReceipt& receipt) {
    const std::span<const TextureId> drawnTargets = receipt.DrawnTargets();
    for (TargetState& t : targets_) {
        for (const TextureTargetSize& sized : receipt.AppliedSizes()) {
            if (sized.id.value == t.id) t.applied = sized.size;
        }
        const bool drawn = std::find(drawnTargets.begin(), drawnTargets.end(), TextureId{t.id}) != drawnTargets.end();
        if (drawn && t.applied && t.applied == t.size) t.picture = true;
    }
}

void RenderTextures::ClearPending() {
    pending_.clear();
    for (TargetState& t : targets_) {
        t.unanswered = t.asked && t.applied != t.size;
        t.asked = false;
    }
    ResolveSizes();
}

void RenderTextures::EndFrame(const RenderCommands& commands, const RenderFrameReceipt& receipt) {
    for (const RenderPass& pass : commands.passes) {
        const PassWrites writes = WritesOf(pass);
        for (const std::optional<TextureId>& on : {writes.colour ? writes.colourTarget : std::nullopt, writes.depthTarget}) {
            if (!on) continue;
            for (TargetState& t : targets_) {
                if (t.id == on->value && t.size) t.drawn = true;
            }
        }
    }
    Apply(receipt);
    ClearPending();
}

std::optional<TargetSize> RenderTextures::SizeOf(const TextureId id) const noexcept {
    for (const TargetState& t : targets_) {
        if (t.id == id.value) return t.size;
    }
    return std::nullopt;
}

bool RenderTextures::WasDrawn(const TextureId id) const noexcept {
    for (const TargetState& t : targets_) {
        if (t.id == id.value) return t.drawn;
    }
    return false;
}

bool RenderTextures::HasPicture(const TextureId id) const noexcept {
    for (const TargetState& t : targets_) {
        if (t.id == id.value) return t.picture;
    }
    return false;
}

const TextureDesc* RenderTextures::Find(const TextureId id) const noexcept {
    for (const auto& [value, desc] : live_) {
        if (value == id.value) return std::get_if<TextureDesc>(&desc);
    }
    return nullptr;
}

const RenderTargetDesc* RenderTextures::FindTarget(const TextureId id) const noexcept {
    for (const auto& [value, desc] : live_) {
        if (value == id.value) return std::get_if<RenderTargetDesc>(&desc);
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
