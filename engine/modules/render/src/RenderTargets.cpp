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

} // namespace GYO::Engine::Render
