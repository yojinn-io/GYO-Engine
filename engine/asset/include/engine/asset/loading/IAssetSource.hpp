#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "engine/asset/AssetError.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace Engine::Asset::Loading {
    using AssetError = Base::Error<AssetErrorCode>;

    // バイト列の所有バッファ（I/O結果）
    using ByteBuffer = std::vector<std::byte>;

    // IAssetSource（アイ・アセット・ソース）
    // - 実体の読み出し担当（filesystem / pak / zip / memory などの抽象）
    // - 変換（decode）はしない（Loaderの責務）
    // - 失敗は AssetError に集約
    class IAssetSource {
    public:
        virtual ~IAssetSource() = default;

        virtual Base::Result<ByteBuffer, AssetError>
        ReadAll(std::string_view resolvedPath) = 0;
    };

} // namespace Engine::Asset::Loading
