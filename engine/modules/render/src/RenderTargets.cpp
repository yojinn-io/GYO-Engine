#include "engine/render/RenderTargets.hpp"

#include <algorithm>
#include <atomic>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Render {

namespace {

std::uint64_t NextTable() noexcept {
    static std::atomic<std::uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

RenderTargets::RenderTargets() : table_(NextTable()) {}

Camera::CameraTarget RenderTargets::Create(const SourceTargetDesc desc) {
    if (const auto* fixed = std::get_if<TargetSize>(&desc.size)) {
        GYO_ASSERT(fixed->width > 0 && fixed->height > 0 && "a target of zero size");
    } else {
        const Camera::CameraTarget followed = std::get<Camera::CameraTarget>(desc.size);
        GYO_ASSERT((followed == Camera::kMainTarget || Find(followed) != nullptr) && "a target following an id that is not a live target");
    }
    GYO_ASSERT(next_ != 0 && "every target id has been given");
    const Camera::CameraTarget target{next_++};
    live_.emplace_back(target.id, desc);
    return target;
}

void RenderTargets::Release(const Camera::CameraTarget target) {
    const auto it = std::find_if(live_.begin(), live_.end(), [&](const auto& entry) { return entry.first == target.id; });
    GYO_ASSERT(it != live_.end() && "a release of a target that is not live");
    live_.erase(it);
}

const SourceTargetDesc* RenderTargets::Find(const Camera::CameraTarget target) const noexcept {
    for (const auto& [id, desc] : live_) {
        if (id == target.id) return &desc;
    }
    return nullptr;
}

std::optional<Camera::TargetSize> TargetSizeOf(const SourceTargetDesc& desc, const Camera::CameraTargetSizes& sizes) {
    if (const auto* fixed = std::get_if<TargetSize>(&desc.size)) {
        return Camera::TargetSize{{static_cast<std::int32_t>(fixed->width), static_cast<std::int32_t>(fixed->height)}, 1.0F};
    }
    const Camera::TargetSize* followed = sizes.Find(std::get<Camera::CameraTarget>(desc.size));
    if (followed == nullptr) return std::nullopt;
    return *followed;
}

const TextureId* TargetTextures::Find(const Camera::CameraTarget source) const noexcept {
    for (const Entry& entry : entries) {
        if (entry.source == source.id) return &entry.texture;
    }
    return nullptr;
}

void SyncTargetTextures(const RenderTargets* source, RenderTextures& textures, TargetTextures& mapped) {
    const std::uint64_t table = source != nullptr ? source->Table() : 0;
    if (table != mapped.table) {
        for (const TargetTextures::Entry& gone : mapped.entries) textures.Release(gone.texture);
        mapped.entries.clear();
        mapped.table = table;
    }
    std::vector<TargetTextures::Entry> next;
    if (source != nullptr) {
        // In id order: a followed target (made before its follower) is already in next.
        for (const auto& [id, desc] : source->Live()) {
            RenderTargetDesc made;
            made.filter = desc.filter;
            if (const auto* fixed = std::get_if<TargetSize>(&desc.size)) {
                made.size = *fixed;
            } else if (const Camera::CameraTarget followed = std::get<Camera::CameraTarget>(desc.size); followed != Camera::kMainTarget) {
                const auto of = std::find_if(next.begin(), next.end(), [&](const TargetTextures::Entry& e) { return e.source == followed.id; });
                if (of == next.end()) continue;  // it follows a target the source let go: nothing to take a size from
                made.size = FollowTarget{of->texture};
            }
            // Within one table an id is never given twice and a description never changes: the same id is the same target.
            const auto kept = std::find_if(mapped.entries.begin(), mapped.entries.end(),
                                           [&](const TargetTextures::Entry& e) { return e.source == id; });
            if (kept != mapped.entries.end()) {
                next.push_back(*kept);
                mapped.entries.erase(kept);
            } else {
                next.push_back(TargetTextures::Entry{id, textures.CreateTarget(made)});
            }
        }
    }
    // What is left: targets the source let go (or that follow one it let go). Released after the new ones are made, in
    // reverse, so a follower goes before the target it follows.
    for (auto it = mapped.entries.rbegin(); it != mapped.entries.rend(); ++it) textures.Release(it->texture);
    mapped.entries = std::move(next);
}

std::optional<TextureId> HdrTargets::PictureOf(const std::uint64_t camera) const noexcept {
    for (const Picture& p : pictures) {
        if (p.camera == camera) return p.picture;
    }
    return std::nullopt;
}

std::optional<TextureId> HdrTargets::DepthOf(const std::optional<TextureId>& output) const noexcept {
    for (const Depth& d : depths) {
        if (d.output == output) return d.depth;
    }
    return std::nullopt;
}

void SyncHdrTargets(const std::span<const HdrCamera> cameras, RenderTextures& textures, HdrTargets& targets) {
    const auto make = [&](const std::optional<TextureId>& output, const RenderTargetFormat format) {
        RenderTargetDesc desc;
        desc.size = FollowTarget{output};
        desc.format = format;
        desc.filter = TextureFilter::Nearest;
        return textures.CreateTarget(desc);
    };
    std::vector<HdrTargets::Picture> pictures;
    std::vector<HdrTargets::Depth> depths;
    for (const HdrCamera& camera : cameras) {
        const auto kept = std::find_if(targets.pictures.begin(), targets.pictures.end(), [&](const HdrTargets::Picture& p) {
            return p.camera == camera.camera && p.output == camera.output;
        });
        if (kept != targets.pictures.end()) {
            pictures.push_back(*kept);
            targets.pictures.erase(kept);
        } else {
            pictures.push_back(HdrTargets::Picture{camera.camera, camera.output, make(camera.output, RenderTargetFormat::Rgba16Float)});
        }
        if (std::any_of(depths.begin(), depths.end(), [&](const HdrTargets::Depth& d) { return d.output == camera.output; })) continue;
        const auto keptDepth = std::find_if(targets.depths.begin(), targets.depths.end(),
                                            [&](const HdrTargets::Depth& d) { return d.output == camera.output; });
        if (keptDepth != targets.depths.end()) {
            depths.push_back(*keptDepth);
            targets.depths.erase(keptDepth);
        } else {
            depths.push_back(HdrTargets::Depth{camera.output, make(camera.output, RenderTargetFormat::Depth)});
        }
    }
    for (const HdrTargets::Picture& gone : targets.pictures) textures.Release(gone.picture);
    for (const HdrTargets::Depth& gone : targets.depths) textures.Release(gone.depth);
    targets.pictures = std::move(pictures);
    targets.depths = std::move(depths);
}

} // namespace GYO::Engine::Render
