#include "engine/asset/loading/NativeFileAssetSource.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/path/Uri.hpp"
#include "engine/os/Files.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace Engine::Asset::Loading {

// The bytes go through engine/io's native file system, which calls engine/os
// (docs/architecture/io.md): OS limits such as the Windows path length are
// handled there, not here. A relative path is made absolute against the
// current directory first, as the OS would. A failure keeps the cause
// (Base::CauseDetail): the native path, the OS error code and the path length.
Base::Result<ByteBuffer, AssetError>
NativeFileAssetSource::ReadAll(const std::string_view resolvedPath) {
    if (resolvedPath.empty()) {
        return Base::Err(AssetError::Make(
            AssetErrorCode::InvalidPath,
            "NativeFileAssetSource: path is empty"));
    }

    const std::string path(resolvedPath);
    const auto failed = [&path](const auto& cause, const bool causeNamesPath) {
        return Base::Err(AssetError::Make(
            AssetErrorCode::SourceReadFailed,
            "NativeFileAssetSource: cannot read file",
            Base::CauseDetail(cause, causeNamesPath ? std::string_view{} : std::string_view(path))));
    };

    std::string absolute = path;
    if (!Os::Files::IsAbsolutePath(absolute)) {
        auto made = Os::Files::MakeAbsolute(absolute);
        if (!made) return failed(made.error(), false);
        absolute = std::move(made.value());
    }
    auto uri = IO::Path::FileUriFromNativePath(absolute);
    if (!uri) return failed(uri.error(), false);

    IO::FS::NativeFileSystem files;
    auto bytes = IO::Helpers::ReadAllBytes(files, uri.value());
    // The native file system names the absolute path in its detail; the
    // caller's path is added only when it was relative.
    if (!bytes) return failed(bytes.error(), absolute == path);
    return std::move(bytes.value());
}

} // namespace Engine::Asset::Loading
