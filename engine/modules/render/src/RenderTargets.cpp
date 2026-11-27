#include "engine/render/RenderTargets.hpp"

#include <algorithm>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Render {

Camera::CameraTarget RenderTargets::Create(const RenderTargetDesc desc) {
    GYO_ASSERT((!desc.size || (desc.size->width > 0 && desc.size->height > 0)) && "a target of zero size");
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

const RenderTargetDesc* RenderTargets::Find(const Camera::CameraTarget target) const noexcept {
    for (const auto& [id, desc] : live_) {
        if (id == target.id) return &desc;
    }
    return nullptr;
}

std::optional<Camera::TargetSize> TargetSizeOf(const RenderTargetDesc& desc, const Camera::TargetSize* main) {
    if (desc.size) {
        return Camera::TargetSize{{static_cast<std::int32_t>(desc.size->width), static_cast<std::int32_t>(desc.size->height)}, 1.0F};
    }
    if (main == nullptr) return std::nullopt;
    return *main;
}

const TextureId* TargetTextures::Find(const std::uint32_t source) const noexcept {
    for (const Entry& entry : entries) {
        if (entry.source == source) return &entry.texture;
    }
    return nullptr;
}

void SyncTargetTextures(const RenderTargets* source, RenderTextures& textures, TargetTextures& mapped) {
    std::vector<TargetTextures::Entry> next;
    if (source != nullptr) {
        for (const auto& [id, desc] : source->Live()) {
            const auto kept = std::find_if(mapped.entries.begin(), mapped.entries.end(),
                                           [&](const TargetTextures::Entry& e) { return e.source == id && e.desc == desc; });
            if (kept != mapped.entries.end()) {
                next.push_back(*kept);
                mapped.entries.erase(kept);
            } else {
                next.push_back(TargetTextures::Entry{id, desc, textures.CreateTarget(desc)});
            }
        }
    }
    // What is left: targets the source let go, or whose description changed.
    for (const TargetTextures::Entry& gone : mapped.entries) textures.Release(gone.texture);
    mapped.entries = std::move(next);
}

std::optional<std::uint32_t> PassTargetOf(const std::uint32_t cameraTarget, const TargetTextures& mapped) {
    if (cameraTarget == 0) return 0U;
    const TextureId* texture = mapped.Find(cameraTarget);
    if (texture == nullptr) return std::nullopt;
    return texture->value;
}

} // namespace GYO::Engine::Render
