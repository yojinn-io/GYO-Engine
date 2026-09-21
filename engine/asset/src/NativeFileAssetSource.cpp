#include "engine/asset/loading/NativeFileAssetSource.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace Engine::Asset::Loading {

Base::Result<ByteBuffer, AssetError>
NativeFileAssetSource::ReadAll(const std::string_view resolvedPath) {
    if (resolvedPath.empty()) {
        return Base::Result<ByteBuffer, AssetError>::Err(AssetError::Make(
            AssetErrorCode::InvalidPath,
            "NativeFileAssetSource: path is empty"));
    }

    const std::string path(resolvedPath);
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return Base::Result<ByteBuffer, AssetError>::Err(AssetError::Make(
            AssetErrorCode::SourceReadFailed,
            "NativeFileAssetSource: cannot open file",
            path));
    }

    const std::streamoff end = static_cast<std::streamoff>(stream.tellg());
    const auto maximumReadSize = static_cast<std::uintmax_t>(
        (std::min)(static_cast<std::uintmax_t>(
                       (std::numeric_limits<std::size_t>::max)()),
                   static_cast<std::uintmax_t>(
                       (std::numeric_limits<std::streamsize>::max)())));
    if (end < 0 || static_cast<std::uintmax_t>(end) > maximumReadSize) {
        return Base::Result<ByteBuffer, AssetError>::Err(AssetError::Make(
            AssetErrorCode::SourceReadFailed,
            "NativeFileAssetSource: invalid file size",
            path));
    }

    const auto size = static_cast<std::size_t>(end);
    ByteBuffer bytes(size);
    stream.seekg(0, std::ios::beg);
    if (!stream) {
        return Base::Result<ByteBuffer, AssetError>::Err(AssetError::Make(
            AssetErrorCode::SourceReadFailed,
            "NativeFileAssetSource: cannot seek to start of file",
            path));
    }

    if (size != 0) {
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(size));
        if (!stream || static_cast<std::size_t>(stream.gcount()) != size) {
            return Base::Result<ByteBuffer, AssetError>::Err(AssetError::Make(
                AssetErrorCode::SourceReadFailed,
                "NativeFileAssetSource: failed to read complete file",
                path));
        }
    }

    return Base::Result<ByteBuffer, AssetError>::Ok(std::move(bytes));
}

} // namespace Engine::Asset::Loading
