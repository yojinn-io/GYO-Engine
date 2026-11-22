#include "engine/asset/AssetId.hpp"

namespace GYO::Engine::Asset {

bool IsValidAssetId(const std::string_view text) noexcept {
    if (text.empty() || text.size() > kMaxAssetIdLength) return false;
    for (const char character : text) {
        const bool allowed = (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
                             character == '.' || character == '_' || character == '-';
        if (!allowed) return false;
    }
    return true;
}

} // namespace GYO::Engine::Asset
