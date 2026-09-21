#pragma once

#include <cstddef>
#include <vector>

namespace Engine::Asset::Loaders {

// Backend-neutral encoded font data. Runtime loading owns the bytes; a text
// rasterizer adapter decides how to interpret TTF/OTF data. No native font
// object belongs in the asset cache.
struct FontAsset final {
    std::vector<std::byte> bytes;
};

} // namespace Engine::Asset::Loaders
