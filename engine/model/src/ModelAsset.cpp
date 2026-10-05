#include "engine/model/ModelAsset.hpp"

#include <cmath>

namespace Engine::Model {

Math::Matrix4 ToMatrix(const Transform& transform) noexcept {
    return Math::ComposeTRS(transform.translation, transform.rotation, transform.scale);
}

std::optional<std::size_t> ModelAsset::FindClip(const std::string_view name) const noexcept {
    for (std::size_t i=0; i<clips.size(); ++i) if (clips[i].name==name) return i;
    return std::nullopt;
}

std::optional<std::size_t> ModelAsset::FindNode(const std::string_view name) const noexcept {
    for (std::size_t i=0; i<nodes.size(); ++i) if (nodes[i].name==name) return i;
    return std::nullopt;
}

} // namespace Engine::Model
