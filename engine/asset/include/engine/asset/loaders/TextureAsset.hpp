#pragma once

#include <cstdint>
#include <vector>

namespace Engine::Asset::Loaders {

// Backend-neutral decoded image data. Runtime loaders fill this CPU-side
// representation; a render backend decides how and when to upload it.
struct TextureAsset final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
};

} // namespace Engine::Asset::Loaders
