#include "adapters/stb_image/StbPngDecode.hpp"

#include <climits>
#include <cstring>
#include <memory>

// The configuration (STBI_ONLY_PNG, STBI_NO_STDIO, ...) comes with the
// gyo_stb_image target, so this header sees the same one as the
// implementation (third_party/stb_image/CMakeLists.txt).
#include "stb_image.h"

namespace GYO::Engine::Image::Detail {
namespace {

struct StbFree final {
    void operator()(stbi_uc* pixels) const noexcept { stbi_image_free(pixels); }
};

} // namespace

std::optional<DecodedRgba8> DecodePngToRgba8(const std::span<const std::byte> bytes) {
    // stb_image takes the length as an int.
    if (bytes.size() > static_cast<std::size_t>(INT_MAX)) return std::nullopt;
    int width = 0;
    int height = 0;
    int channelsInFile = 0;
    // The failure reason and the flip flags of stb_image are global or
    // thread-local state: neither is read nor set here (asset AS-3).
    const std::unique_ptr<stbi_uc, StbFree> pixels(
        stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width,
                              &height, &channelsInFile, STBI_rgb_alpha));
    if (pixels == nullptr || width <= 0 || height <= 0) return std::nullopt;
    DecodedRgba8 out;
    out.width = static_cast<std::uint32_t>(width);
    out.height = static_cast<std::uint32_t>(height);
    out.pixels.resize(std::size_t{out.width} * out.height * 4u);
    std::memcpy(out.pixels.data(), pixels.get(), out.pixels.size());
    return out;
}

} // namespace GYO::Engine::Image::Detail
