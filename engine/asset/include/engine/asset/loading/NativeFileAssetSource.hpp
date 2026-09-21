#pragma once

#include <string_view>

#include "engine/asset/loading/IAssetSource.hpp"

namespace Engine::Asset::Loading {

// Reads paths already resolved by AssetPathResolver from the native file
// system. It owns no catalog, cache, or decoding policy.
class NativeFileAssetSource final : public IAssetSource {
public:
    Base::Result<ByteBuffer, AssetError>
    ReadAll(std::string_view resolvedPath) override;
};

} // namespace Engine::Asset::Loading
